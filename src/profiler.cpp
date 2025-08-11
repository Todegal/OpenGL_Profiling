#include "profiler.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

Profiler::Profiler() : timer()
{
}

Profiler::~Profiler()
{
}

void Profiler::beginRegion(const std::string& name)
{
        const auto now = timer.getNow();
        std::shared_ptr<TimedRegion> region;

        if (regionCallStack.empty())
        {
                auto& r = topRegions[name];
                if (!r)
                {
                        r = std::make_shared<TimedRegion>();
                        r->name = name;
                }

                region = r;
        }
        else
        {
                auto& r = regionCallStack.top()->children[name];
                if (!r)
                {
                        r = std::make_shared<TimedRegion>();
                        r->name = name;
                }

                region = r;
        }

        region->startTime = now;
        regionCallStack.push(region);
}

void Profiler::endRegion()
{
        if (regionCallStack.empty())
        {
                throw std::runtime_error(
                    "Attempting to end section when none have strarted, stack is obviously broken...");
        }

        auto top = regionCallStack.top();

        const auto now = timer.getNow();
        const auto sampleDuration = now - top->startTime;

        top->lastSample = std::chrono::duration_cast<std::chrono::nanoseconds>(sampleDuration);
        top->totalDuration += std::chrono::duration_cast<std::chrono::nanoseconds>(sampleDuration);

        regionCallStack.pop();
}

Profiler engineProfiler;

Profiler& getProfiler()
{
        return engineProfiler;
}
