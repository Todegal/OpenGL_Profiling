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
                Mesh() : offset(0), count(0), albedoTexture(0), metallicRoughnessTexture(0)
                {
                }

                size_t offset;
                size_t count;

                gl::GLuint albedoTexture;
                glm::vec4 albedoFactor;

                gl::GLuint metallicRoughnessTexture;
                glm::vec4 metallicRoughnessFactor;
        };

        std::vector<Mesh> meshes;

	GLBuffer sceneVertexBuffer;
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

        GLBuffer frameUniformsBuffer;

        struct ObjectMatrices
        {
                glm::mat4 modelMatrix;
                glm::mat3x4 normalMatrix;
        } objectMatrices;

        GLBuffer objectMatricesBuffer;

        struct PointLight
        {
                glm::vec4 position;
                glm::vec4 colour;
        };

        std::vector<PointLight> pointLights;
        GLBuffer pointLightBuffer;
};
