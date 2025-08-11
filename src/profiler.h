#pragma once

#include <chrono>
#include <string>

namespace Profiler
{

using clock_t = std::chrono::steady_clock;
using time_point_t = clock_t::time_point;

struct RegionInfo
{
        std::string name;
        std::string file;
        int line;
};

struct ProfileEvent
{
        uint32_t id;
        clock_t::time_point startTime;
        clock_t::time_point endTime;
        int depth; // Callstack depth
};

uint32_t RegisterRegion(const char* name, const char* file, int line);

void BeginRegion(uint32_t id) noexcept;
void EndRegion() noexcept;

void EndFrame();

const std::vector<ProfileEvent>& GetCurrentFrameEvents();
const std::vector<ProfileEvent>& GetLastFrameEvents();
const RegionInfo& GetRegionInfo(uint32_t id);

struct ProfilerStats
{
        size_t totalEvents = 0;
        size_t maxDepth = 0;
        std::chrono::nanoseconds totalTime{0};
};

ProfilerStats GetFrameStats();

}; // namespace Profiler

class ScopedProfile
{
      public:
        ScopedProfile(const uint32_t id) noexcept
        {
                Profiler::BeginRegion(id);
        }

        ~ScopedProfile() noexcept
        {
                Profiler::EndRegion();
        }

        ScopedProfile(const ScopedProfile&) = delete;
        ScopedProfile& operator=(const ScopedProfile&) = delete;

        ScopedProfile(ScopedProfile&&) = delete;
        ScopedProfile& operator=(ScopedProfile&&) = delete;
};

#if defined(__clang__) || defined(__GNUC__)
#define FUNC_NAME __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define FUNC_NAME __FUNCSIG__
#else
#define FUNC_NAME __func__
#endif

#ifndef NDEBUG

#define COMBINE1(X, Y) X##Y // helper macro
#define COMBINE(X, Y) COMBINE1(X, Y)

#define PROFILE_SCOPE(name)                                                                                            \
        static const uint32_t COMBINE(__profilerID, __LINE__) = Profiler::RegisterRegion(name, __FILE__, __LINE__);    \
        ScopedProfile COMBINE(__scopedProfile, __LINE__)(COMBINE(__profilerID, __LINE__));

#define PROFILE_FUNCTION() PROFILE_SCOPE(FUNC_NAME)

#else

#define PROFILE_FUNCTION() ((void)0)
#define PROFILE_SCOPE(name) ((void)0)

#endif
