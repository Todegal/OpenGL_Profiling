#pragma once

#include <chrono>
#include <functional>
#include <ratio>
#include <vector>

// Independent timer, will start from acquisition
// Clock must be std::chrono::clock
template <typename Clock = std::chrono::steady_clock>
class Timer
{
        static_assert(std::chrono::is_clock<Clock>::value, "Clock must be a valid std::chrono clock!");

      public:
        Timer() : startTime(Clock::now())
        {
                elapsedLastUpdate = std::chrono::nanoseconds::zero();
                deltaTime = std::chrono::nanoseconds::zero();

                isPaused = false;
                pausedTotal = std::chrono::nanoseconds::zero();
                pausedStart = startTime;
        }

        ~Timer() = default;

        // Returns absolute time point
        static const std::chrono::time_point<Clock> getNow()
        {
                return Clock::now();
        }

        // Returns duration since timer's construction, minus any time where the timer has been paused
        template <typename _Dur>
        const _Dur getElapsedTime() const
        {
                const auto now = isPaused ? pausedStart : Clock::now();
                return std::chrono::duration_cast<_Dur>(now - (startTime + pausedTotal));
        }

        float getElapsedTimeSeconds() const
        {
                return getElapsedTime<std::chrono::duration<float, std::ratio<1>>>().count();
        }

        void update()
        {
                if (isPaused) return;

                const auto elapsed = getElapsedTime<std::chrono::nanoseconds>();
                deltaTime = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed - elapsedLastUpdate);

                for (FixedIntervalFunction& f : fixedIntervalFunctions)
                {
                        if ((elapsed - f.lastCall) >= f.interval)
                        {
                                f.function();
                                f.lastCall += f.interval;
                        }
                }

                for (VariableIntervalFunction& f : variableIntervalFunctions)
                {
                        if (elapsed >= f.nextCall) { f.nextCall = f.function(); }
                }

                elapsedLastUpdate = elapsed;
        }

        void pause()
        {
                if (!isPaused)
                {
                        pausedStart = Clock::now();
                        isPaused = true;
                }
        }

        void resume()
        {
                if (isPaused)
                {
                        pausedTotal += Clock::now() - pausedStart;
                        isPaused = false;
                }
        }

        const std::chrono::time_point<Clock> getLastUpdate() const
        {
                return elapsedLastUpdate;
        }

        float getDeltaTimeSeconds() const
        {
                return getDeltaTime<std::chrono::duration<float, std::ratio<1>>>().count();
        }

        float getDeltaTimeMilliseconds() const
        {
                return getDeltaTime<std::chrono::duration<float, std::milli>>().count();
        }

        template <typename _Dur>
        const _Dur getDeltaTime() const
        {
                return std::chrono::duration_cast<_Dur>(deltaTime);
        }

        template <typename _Dur>
        void addFixedIntervalFunction(const std::function<void(void)>& function, const _Dur& interval = _Dur(1))
        {
                auto nsInterval = std::chrono::duration_cast<std::chrono::nanoseconds>(interval);

                FixedIntervalFunction fixedIntervalFunction{};
                fixedIntervalFunction.function = function;
                fixedIntervalFunction.interval = nsInterval;
                fixedIntervalFunction.lastCall = getElapsedTime<std::chrono::nanoseconds>();

                fixedIntervalFunctions.push_back(fixedIntervalFunction);
        }

        void addVariableIntervalFunction(std::function<std::chrono::nanoseconds(void)> function,
                                         std::chrono::nanoseconds firstCall)
        {
                VariableIntervalFunction variableIntervalFunction{};
                variableIntervalFunction.function = function;
                variableIntervalFunction.nextCall = firstCall;

                variableIntervalFunctions.push_back(variableIntervalFunction);
        }

      private:
        const std::chrono::time_point<Clock> startTime;

        std::chrono::nanoseconds elapsedLastUpdate;
        std::chrono::nanoseconds deltaTime;

        struct FixedIntervalFunction
        {
                std::function<void()> function;
                std::chrono::nanoseconds interval;
                std::chrono::nanoseconds lastCall;
        };

        std::vector<FixedIntervalFunction> fixedIntervalFunctions;

        struct VariableIntervalFunction
        {
                std::function<std::chrono::nanoseconds(void)> function;
                std::chrono::nanoseconds nextCall;
        };

        std::vector<VariableIntervalFunction> variableIntervalFunctions;

        bool isPaused;
        std::chrono::time_point<Clock> pausedStart;
        std::chrono::nanoseconds pausedTotal;
};
