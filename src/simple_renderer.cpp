#include "simple_renderer.h"

#include <GLFW/glfw3.h>
#include <glbinding/gl/bitfield.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "opengl_context.h"
#include "profiler.h"

#include <array>

SimpleRenderer::SimpleRenderer(GLContext& glContext, const RawScene& scene, const Camera& camera, const Timer<>& timer)
    : glContext(glContext), timer(timer), camera(camera),
      shaderProgram(glContext, {std::make_shared<GLShader>(glContext, "shaders/forward_pass/forward_pass.vert.glsl",
                                                           gl::GLenum::GL_VERTEX_SHADER),
                                std::make_shared<GLShader>(glContext, "shaders/forward_pass/forward_pass.frag.glsl",
                                                           gl::GLenum::GL_FRAGMENT_SHADER)}),
      hdrShaderProgram(glContext, {std::make_shared<GLShader>(glContext, "shaders/hdr_pass/hdr_pass.vert.glsl",
                                                              gl::GLenum::GL_VERTEX_SHADER),
                                   std::make_shared<GLShader>(glContext, "shaders/hdr_pass/hdr_pass.frag.glsl",
                                                              gl::GLenum::GL_FRAGMENT_SHADER)}),
      frameUniformsBuffer(
          std::make_unique<GLBuffer>(glContext, sizeof(FrameUniforms), gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT)),
      objectMatricesBuffer(
          std::make_unique<GLBuffer>(glContext, sizeof(ObjectMatrices), gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT)),
      pointLights(4), pointLightBuffer(std::make_unique<GLBuffer>(glContext, sizeof(PointLight) * pointLights.size(),
                                                                  gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT))
{
        PROFILE_FUNCTION();

        // Load scene meshes
        // -- Load buffers
        // -- Layout VAO
        // -- Load texture
        //
        // Load Uniform Buffers

        meshes.reserve(scene.getMeshes().size());

        const auto& materials = scene.getMaterials();

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
                const std::shared_ptr<RenderMesh> m = std::make_shared<RenderMesh>(glContext);
                meshes.push_back(m);

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
                m->vao.bindVertexBuffer(0, *m->vbo, 0, vertexSize);
                m->vao.defineAttribute(0, 0, 3, gl::GLenum::GL_FLOAT, false, 0); // positions
                m->vao.defineAttribute(1, 0, 3, gl::GLenum::GL_FLOAT, false,
                                       static_cast<gl::GLuint>(sizeof(glm::vec3))); // normals
                m->vao.defineAttribute(2, 0, 2, gl::GLenum::GL_FLOAT, false,
                                       static_cast<gl::GLuint>(sizeof(glm::vec3) * 2)); // texcoords

                // Load the texture
                std::shared_ptr<const RawTexture> albedo = materials[mesh->getMaterial()]->getAlbedoTexture();
                if (albedo)
                {
                        m->albedoTexture = std::make_unique<GLTexture2D>(glContext, *albedo);
                        m->albedoTexture->setParameter(gl::GLenum::GL_TEXTURE_MIN_FILTER,
                                                       gl::GLenum::GL_LINEAR_MIPMAP_LINEAR);
                        m->albedoTexture->fillMipmaps();
                }
                else { m->albedoTexture = nullptr; }

                m->albedoFactor = materials[mesh->getMaterial()]->getAlbedoFactor();

                std::shared_ptr<const RawTexture> metallicRoughness =
                    materials[mesh->getMaterial()]->getMetallicRoughnessTexture();
                if (metallicRoughness)
                {
                        m->metallicRoughnessTexture = std::make_unique<GLTexture2D>(glContext, *metallicRoughness);
                        m->metallicRoughnessTexture->setParameter(gl::GLenum::GL_TEXTURE_MIN_FILTER,
                                                       gl::GLenum::GL_LINEAR_MIPMAP_LINEAR);
                        m->metallicRoughnessTexture->fillMipmaps();
                }
                else { m->metallicRoughnessTexture = nullptr; }

                m->metallicRoughnessFactor = materials[mesh->getMaterial()]->getAlbedoFactor();

                std::shared_ptr<const RawTexture> normal = materials[mesh->getMaterial()]->getNormalTexture();
                if (normal)
                {
                        m->normalTexture = std::make_unique<GLTexture2D>(glContext, *normal);
                        m->normalTexture->setParameter(gl::GLenum::GL_TEXTURE_MIN_FILTER,
                                                       gl::GLenum::GL_LINEAR_MIPMAP_LINEAR);
                        m->normalTexture->fillMipmaps();
                }
                else { m->normalTexture = nullptr; }

                m->normalScale = materials[mesh->getMaterial()]->getNormalScale();
        }

        // NOW, let's create the uniform buffers
        {
                PROFILE_SCOPE("create_render_buffers");

                objectMatricesBuffer->bindBase(gl::GLenum::GL_UNIFORM_BUFFER, 0);
                shaderProgram.setUniformBlockBinding("ObjectBuffer", 0);

                frameUniformsBuffer->bindBase(gl::GLenum::GL_UNIFORM_BUFFER, 1);
                shaderProgram.setUniformBlockBinding("FrameUniformsBuffer", 1);

                pointLightBuffer->bindBase(gl::GLenum::GL_SHADER_STORAGE_BUFFER, 0);
                shaderProgram.setShaderStorageBlockBinding("PointLightBuffer", 0);
        }

        {
                PROFILE_SCOPE("create_framebuffer");

                gl::glCreateFramebuffers(1, &hdrFramebuffer);

                gl::glCreateTextures(gl::GLenum::GL_TEXTURE_2D, 1, &hdrTexture);

                const auto& dimensions = glContext.getWindow().getFramebufferSize();
                gl::glTextureStorage2D(hdrTexture, 1, gl::GLenum::GL_RGB32F, dimensions.x, dimensions.y);
                gl::glNamedFramebufferTexture(hdrFramebuffer, gl::GLenum::GL_COLOR_ATTACHMENT0, hdrTexture, 0);

                gl::GLuint depthRBO;
                gl::glCreateRenderbuffers(1, &depthRBO);
                gl::glNamedRenderbufferStorage(depthRBO, gl::GLenum::GL_DEPTH24_STENCIL8, dimensions.x, dimensions.y);
                gl::glNamedFramebufferRenderbuffer(hdrFramebuffer, gl::GLenum::GL_DEPTH_STENCIL_ATTACHMENT,
                                                   gl::GLenum::GL_RENDERBUFFER, depthRBO);

                if (gl::glCheckNamedFramebufferStatus(hdrFramebuffer, gl::GLenum::GL_FRAMEBUFFER) !=
                    gl::GLenum::GL_FRAMEBUFFER_COMPLETE)
                {
                        throw std::runtime_error("Failed to complete HDR Framebuffer!");
                }

                std::array<const glm::vec3, 3> fullscreenTri = {glm::vec3{1000.0f, -1000.0f, 0.0f},
                                                                glm::vec3{0.0f, 1000.0f, 0.0f},
                                                                glm::vec3{-1000.0f, -1000.0f, 0.0f}};

                fullscreenTriBuffer = std::make_unique<GLBuffer>(glContext, sizeof(glm::vec3) * fullscreenTri.size(),
                                                                 gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT);
                fullscreenTriBuffer->subData<glm::vec3>(0, fullscreenTri);

                fullscreenTriVAO = std::make_unique<GLVertexArray>(glContext);

                fullscreenTriVAO->bindVertexBuffer(0, *fullscreenTriBuffer, 0, sizeof(glm::vec3));
                fullscreenTriVAO->defineAttribute(0, 0, 3, gl::GLenum::GL_FLOAT, false, 0);
        }
}

SimpleRenderer::~SimpleRenderer()
{
}

void SimpleRenderer::render()
{
        PROFILE_FUNCTION();
        // Set viewport
        // Clear the screen
        // Update uniform buffers
        // Enable the shader
        // Render each mesh

        const auto& screenDimensions = glm::max(glContext.getWindow().getFramebufferSize(), {1.0f, 1.0f});

        const static auto clearColour = glm::vec4(glm::vec3(0.15f), 1.0f);
        const static auto clearDepth = 1.0f;

        glContext.enable(gl::GLenum::GL_CULL_FACE);
        glContext.enable(gl::GLenum::GL_DEPTH_TEST);

        gl::glBindFramebuffer(gl::GLenum::GL_DRAW_FRAMEBUFFER, hdrFramebuffer);
        gl::glViewport(0, 0, screenDimensions.x, screenDimensions.y);

        gl::glClearNamedFramebufferfv(hdrFramebuffer, gl::GLenum::GL_COLOR, 0, glm::value_ptr(clearColour));
        gl::glClearNamedFramebufferfv(hdrFramebuffer, gl::GLenum::GL_DEPTH, 0, &clearDepth);

        // Update uniform buffers
        {
                PROFILE_SCOPE("update_uniform_buffers");

                frameUniforms.projectionMatrix = glm::perspective(
                    glm::radians(camera.getFov()),
                    static_cast<float>(screenDimensions.x) / static_cast<float>(screenDimensions.y), 0.01f, 100000.0f);
                frameUniforms.viewMatrix = camera.getViewTransform().getMatrix();
                frameUniforms.cameraPosition = camera.getEye().getv();
                frameUniforms.numPointLights = static_cast<int>(pointLights.size());

                // matrixUniforms.modelMatrix =
                //     glm::rotate(glm::mat4(1.0), timer.getElapsedTimeSeconds(), glm::vec3(0, 1, 0));
                objectMatrices.modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.005f));
                objectMatrices.normalMatrix = glm::transpose(glm::inverse(glm::mat3(objectMatrices.modelMatrix)));

                pointLights[0].colour = glm::vec4(10.0f, 0.0f, 0.0f, 0.0f);
                pointLights[0].position = glm::vec4(2.0f, 1.0f, 0.0f, 0.0f);

                pointLights[1].colour = glm::vec4(0.0f, 10.0f, 0.0f, 0.0f);
                pointLights[1].position = glm::vec4(0.0f, 3.0f, 0.0f, 0.0f);

                pointLights[2].colour = glm::vec4(0.0f, 0.0f, 10.0f, 0.0f);
                pointLights[2].position = glm::vec4(0.0f, 1.0f, 2.0f, 0.0f);

                pointLights[3].colour = glm::vec4(5.0f, 5.0f, 5.0f, 0.0f);
                pointLights[3].position = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);

                frameUniformsBuffer->subData<FrameUniforms>(0, std::span(&frameUniforms, 1));
                objectMatricesBuffer->subData<ObjectMatrices>(0, std::span(&objectMatrices, 1));
                pointLightBuffer->subData<PointLight>(0, pointLights);
        }

        shaderProgram.useProgram();

        for (const auto& m : meshes)
        {
                if (m->albedoTexture)
                {
                        m->albedoTexture->bindUnit(0);

                        shaderProgram.setUniformValue("uBaseColour.textureMap", 0);
                        shaderProgram.setUniformValue("uBaseColour.isTextureEnabled", true);
                }

                shaderProgram.setUniformValue("uBaseColour.factor", m->albedoFactor);

                if (m->metallicRoughnessTexture)
                {
                        m->metallicRoughnessTexture->bindUnit(1);

                        shaderProgram.setUniformValue("uMetallicRoughness.textureMap", 1);
                        shaderProgram.setUniformValue("uMetallicRoughness.isTextureEnabled", true);
                }

                shaderProgram.setUniformValue("uMetallicRoughness.factor", m->metallicRoughnessFactor);

                if (m->normalTexture)
                {
                        m->normalTexture->bindUnit(2);

                        shaderProgram.setUniformValue("uNormalMap.textureMap", 2);
                        shaderProgram.setUniformValue("uNormalMap.isTextureEnabled", true);
                }

                shaderProgram.setUniformValue("uNormalMap.factor", {m->normalScale, 0.0f, 0.0f, 0.0f});

                m->vao.bind();

                m->ebo->bind(gl::GLenum::GL_ELEMENT_ARRAY_BUFFER);

                gl::glDrawElements(gl::GLenum::GL_TRIANGLES, static_cast<gl::GLsizei>(m->vertexCount),
                                   gl::GLenum::GL_UNSIGNED_INT, (void*)0);
        }

        gl::glBindFramebuffer(gl::GLenum::GL_FRAMEBUFFER, 0);
        gl::glViewport(0, 0, screenDimensions.x, screenDimensions.y);

        hdrShaderProgram.useProgram();

        gl::glBindTextureUnit(0, hdrTexture);
        hdrShaderProgram.setUniformValue("uColourTexture", 0);
        hdrShaderProgram.setUniformValue("uScreenDimensions", screenDimensions);

        fullscreenTriVAO->bind();

        glContext.disable(gl::GLenum::GL_CULL_FACE);
        glContext.disable(gl::GLenum::GL_DEPTH_TEST);

        gl::glDrawArrays(gl::GLenum::GL_TRIANGLES, 0, 3);
}
