#pragma once

#include <chrono>

template<typename F>
std::chrono::high_resolution_clock::duration fixedTimeStepLags;

// Massively over-engineered timing class
class Timer
{
public:
	using f_seconds = std::chrono::duration<float>;
	using f_mlliseconds = std::chrono::duration<float, std::milli>;

	Timer()
		: deltaTime(hrClock::duration::zero())
	{ }

private:
	using hrClock = std::chrono::high_resolution_clock;

	hrClock::time_point startTime;
	hrClock::time_point currentTime;
	hrClock::time_point lastTime;

	hrClock::duration deltaTime;

public:
	void start();

	void tick();

public:
	template<typename T>
	inline T getDeltaTime()
	{
		return std::chrono::duration_cast<T>(deltaTime);
	}

	template<typename T>
	inline T getTimeElapsed()
	{
		return std::chrono::duration_cast<T>(currentTime - startTime);
	}

	// Run F function every _Ratio duration ratio with the arguments
	template<typename _Ratio, typename F, typename... Args>
	void fixedTimeStep(F function, const Args&... args);
};

template<typename _Ratio, typename F, typename ...Args >
inline void Timer::fixedTimeStep(F function, const Args&... args)
{
	fixedTimeStepLags<F> += deltaTime;

	constexpr hrClock::duration fixedDuration = std::chrono::duration_cast<hrClock::duration>(std::chrono::duration<int, _Ratio>(1));

	while (fixedTimeStepLags<F> >= fixedDuration)
	{
		function(args...);
		fixedTimeStepLags<F> -= fixedDuration;
	}
}