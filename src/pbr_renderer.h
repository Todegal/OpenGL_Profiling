#pragma once

#include "camera.h"
#include "forward_render_pass.h"
#include "hdr_render_pass.h"
#include "imgui_context.h"
#include "opengl_context.h"
#include "render_context.h"
#include "render_pass.h"

#include <array>

class PBRRenderer
{
      public:
        PBRRenderer(GLContext& context, const RawScene& sceneData, const SceneGraph& sceneGraph, std::shared_ptr<Camera> initialCamera);
        ~PBRRenderer() = default;

        // No copy/move
        PBRRenderer(const PBRRenderer&) = delete;
        PBRRenderer& operator=(const PBRRenderer) = delete;

        // void setScene(const RawScene& scene);
        void setCamera(std::shared_ptr<Camera> newCamera)
        {
                renderContext.camera = newCamera;
        }

        void setRenderFlags(RenderFlags flags)
        {
                renderContext.renderFlags = flags;
        }

        // void resize(glm::ivec2 screenSize);

        // void imguiFrame(imgui_data& data);
        void frame();

      private:
        GLContext& glContext;
        RenderContext renderContext;

        const SceneGraph& sceneGraph;

        std::unique_ptr<HDRRenderPass> hdrPass;
        std::unique_ptr<ForwardRenderPass> forwardPass;
};