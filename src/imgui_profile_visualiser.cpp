#include "imgui_profile_visualiser.h"

#include "profiler.h"
#include <algorithm>
#include <imgui.h>
#include <iomanip>
#include <sstream>

ImGuiProfileVisualiser::ImGuiProfileVisualiser()
{
        PROFILE_FUNCTION();
}

void ImGuiProfileVisualiser::render(bool* open)
{
        PROFILE_FUNCTION();

        ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(m_windowTitle.c_str(), open))
        {
                ImGui::End();
                return;
        }

        renderModeSelector();

        // Update button
        if (ImGui::Button("Refresh Data")) { processEvents(m_viewMode); }

        ImGui::Separator();

        // Split layout
        {
                ImGui::BeginChild("LeftProfilerPane", ImVec2(300, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX,
                                  ImGuiWindowFlags_HorizontalScrollbar);
                renderTreeView();
                ImGui::EndChild();
        }

        ImGui::SameLine();

        {
                ImGui::BeginChild("RightProfilerPane", ImVec2(0, 0), ImGuiChildFlags_Borders);

                {
                        ImGui::BeginChild("TimelinePane", ImVec2(0, 400), ImGuiChildFlags_ResizeY);
                        renderTimeline();
                        ImGui::EndChild();
                }

                {
                        ImGui::BeginChild("StatisticsPane", ImVec2(0, 100));
                        ImGui::Separator();
                        renderStatistics();
                        ImGui::EndChild();
                }

                ImGui::EndChild();
        }

        ImGui::End();
}

void ImGuiProfileVisualiser::updateInitEvents(const std::vector<Profiler::ProfileEvent>& events)
{
        initEvents = events;
        processEvents(ViewMode::Initialization);
}

void ImGuiProfileVisualiser::processEvents(ViewMode type)
{
        PROFILE_FUNCTION();

        const auto& events = (type == ViewMode::Initialization) ? initEvents : Profiler::GetLastFrameEvents();
        if (type == ViewMode::Frame) { frameEvents = events; }

        if (events.empty())
        {
                rootNodes.clear();
                totalFrameTime = std::chrono::nanoseconds{0};
                return;
        }

        // Build the event tree
        rootNodes = buildEventTree(events);

        // Calculate total time and minimum start time
        auto minTime = events[0].startTime;
        auto maxTime = events[0].endTime;

        for (const auto& event : events)
        {
                if (event.startTime < minTime) minTime = event.startTime;
                if (event.endTime > maxTime) maxTime = event.endTime;
        }

        minStartTime = std::chrono::duration_cast<std::chrono::nanoseconds>(minTime.time_since_epoch());
        totalFrameTime = std::chrono::duration_cast<std::chrono::nanoseconds>(maxTime - minTime);

        // Calculate relative timings for all nodes
        for (auto& root : rootNodes)
        {
                calculateTimings(root, minStartTime);
        }
}

std::vector<std::shared_ptr<ImGuiProfileVisualiser::EventNode>> ImGuiProfileVisualiser::buildEventTree(
    const std::vector<Profiler::ProfileEvent>& events)
{
        PROFILE_FUNCTION();
        std::vector<std::shared_ptr<EventNode>> roots;
        std::vector<std::shared_ptr<EventNode>> stack;

        for (const auto& event : events)
        {
                auto node = std::make_shared<EventNode>();
                node->id = event.id;

                // Get region info
                const auto& regionInfo = Profiler::GetRegionInfo(event.id);
                node->name = regionInfo.name;
                node->file = regionInfo.file;
                node->line = regionInfo.line;

                node->duration = std::chrono::duration_cast<std::chrono::nanoseconds>(event.endTime - event.startTime);
                node->startOffset =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(event.startTime.time_since_epoch());
                node->depth = event.depth;
                node->color = getEventColor(event.id);

                // Build hierarchy based on depth
                while (!stack.empty() && stack.back()->depth >= event.depth)
                {
                        stack.pop_back();
                }

                if (stack.empty()) { roots.push_back(node); }
                else
                {
                        stack.back()->children.push_back(node);
                }

                stack.push_back(node);
        }

        return roots;
}

void ImGuiProfileVisualiser::calculateTimings(std::shared_ptr<EventNode> node, std::chrono::nanoseconds baseTime)
{
        PROFILE_FUNCTION();

        node->startOffset -= baseTime;

        for (auto& child : node->children)
        {
                calculateTimings(child, baseTime);
        }
}

void ImGuiProfileVisualiser::renderModeSelector()
{
        PROFILE_FUNCTION();

        ImGui::Text("View Mode:");
        ImGui::SameLine();

        bool isInit = (m_viewMode == ViewMode::Initialization);
        bool isFrame = (m_viewMode == ViewMode::Frame);

        if (ImGui::RadioButton("Initialization", isInit))
        {
                m_viewMode = ViewMode::Initialization;
                processEvents(m_viewMode);
        }

        ImGui::SameLine();

        if (ImGui::RadioButton("Frame", isFrame))
        {
                m_viewMode = ViewMode::Frame;
                processEvents(m_viewMode);
        }
}

void ImGuiProfileVisualiser::renderTreeView()
{
        PROFILE_FUNCTION();

        ImGui::Text("Event Hierarchy");
        ImGui::Separator();

        if (rootNodes.empty())
        {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No profiling data available");
                return;
        }

        for (auto& root : rootNodes)
        {
                renderTreeNodeRecursive(root);
        }
}

void ImGuiProfileVisualiser::renderTreeNodeRecursive(std::shared_ptr<EventNode> node)
{
        ImGui::PushID(static_cast<int>(node->id));

        // Color indicator
        ImGui::ColorButton("##color", ImGui::ColorConvertU32ToFloat4(node->color), ImGuiColorEditFlags_NoTooltip,
                           ImVec2(16, 16));
        ImGui::SameLine();

        // Node with duration info
        std::string label = node->name + " (" + formatDuration(node->duration) + ")";

        bool nodeOpen = false;
        if (!node->children.empty())
        {
                nodeOpen = ImGui::TreeNodeEx(label.c_str(), node->isExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0);
                node->isExpanded = nodeOpen;
        }
        else
        {
                ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
        }

        // Handle selection
        if (ImGui::IsItemClicked()) { selectedEventId = static_cast<int>(node->id); }

        // Tooltip with detailed info
        if (ImGui::IsItemHovered())
        {
                ImGui::BeginTooltip();
                ImGui::Text("Event: %s", node->name.c_str());
                ImGui::Text("Duration: %s", formatDuration(node->duration).c_str());
                ImGui::Text("File: %s:%d", node->file.c_str(), node->line);
                if (totalFrameTime.count() > 0)
                {
                        ImGui::Text("Percentage: %s", formatPercentage(node->duration, totalFrameTime).c_str());
                }
                ImGui::EndTooltip();
        }

        // Render children
        if (nodeOpen && !node->children.empty())
        {
                for (auto& child : node->children)
                {
                        renderTreeNodeRecursive(child);
                }
                ImGui::TreePop();
        }

        ImGui::PopID();
}

void ImGuiProfileVisualiser::renderTimeline()
{
        PROFILE_FUNCTION();

        ImGui::Text("Timeline View");
        ImGui::Separator();

        if (rootNodes.empty())
        {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No profiling data available");
                return;
        }

        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        canvasSize.y -= 40;

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Background
        drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                                IM_COL32(25, 25, 25, 255));

        // Draw timeline
        for (auto& root : rootNodes)
        {
                renderTimelineNode(root, canvasPos, canvasSize);
        }

        // Time scale at bottom
        const int numTicks = 10;
        float tickSpacing = canvasSize.x / numTicks;

        for (int i = 0; i <= numTicks; ++i)
        {
                float x = canvasPos.x + static_cast<float>(i) * tickSpacing;
                float timeRatio = static_cast<float>(i) / numTicks;
                auto tickTime = std::chrono::nanoseconds(totalFrameTime.count() * static_cast<long long>(timeRatio));

                drawList->AddLine(ImVec2(x, canvasPos.y + canvasSize.y - 20), ImVec2(x, canvasPos.y + canvasSize.y),
                                  IM_COL32(255, 255, 255, 100));

                std::string timeLabel = formatDuration(tickTime);
                ImVec2 textSize = ImGui::CalcTextSize(timeLabel.c_str());
                drawList->AddText(ImVec2(x - textSize.x * 0.5f, canvasPos.y + canvasSize.y + 2),
                                  IM_COL32(255, 255, 255, 255), timeLabel.c_str());
        }

        // Handle input
        handleTimelineInput(canvasPos, canvasSize);

        // Reserve space
        ImGui::Dummy(ImVec2(canvasSize.x, canvasSize.y + 25));
}

void ImGuiProfileVisualiser::renderTimelineNode(std::shared_ptr<EventNode> node, const ImVec2& canvasPos,
                                                const ImVec2& canvasSize)
{
        if (totalFrameTime.count() == 0) return;

        ImVec2 nodePos = calculateNodePosition(node, canvasSize);
        ImVec2 nodeSize = calculateNodeSize(node, canvasSize);

        nodePos.x += canvasPos.x;
        nodePos.y += canvasPos.y;

        if (nodeSize.x < NODE_MIN_WIDTH) return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Draw node rectangle
        drawList->AddRectFilled(nodePos, ImVec2(nodePos.x + nodeSize.x, nodePos.y + nodeSize.y), node->color);

        drawList->AddRect(nodePos, ImVec2(nodePos.x + nodeSize.x, nodePos.y + nodeSize.y),
                          IM_COL32(255, 255, 255, 100));

        const auto textWidth = ImGui::CalcTextSize(node->name.c_str());
        if (nodeSize.x > textWidth.x)
        {
                ImVec2 textPos = ImVec2(nodePos.x + 4, nodePos.y + 4);
                drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), node->name.c_str());
        }

        // Check for hover
        ImVec2 mousePos = ImGui::GetMousePos();
        if (mousePos.x >= nodePos.x && mousePos.x <= nodePos.x + nodeSize.x && mousePos.y >= nodePos.y &&
            mousePos.y <= nodePos.y + nodeSize.y)
        {
                // Highlight on hover
                drawList->AddRect(nodePos, ImVec2(nodePos.x + nodeSize.x, nodePos.y + nodeSize.y),
                                  IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);

                // Show tooltip
                if (ImGui::IsMouseHoveringRect(nodePos, ImVec2(nodePos.x + nodeSize.x, nodePos.y + nodeSize.y)))
                {
                        ImGui::BeginTooltip();
                        ImGui::Text("Event: %s", node->name.c_str());
                        ImGui::Text("Duration: %s", formatDuration(node->duration).c_str());
                        ImGui::Text("Start: %s", formatDuration(node->startOffset).c_str());
                        ImGui::Text("Percentage: %s", formatPercentage(node->duration, totalFrameTime).c_str());
                        ImGui::EndTooltip();
                }
        }

        // Render children
        for (auto& child : node->children)
        {
                renderTimelineNode(child, canvasPos, canvasSize);
        }
}

ImVec2 ImGuiProfileVisualiser::calculateNodePosition(std::shared_ptr<EventNode> node, const ImVec2& canvasSize) const
{
        float x = timeToPixel(node->startOffset, canvasSize.x);
        float y = static_cast<float>(node->depth) * DEPTH_HEIGHT + TIMELINE_PADDING;
        return ImVec2(x, y);
}

ImVec2 ImGuiProfileVisualiser::calculateNodeSize(std::shared_ptr<EventNode> node, const ImVec2& canvasSize) const
{
        float width = std::max(timeToPixel(node->duration, canvasSize.x), NODE_MIN_WIDTH);
        float height = DEPTH_HEIGHT - 2;
        return ImVec2(width, height);
}

float ImGuiProfileVisualiser::timeToPixel(std::chrono::nanoseconds time, float canvasWidth) const
{
        if (totalFrameTime.count() == 0) return 0.0f;
        return (static_cast<float>(time.count()) / static_cast<float>(totalFrameTime.count())) * canvasWidth;
}

void ImGuiProfileVisualiser::renderStatistics()
{
        PROFILE_FUNCTION();

        ImGui::Text("Statistics");
        ImGui::Separator();

        if (totalFrameTime.count() == 0)
        {
                ImGui::Text("No timing data available");
                return;
        }

        ImGui::Text("Total Time: %s", formatDuration(totalFrameTime).c_str());

        const auto& events = (m_viewMode == ViewMode::Initialization) ? initEvents : frameEvents;
        ImGui::Text("Event Count: %zu", events.size());

        if (!events.empty())
        {
                int maxDepth = 0;
                for (const auto& event : events)
                {
                        maxDepth = std::max(maxDepth, event.depth);
                }

                ImGui::Text("Max Depth: %d", maxDepth + 1);
        }
}

void ImGuiProfileVisualiser::handleTimelineInput(const ImVec2& canvasPos, const ImVec2& canvasSize)
{
        // Handle mouse clicks on timeline for selection
        if (ImGui::IsMouseClicked(0))
        {
                ImVec2 mousePos = ImGui::GetMousePos();
                if (mousePos.x >= canvasPos.x && mousePos.x <= canvasPos.x + canvasSize.x &&
                    mousePos.y >= canvasPos.y && mousePos.y <= canvasPos.y + canvasSize.y)
                {
                        // Timeline clicked - could implement zoom or selection here
                }
        }
}

ImU32 ImGuiProfileVisualiser::getEventColor(uint32_t id) const
{
        // Generate color based on ID hash
        uint32_t hash = id;
        hash ^= hash >> 16;
        hash *= 0x85ebca6b;
        hash ^= hash >> 13;
        hash *= 0xc2b2ae35;
        hash ^= hash >> 16;

        uint8_t r = static_cast<uint8_t>((hash & 0xFF0000) >> 16);
        uint8_t g = static_cast<uint8_t>((hash & 0x00FF00) >> 8);
        uint8_t b = static_cast<uint8_t>(hash & 0x0000FF);

        // CLAMP
        r = std::min(std::max(r, static_cast<uint8_t>(50)), static_cast<uint8_t>(150));
        g = std::min(std::max(g, static_cast<uint8_t>(50)), static_cast<uint8_t>(150));
        b = std::min(std::max(b, static_cast<uint8_t>(50)), static_cast<uint8_t>(150));

        return IM_COL32(r, g, b, 255);
}

std::string ImGuiProfileVisualiser::formatDuration(std::chrono::nanoseconds duration) const
{
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);

        auto ns = duration.count();

        if (ns >= 1000000000) // >= 1 second
        {
                oss << (static_cast<double>(ns) / 1e9f) << "s";
        }
        else if (ns >= 1000000) // >= 1 millisecond
        {
                oss << (static_cast<double>(ns) / 1e6f) << "ms";
        }
        else if (ns >= 1000) // >= 1 microsecond
        {
                oss << (static_cast<double>(ns) / 1e3f) << "us";
        }
        else
        {
                oss << ns << "ns";
        }

        return oss.str();
}

std::string ImGuiProfileVisualiser::formatPercentage(std::chrono::nanoseconds part,
                                                     std::chrono::nanoseconds total) const
{
        if (total.count() == 0) return "0.0%";

        double percentage = (static_cast<double>(part.count()) / static_cast<double>(total.count())) * 100.0;

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << percentage << "%";
        return oss.str();
}
