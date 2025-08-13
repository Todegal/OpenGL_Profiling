#include "simple_renderer.h"

#include <GLFW/glfw3.h>
#include <glbinding/gl/bitfield.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "opengl_context.h"
#include "profiler.h"

#include <array>
#include <cstddef>

SimpleRenderer::SimpleRenderer(GLContext& glContext, const RawScene& scene, const Camera& camera, const Timer<>& timer)
    : glContext(glContext), timer(timer), camera(camera),
      shaderProgram(
          glContext,
          {std::make_shared<GLShader>(glContext, "shaders/simple_renderer.vert.glsl", gl::GLenum::GL_VERTEX_SHADER),
           std::make_shared<GLShader>(glContext, "shaders/simple_renderer.frag.glsl", gl::GLenum::GL_FRAGMENT_SHADER)}),
      renderUniformsBuffer(glContext), matrixUniformsBuffer(glContext), lights(3), lightsBuffer(glContext)
{
        PROFILE_FUNCTION();

        renderUniforms.cameraPosition = glm::vec3(0, 0, 1);

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
                const std::vector<glm::vec2>& texCoords = mesh->getTexCoords();
                const std::vector<glm::vec3>& normals = mesh->getNormals();

                const std::vector<uint32_t>& indices = mesh->getIndices();

                // Size calculations
                // 	the data here will be interleaved like so
                // 	positions[i] -> vec3
                // 	texCoords[i] -> vec2
                // 	normals[i] -> vec3

                const gl::GLsizei totalBufferSize = (sizeof(glm::vec3) * positions.size()) +
                                                    (sizeof(glm::vec2) * texCoords.size()) +
                                                    (sizeof(glm::vec3) * normals.size());

                const std::array<std::uintptr_t, 3> offsets = {0, sizeof(glm::vec3) * positions.size(),
                                                               sizeof(glm::vec3) * positions.size() +
                                                                   sizeof(glm::vec2) * texCoords.size()};

                // Create mesh and allocate size
                const std::shared_ptr<Mesh> m = std::make_shared<Mesh>(glContext);
                meshes.push_back(m);

                m->vbo.allocate<std::byte>(totalBufferSize, gl::GLenum::GL_DYNAMIC_DRAW);

                m->vbo.subData<glm::vec3>(offsets[0], positions);
                m->vbo.subData<glm::vec2>(offsets[1], texCoords);
                m->vbo.subData<glm::vec3>(offsets[2], normals);

                // Fill the index buffer, sane and normal
                m->ebo.bufferData<uint32_t>(indices, gl::GLenum::GL_STATIC_DRAW); // BEAUTIFUL
                m->vertexCount = indices.size();

                // Layout the VAO
                m->vao.bind();
                m->vbo.bind<gl::GLenum::GL_ARRAY_BUFFER>();

                gl::glEnableVertexAttribArray(0);
                gl::glVertexAttribPointer(0, 3, gl::GLenum::GL_FLOAT, gl::GL_FALSE, 0, (void*)offsets[0]);

                gl::glEnableVertexAttribArray(1);
                gl::glVertexAttribPointer(1, 2, gl::GLenum::GL_FLOAT, gl::GL_FALSE, 0, (void*)offsets[1]);

                gl::glEnableVertexAttribArray(2);
                gl::glVertexAttribPointer(2, 3, gl::GLenum::GL_FLOAT, gl::GL_FALSE, 0, (void*)offsets[2]);

                // Load the texture
                const std::unique_ptr<RawTexture>& albedo = materials[mesh->getMaterialIndex()]->getAlbedoTexture();

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

        // NOW, let's create the uniform buffers
        {
                PROFILE_SCOPE("create_render_buffers");

                renderUniformsBuffer.allocate<RenderUniforms>(1, gl::GLenum::GL_DYNAMIC_DRAW);
                matrixUniformsBuffer.allocate<MatrixUniforms>(1, gl::GLenum::GL_DYNAMIC_DRAW); // ALSO BEAUTIFUL
                lightsBuffer.allocate<Light>(lights.size(), gl::GLenum::GL_DYNAMIC_DRAW);

                matrixUniformsBuffer.bindBase<gl::GLenum::GL_UNIFORM_BUFFER>(0);
                gl::GLuint blockIndex = gl::glGetUniformBlockIndex(shaderProgram.getProgramId(), "MatrixUniforms");
                gl::glUniformBlockBinding(shaderProgram.getProgramId(), blockIndex, 0);

                renderUniformsBuffer.bindBase<gl::GLenum::GL_UNIFORM_BUFFER>(1);
                blockIndex = gl::glGetUniformBlockIndex(shaderProgram.getProgramId(), "RenderUniforms");
                gl::glUniformBlockBinding(shaderProgram.getProgramId(), blockIndex, 1);

                lightsBuffer.bindBase<gl::GLenum::GL_SHADER_STORAGE_BUFFER>(0);
                blockIndex = gl::glGetProgramResourceIndex(shaderProgram.getProgramId(),
                                                           gl::GLenum::GL_SHADER_STORAGE_BLOCK, "LightBuffer");
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

                renderUniforms.cameraPosition = camera.getEye();
                renderUniforms.numLights = lights.size();

                matrixUniforms.projectionMatrix = glm::perspective(
                    glm::radians(camera.getFov()),
                    static_cast<float>(screenDimensions.x) / static_cast<float>(screenDimensions.y), 0.01f, 100000.0f);

                matrixUniforms.viewMatrix = camera.getViewMatrix();

                // matrixUniforms.modelMatrix =
                //     glm::rotate(glm::mat4(1.0), timer.getElapsedTimeSeconds(), glm::vec3(0, 1, 0));
                matrixUniforms.modelMatrix = glm::mat4(1.0);

                matrixUniforms.normalMatrix = glm::transpose(glm::inverse(glm::mat3(matrixUniforms.modelMatrix)));

                lights[0].colour = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
                lights[0].position = glm::vec4(glm::sin(timer.getElapsedTimeSeconds()), 2.0f, 0.0f, 0.0f);

                lights[1].colour = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                lights[1].position = glm::vec4(0.0f, 2.0f, glm::sin(timer.getElapsedTimeSeconds()), 0.0f);

                lights[2].colour = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
                lights[2].position = glm::vec4(0.0f, glm::sin(timer.getElapsedTimeSeconds()), 2.0f, 0.0f);

                matrixUniformsBuffer.subData<MatrixUniforms>(0, std::span(&matrixUniforms, 1));
                renderUniformsBuffer.subData<RenderUniforms>(0, std::span(&renderUniforms, 1));
                lightsBuffer.subData<Light>(0, lights);
        }

        gl::glUseProgram(shaderProgram.getProgramId());

        const gl::GLint albedoLoc = gl::glGetUniformLocation(shaderProgram.getProgramId(), "uAlbedoTexture");
        gl::glUniform1i(albedoLoc, 0);

        for (const auto& m : meshes)
        {
                gl::glBindTextureUnit(0, m->albedoTexture);

                m->vao.bind();

                m->ebo.bind<gl::GLenum::GL_ELEMENT_ARRAY_BUFFER>();

                gl::glDrawElements(gl::GLenum::GL_TRIANGLES, m->vertexCount, gl::GLenum::GL_UNSIGNED_INT, (void*)0);
        }
}
