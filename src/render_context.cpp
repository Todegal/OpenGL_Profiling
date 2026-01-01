#include "render_context.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <execution>

#include "scene_graph.h"

RenderContext::RenderContext(GLContext& context, const RawScene& scene, const SceneGraph& sceneGraph,
                             std::shared_ptr<Camera> initialCamera)
    : glContext(context), sceneGraph(sceneGraph), camera(initialCamera)
{
        PROFILE_FUNCTION();

        textures.reserve(scene.getTextures().size());
        for (const auto& texture : scene.getTextures())
        {
                const auto renderTexture = std::make_shared<GLTexture2D>(glContext, *texture);
                renderTexture->setParameter(gl::GLenum::GL_TEXTURE_MIN_FILTER, gl::GLenum::GL_LINEAR_MIPMAP_LINEAR);
                renderTexture->fillMipmaps();

                textures.push_back(renderTexture);
        }

        materials.reserve(scene.getMaterials().size());
        for (const auto& material : scene.getMaterials())
        {
                const std::shared_ptr<RenderMaterial> renderMaterial = std::make_shared<RenderMaterial>();

                // Load the texture
                if (material->getHasAlbedoTexture())
                {
                        renderMaterial->albedoTexture = textures.at(material->getAlbedoTextureIdx());
                }
                renderMaterial->albedoFactor = material->getAlbedoFactor();

                if (material->getHasMetallicRoughnessTexture())
                {
                        renderMaterial->metallicRoughnessTexture =
                            textures.at(material->getMatallicRoughnessTextureIdx());
                }
                renderMaterial->metallicRoughnessFactor = material->getAlbedoFactor();

                if (material->getHasNormalTexture())
                {
                        renderMaterial->normalTexture = textures.at(material->getNormalTextureIdx());
                }

                renderMaterial->normalScale = material->getNormalScale();

                materials.push_back(renderMaterial);
        }

        meshes.reserve(scene.getMeshes().size());
        for (const auto& mesh : scene.getMeshes())
        {
                // Define some aliases
                const std::vector<RawMesh::Vertex> vertices = mesh->getVertices();
                const std::vector<uint32_t>& indices = mesh->getIndices();

                const std::size_t totalBufferSize = sizeof(RawMesh::Vertex) * vertices.size();

                // Create mesh and allocate size
                const std::shared_ptr<RenderMesh> renderMesh = std::make_shared<RenderMesh>();

                renderMesh->vbo = std::make_unique<GLBuffer>(glContext, totalBufferSize,
                                                             gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT);
                renderMesh->vbo->subData<RawMesh::Vertex>(0, vertices);

                // Fill the index buffer, sane and normal
                renderMesh->ebo = std::make_unique<GLBuffer>(glContext, sizeof(std::uint32_t) * indices.size(),
                                                             gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT);
                renderMesh->ebo->subData<std::uint32_t>(0, indices);

                renderMesh->vertexCount = indices.size();

                // Layout the VAO
                renderMesh->vao = std::make_unique<GLVertexArray>(glContext);
                renderMesh->vao->bindVertexBuffer(0, *renderMesh->vbo, 0, sizeof(RawMesh::Vertex));
                renderMesh->vao->defineAttribute(0, 0, 3, gl::GLenum::GL_FLOAT, false,
                                                 offsetof(RawMesh::Vertex, position)); // positions
                renderMesh->vao->defineAttribute(1, 0, 2, gl::GLenum::GL_FLOAT, false,
                                                 offsetof(RawMesh::Vertex, texCoord)); // texcoords
                renderMesh->vao->defineAttribute(2, 0, 3, gl::GLenum::GL_FLOAT, false,
                                                 offsetof(RawMesh::Vertex, normal)); // normals
                renderMesh->vao->defineAttribute(3, 0, 3, gl::GLenum::GL_FLOAT, false,
                                                 offsetof(RawMesh::Vertex, tangent)); // tangent
                renderMesh->vao->defineAttribute(4, 0, 3, gl::GLenum::GL_FLOAT, false,
                                                 offsetof(RawMesh::Vertex, bitangent)); // bitangent

                renderMesh->material = materials.at(mesh->getMaterialIdx());
                renderMesh->centre = mesh->getCentre();

                meshes.push_back(renderMesh);

                // if (scene.getMaterials().at(mesh->getMaterial())->isTranslucent())
                //{
                //         translucentMeshes.push_back(m);
                // }
                // else { opaqueMeshes.push_back(m); }
        }

        const static std::array<const glm::vec3, 3> fullscreenTri = {
            glm::vec3{1000.0f, -1000.0f, 0.0f}, glm::vec3{0.0f, 1000.0f, 0.0f}, glm::vec3{-1000.0f, -1000.0f, 0.0f}};

        fullscreenTriBuffer = std::make_unique<GLBuffer>(glContext, sizeof(glm::vec3) * fullscreenTri.size(),
                                                         gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT);
        fullscreenTriBuffer->subData<glm::vec3>(0, fullscreenTri);

        fullscreenTriVAO = std::make_unique<GLVertexArray>(glContext);

        fullscreenTriVAO->bindVertexBuffer(0, *fullscreenTriBuffer, 0, sizeof(glm::vec3));
        fullscreenTriVAO->defineAttribute(0, 0, 3, gl::GLenum::GL_FLOAT, false, 0);

        // globalBuffers["ObjectBuffer"] = std::make_unique<GLBuffer>(glContext, sizeof(ObjectMatrices),
        //                                                            gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT);

        const auto& screenDimensions = glContext.getWindow().getFramebufferSize();

        framebuffer = std::make_unique<GLFramebuffer>(glContext);
        defaultColourTarget =
            std::make_unique<GLTexture2D>(glContext, screenDimensions.x, screenDimensions.y, gl::GLenum::GL_RGB8, 1);
        depthRenderbuffer = std::make_unique<GLRenderbuffer>(glContext, screenDimensions.x, screenDimensions.y,
                                                             gl::GLenum::GL_DEPTH24_STENCIL8);

        framebuffer->bindAttachment(gl::GLenum::GL_COLOR_ATTACHMENT0, *defaultColourTarget, 0);
        framebuffer->bindAttachment(gl::GLenum::GL_DEPTH_STENCIL_ATTACHMENT, *depthRenderbuffer);

        if (!framebuffer->isComplete()) { throw std::runtime_error("Failed to complete default framebuffer!"); }
}

void RenderContext::drawScene()
{
        PROFILE_FUNCTION();

        glContext.enable(gl::GLenum::GL_CULL_FACE);
        glContext.enable(gl::GLenum::GL_DEPTH_TEST);

        const auto& nodes = sceneGraph.getNodes();

        for (const auto& node : nodes)
        {
                const auto& transform = node->getWorldTransform();

                ObjectMatrices objectMatrices;
                objectMatrices.modelMatrix = transform.getMatrix();
                objectMatrices.normalMatrix = glm::mat4(transform.getNormalMatrix());

                globalBuffers.at("ObjectBuffer")->subData<ObjectMatrices>(0, {&objectMatrices, 1});

                for (const auto& meshIdx : node->getMeshIndices())
                {
                        const auto& mesh = meshes[meshIdx];

                        mesh->vao->bind();
                        mesh->ebo->bind(gl::GLenum::GL_ELEMENT_ARRAY_BUFFER);

                        gl::glDrawElements(gl::GLenum::GL_TRIANGLES, static_cast<gl::GLsizei>(mesh->vertexCount),
                                           gl::GLenum::GL_UNSIGNED_INT, (void*)0);
                }
        }
}

#pragma optimize("", off)

void RenderContext::drawScene(GLShaderProgram& shaderProgram)
{
        // PROFILE_FUNCTION();

        // glContext.enable(gl::GLenum::GL_CULL_FACE);
        // glContext.enable(gl::GLenum::GL_DEPTH_TEST);

        shaderProgram.useProgram();

        const auto& nodes = sceneGraph.getNodes();
        const auto objectBuffer = globalBuffers.at("ObjectBuffer");

        for (const auto& node : nodes)
        {
                if (node->getMeshIndices().empty()) { continue; }

                const auto worldTransform = node->getWorldTransform();
                ObjectMatrices objectMatrices{};
                objectMatrices.modelMatrix = worldTransform.getMatrix();
                objectMatrices.normalMatrix = glm::mat4(worldTransform.getNormalMatrix());

                objectBuffer->subData<ObjectMatrices>(0, {&objectMatrices, 1});

                for (const auto meshIdx : node->getMeshIndices())
                {
                        const auto& mesh = meshes[meshIdx];
                        const auto material = mesh->material;

                        loadMaterialProperties(*(material), shaderProgram);

                        mesh->vao->bind();

                        mesh->ebo->bind(gl::GLenum::GL_ELEMENT_ARRAY_BUFFER);

                        gl::glDrawElements(gl::GLenum::GL_TRIANGLES, static_cast<gl::GLsizei>(mesh->vertexCount),
                                           gl::GLenum::GL_UNSIGNED_INT, (void*)0);
                }
        }
}

// void RenderContext::drawTranslucentScene(GLShaderProgram& shaderProgram)
//{
//         PROFILE_FUNCTION();
//
//         glContext.enable(gl::GLenum::GL_CULL_FACE);
//         glContext.enable(gl::GLenum::GL_DEPTH_TEST);
//
//         glContext.enable(gl::GLenum::GL_BLEND);
//         gl::glBlendFunc(gl::GLenum::GL_SRC_ALPHA, gl::GLenum::GL_ONE_MINUS_SRC_ALPHA);
//         // gl::glBlendFunc(gl::GLenum::GL_ONE, gl::GLenum::GL_ONE_MINUS_SRC_ALPHA);
//
//         gl::glDepthMask(false);
//
//         shaderProgram.useProgram();
//
//         // todo: WORK OUT A TRANSFORM SYSTEM!!
//         ObjectMatrices objectMatrices;
//         objectMatrices.modelMatrix = localToWorld.getMatrix();
//         objectMatrices.normalMatrix = localToWorld.getNormalMatrix();
//
//         globalBuffers.at("ObjectBuffer")->subData<ObjectMatrices>(0, {&objectMatrices, 1});
//
//         // todo: THIS NEEDS A TRANSFORM SYTEM!!
//         std::sort(std::execution::par, translucentMeshes.begin(), translucentMeshes.end(),
//                   [&](const std::shared_ptr<RenderMesh>& a, const std::shared_ptr<RenderMesh>& b) -> bool {
//                           const Point3<WorldSpace> aCenter =
//                           localToWorld.transformPoint(Point3<LocalSpace>(a->centre)); const Point3<WorldSpace>
//                           bCenter = localToWorld.transformPoint(Point3<LocalSpace>(b->centre));
//
//                           const float aDistance = aCenter.distance(camera->getEye());
//                           const float bDistance = bCenter.distance(camera->getEye());
//
//                           return aDistance > bDistance;
//                   });
//
//         for (const auto& mesh : translucentMeshes)
//         {
//
//                 loadMaterialProperties(*materials[mesh->materialIdx], shaderProgram);
//
//                 mesh->vao->bind();
//                 mesh->ebo->bind(gl::GLenum::GL_ELEMENT_ARRAY_BUFFER);
//
//                 gl::glDrawElements(gl::GLenum::GL_TRIANGLES, static_cast<gl::GLsizei>(mesh->vertexCount),
//                                    gl::GLenum::GL_UNSIGNED_INT, (void*)0);
//         }
//
//         gl::glDepthMask(true);
// }
//
// void RenderContext::drawOpaqueScene(GLShaderProgram& shaderProgram)
//{
//         PROFILE_FUNCTION();
//
//         glContext.enable(gl::GLenum::GL_CULL_FACE);
//         glContext.enable(gl::GLenum::GL_DEPTH_TEST);
//         glContext.disable(gl::GLenum::GL_BLEND);
//
//         shaderProgram.useProgram();
//
//         // todo: WORK OUT A TRANSFORM SYSTEM!!
//         ObjectMatrices objectMatrices;
//         objectMatrices.modelMatrix = localToWorld.getMatrix();
//         objectMatrices.normalMatrix = localToWorld.getNormalMatrix();
//
//         globalBuffers.at("ObjectBuffer")->subData<ObjectMatrices>(0, {&objectMatrices, 1});
//
//         for (const auto& mesh : opaqueMeshes)
//         {
//                 loadMaterialProperties(*materials[mesh->materialIdx], shaderProgram);
//
//                 mesh->vao->bind();
//                 mesh->ebo->bind(gl::GLenum::GL_ELEMENT_ARRAY_BUFFER);
//
//                 gl::glDrawElements(gl::GLenum::GL_TRIANGLES, static_cast<gl::GLsizei>(mesh->vertexCount),
//                                    gl::GLenum::GL_UNSIGNED_INT, (void*)0);
//         }
// }

void RenderContext::drawFullscreen()
{
        PROFILE_FUNCTION();

        glContext.disable(gl::GLenum::GL_CULL_FACE);

        fullscreenTriVAO->bind();

        gl::glDrawArrays(gl::GLenum::GL_TRIANGLES, 0, 3);
}

void RenderContext::resize()
{
        const auto& screenDimensions = glContext.getWindow().getFramebufferSize();

        framebuffer = std::make_unique<GLFramebuffer>(glContext);
        defaultColourTarget =
            std::make_unique<GLTexture2D>(glContext, screenDimensions.x, screenDimensions.y, gl::GLenum::GL_RGB8, 1);
        depthRenderbuffer = std::make_unique<GLRenderbuffer>(glContext, screenDimensions.x, screenDimensions.y,
                                                             gl::GLenum::GL_DEPTH24_STENCIL8);

        framebuffer->bindAttachment(gl::GLenum::GL_COLOR_ATTACHMENT0, *defaultColourTarget, 0);
        framebuffer->bindAttachment(gl::GLenum::GL_DEPTH_STENCIL_ATTACHMENT, *depthRenderbuffer);

        if (!framebuffer->isComplete()) { throw std::runtime_error("Failed to complete default framebuffer!"); }
}

void RenderContext::loadMaterialProperties(const RenderMaterial& material, GLShaderProgram& shaderProgram)
{
        if (material.albedoTexture)
        {
                material.albedoTexture->bindUnit(0);

                shaderProgram.setUniformValue("uBaseColour.textureMap", 0);
                shaderProgram.setUniformValue("uBaseColour.isTextureEnabled", true);
        }
        else { shaderProgram.setUniformValue("uBaseColour.isTextureEnabled", false); }

        shaderProgram.setUniformValue("uBaseColour.factor", material.albedoFactor);

        if (material.metallicRoughnessTexture)
        {
                material.metallicRoughnessTexture->bindUnit(1);

                shaderProgram.setUniformValue("uMetallicRoughness.textureMap", 1);
                shaderProgram.setUniformValue("uMetallicRoughness.isTextureEnabled", true);
        }
        else { shaderProgram.setUniformValue("uMetallicRoughness.isTextureEnabled", false); }

        shaderProgram.setUniformValue("uMetallicRoughness.factor", material.metallicRoughnessFactor);

        if (material.normalTexture && renderFlags.get<RenderFlags::NORMALS_ENABLED>())
        {
                material.normalTexture->bindUnit(2);

                shaderProgram.setUniformValue("uNormalMap.textureMap", 2);
                shaderProgram.setUniformValue("uNormalMap.isTextureEnabled", true);
        }
        else { shaderProgram.setUniformValue("uNormalMap.isTextureEnabled", false); }

        shaderProgram.setUniformValue("uNormalMap.factor", {material.normalScale, 0.0f, 0.0f, 0.0f});
}
