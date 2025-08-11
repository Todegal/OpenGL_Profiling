#pragma once

#include "timer.h"

#include <chrono>
#include <memory>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>

class Profiler
{
      private:
        using clock_t = std::chrono::high_resolution_clock;
        using sample_t = std::tuple<std::string, std::chrono::time_point<clock_t>>;

      public:
        Profiler();
        ~Profiler();

        Profiler(const Profiler&) = delete;
        Profiler& operator=(const Profiler&) = delete;

        void beginRegion(const std::string& name);
        void endRegion();

        struct TimedRegion
        {
                std::string name;
                std::chrono::time_point<clock_t> startTime;

                std::chrono::nanoseconds lastSample; // time it took last time this region was profiled
                std::chrono::nanoseconds totalDuration;

                std::unordered_map<std::string, std::shared_ptr<TimedRegion>> children;

                float getSampleMilliseconds() const
                {
                        return static_cast<float>(lastSample.count()) / 1e6f;
                }

                float getDurationMilliseconds() const
                {
                        return static_cast<float>(totalDuration.count()) / 1e6f;
                }
        };

        const std::unordered_map<std::string, std::shared_ptr<TimedRegion>>& getTopRegions() const
        {
                return topRegions;
        };

      private:
        const Timer<clock_t> timer;
        std::stack<std::shared_ptr<TimedRegion>> regionCallStack;

        std::unordered_map<std::string, std::shared_ptr<TimedRegion>> topRegions;
};

extern Profiler& getProfiler();

class ScopedProfile
{
      public:
        ScopedProfile(const char* name)
        {
                getProfiler().beginRegion(name);
        }
        ~ScopedProfile()
        {
                getProfiler().endRegion();
        }
};

#if defined(__clang__) || defined(__GNUC__)
#define FUNC_NAME __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define FUNC_NAME __FUNCSIG__
#else
#define FUNC_NAME __func__
#endif

#ifndef NDEBUG

#define PROFILE_FUNCTION() ScopedProfile __scopedProfile(FUNC_NAME)
#define PROFILE_SCOPE(name) ScopedProfile __scopedProfile(name)

#else

#define PROFILE_FUNCTION() ((void)0)
#define PROFILE_SCOPE(name) ((void)0)

#endif
