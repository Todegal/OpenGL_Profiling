#include "simple_renderer.h"

#include <GLFW/glfw3.h>
#include <cstring>
#include <glbinding/gl/bitfield.h>
#include <glbinding/gl/boolean.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "mem_print.h"
#include "opengl_context.h"
#include "profiler.h"

#include <cstddef>

SimpleRenderer::SimpleRenderer(GLContext& glContext, const RawScene& scene, const Camera& camera, const Timer<>& timer)
    : glContext(glContext), timer(timer), camera(camera), sceneVertexArray(glContext),
      shaderProgram(glContext, {std::make_shared<GLShader>(glContext, "shaders/forward_pass/forward_pass.vert.glsl",
                                                           gl::GLenum::GL_VERTEX_SHADER),
                                std::make_shared<GLShader>(glContext, "shaders/forward_pass/forward_pass.frag.glsl",
                                                           gl::GLenum::GL_FRAGMENT_SHADER)}),
      pointLights(4)
{
        PROFILE_FUNCTION();

        // FIRST LOAD MATERIALS
        materials.resize(scene.getMaterials().size());

        for (size_t i = 0; i < scene.getMaterials().size(); ++i)
        {
                const auto& material = scene.getMaterials()[i];

                auto& m = materials[i];

                // Load the texture
                const std::unique_ptr<RawTexture>& albedo = material->getAlbedoTexture();
                if (albedo)
                {
                        gl::GLenum format;
                        gl::GLenum internalFormat;
                        switch (albedo->getChannels())
                        {
                        case 1:
                                format = gl::GLenum::GL_RED;
                                internalFormat = gl::GLenum::GL_R8;
                                break;
                        case 2:
                                format = gl::GLenum::GL_RG;
                                internalFormat = gl::GLenum::GL_RG8;
                                break;
                        case 3:
                                format = gl::GLenum::GL_RGB;
                                internalFormat = gl::GLenum::GL_RGB8;
                                break;
                        default:
                                format = gl::GLenum::GL_RGBA;
                                internalFormat = gl::GLenum::GL_RGBA8;
                                break;
                        }

                        const auto& width = albedo->getWidth();
                        const auto& height = albedo->getHeight();
                        const gl::GLsizei maxLevels =
                            std::max(static_cast<gl::GLsizei>(std::floor(std::log2(std::max(width, height)))), 1);

                        gl::glCreateTextures(gl::GLenum::GL_TEXTURE_2D, 1, &m.albedoTexture);
                        gl::glTextureStorage2D(m.albedoTexture, maxLevels, internalFormat, width, height);
                        gl::glTextureSubImage2D(m.albedoTexture, 0, 0, 0, width, height, format,
                                                gl::GLenum::GL_UNSIGNED_BYTE, albedo->getData().data());

                        gl::glGenerateTextureMipmap(m.albedoTexture);
                }

                m.albedoFactor = material->getAlbedoFactor();

                const std::unique_ptr<RawTexture>& metallicRoughness = material->getMetallicRoughnessTexture();
                if (metallicRoughness)
                {
                        gl::GLenum format;
                        gl::GLenum internalFormat;
                        switch (metallicRoughness->getChannels())
                        {
                        case 1:
                                format = gl::GLenum::GL_RED;
                                internalFormat = gl::GLenum::GL_R8;
                                break;
                        case 2:
                                format = gl::GLenum::GL_RG;
                                internalFormat = gl::GLenum::GL_RG8;
                                break;
                        case 3:
                                format = gl::GLenum::GL_RGB;
                                internalFormat = gl::GLenum::GL_RGB8;
                                break;
                        default:
                                format = gl::GLenum::GL_RGBA;
                                internalFormat = gl::GLenum::GL_RGBA8;
                                break;
                        }

                        const auto& width = metallicRoughness->getWidth();
                        const auto& height = metallicRoughness->getHeight();
                        const gl::GLsizei maxLevels =
                            static_cast<gl::GLsizei>(std::floor(std::log2(std::max(width, height)))) + 1;

                        gl::glCreateTextures(gl::GLenum::GL_TEXTURE_2D, 1, &m.metallicRoughnessTexture);
                        gl::glTextureStorage2D(m.metallicRoughnessTexture, maxLevels, internalFormat, width, height);
                        gl::glTextureSubImage2D(m.metallicRoughnessTexture, 0, 0, 0, width, height, format,
                                                gl::GLenum::GL_UNSIGNED_BYTE, metallicRoughness->getData().data());

                        gl::glGenerateTextureMipmap(m.metallicRoughnessTexture);
                }

                m.metallicRoughnessFactor = material->getAlbedoFactor();
        }

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

        sceneVertexBuffer = std::make_unique<GLImmutableBuffer>(glContext, vertexDataSize + indexDataSize,
                                                                gl::BufferStorageMask::GL_MAP_WRITE_BIT |
                                                                    gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT);

        std::byte* sceneVertexBufferMap = sceneVertexBuffer->map<std::byte>(gl::GLenum::GL_WRITE_ONLY);

        sceneVertexArray.bindVertexBuffer(0, sceneVertexBuffer->getID(), 0, sizeof(Vertex));
        sceneVertexArray.bindElementBuffer(sceneVertexBuffer->getID());

        sceneVertexArray.defineAttribute(0, 0, 3, gl::GLenum::GL_FLOAT, gl::GL_FALSE, offsetof(Vertex, position));
        sceneVertexArray.defineAttribute(1, 0, 3, gl::GLenum::GL_FLOAT, gl::GL_FALSE, offsetof(Vertex, normal));
        sceneVertexArray.defineAttribute(2, 0, 2, gl::GLenum::GL_FLOAT, gl::GL_FALSE, offsetof(Vertex, texCoord));

        size_t vertexDataWriteHead = 0;
        size_t indexDataWriteHead = vertexDataSize;

        size_t meshIndicesOffset = 0;
        size_t meshVerticesOffset = 0;

        meshes.resize(scene.getMeshes().size());

        spdlog::debug("Buffer size={} VB size={} IB size={} indicesStart={}",
                      humanReadableSize(sceneVertexBuffer->getAllocatedSize()), humanReadableSize(vertexDataSize),
                      humanReadableSize(indexDataSize), humanReadableSize(indicesStart));

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
                        *(reinterpret_cast<Vertex*>(sceneVertexBufferMap + vertexDataWriteHead)) =
                            Vertex{positions[i], normals[i], texCoords[i]};
                        vertexDataWriteHead += sizeof(Vertex);
                }

                std::memcpy((sceneVertexBufferMap + indexDataWriteHead), indices.data(),
                            sizeof(uint32_t) * indices.size());

                indexDataWriteHead += sizeof(uint32_t) * indices.size();

                auto& m = meshes[i];

                m.indicesOffset = meshIndicesOffset;
                m.indicesCount = indices.size();

                m.vertexOffset = meshVerticesOffset;
                m.vertexCount = positions.size();

                meshIndicesOffset += m.indicesCount;
                meshVerticesOffset += m.vertexCount;

                m.materialIdx = mesh->getMaterialIndex();
        }

        sceneVertexBuffer->unmap();

        // NOW, let's create the uniform buffers
        {
                PROFILE_SCOPE("create_render_buffers");

                frameUniformsBuffer =
                    std::make_unique<GLMutableBuffer>(glContext, sizeof(FrameUniforms), gl::GLenum::GL_DYNAMIC_DRAW);
                objectMatricesBuffer =
                    std::make_unique<GLMutableBuffer>(glContext, sizeof(ObjectMatrices), gl::GLenum::GL_DYNAMIC_DRAW);
                pointLightBuffer = std::make_unique<GLMutableBuffer>(glContext, sizeof(PointLight) * pointLights.size(),
                                                                     gl::GLenum::GL_DYNAMIC_DRAW);

                objectMatricesBuffer->bindBase(gl::GLenum::GL_UNIFORM_BUFFER, 0);
                gl::GLuint blockIndex = gl::glGetUniformBlockIndex(shaderProgram.getProgramId(), "ObjectBuffer");
                gl::glUniformBlockBinding(shaderProgram.getProgramId(), blockIndex, 0);

                frameUniformsBuffer->bindBase(gl::GLenum::GL_UNIFORM_BUFFER, 1);
                blockIndex = gl::glGetUniformBlockIndex(shaderProgram.getProgramId(), "FrameUniformsBuffer");
                gl::glUniformBlockBinding(shaderProgram.getProgramId(), blockIndex, 1);

                pointLightBuffer->bindBase(gl::GLenum::GL_SHADER_STORAGE_BUFFER, 0);
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

        glContext.enable(gl::GLenum::GL_BLEND);

        gl::glBlendFunc(gl::GLenum::GL_SRC_ALPHA, gl::GLenum::GL_ONE_MINUS_SRC_ALPHA);

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

                // pointLights[0].colour = glm::vec4(20.0f, 0.0f, 0.0f, 0.0f);
                // pointLights[0].position = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
                //
                // pointLights[1].colour = glm::vec4(0.0f, 20.0f, 0.0f, 0.0f);
                // pointLights[1].position = glm::vec4(0.0f, 2.0f, 0.0f, 0.0f);
                //
                // pointLights[2].colour = glm::vec4(0.0f, 0.0f, 20.0f, 0.0f);
                // pointLights[2].position = glm::vec4(0.0f, 1.0f, 1.0f, 0.0f);

                pointLights[3].colour = glm::vec4(500.0f);
                pointLights[3].position = glm::vec4(10.0f, 10.0f, 4.0f, 0.0f);

                frameUniformsBuffer->subData<FrameUniforms>(0, std::span(&frameUniforms, 1));
                objectMatricesBuffer->subData<ObjectMatrices>(0, std::span(&objectMatrices, 1));
                pointLightBuffer->subData<PointLight>(0, pointLights);
        }

        gl::glUseProgram(shaderProgram.getProgramId());

        sceneVertexArray.bind();

        for (const auto& m : meshes)
        {
                const auto& material = materials[m.materialIdx];

                if (material.albedoTexture)
                {
                        gl::glBindTextureUnit(0, material.albedoTexture);

                        const gl::GLint albedoLoc =
                            gl::glGetUniformLocation(shaderProgram.getProgramId(), "uBaseColour.textureMap");
                        gl::glUniform1i(albedoLoc, 0);

                        gl::glUniform1i(
                            gl::glGetUniformLocation(shaderProgram.getProgramId(), "uBaseColour.isTextureEnabled"),
                            true);
                }

                gl::glUniform4fv(gl::glGetUniformLocation(shaderProgram.getProgramId(), "uBaseColour.factor"), 1,
                                 glm::value_ptr(material.albedoFactor));

                if (material.metallicRoughnessTexture)
                {
                        gl::glBindTextureUnit(1, material.metallicRoughnessTexture);

                        const gl::GLint mrLoc =
                            gl::glGetUniformLocation(shaderProgram.getProgramId(), "uMetallicRoughness.textureMap");
                        gl::glUniform1i(mrLoc, 1);

                        gl::glUniform1i(gl::glGetUniformLocation(shaderProgram.getProgramId(),
                                                                 "uMetallicRoughness.isTextureEnabled"),
                                        true);
                }

                gl::glUniform4fv(gl::glGetUniformLocation(shaderProgram.getProgramId(), "uMetallicRoughness.factor"), 1,
                                 glm::value_ptr(material.metallicRoughnessFactor));

                const auto byteOffset = indicesStart + m.indicesOffset * sizeof(uint32_t);
                gl::glDrawElementsBaseVertex(gl::GLenum::GL_TRIANGLES, static_cast<gl::GLsizei>(m.indicesCount),
                                             gl::GLenum::GL_UNSIGNED_INT,
                                             reinterpret_cast<void*>(static_cast<uintptr_t>(byteOffset)),
                                             static_cast<gl::GLint>(m.vertexOffset));
        }
}
