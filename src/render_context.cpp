#include "render_context.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <execution>

static constexpr glm::vec3 scale = glm::vec3(0.005f);

RenderContext::RenderContext(GLContext& context, const RawScene& scene, std::shared_ptr<Camera> initialCamera)
    : renderFlags(), frameUniforms(), globalTextures(), globalBuffers(), globalResources(), glContext(context),
      camera(initialCamera), framebuffer(), defaultColourTarget(), depthRenderbuffer(), sceneMeshes(), opaqueMeshes(),
      translucentMeshes(), materials(), fullscreenTriBuffer(), fullscreenTriVAO()
{
        PROFILE_FUNCTION();

        for (const auto& material : scene.getMaterials())
        {
                const std::shared_ptr<RenderMaterial> m = std::make_shared<RenderMaterial>();
                materials.push_back(m);

                // Load the texture
                std::shared_ptr<const RawTexture> albedo = material->getAlbedoTexture();
                if (albedo)
                {
                        m->albedoTexture = std::make_unique<GLTexture2D>(glContext, *albedo);
                        m->albedoTexture->setParameter(gl::GLenum::GL_TEXTURE_MIN_FILTER,
                                                       gl::GLenum::GL_LINEAR_MIPMAP_LINEAR);
                        m->albedoTexture->fillMipmaps();
                }
                else { m->albedoTexture = nullptr; }

                m->albedoFactor = material->getAlbedoFactor();

                std::shared_ptr<const RawTexture> metallicRoughness = material->getMetallicRoughnessTexture();
                if (metallicRoughness)
                {
                        m->metallicRoughnessTexture = std::make_unique<GLTexture2D>(glContext, *metallicRoughness);
                        m->metallicRoughnessTexture->setParameter(gl::GLenum::GL_TEXTURE_MIN_FILTER,
                                                                  gl::GLenum::GL_LINEAR_MIPMAP_LINEAR);
                        m->metallicRoughnessTexture->fillMipmaps();
                }
                else { m->metallicRoughnessTexture = nullptr; }

                m->metallicRoughnessFactor = material->getAlbedoFactor();

                std::shared_ptr<const RawTexture> normal = material->getNormalTexture();
                if (normal)
                {
                        m->normalTexture = std::make_unique<GLTexture2D>(glContext, *normal);
                        m->normalTexture->setParameter(gl::GLenum::GL_TEXTURE_MIN_FILTER,
                                                       gl::GLenum::GL_LINEAR_MIPMAP_LINEAR);
                        m->normalTexture->fillMipmaps();
                }
                else { m->normalTexture = nullptr; }

                m->normalScale = material->getNormalScale();
        }

        for (const auto& mesh : scene.getMeshes())
        {
                // Define some aliases
                const std::vector<glm::vec3>& positions = mesh->getPositions();
                const std::vector<glm::vec3>& normals = mesh->getNormals();
                const std::vector<glm::vec2>& texCoords = mesh->getTexCoords();

                const std::vector<uint32_t>& indices = mesh->getIndices();

                const std::size_t totalBufferSize = (sizeof(glm::vec3) * positions.size()) +
                                                    (sizeof(glm::vec3) * texCoords.size()) +
                                                    (sizeof(glm::vec2) * normals.size());

                const std::size_t vertexSize = sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec2);

                // Create mesh and allocate size
                const std::shared_ptr<RenderMesh> m = std::make_shared<RenderMesh>();
                sceneMeshes.push_back(m);
                if (scene.getMaterials().at(mesh->getMaterialIndex())->isTranslucent())
                {
                        translucentMeshes.push_back(m);
                }
                else { opaqueMeshes.push_back(m); }

                m->vbo = std::make_unique<GLBuffer>(glContext, totalBufferSize,
                                                    gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT);

                // Interleave data
                std::size_t offset = 0;
                for (std::size_t i = 0; i < positions.size(); ++i)
                {
                        m->vbo->subData<glm::vec3>(offset, {&positions[i], 1});
                        m->vbo->subData<glm::vec3>(offset + sizeof(glm::vec3), {&normals[i], 1});
                        m->vbo->subData<glm::vec2>(offset + (sizeof(glm::vec3) * 2), {&texCoords[i], 1});
                        offset += vertexSize;
                }

                // Fill the index buffer, sane and normal
                m->ebo = std::make_unique<GLBuffer>(glContext, sizeof(std::uint32_t) * indices.size(),
                                                    gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT);
                m->ebo->subData<std::uint32_t>(0, indices);

                m->vertexCount = indices.size();

                // Layout the VAO
                m->vao = std::make_unique<GLVertexArray>(glContext);
                m->vao->bindVertexBuffer(0, *m->vbo, 0, vertexSize);
                m->vao->defineAttribute(0, 0, 3, gl::GLenum::GL_FLOAT, false, 0); // positions
                m->vao->defineAttribute(1, 0, 3, gl::GLenum::GL_FLOAT, false,
                                        static_cast<gl::GLuint>(sizeof(glm::vec3))); // normals
                m->vao->defineAttribute(2, 0, 2, gl::GLenum::GL_FLOAT, false,
                                        static_cast<gl::GLuint>(sizeof(glm::vec3) * 2)); // texcoords

                m->materialIdx = mesh->getMaterialIndex();
                m->localCentre = mesh->getCentre();
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

        // todo: WORK OUT A TRANSFORM SYSTEM!!
        ObjectMatrices objectMatrices;
        objectMatrices.modelMatrix = glm::scale(glm::mat4(1.0f), scale);
        objectMatrices.normalMatrix = glm::transpose(glm::inverse(glm::mat3(objectMatrices.modelMatrix)));

        for (const auto& mesh : sceneMeshes)
        {
                globalBuffers.at("ObjectBuffer")->subData<ObjectMatrices>(0, {&objectMatrices, 1});

                mesh->vao->bind();
                mesh->ebo->bind(gl::GLenum::GL_ELEMENT_ARRAY_BUFFER);

                gl::glDrawElements(gl::GLenum::GL_TRIANGLES, static_cast<gl::GLsizei>(mesh->vertexCount),
                                   gl::GLenum::GL_UNSIGNED_INT, (void*)0);
        }
}

void RenderContext::drawScene(GLShaderProgram& shaderProgram)
{
        PROFILE_FUNCTION();

        glContext.enable(gl::GLenum::GL_CULL_FACE);
        glContext.enable(gl::GLenum::GL_DEPTH_TEST);

        shaderProgram.useProgram();

        // todo: WORK OUT A TRANSFORM SYSTEM!!
        ObjectMatrices objectMatrices;
        objectMatrices.modelMatrix = glm::scale(glm::mat4(1.0f), scale);
        objectMatrices.normalMatrix = glm::transpose(glm::inverse(glm::mat3(objectMatrices.modelMatrix)));

        globalBuffers.at("ObjectBuffer")->subData<ObjectMatrices>(0, {&objectMatrices, 1});

        for (const auto& mesh : sceneMeshes)
        {
                loadMaterialProperties(*materials[mesh->materialIdx], shaderProgram);

                mesh->vao->bind();
                mesh->ebo->bind(gl::GLenum::GL_ELEMENT_ARRAY_BUFFER);

                gl::glDrawElements(gl::GLenum::GL_TRIANGLES, static_cast<gl::GLsizei>(mesh->vertexCount),
                                   gl::GLenum::GL_UNSIGNED_INT, (void*)0);
        }
}

void RenderContext::drawTranslucentScene(GLShaderProgram& shaderProgram)
{
        PROFILE_FUNCTION();

        glContext.enable(gl::GLenum::GL_CULL_FACE);
        glContext.enable(gl::GLenum::GL_DEPTH_TEST);

        glContext.enable(gl::GLenum::GL_BLEND);
        gl::glBlendFunc(gl::GLenum::GL_SRC_ALPHA, gl::GLenum::GL_ONE_MINUS_SRC_ALPHA);
        // gl::glBlendFunc(gl::GLenum::GL_ONE, gl::GLenum::GL_ONE_MINUS_SRC_ALPHA);

        gl::glDepthMask(false);

        shaderProgram.useProgram();

        // todo: WORK OUT A TRANSFORM SYSTEM!!
        ObjectMatrices objectMatrices;
        objectMatrices.modelMatrix = glm::scale(glm::mat4(1.0f), scale);
        objectMatrices.normalMatrix = glm::transpose(glm::inverse(glm::mat3(objectMatrices.modelMatrix)));

        globalBuffers.at("ObjectBuffer")->subData<ObjectMatrices>(0, {&objectMatrices, 1});

        // todo: THIS NEEDS A TRANSFORM SYTEM!!
        std::sort(std::execution::par, translucentMeshes.begin(), translucentMeshes.end(),
                  [&](const std::shared_ptr<RenderMesh>& a, const std::shared_ptr<RenderMesh>& b) -> bool {
                          const glm::vec3 aWorldCentre =
                              glm::vec3(glm::vec4(a->localCentre, 1.0f) * objectMatrices.modelMatrix);
                          const glm::vec3 bWorldCentre =
                              glm::vec3(glm::vec4(b->localCentre, 1.0f) * objectMatrices.modelMatrix);

                          const float aDist =
                              glm::dot(camera->getEye() - aWorldCentre, camera->getEye() - aWorldCentre);
                          const float bDist =
                              glm::dot(camera->getEye() - bWorldCentre, camera->getEye() - bWorldCentre);

                          return aDist > bDist;
                  });

        for (const auto& mesh : translucentMeshes)
        {

                loadMaterialProperties(*materials[mesh->materialIdx], shaderProgram);

                mesh->vao->bind();
                mesh->ebo->bind(gl::GLenum::GL_ELEMENT_ARRAY_BUFFER);

                gl::glDrawElements(gl::GLenum::GL_TRIANGLES, static_cast<gl::GLsizei>(mesh->vertexCount),
                                   gl::GLenum::GL_UNSIGNED_INT, (void*)0);
        }

        gl::glDepthMask(true);
}

void RenderContext::drawOpaqueScene(GLShaderProgram& shaderProgram)
{
        PROFILE_FUNCTION();

        glContext.enable(gl::GLenum::GL_CULL_FACE);
        glContext.enable(gl::GLenum::GL_DEPTH_TEST);
        glContext.disable(gl::GLenum::GL_BLEND);

        shaderProgram.useProgram();

        // todo: WORK OUT A TRANSFORM SYSTEM!!
        ObjectMatrices objectMatrices;
        objectMatrices.modelMatrix = glm::scale(glm::mat4(1.0f), scale);
        objectMatrices.normalMatrix = glm::transpose(glm::inverse(glm::mat3(objectMatrices.modelMatrix)));

        globalBuffers.at("ObjectBuffer")->subData<ObjectMatrices>(0, {&objectMatrices, 1});

        for (const auto& mesh : opaqueMeshes)
        {
                loadMaterialProperties(*materials[mesh->materialIdx], shaderProgram);

                mesh->vao->bind();
                mesh->ebo->bind(gl::GLenum::GL_ELEMENT_ARRAY_BUFFER);

                gl::glDrawElements(gl::GLenum::GL_TRIANGLES, static_cast<gl::GLsizei>(mesh->vertexCount),
                                   gl::GLenum::GL_UNSIGNED_INT, (void*)0);
        }
}

void RenderContext::drawFullscreen()
{
        PROFILE_FUNCTION();

        glContext.disable(gl::GLenum::GL_CULL_FACE);

        fullscreenTriVAO->bind();

        gl::glDrawArrays(gl::GLenum::GL_TRIANGLES, 0, 3);
}

void RenderContext::loadMaterialProperties(const RenderMaterial& material, GLShaderProgram& shaderProgram)
{
        if (material.albedoTexture)
        {
                material.albedoTexture->bindUnit(0);

                shaderProgram.setUniformValue("uBaseColour.textureMap", 0);
                shaderProgram.setUniformValue("uBaseColour.isTextureEnabled", true);
        }

        shaderProgram.setUniformValue("uBaseColour.factor", material.albedoFactor);

        if (material.metallicRoughnessTexture)
        {
                material.metallicRoughnessTexture->bindUnit(1);

                shaderProgram.setUniformValue("uMetallicRoughness.textureMap", 1);
                shaderProgram.setUniformValue("uMetallicRoughness.isTextureEnabled", true);
        }

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
