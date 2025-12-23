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
      frameUniformsBuffer(std::make_unique<GLImmutableBuffer>(glContext, sizeof(FrameUniforms),
                                                              gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT)),
      objectMatricesBuffer(std::make_unique<GLImmutableBuffer>(glContext, sizeof(ObjectMatrices),
                                                               gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT)),
      pointLights(1), pointLightBuffer(std::make_unique<GLMutableBuffer>(glContext, 0, gl::GLenum::GL_STATIC_DRAW))
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

                const std::array<std::size_t, 3> offsets = {0, sizeof(glm::vec3) * positions.size(),
                                                            sizeof(glm::vec3) * positions.size() +
                                                                sizeof(glm::vec3) * normals.size()};

                // Create mesh and allocate size
                const std::shared_ptr<Mesh> m = std::make_shared<Mesh>(glContext);
                meshes.push_back(m);

                m->vbo = std::make_unique<GLImmutableBuffer>(glContext, totalBufferSize,
                                                             gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT);

                m->vbo->subData<glm::vec3>(offsets[0], positions);
                m->vbo->subData<glm::vec3>(offsets[1], normals);
                m->vbo->subData<glm::vec2>(offsets[2], texCoords);

                // Fill the index buffer, sane and normal
                m->ebo = std::make_unique<GLImmutableBuffer>(glContext, std::span<const std::uint32_t>(indices),
                                                             gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT);

                // m->ebo.bufferData<uint32_t>(indices, gl::GLenum::GL_STATIC_DRAW); // BEAUTIFUL
                m->vertexCount = indices.size();

                // Layout the VAO
                m->vao.bind();
                m->vbo->bind(gl::GLenum::GL_ARRAY_BUFFER);

                gl::glEnableVertexAttribArray(0); // positions
                gl::glVertexAttribPointer(0, 3, gl::GLenum::GL_FLOAT, gl::GL_FALSE, 0, (void*)offsets[0]);

                gl::glEnableVertexAttribArray(1); // normals
                gl::glVertexAttribPointer(1, 3, gl::GLenum::GL_FLOAT, gl::GL_FALSE, 0, (void*)offsets[1]);

                gl::glEnableVertexAttribArray(2); // texcoords
                gl::glVertexAttribPointer(2, 2, gl::GLenum::GL_FLOAT, gl::GL_FALSE, 0, (void*)offsets[2]);

                // Load the texture
                const std::unique_ptr<RawTexture>& albedo = materials[mesh->getMaterialIndex()]->getAlbedoTexture();
                if (albedo)
                {
                        gl::GLenum format;
                        switch (albedo->getChannels())
                        {
                        case 1:
                                format = gl::GLenum::GL_R;
                                break;
                        case 2:
                                format = gl::GLenum::GL_RG;
                                break;
                        case 3:
                                format = gl::GLenum::GL_RGB;
                                break;
                        default:
                                format = gl::GLenum::GL_RGBA;
                                break;
                        }

                        gl::glCreateTextures(gl::GLenum::GL_TEXTURE_2D, 1, &m->albedoTexture);

                        gl::glTextureStorage2D(m->albedoTexture, 1, gl::GLenum::GL_RGBA32F, albedo->getWidth(),
                                               albedo->getHeight());
                        gl::glTextureSubImage2D(m->albedoTexture, 0, 0, 0, albedo->getWidth(), albedo->getHeight(),
                                                format, gl::GLenum::GL_UNSIGNED_BYTE, albedo->getData().data());

                        gl::glTextureParameteri(m->albedoTexture, gl::GLenum::GL_TEXTURE_MIN_FILTER,
                                                gl::GLenum::GL_LINEAR_MIPMAP_LINEAR);

                        gl::glGenerateTextureMipmap(m->albedoTexture);
                }
                else
                {
                        m->albedoTexture = 0;
                }

                m->albedoFactor = materials[mesh->getMaterialIndex()]->getAlbedoFactor();

                const std::unique_ptr<RawTexture>& metallicRoughness =
                    materials[mesh->getMaterialIndex()]->getMetallicRoughnessTexture();
                if (metallicRoughness)
                {
                        gl::GLenum format;
                        switch (metallicRoughness->getChannels())
                        {
                        case 1:
                                format = gl::GLenum::GL_R;
                                break;
                        case 2:
                                format = gl::GLenum::GL_RG;
                                break;
                        case 3:
                                format = gl::GLenum::GL_RGB;
                                break;
                        default:
                                format = gl::GLenum::GL_RGBA;
                                break;
                        }

                        gl::glCreateTextures(gl::GLenum::GL_TEXTURE_2D, 1, &m->metallicRoughnessTexture);

                        gl::glTextureStorage2D(m->metallicRoughnessTexture, 1, gl::GLenum::GL_RGBA32F,
                                               static_cast<gl::GLsizei>(metallicRoughness->getWidth()),
                                               static_cast<gl::GLsizei>(metallicRoughness->getHeight()));
                        gl::glTextureSubImage2D(m->metallicRoughnessTexture, 0, 0, 0,
                                                static_cast<gl::GLsizei>(metallicRoughness->getWidth()),
                                                static_cast<gl::GLsizei>(metallicRoughness->getHeight()), format,
                                                gl::GLenum::GL_UNSIGNED_BYTE, metallicRoughness->getData().data());

                        gl::glTextureParameteri(m->metallicRoughnessTexture, gl::GLenum::GL_TEXTURE_MIN_FILTER,
                                                gl::GLenum::GL_LINEAR_MIPMAP_LINEAR);

                        gl::glGenerateTextureMipmap(m->metallicRoughnessTexture);
                }
                else
                {
                        m->metallicRoughnessTexture = 0;
                }

                m->metallicRoughnessFactor = materials[mesh->getMaterialIndex()]->getAlbedoFactor();
        }

        // NOW, let's create the uniform buffers
        {
                PROFILE_SCOPE("create_render_buffers");

                objectMatricesBuffer->bindBase(gl::GLenum::GL_UNIFORM_BUFFER, 0);
                shaderProgram.setUniformBlockBinding("ObjectBuffer", 0);

                frameUniformsBuffer->bindBase(gl::GLenum::GL_UNIFORM_BUFFER, 1);
                // shaderProgram.setUniformBlockBinding("FrameUniformsBuffer", 1);
                gl::GLuint blockIndex = gl::glGetUniformBlockIndex(shaderProgram.getProgramId(), "FrameUniformsBuffer");
                gl::glUniformBlockBinding(shaderProgram.getProgramId(), blockIndex, 1);

                pointLightBuffer->bindBase(gl::GLenum::GL_SHADER_STORAGE_BUFFER, 0);
                shaderProgram.setShaderStorageBlockBinding("PointLightBuffer", 0);
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

        glContext.enable(gl::GLenum::GL_CULL_FACE);
        gl::glCullFace(gl::GLenum::GL_BACK);

        const glm::ivec2 screenDimensions = glContext.getWindow().getFramebufferSize();
        gl::glViewport(0, 0, screenDimensions.x, screenDimensions.y);

        gl::glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        gl::glClear(gl::ClearBufferMask::GL_COLOR_BUFFER_BIT | gl::ClearBufferMask::GL_DEPTH_BUFFER_BIT);

        // Update uniform buffers
        {
                PROFILE_SCOPE("update_uniform_buffers");

                frameUniforms.projectionMatrix = glm::perspective(
                    glm::radians(camera.getFov()),
                    static_cast<float>(screenDimensions.x) / static_cast<float>(screenDimensions.y), 0.01f, 100000.0f);
                frameUniforms.viewMatrix = camera.getViewMatrix();
                frameUniforms.cameraPosition = camera.getEye();
                frameUniforms.numPointLights = static_cast<int>(pointLights.size());

                // matrixUniforms.modelMatrix =
                //     glm::rotate(glm::mat4(1.0), timer.getElapsedTimeSeconds(), glm::vec3(0, 1, 0));
                objectMatrices.modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
                objectMatrices.normalMatrix = glm::transpose(glm::inverse(glm::mat3(objectMatrices.modelMatrix)));

                pointLights[0].colour = glm::vec4(20.0f, 20.0f, 20.0f, 0.0f);
                pointLights[0].position = glm::vec4(0.0f, 2.0f, 1.0f, 0.0f);

                frameUniformsBuffer->subData<FrameUniforms>(0, std::span(&frameUniforms, 1));
                objectMatricesBuffer->subData<ObjectMatrices>(0, std::span(&objectMatrices, 1));
                pointLightBuffer->setData<PointLight>(pointLights, gl::GLenum::GL_STATIC_DRAW);
        }

        shaderProgram.useProgram();

        for (const auto& m : meshes)
        {
                if (m->albedoTexture)
                {
                        gl::glBindTextureUnit(0, m->albedoTexture);

                        shaderProgram.setUniformValue("uBaseColour.textureMap", 0);
                        shaderProgram.setUniformValue("uBaseColour.isTextureEnabled", true);
                }

                shaderProgram.setUniformValue("uBaseColour.factor", m->albedoFactor);

                if (m->metallicRoughnessTexture)
                {
                        gl::glBindTextureUnit(1, m->metallicRoughnessTexture);

                        shaderProgram.setUniformValue("uMetallicRoughness.textureMap", 1);
                        shaderProgram.setUniformValue("uMetallicRoughness.isTextureEnabled", true);
                }

                shaderProgram.setUniformValue("uMetallicRoughness.factor", m->metallicRoughnessFactor);

                m->vao.bind();

                m->ebo->bind(gl::GLenum::GL_ELEMENT_ARRAY_BUFFER);

                gl::glDrawElements(gl::GLenum::GL_TRIANGLES, static_cast<gl::GLsizei>(m->vertexCount),
                                   gl::GLenum::GL_UNSIGNED_INT, (void*)0);
        }
}
