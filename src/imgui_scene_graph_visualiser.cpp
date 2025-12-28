#include "imgui_scene_graph_visualiser.h"

#include "profiler.h"

#include <glm/gtx/string_cast.hpp>

ImGuiSceneGraph::ImGuiSceneGraph(const SceneGraph& sceneGraph) : sceneGraph(sceneGraph)
{
}

void ImGuiSceneGraph::renderSceneNodeRecursive(std::shared_ptr<SceneNode> node)
{
        ImGui::PushID(node->getID());

        // Highlight selected node
        bool isSelected = (selectedNodeId == static_cast<std::int64_t>(node->getID()));
        if (isSelected) { ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.8f, 0.8f)); }

        // Node icon/color indicator
        ImGui::ColorButton("##color", ImVec4(0.2f, 0.7f, 0.9f, 1.0f),
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(16, 16));
        ImGui::SameLine();

        // Tree node
        bool nodeOpen = false;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;

        if (isSelected) { flags |= ImGuiTreeNodeFlags_Selected; }

        if (node->getChildren().empty())
        {
                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                ImGui::TreeNodeEx(node->getName().c_str(), flags);
        }
        else { nodeOpen = ImGui::TreeNodeEx(node->getName().c_str(), flags); }

        if (ImGui::IsItemClicked()) { selectedNodeId = node->getID(); }

        if (isSelected) { ImGui::PopStyleColor(); }

        // Render children
        if (nodeOpen && !node->getChildren().empty())
        {
                for (auto& child : node->getChildren())
                {
                        renderSceneNodeRecursive(child);
                }
                ImGui::TreePop();
        }

        ImGui::PopID();
}

void ImGuiSceneGraph::renderInfoPanel()
{
        const auto& node = sceneGraph.getNode(selectedNodeId);

        // Header with node name
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.7f, 0.9f, 1.0f));
        ImGui::TextWrapped("%s", node->getName().c_str());
        ImGui::PopStyleColor();

        ImGui::Separator();
        ImGui::Spacing();

        // Properties
        ImGui::Text("Transform");
        ImGui::Indent();

        ImGui::Text("Position: %s", glm::to_string(node->getWorldPosition().getv()).c_str());
        ImGui::Text("Rotation: %s", glm::to_string(node->getWorldTransform().getRotation()).c_str());
        ImGui::Text("Scale: %s", glm::to_string(node->getWorldTransform().getScale()).c_str());

        ImGui::Unindent();
}

void ImGuiSceneGraph::render(bool* open)
{
        PROFILE_FUNCTION();

        ImGui::SetNextWindowSize(ImVec2(100, 400), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Scene Graph", open))
        {
                ImGui::End();
                return;
        }
        // Split layout
        {
                ImGui::BeginChild("TreePane", ImVec2(0, 400), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY,
                                  ImGuiWindowFlags_HorizontalScrollbar);
                renderSceneNodeRecursive(sceneGraph.getRoot());
                ImGui::EndChild();
        }

        if (selectedNodeId != -1)
        {
                ImGui::BeginChild("InfoPane", ImVec2(0, 0), ImGuiChildFlags_Borders);
                renderInfoPanel();
                ImGui::EndChild();
        }

        ImGui::End();
}
