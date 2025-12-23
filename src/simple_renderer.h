#pragma once

#include "camera.h"
#include "opengl_context.h"
#include "opengl_shader.h"
#include "raw_data.h"
#include "timer.h"

#include <glbinding/gl/bitfield.h>
#include <glbinding/gl/enum.h>

// Loads models from the scene and renders them statically, with simple albedo textures
// and with blinn-phong shading
class SimpleRenderer
{
      public:
        SimpleRenderer(GLContext& glContext, const RawScene& scene, const Camera& camera, const Timer<>& timer);
        ~SimpleRenderer();

        SimpleRenderer(const SimpleRenderer&) = delete;
        SimpleRenderer& operator=(const SimpleRenderer&) = delete;

        SimpleRenderer(SimpleRenderer&&) = delete;
        SimpleRenderer& operator=(SimpleRenderer&&) = delete;

        void render();

      private:
        GLContext& glContext;
        const Timer<>& timer;

        const Camera& camera;

        struct Mesh
        {
                Mesh(const GLContext& context) : vao(context)
                {
                }

                std::unique_ptr<GLImmutableBuffer> vbo;
                std::unique_ptr<GLImmutableBuffer> ebo;
                GLVertexArray vao;

                std::size_t vertexCount;

                gl::GLuint albedoTexture;
                glm::vec4 albedoFactor;

                gl::GLuint metallicRoughnessTexture;
                glm::vec4 metallicRoughnessFactor;
        };

        std::vector<std::shared_ptr<Mesh>> meshes;

        GLShaderProgram shaderProgram;

        struct FrameUniforms
        {
                glm::mat4 projectionMatrix;
                glm::mat4 viewMatrix;
                glm::vec3 cameraPosition;
                // float p0;
                // glm::vec4 cascadePlanes;
                // float shadowNearPlane;
                // float shadowFarPlane;
                int numPointLights;
                int numDirectionalLights;
        } frameUniforms;

        std::unique_ptr<GLImmutableBuffer> frameUniformsBuffer;

        struct ObjectMatrices
        {
                glm::mat4 modelMatrix;
                glm::mat3x4 normalMatrix;
        } objectMatrices;

        std::unique_ptr<GLImmutableBuffer> objectMatricesBuffer;

        struct PointLight
        {
                glm::vec4 position;
                glm::vec4 colour;
        };

        std::vector<PointLight> pointLights;
        std::unique_ptr<GLMutableBuffer> pointLightBuffer;
};
