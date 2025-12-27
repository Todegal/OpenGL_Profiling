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
        void refresh() override;

      private:
        GLShaderProgram hdrPassShader;
};