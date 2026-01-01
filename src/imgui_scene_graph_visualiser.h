#pragma once

#include <imgui.h>

#include "scene_graph.h"

class ImGuiSceneGraph
{
      public:
        ImGuiSceneGraph(const SceneGraph& sceneGraph);
        ~ImGuiSceneGraph() = default;

        void render(bool* open);

      private:
        const SceneGraph& sceneGraph;

        int selectedNodeId = -1;

        void renderSceneNodeRecursive(std::shared_ptr<SceneNode> node, std::uint32_t id = 0);
        void renderInfoPanel();
};
