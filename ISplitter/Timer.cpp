#include "Timer.h"

void Timer::start()
{
	mStartTime = std::chrono::system_clock::now();
	mRunning = true;
}

void Timer::stop()
{
	mEndTime = std::chrono::system_clock::now();
	mRunning = false;
}

double Timer::elapsed()
{
	TimePoint endTime;

	if (mRunning)
	{
		endTime = std::chrono::system_clock::now();
	}
	else
	{
		endTime = mEndTime;
	}

	return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(endTime - mStartTime).count());
}
