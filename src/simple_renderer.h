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
                Mesh() = delete;
                Mesh(const GLContext& c) : vbo(c), ebo(c), vao(c)
                {
                }

                GLBuffer vbo;
                GLBuffer ebo;
                GLVertexArray vao;

                uint32_t vertexCount;

                gl::GLuint albedoTexture;
        };

        std::vector<std::shared_ptr<Mesh>> meshes;

        GLShaderProgram shaderProgram;

        struct RenderUniforms
        {
                glm::vec3 cameraPosition;
                int numLights;
        } renderUniforms;

        GLBuffer renderUniformsBuffer;

        struct MatrixUniforms
        {
                glm::mat4 projectionMatrix;
                glm::mat4 viewMatrix;
                glm::mat4 modelMatrix;
                glm::mat3x4 normalMatrix;
        } matrixUniforms;

        GLBuffer matrixUniformsBuffer;

        struct Light
        {
                glm::vec4 position;
                glm::vec4 colour;
        };

        std::vector<Light> lights;
        GLBuffer lightsBuffer;
};
