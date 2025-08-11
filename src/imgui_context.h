#pragma once

#include <imgui.h>

#include "timer.h"
#include "imgui_profile_visualiser.h"
#include "window.h"

class EngineImGuiContext
{
      public:
        EngineImGuiContext(const Window& window, Timer<>& timer);
        ~EngineImGuiContext();

        EngineImGuiContext(const EngineImGuiContext&) = delete;
        EngineImGuiContext& operator=(const EngineImGuiContext&) = delete;

        void draw();

	ImGuiProfileVisualiser& getProfiler() { return profiler; }

      private:
        bool showMetrics;
	bool showProfiler;

        float frametime;
	ImGuiProfileVisualiser profiler;

        void drawMetrics();
        void drawMenuBar();
};
