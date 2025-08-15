#include "simple_renderer.h"

#include <GLFW/glfw3.h>
#include <glbinding/gl/bitfield.h>
#include <glbinding/gl/boolean.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "opengl_context.h"
#include "profiler.h"

#include <array>
#include <cstddef>

SimpleRenderer::SimpleRenderer(GLContext& glContext, const RawScene& scene, const Camera& camera, const Timer<>& timer)
    : glContext(glContext), timer(timer), camera(camera), sceneVertexBuffer(glContext), sceneVertexArray(glContext),
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

        struct Vertex
        {
                glm::vec3 position;
                glm::vec3 normal;
                glm::vec2 texCoord;
        };

        size_t numVertices = 0;
        size_t numIndices = 0;

        for (const auto& mesh : scene.getMeshes())
        {
                numVertices += mesh->getPositions().size();
                numIndices += mesh->getIndices().size();
        }

        const size_t vertexDataSize = numVertices * sizeof(Vertex);
        const size_t indexDataSize = numIndices * sizeof(uint32_t);

        indicesStart = vertexDataSize;

        sceneVertexBuffer.allocate<std::byte>(vertexDataSize + indexDataSize, gl::GLenum::GL_DYNAMIC_DRAW);

        sceneVertexArray.bindVertexBuffer(0, sceneVertexBuffer, 0, sizeof(Vertex));
        sceneVertexArray.bindElementBuffer(sceneVertexBuffer);

        sceneVertexArray.defineAttribute(0, 0, 3, gl::GLenum::GL_FLOAT, gl::GL_FALSE, offsetof(Vertex, position));
        sceneVertexArray.defineAttribute(1, 0, 3, gl::GLenum::GL_FLOAT, gl::GL_FALSE, offsetof(Vertex, normal));
        sceneVertexArray.defineAttribute(2, 0, 2, gl::GLenum::GL_FLOAT, gl::GL_FALSE, offsetof(Vertex, texCoord));

        size_t vertexDataWriteHead = 0;
        size_t indexDataWriteHead = vertexDataSize;

        size_t meshIndicesOffset = 0;

        meshes.resize(scene.getMeshes().size());

        for (size_t i = 0; i < scene.getMeshes().size(); ++i)
        {
                const auto& mesh = scene.getMeshes()[i];

                // Define some aliases
                const std::vector<glm::vec3>& positions = mesh->getPositions();
                const std::vector<glm::vec3>& normals = mesh->getNormals();
                const std::vector<glm::vec2>& texCoords = mesh->getTexCoords();

                const std::vector<uint32_t>& indices = mesh->getIndices();

                // sub in the data, interleaved... bit of a pain but it's only once!
                for (size_t i = 0; i < positions.size(); ++i)
                {
                        const Vertex v = {positions[i], normals[i], texCoords[i]};
                        sceneVertexBuffer.subData<Vertex>(vertexDataWriteHead, {&v, 1});
                        vertexDataWriteHead += sizeof(Vertex);
                }

                sceneVertexBuffer.subData<uint32_t>(indexDataWriteHead, indices);
                indexDataWriteHead += sizeof(uint32_t) * indices.size();

                auto& m = meshes[i];

                m.offset = meshIndicesOffset;
                m.count = positions.size();

                meshIndicesOffset += m.count;

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

                        gl::glGenTextures(1, &m.albedoTexture);
                        gl::glBindTexture(gl::GLenum::GL_TEXTURE_2D, m.albedoTexture);
                        gl::glTexImage2D(gl::GLenum::GL_TEXTURE_2D, 0, gl::GLenum::GL_RGBA, albedo->getWidth(),
                                         albedo->getHeight(), 0, format, gl::GLenum::GL_UNSIGNED_BYTE,
                                         albedo->getData().data());

                        gl::glGenerateMipmap(gl::GLenum::GL_TEXTURE_2D);
                }

                m.albedoFactor = materials[mesh->getMaterialIndex()]->getAlbedoFactor();

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

                        gl::glGenTextures(1, &m.metallicRoughnessTexture);
                        gl::glBindTexture(gl::GLenum::GL_TEXTURE_2D, m.metallicRoughnessTexture);
                        gl::glTexImage2D(gl::GLenum::GL_TEXTURE_2D, 0, gl::GLenum::GL_RGBA,
                                         metallicRoughness->getWidth(), metallicRoughness->getHeight(), 0, format,
                                         gl::GLenum::GL_UNSIGNED_BYTE, metallicRoughness->getData().data());

                        gl::glGenerateMipmap(gl::GLenum::GL_TEXTURE_2D);
                }

                m.metallicRoughnessFactor = materials[mesh->getMaterialIndex()]->getAlbedoFactor();
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

        const glm::ivec2 screenDimensions = glContext.getWindow().getFramebufferSize();
        gl::glViewport(0, 0, screenDimensions.x, screenDimensions.y);

        gl::glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        gl::glClear(gl::ClearBufferMask::GL_COLOR_BUFFER_BIT | gl::ClearBufferMask::GL_DEPTH_BUFFER_BIT);

        // Update uniform buffers
        {
                // PROFILE_SCOPE("update_uniform_buffers");

                frameUniforms.projectionMatrix =
                    glm::perspective(glm::radians(camera.getFov()),
                                     std::max(static_cast<float>(screenDimensions.x), 1.0f) /
                                         std::max(static_cast<float>(screenDimensions.y), 1.0f),
                                     0.01f, 100000.0f);

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

        sceneVertexArray.bind();

        for (const auto& m : meshes)
        {
                if (m.albedoTexture)
                {
                        gl::glBindTextureUnit(0, m.albedoTexture);

                        const gl::GLint albedoLoc =
                            gl::glGetUniformLocation(shaderProgram.getProgramId(), "uBaseColour.textureMap");
                        gl::glUniform1i(albedoLoc, 0);

                        gl::glUniform1i(
                            gl::glGetUniformLocation(shaderProgram.getProgramId(), "uBaseColour.isTextureEnabled"),
                            true);
                }

                gl::glUniform4fv(gl::glGetUniformLocation(shaderProgram.getProgramId(), "uBaseColour.factor"), 1,
                                 glm::value_ptr(m.albedoFactor));

                if (m.metallicRoughnessTexture)
                {
                        gl::glBindTextureUnit(1, m.metallicRoughnessTexture);

                        const gl::GLint mrLoc =
                            gl::glGetUniformLocation(shaderProgram.getProgramId(), "uMetallicRoughness.textureMap");
                        gl::glUniform1i(mrLoc, 1);

                        gl::glUniform1i(gl::glGetUniformLocation(shaderProgram.getProgramId(),
                                                                 "uMetallicRoughness.isTextureEnabled"),
                                        true);
                }

                gl::glUniform4fv(gl::glGetUniformLocation(shaderProgram.getProgramId(), "uMetallicRoughness.factor"), 1,
                                 glm::value_ptr(m.metallicRoughnessFactor));

                gl::glDrawElementsBaseVertex(gl::GLenum::GL_TRIANGLES, m.count, gl::GLenum::GL_UNSIGNED_INT,
                                             (void*)(indicesStart + (m.offset * sizeof(uint32_t))), m.offset);
        }

}
