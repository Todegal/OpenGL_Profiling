#pragma once

#include "render_pass.h"

class ForwardRenderPass : public RenderPass
{
      public:
        ForwardRenderPass(GLContext& context, RenderContext& renderContext);

      public:
        void frameStart() override;
        void frameExecute() override;
        void frameEnd() override;
        void resize() override;

      private:
        GLShaderProgram forwardPassShader;
};