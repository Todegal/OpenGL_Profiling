#pragma once

#include "camera.h"
#include "opengl_context.h"
#include "opengl_shader.h"
#include "raw_data.h"
#include "render_context.h"

#include <array>
#include <stack>
#include <unordered_map>

class RenderPass
{
      public:
        RenderPass(GLContext& context, RenderContext& renderContext) : renderContext(renderContext), glContext(context) {};

      protected:
        RenderContext& renderContext;
        GLContext& glContext;

      public:
        virtual void frameStart() = 0;
        virtual void frameExecute() = 0;
        virtual void frameEnd() = 0;

        virtual void refresh() = 0;
};