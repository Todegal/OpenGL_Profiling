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

        struct RenderMesh
        {
                RenderMesh(const GLContext& context) : vao(context)
                {
                }

                std::unique_ptr<GLBuffer> vbo;
                std::unique_ptr<GLBuffer> ebo;
                GLVertexArray vao;

                std::size_t vertexCount;

                std::unique_ptr<GLTexture2D> albedoTexture;
                glm::vec4 albedoFactor;

                std::unique_ptr<GLTexture2D> metallicRoughnessTexture;
                glm::vec4 metallicRoughnessFactor;

                std::unique_ptr<GLTexture2D> normalTexture;
                float normalScale;
        };

        std::vector<std::shared_ptr<RenderMesh>> meshes;

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

        std::unique_ptr<GLBuffer> frameUniformsBuffer;

        struct ObjectMatrices
        {
                glm::mat4 modelMatrix;
                glm::mat3x4 normalMatrix;
        } objectMatrices;

        std::unique_ptr<GLBuffer> objectMatricesBuffer;

        struct PointLight
        {
                glm::vec4 position;
                glm::vec4 colour;
        };

        std::vector<PointLight> pointLights;
        std::unique_ptr<GLBuffer> pointLightBuffer;

        // HDR Pass Stuff - ONLY PASS, then refactor pls
        gl::GLuint hdrFramebuffer;
        gl::GLuint hdrTexture;

        GLShaderProgram hdrShaderProgram;

        std::unique_ptr<GLBuffer> fullscreenTriBuffer;
        std::unique_ptr<GLVertexArray> fullscreenTriVAO;
};
