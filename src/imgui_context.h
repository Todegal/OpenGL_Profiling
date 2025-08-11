#pragma once

#include <imgui.h>

#include "timer.h"
#include "window.h"

class EngineImGuiContext
{
      public:
        EngineImGuiContext(const Window& window, Timer<>& timer);
        ~EngineImGuiContext();

        EngineImGuiContext(const EngineImGuiContext&) = delete;
        EngineImGuiContext& operator=(const EngineImGuiContext&) = delete;

        void draw();

      private:
        bool showMetrics;

        float frametime;

        void drawMetrics();
        void drawMenuBar();
};
