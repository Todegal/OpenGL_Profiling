#pragma once

#include "render_pass.h"

class HDRRenderPass : public RenderPass
{
      public:
        HDRRenderPass(GLContext& context, RenderContext& renderContext);

      public:
        // Inherited via RenderPass
        void frameStart() override;
        void frameExecute() override;
        void frameEnd() override;
        void resize() override;

      private:
        GLShaderProgram hdrPassShader;
};