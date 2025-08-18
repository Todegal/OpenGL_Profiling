#pragma once

#include "camera.h"
#include "opengl_context.h"
#include "opengl_shader.h"
#include "raw_data.h"
#include "timer.h"

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
                Mesh() : indicesOffset(0), indicesCount(0), vertexOffset(0), vertexCount(0), materialIdx(0)
                {
                }

                size_t indicesOffset;
                size_t indicesCount;

                size_t vertexOffset;
                size_t vertexCount;

                size_t materialIdx;
        };

        std::vector<Mesh> meshes;

        struct Material
        {
                Material()
                    : albedoTexture(0), albedoFactor(0.0f), metallicRoughnessTexture(0), metallicRoughnessFactor(0.0f)
                {
                }

                gl::GLuint albedoTexture;
                glm::vec4 albedoFactor;

                gl::GLuint metallicRoughnessTexture;
                glm::vec4 metallicRoughnessFactor;
        };

        std::vector<Material> materials;

        std::unique_ptr<GLImmutableBuffer> sceneVertexBuffer;
        GLVertexArray sceneVertexArray;
        size_t indicesStart;

        GLShaderProgram shaderProgram;

        struct FrameUniforms
        {
                glm::mat4 projectionMatrix;
                glm::mat4 viewMatrix;
                glm::vec3 cameraPosition;
                float p0;
                glm::vec4 cascadePlanes;
                float shadowNearPlane;
                float shadowFarPlane;
                int numPointLights;
                int numDirectionalLights;
        } frameUniforms;

        std::unique_ptr<GLMutableBuffer> frameUniformsBuffer;

        struct ObjectMatrices
        {
                glm::mat4 modelMatrix;
                glm::mat3x4 normalMatrix;
        } objectMatrices;

        std::unique_ptr<GLMutableBuffer> objectMatricesBuffer;

        struct PointLight
        {
                glm::vec4 position;
                glm::vec4 colour;
        };

        std::vector<PointLight> pointLights;
        std::unique_ptr<GLMutableBuffer> pointLightBuffer;
};
