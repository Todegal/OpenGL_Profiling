#pragma once

#include <imgui.h>

#include "imgui_profile_visualiser.h"
#include "imgui_scene_graph_visualiser.h"
#include "timer.h"
#include "window.h"

class EngineImGuiContext
{
      public:
        EngineImGuiContext(const Window& window, const SceneGraph& sceneGraph, Timer<>& timer);
        ~EngineImGuiContext();

        EngineImGuiContext(const EngineImGuiContext&) = delete;
        EngineImGuiContext& operator=(const EngineImGuiContext&) = delete;


        void draw();

        ImGuiProfileVisualiser& getProfiler()
        {
                return profiler;
        }

      private:
        bool showMetrics;
        bool showProfiler;
        bool showSceneGraph;

        float frametime;
        ImGuiProfileVisualiser profiler;
        ImGuiSceneGraph sceneGraphVisualiser;

        void drawMetrics();
        void drawMenuBar();
};
