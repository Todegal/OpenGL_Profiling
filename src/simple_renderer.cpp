#include "simple_renderer.h"

#include <GLFW/glfw3.h>
#include <glbinding/gl/bitfield.h>
#include <glbinding/gl/functions.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "opengl_context.h"
#include "profiler.h"

#include <array>
#include <cstddef>

SimpleRenderer::SimpleRenderer(GLContext& glContext, const RawScene& scene, const Camera& camera, const Timer<>& timer)
    : glContext(glContext), timer(timer), camera(camera),
      shaderProgram(glContext, {std::make_shared<GLShader>(glContext, "shaders/forward_pass/forward_pass.vert.glsl",
                                                           gl::GLenum::GL_VERTEX_SHADER),
                                std::make_shared<GLShader>(glContext, "shaders/forward_pass/forward_pass.frag.glsl",
                                                           gl::GLenum::GL_FRAGMENT_SHADER)}),
      frameUniformsBuffer(glContext), objectMatricesBuffer(glContext), pointLights(1), pointLightBuffer(glContext)
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

                const gl::GLsizei totalBufferSize = (sizeof(glm::vec3) * positions.size()) +
                                                    (sizeof(glm::vec3) * texCoords.size()) +
                                                    (sizeof(glm::vec2) * normals.size());

                const std::array<std::uintptr_t, 3> offsets = {0, sizeof(glm::vec3) * positions.size(),
                                                               sizeof(glm::vec3) * positions.size() +
                                                                   sizeof(glm::vec3) * normals.size()};

                // Create mesh and allocate size
                const std::shared_ptr<Mesh> m = std::make_shared<Mesh>(glContext);
                meshes.push_back(m);

                m->vbo.allocate<std::byte>(totalBufferSize, gl::GLenum::GL_DYNAMIC_DRAW);

                m->vbo.subData<glm::vec3>(offsets[0], positions);
                m->vbo.subData<glm::vec3>(offsets[1], normals);
                m->vbo.subData<glm::vec2>(offsets[2], texCoords);

                // Fill the index buffer, sane and normal
                m->ebo.bufferData<uint32_t>(indices, gl::GLenum::GL_STATIC_DRAW); // BEAUTIFUL
                m->vertexCount = indices.size();

                // Layout the VAO
                m->vao.bind();
                m->vbo.bind<gl::GLenum::GL_ARRAY_BUFFER>();

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

                        gl::glGenTextures(1, &m->albedoTexture);
                        gl::glBindTexture(gl::GLenum::GL_TEXTURE_2D, m->albedoTexture);
                        gl::glTexImage2D(gl::GLenum::GL_TEXTURE_2D, 0, gl::GLenum::GL_RGBA, albedo->getWidth(),
                                         albedo->getHeight(), 0, format, gl::GLenum::GL_UNSIGNED_BYTE,
                                         albedo->getData().data());

                        gl::glGenerateMipmap(gl::GLenum::GL_TEXTURE_2D);
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

                        gl::glGenTextures(1, &m->metallicRoughnessTexture);
                        gl::glBindTexture(gl::GLenum::GL_TEXTURE_2D, m->metallicRoughnessTexture);
                        gl::glTexImage2D(gl::GLenum::GL_TEXTURE_2D, 0, gl::GLenum::GL_RGBA,
                                         metallicRoughness->getWidth(), metallicRoughness->getHeight(), 0, format,
                                         gl::GLenum::GL_UNSIGNED_BYTE, metallicRoughness->getData().data());

                        gl::glGenerateMipmap(gl::GLenum::GL_TEXTURE_2D);
                }

                m->metallicRoughnessFactor = materials[mesh->getMaterialIndex()]->getAlbedoFactor();
        }

        // NOW, let's create the uniform buffers
        {
                PROFILE_SCOPE("create_render_buffers");

                frameUniformsBuffer.allocate<FrameUniforms>(1, gl::GLenum::GL_DYNAMIC_DRAW);
                objectMatricesBuffer.allocate<ObjectMatrices>(1, gl::GLenum::GL_DYNAMIC_DRAW); // ALSO BEAUTIFUL
                pointLightBuffer.allocate<PointLight>(pointLights.size(), gl::GLenum::GL_DYNAMIC_DRAW);

                objectMatricesBuffer.bindBase<gl::GLenum::GL_UNIFORM_BUFFER>(0);
                gl::GLuint blockIndex = gl::glGetUniformBlockIndex(shaderProgram.getProgramId(), "ObjectBuffer");
                gl::glUniformBlockBinding(shaderProgram.getProgramId(), blockIndex, 0);

                frameUniformsBuffer.bindBase<gl::GLenum::GL_UNIFORM_BUFFER>(1);
                blockIndex = gl::glGetUniformBlockIndex(shaderProgram.getProgramId(), "FrameUniformsBuffer");
                gl::glUniformBlockBinding(shaderProgram.getProgramId(), blockIndex, 1);

                pointLightBuffer.bindBase<gl::GLenum::GL_SHADER_STORAGE_BUFFER>(0);
                blockIndex = gl::glGetProgramResourceIndex(shaderProgram.getProgramId(),
                                                           gl::GLenum::GL_SHADER_STORAGE_BLOCK, "PointLightBuffer");
                gl::glShaderStorageBlockBinding(shaderProgram.getProgramId(), blockIndex, 0);
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
                // PROFILE_SCOPE("update_uniform_buffers");

                frameUniforms.projectionMatrix = glm::perspective(
                    glm::radians(camera.getFov()),
                    static_cast<float>(screenDimensions.x) / static_cast<float>(screenDimensions.y), 0.01f, 100000.0f);

                frameUniforms.viewMatrix = camera.getViewMatrix();

                frameUniforms.cameraPosition = camera.getEye();

                frameUniforms.numPointLights = pointLights.size();

                // matrixUniforms.modelMatrix =
                //     glm::rotate(glm::mat4(1.0), timer.getElapsedTimeSeconds(), glm::vec3(0, 1, 0));
                objectMatrices.modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
                objectMatrices.normalMatrix = glm::transpose(glm::inverse(glm::mat3(objectMatrices.modelMatrix)));

                pointLights[0].colour = glm::vec4(20.0f, 20.0f, 20.0f, 0.0f);
                pointLights[0].position = glm::vec4(0.0f, 2.0f, 1.0f, 0.0f);

                frameUniformsBuffer.subData<FrameUniforms>(0, std::span(&frameUniforms, 1));
                objectMatricesBuffer.subData<ObjectMatrices>(0, std::span(&objectMatrices, 1));
                pointLightBuffer.subData<PointLight>(0, pointLights);
        }

        gl::glUseProgram(shaderProgram.getProgramId());

        for (const auto& m : meshes)
        {
                if (m->albedoTexture)
                {
                        gl::glBindTextureUnit(0, m->albedoTexture);

                        const gl::GLint albedoLoc =
                            gl::glGetUniformLocation(shaderProgram.getProgramId(), "uBaseColour.textureMap");
                        gl::glUniform1i(albedoLoc, 0);

                        gl::glUniform1i(
                            gl::glGetUniformLocation(shaderProgram.getProgramId(), "uBaseColour.isTextureEnabled"),
                            true);
                }

                gl::glUniform4fv(gl::glGetUniformLocation(shaderProgram.getProgramId(), "uBaseColour.factor"), 1,
                                 glm::value_ptr(m->albedoFactor));

                if (m->metallicRoughnessTexture)
                {
                        gl::glBindTextureUnit(1, m->metallicRoughnessTexture);

                        const gl::GLint mrLoc =
                            gl::glGetUniformLocation(shaderProgram.getProgramId(), "uMetallicRoughness.textureMap");
                        gl::glUniform1i(mrLoc, 1);

                        gl::glUniform1i(
                            gl::glGetUniformLocation(shaderProgram.getProgramId(), "uMetallicRoughness.isTextureEnabled"),
                            true);
                }

                gl::glUniform4fv(gl::glGetUniformLocation(shaderProgram.getProgramId(), "uMetallicRoughness.factor"), 1,
                                 glm::value_ptr(m->metallicRoughnessFactor));

                m->vao.bind();

                m->ebo.bind<gl::GLenum::GL_ELEMENT_ARRAY_BUFFER>();

                gl::glDrawElements(gl::GLenum::GL_TRIANGLES, m->vertexCount, gl::GLenum::GL_UNSIGNED_INT, (void*)0);
        }
}
