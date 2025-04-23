#include "timer.h"

void Timer::start()
{
	startTime = hrClock::now();
	currentTime = startTime;
	lastTime = startTime;
}

void Timer::tick()
{
	lastTime = currentTime;

	currentTime = hrClock::now();
	deltaTime = currentTime - lastTime;
}
