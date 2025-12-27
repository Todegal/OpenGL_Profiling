#include "render_context.h"

#include <glm/gtc/matrix_transform.hpp>

RenderContext::RenderContext(GLContext& context, const RawScene& scene, std::shared_ptr<Camera> initialCamera)
    : frameUniforms(), globalTextures(), globalBuffers(), globalResources(), glContext(context),
      camera(initialCamera), sceneMeshes(), opaqueMeshes(), translucentMeshes(), materials(), fullscreenTriBuffer(),
      fullscreenTriVAO()
{
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
                const std::shared_ptr<Mesh> m = std::make_shared<Mesh>();
                sceneMeshes.push_back(m);
                opaqueMeshes.push_back(m);

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
        }

        for (const auto& material : scene.getMaterials())
        {
                const std::shared_ptr<Material> m = std::make_shared<Material>();
                materials.push_back(m);

                // Load the texture
                std::shared_ptr<const RawTexture> albedo = material->getAlbedoTexture();
                if (albedo)
                {
                        m->albedoTexture = std::make_unique<GLTexture2D>(glContext, *albedo);
                        m->albedoTexture->fillMipmaps();
                }
                else { m->albedoTexture = nullptr; }

                m->albedoFactor = material->getAlbedoFactor();

                std::shared_ptr<const RawTexture> metallicRoughness = material->getMetallicRoughnessTexture();
                if (metallicRoughness)
                {
                        m->metallicRoughnessTexture = std::make_unique<GLTexture2D>(glContext, *metallicRoughness);
                        m->metallicRoughnessTexture->fillMipmaps();
                }
                else { m->metallicRoughnessTexture = nullptr; }

                m->metallicRoughnessFactor = material->getAlbedoFactor();

                std::shared_ptr<const RawTexture> normal = material->getNormalTexture();
                if (normal)
                {
                        m->normalTexture = std::make_unique<GLTexture2D>(glContext, *normal);
                        m->normalTexture->fillMipmaps();
                }
                else { m->normalTexture = nullptr; }

                m->normalScale = material->getNormalScale();
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

        gl::glCreateFramebuffers(1, &framebuffer);

        gl::glCreateTextures(gl::GLenum::GL_TEXTURE_2D, 1, &defaultColourTarget);
        gl::glTextureStorage2D(defaultColourTarget, 1, gl::GLenum::GL_RGB8, screenDimensions.x, screenDimensions.y);
        gl::glNamedFramebufferTexture(framebuffer, gl::GLenum::GL_COLOR_ATTACHMENT0, defaultColourTarget, 0);

        gl::glCreateRenderbuffers(1, &depthRenderBuffer);
        gl::glNamedRenderbufferStorage(depthRenderBuffer, gl::GLenum::GL_DEPTH24_STENCIL8, screenDimensions.x,
                                       screenDimensions.y);
        gl::glNamedFramebufferRenderbuffer(framebuffer, gl::GLenum::GL_DEPTH_STENCIL_ATTACHMENT,
                                           gl::GLenum::GL_RENDERBUFFER, depthRenderBuffer);

        if (gl::glCheckNamedFramebufferStatus(framebuffer, gl::GLenum::GL_FRAMEBUFFER) !=
            gl::GLenum::GL_FRAMEBUFFER_COMPLETE)
        {
                throw std::runtime_error("Failed to complete default framebuffer!");
        }
}

void RenderContext::drawScene()
{
        glContext.enable(gl::GLenum::GL_CULL_FACE);
        glContext.enable(gl::GLenum::GL_DEPTH_TEST);

        for (const auto& mesh : sceneMeshes)
        {
                // todo: WORK OUT A TRANSFORM SYSTEM!!
                ObjectMatrices objectMatrices;
                objectMatrices.modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.005f));
                objectMatrices.normalMatrix = glm::transpose(glm::inverse(glm::mat3(objectMatrices.modelMatrix)));

                globalBuffers.at("ObjectBuffer")->subData<ObjectMatrices>(0, {&objectMatrices, 1});

                mesh->vao->bind();
                mesh->ebo->bind(gl::GLenum::GL_ELEMENT_ARRAY_BUFFER);

                gl::glDrawElements(gl::GLenum::GL_TRIANGLES, static_cast<gl::GLsizei>(mesh->vertexCount),
                                   gl::GLenum::GL_UNSIGNED_INT, (void*)0);
        }
}

void RenderContext::drawScene(GLShaderProgram& shaderProgram)
{
        glContext.enable(gl::GLenum::GL_CULL_FACE);
        glContext.enable(gl::GLenum::GL_DEPTH_TEST);

        shaderProgram.useProgram();

        for (const auto& mesh : sceneMeshes)
        {
                // todo: WORK OUT A TRANSFORM SYSTEM!!
                ObjectMatrices objectMatrices;
                objectMatrices.modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.005f));
                objectMatrices.normalMatrix = glm::transpose(glm::inverse(glm::mat3(objectMatrices.modelMatrix)));

                globalBuffers.at("ObjectBuffer")->subData<ObjectMatrices>(0, {&objectMatrices, 1});

                loadMaterialProperties(*materials[mesh->materialIdx], shaderProgram);

                mesh->vao->bind();
                mesh->ebo->bind(gl::GLenum::GL_ELEMENT_ARRAY_BUFFER);

                gl::glDrawElements(gl::GLenum::GL_TRIANGLES, static_cast<gl::GLsizei>(mesh->vertexCount),
                                   gl::GLenum::GL_UNSIGNED_INT, (void*)0);
        }
}

void RenderContext::drawFullscreen()
{
        glContext.disable(gl::GLenum::GL_CULL_FACE);

        fullscreenTriVAO->bind();

        gl::glDrawArrays(gl::GLenum::GL_TRIANGLES, 0, 3);
}

void RenderContext::loadMaterialProperties(const Material& material, GLShaderProgram& shaderProgram)
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

        if (material.normalTexture)
        {
                material.normalTexture->bindUnit(2);

                shaderProgram.setUniformValue("uNormalMap.textureMap", 2);
                shaderProgram.setUniformValue("uNormalMap.isTextureEnabled", true);
        }

        shaderProgram.setUniformValue("uNormalMap.factor", {material.normalScale, 0.0f, 0.0f, 0.0f});
}
