#pragma once

#include <mutex>
#include <queue>
#include <memory>
#include <string>
#include <iostream>
#include <atomic>

template <typename T>
class threadsafe_queue
{
private:
	mutable std::mutex mDataQueueMutex;
	std::queue<T> mDataQueue;
	std::condition_variable mPopDataCondition;
	std::condition_variable mPushDataCondition;
	const size_t mMaxLength = 0;

	static const std::string TAG;	
	std::atomic_bool mFlushed{ false };

public:
	threadsafe_queue(size_t maxLength)
		: mMaxLength(maxLength)
	{}

	virtual ~threadsafe_queue()
	{
		flush();
	}	

	size_t max_length() const { return mMaxLength; }


	bool push(T new_value, int32_t nWaitForBuffersFreeTimeOutMsec)
	{
		using namespace std;

		bool result = true;

		bool waitInfinite = nWaitForBuffersFreeTimeOutMsec == -1;
		std::chrono::milliseconds waitMs = waitInfinite ?
			std::chrono::milliseconds{ 0 } : std::chrono::milliseconds{ nWaitForBuffersFreeTimeOutMsec };		

		std::unique_lock lock(mDataQueueMutex);
		if (waitInfinite) {
			mPushDataCondition.wait(lock, [this] {return mFlushed || (mDataQueue.size() < mMaxLength); });
		}
		else {
			if (!mPushDataCondition.wait_for(lock, waitMs, [this] {return mFlushed || (mDataQueue.size() < mMaxLength); })) {

				mDataQueue.pop();
				result = false;
			}
		}
		
		if (mFlushed) return false;
			
		mDataQueue.push (std::move(new_value));

		lock.unlock();
		mPopDataCondition.notify_one();

		return result;
	}

	bool wait_and_pop(T& value, int32_t nWaitForBuffersFreeTimeOutMsec)
	{
		using namespace std;

		bool waitInfinite = nWaitForBuffersFreeTimeOutMsec == -1;
		std::chrono::milliseconds waitMs = waitInfinite ?
			std::chrono::milliseconds{ 0 } : std::chrono::milliseconds{ nWaitForBuffersFreeTimeOutMsec };

		std::unique_lock<std::mutex> lock(mDataQueueMutex);

		if (waitInfinite) {
			mPopDataCondition.wait(lock, [this] {return mFlushed || !mDataQueue.empty(); });
		}
		else {
			if (!mPopDataCondition.wait_for(lock, waitMs, [this] {return mFlushed || !mDataQueue.empty(); })) {

				return false;
			}
		}

		if (mFlushed) return false;

		value = std::move(mDataQueue.front());
		mDataQueue.pop();

		lock.unlock();
		mPushDataCondition.notify_one();

		return true;
	}

	std::shared_ptr<T> wait_and_pop()
	{
		std::unique_lock<std::mutex> lock(mDataQueueMutex);
		mPopDataCondition.wait(lock, [this] {return mFlushed || !mDataQueue.empty(); });

		if (mFlushed) return std::shared_ptr<T>{};

		auto res{std::make_shared<T>(std::move(mDataQueue.front())) };
		mDataQueue.pop();

		lock.unlock();
		mPushDataCondition.notify_one();

		return res;
	}

	bool try_pop(T& value)
	{
		std::unique_lock lock(mDataQueueMutex);
		if (mDataQueue.empty())
			return false;

		value = std::move(mDataQueue.front());
		mDataQueue.pop();

		lock.unlock();
		mPushDataCondition.notify_one();

		return true;
	}

	std::shared_ptr<T> try_pop()
	{
		std::unique_lock lock(mDataQueueMutex);
		if (mDataQueue.empty())
			return std::shared_ptr<T>();

		auto res{ std::make_shared<T>(std::move(mDataQueue.front())) };
		mDataQueue.pop();

		lock.unlock();
		mPushDataCondition.notify_one();

		return res;
	}

	bool empty() const
	{
		std::scoped_lock lock(mDataQueueMutex);
		return mDataQueue.empty();
	}

	size_t size() const 
	{
		std::scoped_lock lock(mDataQueueMutex);
		return mDataQueue.size();
	}

	void flush() {	

		using namespace std;

		{
			std::queue<T> empty;
			std::scoped_lock lock(mDataQueueMutex);
			std::swap(mDataQueue, empty);
		}

		mFlushed = true;

		mPopDataCondition.notify_all();
		mPushDataCondition.notify_all();
	}
};

template<typename T>
const std::string threadsafe_queue<T>::TAG = "threadsafe_queue: ";



