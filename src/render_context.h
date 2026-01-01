#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <bitset>
#include <map>
#include <memory>
#include <string>

#include "camera.h"
#include "opengl_context.h"
#include "opengl_shader.h"
#include "scene_graph.h"

constexpr std::uint8_t NUM_CASCADES =
    5; // todo: this is a magic number which can only be modified at build time, and is shared with the shader system.
       // this is obviously bad so we must find a better way to deal with this possibly by having a non constexpr value
       // which we feed to the shader at shader compile but that is a bit fiddly

class RenderFlags
{
      public:
        enum
        {
                FORWARD_PASS_ENABLED = 0,
                DEFERRED_PASS_ENABLED,
                NORMALS_ENABLED,
                HDR_PASS_ENABLED,
                NUM_FLAGS
        };

        RenderFlags()
        {
                flags.set();
        }

        template <std::size_t FLAG>
        bool get()
        {
                static_assert(FLAG < NUM_FLAGS, "FLAG must be a valid RenderFlag!");

                return flags.test(FLAG);
        }

        template <std::size_t FLAG>
        void set(bool value)
        {
                static_assert(FLAG < NUM_FLAGS, "FLAG must be a valid RenderFlag!");

                flags[FLAG] = value;
        }

      private:
        std::bitset<NUM_FLAGS> flags;
};

// this is a class which stores all the data about the current scene
// basically a big collection of "globals"
class RenderContext
{
      public:
        RenderContext(GLContext& context, const RawScene& sceneData, const SceneGraph& sceneGraph,
                      std::shared_ptr<Camera> initialCamera);
        ~RenderContext() = default;

        RenderContext() = delete;

        RenderContext(const RenderContext&) = delete;
        RenderContext& operator=(const RenderContext&) = delete;

        RenderContext(const RenderContext&&) = delete;
        RenderContext& operator=(const RenderContext&&) = delete;

        void drawScene();
        // void drawOpaqueScene();
        // void drawtranslucentscene();

        void drawScene(GLShaderProgram& shaderProgram);
        // void drawOpaqueScene(GLShaderProgram& shaderProgram);
        // void drawTranslucentScene(GLShaderProgram& shaderProgram);

        // will draw a triangle big enough to cover the whole screen
        void drawFullscreen();

        void resize();

        RenderFlags renderFlags;

        struct PointLight
        {
                glm::vec4 position; // X Y Z + padding
                glm::vec4 radiance; // R G B + padding
        };

        std::vector<PointLight> pointLights;

        struct DirectionalLight
        {
                glm::vec4 direction;                                    // X Y Z + padding
                glm::vec4 radiance;                                     // R G B + padding
                std::array<glm::mat4, NUM_CASCADES> lightSpaceMatrices; // 4 * 4 * 5 = 80 bytes
        };

        std::vector<DirectionalLight> directionalLight;

#pragma warning(disable : 4324)

        struct ObjectMatrices
        {
                glm::mat4 modelMatrix;
                glm::mat4 normalMatrix;
        };

        struct FrameUniforms
        {
                glm::mat4 projectionMatrix;
                glm::mat4 viewMatrix;
                Point3<WorldSpace> cameraPosition;
                float pad0;

                glm::vec4 directionalShadowCascadePlanes;

                float pointShadowNearPlane;
                float pointShadowFarPlane;

                int numPointLights;
                int numDirectionalLights;
        } frameUniforms;

        std::unordered_map<std::string, std::unique_ptr<GLTexture2D>> globalTextures;

        std::unordered_map<std::string, std::shared_ptr<GLBuffer>> globalBuffers;

        std::unordered_map<std::string, gl::GLuint> globalResources; // todo: refactor this away, this is stop gap while
                                                                     // I figure out how to wrap other opengl objects
        std::shared_ptr<Camera> camera;

        std::unique_ptr<GLFramebuffer> framebuffer;
        std::unique_ptr<GLTexture2D> defaultColourTarget;
        std::unique_ptr<GLRenderbuffer> depthRenderbuffer;

      private:
        GLContext& glContext;
        const SceneGraph& sceneGraph;

        std::vector<std::shared_ptr<GLTexture2D>> textures;

        struct RenderMaterial
        {
                std::shared_ptr<GLTexture2D> albedoTexture{};
                glm::vec4 albedoFactor{};

                std::shared_ptr<GLTexture2D> metallicRoughnessTexture{};
                glm::vec4 metallicRoughnessFactor{};

                std::shared_ptr<GLTexture2D> normalTexture{};
                float normalScale{};
        };
        std::vector<std::shared_ptr<RenderMaterial>> materials;

        // Here we store all of the scene data
        struct RenderMesh
        {
                std::unique_ptr<GLBuffer> vbo{};
                std::unique_ptr<GLBuffer> ebo{};
                std::unique_ptr<GLVertexArray> vao{};

                std::size_t vertexCount{};

                std::shared_ptr<RenderMaterial> material{};

                Point3<LocalSpace> centre{};
        };
        std::vector<std::shared_ptr<RenderMesh>> meshes;
        std::vector<std::shared_ptr<RenderMesh>> opaqueMeshes;
        std::vector<std::shared_ptr<RenderMesh>> translucentMeshes;

        // data for a fullscreen tri used in fullscreen rendering
        std::unique_ptr<GLBuffer> fullscreenTriBuffer;
        std::unique_ptr<GLVertexArray> fullscreenTriVAO;

        void loadMaterialProperties(const RenderMaterial& material, GLShaderProgram& shaderProgram);
};
