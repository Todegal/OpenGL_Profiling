#include "profiler.h"

#include <mutex>
#include <stack>
#include <vector>

namespace Profiler
{

struct ThreadData
{
        std::vector<ProfileEvent> events[2];
        std::stack<size_t> eventStack;
        int writeIdx = 0;
        int depth = 0;

        ThreadData()
        {
                events[0].reserve(1024);
                events[1].reserve(1024);
        }
};

static thread_local ThreadData threadData;

static std::vector<RegionInfo> regionRegister;
static std::mutex registerMutex;

uint32_t RegisterRegion(const char* name, const char* file, int line)
{
        std::lock_guard<std::mutex> scopedLock(registerMutex);

        uint32_t id = static_cast<uint32_t>(regionRegister.size());
        regionRegister.push_back({name, file, line});

        return id;
}

void BeginRegion(uint32_t id) noexcept
{
        ProfileEvent e;
        e.id = id;
        e.startTime = clock_t::now();
        e.depth = threadData.depth;

        auto& events = threadData.events[threadData.writeIdx];

        threadData.eventStack.push(events.size());
        threadData.depth++;

        events.push_back(e);
}

void EndRegion() noexcept
{
        if (threadData.eventStack.empty()) { return; }
        const size_t top = threadData.eventStack.top();
        threadData.eventStack.pop();

        auto& events = threadData.events[threadData.writeIdx];
        events.at(top).endTime = clock_t::now();

        threadData.depth--;
}

void EndFrame()
{
        threadData.writeIdx = 1 - threadData.writeIdx; // swap event buffers
        threadData.events[threadData.writeIdx].clear();
        threadData.eventStack = {};
        threadData.depth = 0;
}

const std::vector<ProfileEvent>& GetLastFrameEvents()
{
        return threadData.events[1 - threadData.writeIdx];
}

const std::vector<ProfileEvent>& GetCurrentFrameEvents()
{
    return threadData.events[threadData.writeIdx];
}

const RegionInfo& GetRegionInfo(uint32_t id)
{
        return regionRegister.at(id);
}

ProfilerStats GetFrameStats()
{
        ProfilerStats stats;
        const auto& events = GetLastFrameEvents();

        stats.totalEvents = events.size();

        for (const auto& event : events)
        {
                stats.maxDepth = std::max(stats.maxDepth, static_cast<size_t>(event.depth));
                if (event.endTime > event.startTime)
                {
                        stats.totalTime +=
                            std::chrono::duration_cast<std::chrono::nanoseconds>(event.endTime - event.startTime);
                }
        }

        return stats;
}

}; // namespace Profiler
