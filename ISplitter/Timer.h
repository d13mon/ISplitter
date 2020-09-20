#pragma once

#include <chrono>

class Timer
{
public:
	void start();
	void stop();	
	double elapsed();	

private:
	using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

	TimePoint mStartTime;
	TimePoint mEndTime;
	bool      mRunning = false;
};
