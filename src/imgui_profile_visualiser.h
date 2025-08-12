#pragma once

#include "profiler.h"

#include <imgui.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ImGuiProfileVisualiser
{
      public:
        enum class ViewMode
        {
                Initialization,
                Frame
        };

        ImGuiProfileVisualiser();
        ~ImGuiProfileVisualiser() = default;

        // Main render function - call this in your ImGui render loop
        void render(bool* open);

        void updateInitEvents(const std::vector<Profiler::ProfileEvent>& initEvents);

      private:
        struct EventNode
        {
                uint32_t id;
                std::string name;
                std::string file;
                int line;
                std::chrono::nanoseconds duration;
                std::chrono::nanoseconds startOffset;
                int depth;
                ImU32 color;
                std::vector<std::shared_ptr<EventNode>> children;
                bool isExpanded = true;
        };

        // Core data
        ViewMode m_viewMode = ViewMode::Initialization;
        std::string m_windowTitle = "Profiler";
        std::vector<Profiler::ProfileEvent> initEvents;
        std::vector<Profiler::ProfileEvent> frameEvents;

        // Processed data for visualization
        std::vector<std::shared_ptr<EventNode>> rootNodes;
        std::chrono::nanoseconds totalFrameTime{0};
        std::chrono::nanoseconds minStartTime{0};

        // UI state
        int selectedEventId = -1;

        // Private methods
        void renderModeSelector();
        void renderTreeView();
        void renderTimeline();
        void renderStatistics();
        void renderEventDetails();

        // Data processing
        void processEvents(ViewMode type);
        std::vector<std::shared_ptr<EventNode>> buildEventTree(const std::vector<Profiler::ProfileEvent>& events);
        void calculateTimings(std::shared_ptr<EventNode> node, std::chrono::nanoseconds baseTime);

        // Tree view helpers
        void renderTreeNode(std::shared_ptr<EventNode> node);
        void renderTreeNodeRecursive(std::shared_ptr<EventNode> node);

        // Timeline helpers
        void renderTimelineNode(std::shared_ptr<EventNode> node, const ImVec2& canvasPos, const ImVec2& canvasSize);
        float timeToPixel(std::chrono::nanoseconds time, float canvasWidth) const;
        ImVec2 calculateNodePosition(std::shared_ptr<EventNode> node, const ImVec2& canvasSize) const;
        ImVec2 calculateNodeSize(std::shared_ptr<EventNode> node, const ImVec2& canvasSize) const;

        // Color management
        ImU32 getEventColor(uint32_t eventId) const;

        // Utility functions
        std::string formatDuration(std::chrono::nanoseconds duration) const;
        std::string formatPercentage(std::chrono::nanoseconds part, std::chrono::nanoseconds total) const;

        // Input handling
        void handleTimelineInput(const ImVec2& canvasPos, const ImVec2& canvasSize);

        // Constants
        static constexpr float MIN_VISIBLE_DURATION_MS = 0.001f;
        static constexpr float TIMELINE_PADDING = 10.0f;
        static constexpr float NODE_MIN_WIDTH = 2.0f;
        static constexpr float DEPTH_HEIGHT = 25.0f;
};
