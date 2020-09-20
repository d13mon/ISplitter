#include "pch.h"

#include "ISplitter.h"
#include "ISplitter.cpp"

#include "Timer.h"
#include "Timer.cpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <limits>
#include <memory>
#include <future>

using namespace std;
using namespace std::chrono;

using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

using IntList = std::vector<int>;
using DataSet = IntList;

using InputData = std::tuple<DataSet, int, int, milliseconds, milliseconds, milliseconds, milliseconds>;


class TestISplitterBase : public ::testing::Test
{
protected:
	void SetUp()
	{
		mSplitter = ISplitter::Create(mMaxBuffers, mMaxClients);
	}
	void TearDown()
	{
		mSplitter.reset();
	}

	uint32_t invalidClientId() const {
       return std::numeric_limits<uint32_t>::max();
	}

public:
	ISplitterPtr mSplitter;
	size_t mMaxClients = 2;
	size_t mMaxBuffers = 2;	
};

TEST_F(TestISplitterBase, test_base_Create)
{
	auto splitter = ISplitter::Create(2, 3);
	size_t maxBuffers;
	size_t maxClients;
	splitter->InfoGet(&maxBuffers, &maxClients);	

	ASSERT_EQ(maxBuffers, 2);
	ASSERT_EQ(maxClients, 3);
}

TEST_F(TestISplitterBase, test_base_ClientAdd)
{
	size_t maxBuffers;
	size_t maxClients;
	size_t clientCount = 0;
	mSplitter->InfoGet(&maxBuffers, &maxClients);

	auto res = mSplitter->ClientGetCount(nullptr);
	ASSERT_FALSE(res);
	res = mSplitter->ClientGetCount(&clientCount);
	ASSERT_EQ(clientCount, 0);

	ASSERT_EQ(maxBuffers, mMaxBuffers);
	ASSERT_EQ(maxClients, mMaxClients);

	uint32_t id = 0, id2 = 0, id3 = 0;
	res = mSplitter->ClientAdd(&id);
	cout << "test_ClientAdd: " << "ID = " << id << endl;
	ASSERT_TRUE(id > 0);
	ASSERT_TRUE(res);

	res = mSplitter->ClientAdd(nullptr);
	ASSERT_FALSE(res);

	res = mSplitter->ClientAdd(&id2);
	cout << "test_ClientAdd: " << "ID = " << id2 << endl;
	ASSERT_TRUE(id2 > id);
	ASSERT_TRUE(res);

	res = mSplitter->ClientGetCount(&clientCount);
	ASSERT_TRUE(res);
	ASSERT_EQ(clientCount, 2);

	res = mSplitter->ClientAdd(&id3);	
	ASSERT_FALSE(res);

	res = mSplitter->ClientGetCount(&clientCount);
	ASSERT_TRUE(res);
	ASSERT_EQ(clientCount, 2);	
}


TEST_F(TestISplitterBase, test_base_ClientAddRemove)
{	
	size_t clientCount = 0;		
	auto res = mSplitter->ClientGetCount(&clientCount);
	ASSERT_EQ(clientCount, 0);	

	vector<uint32_t> ids;	
	
	for (size_t i = 0; i < mMaxClients; i++) {
		
		uint32_t id;
		res = mSplitter->ClientAdd(&id);
		ids.push_back(id);		
		ASSERT_TRUE(id > 0);
		ASSERT_TRUE(res);
	}

	for (auto id : ids) {
		cout << "test_ClientAddRemove: " << "ID = " << id << endl;
	}

	res = mSplitter->ClientGetCount(&clientCount);
	ASSERT_TRUE(res);
	ASSERT_EQ(clientCount, mMaxClients);
	
	res = mSplitter->ClientRemove(invalidClientId());
	ASSERT_FALSE(res);
	
	size_t clientCountAfter = 0;
	res = mSplitter->ClientGetCount(&clientCountAfter);
	ASSERT_TRUE(res);
	ASSERT_EQ(clientCount, clientCountAfter);

	res = mSplitter->ClientRemove(ids[0]);
	ASSERT_TRUE(res);
	
	res = mSplitter->ClientGetCount(&clientCountAfter);
	ASSERT_TRUE(res);
	clientCount--;
	ASSERT_EQ(clientCount, clientCountAfter);

	res = mSplitter->ClientRemove(ids[0]);
	ASSERT_FALSE(res);

	res = mSplitter->ClientGetCount(&clientCountAfter);
	ASSERT_TRUE(res);	
	ASSERT_EQ(clientCount, clientCountAfter);

	res = mSplitter->ClientRemove(ids[1]);
	ASSERT_TRUE(res);

	res = mSplitter->ClientGetCount(&clientCountAfter);
	ASSERT_TRUE(res);
	ASSERT_EQ(0, clientCountAfter);
}


TEST_F(TestISplitterBase, test_base_ClientGetByIndex)
{
	size_t clientCount = 0;
	auto res = mSplitter->ClientGetCount(&clientCount);
	ASSERT_EQ(clientCount, 0);

	vector<uint32_t> ids;	

	for (size_t i = 0; i < mMaxClients; i++) {

		uint32_t id;
		res = mSplitter->ClientAdd(&id);
		ids.push_back(id);
		ASSERT_TRUE(id > 0);
		ASSERT_TRUE(res);
	}

	for (auto id : ids) {
		cout << "test_base_ClientGetByIndex: " << "ID = " << id << endl;
	}

	size_t latency;
	size_t dropped;
	uint32_t clientId;
	
	res = mSplitter->ClientGetByIndex(1, nullptr, nullptr, nullptr);
	ASSERT_FALSE(res);
	res = mSplitter->ClientGetByIndex(1, nullptr, &dropped, &latency);
	ASSERT_FALSE(res);
	res = mSplitter->ClientGetByIndex(1, &clientId, nullptr, &latency);
	ASSERT_FALSE(res);
	res = mSplitter->ClientGetByIndex(1, &clientId, &dropped, nullptr);
	ASSERT_FALSE(res);
	res = mSplitter->ClientGetByIndex(invalidClientId(), &clientId, &dropped, &latency);
	ASSERT_FALSE(res);
	res = mSplitter->ClientGetByIndex(0, &clientId, &dropped, &latency);
	ASSERT_TRUE(res);
	ASSERT_TRUE(clientId > 0);
	ASSERT_EQ(latency, 0);
	ASSERT_EQ(dropped, 0);

	res = mSplitter->ClientGetByIndex(1, &clientId, &dropped, &latency);
	ASSERT_TRUE(res);
	ASSERT_TRUE(clientId > 0);
	ASSERT_EQ(latency, 0);
	ASSERT_EQ(dropped, 0);	
}

TEST_F(TestISplitterBase, test_base_ClientGetById)
{
	mSplitter = ISplitter::Create(2, 2);

	size_t clientCount = 0;
	auto res = mSplitter->ClientGetCount(&clientCount);
	ASSERT_EQ(clientCount, 0);

	vector<uint32_t> ids;

	for (size_t i = 0; i < mMaxClients; i++) {

		uint32_t id;
		res = mSplitter->ClientAdd(&id);
		ids.push_back(id);
		ASSERT_TRUE(id > 0);
		ASSERT_TRUE(res);
	}

	for (auto id : ids) {
		cout << "test_base_ClientGetById: " << "ID = " << id << endl;
	}

	size_t latency;
	size_t dropped;

	res = mSplitter->ClientGetById(ids[1], nullptr, &dropped);
	ASSERT_FALSE(res);
	res = mSplitter->ClientGetById(ids[1], nullptr, nullptr);
	ASSERT_FALSE(res);
	res = mSplitter->ClientGetById(ids[1], &latency, nullptr);
	ASSERT_FALSE(res);
	res = mSplitter->ClientGetById(invalidClientId(), &latency, &dropped);
	ASSERT_FALSE(res);

	res = mSplitter->ClientGetById(ids[1], &latency, &dropped);
	ASSERT_TRUE(res);
	ASSERT_EQ(latency, 0);
	ASSERT_EQ(dropped, 0);
}


//=========================================  TestISplitterMain ===================================



class TestISplitterMain : public ::testing::Test
{
protected:
	void SetUp()
	{
		mSplitter = ISplitter::Create(mMaxBuffers, mMaxClients);			
	}

	void TearDown()
	{
		stop();

		if (mDataStreamer.joinable())
			mDataStreamer.join();

		for (size_t i = 0; i < mClients.size(); i++) {
			if (mClients[i].joinable())
				mClients[i].join();
		}

		mSplitter.reset();
	}
	
	uint32_t invalidClientId() const {
		return std::numeric_limits<uint32_t>::max();
	}
	
	DataPtr makeData(int i) {
		DataArray data = { (uint8_t)i };
		return std::make_shared<DataArray>(data);
	}	

	int getDataAsInt(const DataPtr& data) {
		return data && data->size() ? (int)data->at(0) : 0;
	}

	void streamData(size_t count, int32_t waitMsec, const milliseconds& sleepTime) {
		size_t i = 1;
		while (!mStop && i <= count) {			
			mSplitter->Put(std::move(makeData(i)), waitMsec);
			i++;
			this_thread::sleep_for(sleepTime);
		}
	}

	void startStreamer(size_t count, int32_t waitPutDataMsec = 50, const milliseconds& sleepTime = 100ms) {
		mDataStreamer = std::thread(&TestISplitterMain::streamData, this, count, waitPutDataMsec, sleepTime);
	}	

	std::future<int> startStreamerAsync(size_t count, int32_t waitPutDataMsec = 50, const milliseconds& sleepTime = 100ms) {

		auto result = std::async(std::launch::async, [this](size_t count, int32_t waitPutDataMsec, const milliseconds& sleepTime) {
			
			Timer tm;

			int maxDelayMsec = 0;

			size_t i = 1;
			while (!mStop && i <= count) {
				
				tm.start();
				mSplitter->Put(std::move(makeData(i)), waitPutDataMsec);
				auto delayMsec = tm.elapsed();
				if (delayMsec > maxDelayMsec)
					maxDelayMsec = (int)delayMsec;
				
				this_thread::sleep_for(sleepTime);
				i++;
			}			

			return maxDelayMsec;		

		}, count, waitPutDataMsec, sleepTime);

		return result;		
	}


	std::future<int> startStreamerFromDataSet(const DataSet& dataSet, int32_t waitPutDataMsec = 50, const milliseconds& sleepTime = 100ms) {		

		auto result = std::async(std::launch::async, [this](const DataSet& dataSet, int32_t waitPutDataMsec, const milliseconds& sleepTime) {
			
			Timer tm;

			int maxDelayMsec = 0;

			for (const auto & data : dataSet) {

				if (mStop) break;

				tm.start();
				mSplitter->Put(std::move(makeData(data)), waitPutDataMsec);
				auto delayMsec = tm.elapsed();
				if (delayMsec > maxDelayMsec)
					maxDelayMsec = (int)delayMsec;

				this_thread::sleep_for(sleepTime);
			}

			return maxDelayMsec;

		}, dataSet, waitPutDataMsec, sleepTime );

		return result;
	}

	std::future<std::pair<DataPtrList, int>> startClientForDataResult(uint32_t clientId, int32_t waitGetDataMsec, const milliseconds& sleepTime) {

		auto result = std::async(std::launch::async, [this](uint32_t clientId,  int32_t waitGetDataMsec, const milliseconds& sleepTime) {
			DataPtrList dataList;

			Timer tm;
			int maxDelayMsec = 0;

			while (!mStop) {
				DataPtr data;
				tm.start();
				auto res = mSplitter->Get(clientId, data, waitGetDataMsec);
				tm.elapsed();
				auto delayMsec = tm.elapsed();
				if (delayMsec > maxDelayMsec)
					maxDelayMsec = (int)delayMsec;		

				if (data && res != (int)ISplitter::Error::NoNewData) {
                      dataList.push_back(data);
				}
				
				this_thread::sleep_for(sleepTime);
			}
			
			return std::pair<DataPtrList, int>(dataList, maxDelayMsec);			

		}, clientId, waitGetDataMsec, sleepTime);

		return result;
	}

	void stop() {	
		mSplitter->Flush();
		mStop = true;		
	}

	void start() {
		mStop = false;
	}

public:
	ISplitterPtr mSplitter;
	size_t mMaxClients = 3;
	size_t mMaxBuffers = 3;

	vector<thread> mClients;
	thread mDataStreamer;

	atomic_bool mStop = false;
};

TEST_F(TestISplitterMain, test_PutData)
{	
	auto error = mSplitter->Put(std::move(makeData(144)), 50);
	ASSERT_EQ(error, (int32_t)ISplitter::Error::NoClients);	

	ClientIds ids;
	uint32_t id;
	bool res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);
	res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);

	startStreamer(4);
	
	this_thread::sleep_for(1s);

	size_t latency;
	size_t dropped;
	res = mSplitter->ClientGetById(ids[1], &latency, &dropped);
	ASSERT_TRUE(res);
	ASSERT_EQ(latency, mMaxBuffers);
	ASSERT_EQ(dropped, 1);

	res = mSplitter->ClientGetById(ids[0], &latency, &dropped);
	ASSERT_TRUE(res);
	ASSERT_EQ(latency, mMaxBuffers);
	ASSERT_EQ(dropped, 1);

	mSplitter->Put(std::move(makeData(7)), 50);
	res = mSplitter->ClientGetById(ids[0], &latency, &dropped);
	ASSERT_TRUE(res);
	ASSERT_EQ(latency, mMaxBuffers);
	ASSERT_EQ(dropped, 2);	
}

TEST_F(TestISplitterMain, test_PutGet)
{
	ClientIds ids;
	uint32_t id;
	bool res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);
	res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);

	std::vector<InputData> inputDataSets = {
		{{1,2,3,4}, 50, 50, 0ms, 100ms, 80ms, 50ms}
		, {{1,2,3,4,5}, 50, 50, 0ms, 50ms, 200ms, 100ms}
		, {{1,2,3,4,5,6,7,8}, 50, 50, 50ms, 100ms, 50ms, 150ms}
	};
	std::vector<std::pair<DataSet, DataSet>> checkDataSets = {
		{{1,2,3,4}, {1,2,3,4}}
		, {{1,2,3,4,5}, {1,2,3,4,5}}
		, { {1,2,3,4,5,6,7,8}, {1,2,3,4,5,6,7,8}}
	};

	for (size_t i = 0; i < inputDataSets.size(); i++) {

		start();

		const auto &[data, putDelayMsec, getDelayMsec, sleepStart, sleepPut, sleepGet1, sleepGet2] = inputDataSets[i];

		auto streamerResult = startStreamerFromDataSet(data, putDelayMsec, sleepPut);

		this_thread::sleep_for(sleepStart);

		auto client1Result = startClientForDataResult(ids[0], getDelayMsec, sleepGet1);

		auto client2Result = startClientForDataResult(ids[1], getDelayMsec, sleepGet2);

		this_thread::sleep_for(2s);

		stop();

		auto maxPutDelay = streamerResult.get();
		cout << "MaxPutDelay = " << maxPutDelay << endl;
		ASSERT_LE(maxPutDelay, putDelayMsec + putDelayMsec * 0.1);


		auto[dataList1, maxGetDelay1] = client1Result.get();
		auto[dataList2, maxGetDelay2] = client2Result.get();

		cout << "Client " << ids[0] << ": ";
		for (const auto & data : dataList1) {
			cout << getDataAsInt(data) << " ";
		}
		cout << endl;

		cout << "Client " << ids[1] << ": ";
		for (const auto & data : dataList2) {
			cout << getDataAsInt(data) << " ";
		}
		cout << endl;

		/*cout << "MaxGetDelay1 = " << maxGetDelay1 << endl;
		ASSERT_LE(maxGetDelay1, getDelayMsec + getDelayMsec * 0.1);
		cout << "MaxGetDelay2 = " << maxGetDelay2 << endl;
		ASSERT_LE(maxGetDelay2, getDelayMsec + getDelayMsec * 0.1);*/

		ASSERT_EQ(dataList1.size(), checkDataSets[i].first.size());
		for (size_t j = 0; j < dataList1.size(); j++) {
			ASSERT_EQ(getDataAsInt(dataList1[j]), checkDataSets[i].first[j]);
		}
		ASSERT_EQ(dataList2.size(), checkDataSets[i].second.size());
		for (size_t j = 0; j < dataList2.size(); j++) {
			ASSERT_EQ(getDataAsInt(dataList2[j]), checkDataSets[i].second[j]);
		}

		size_t latency;
		size_t dropped;
		res = mSplitter->ClientGetById(ids[0], &latency, &dropped);
		ASSERT_TRUE(res);
		ASSERT_EQ(latency, 0);

		res = mSplitter->ClientGetById(ids[1], &latency, &dropped);
		ASSERT_TRUE(res);
		ASSERT_EQ(latency, 0);
	}
}

TEST_F(TestISplitterMain, test_PutGetWithDrop)
{
	mSplitter = ISplitter::Create(2, 2);
	size_t maxBuffers;
	size_t maxClients;
	mSplitter->InfoGet(&maxBuffers, &maxClients);

	ASSERT_EQ(maxBuffers, 2);
	ASSERT_EQ(maxClients, 2);

	ClientIds ids;
	uint32_t id;
	bool res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);
	res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);

	std::vector<InputData> inputDataSets = {		
		{{1,2,3,4,5,6,7,8,9}, 50, 50, 2500ms, 100ms, 550ms, 50ms}
		, {{1,2,3,4,5,6,7,8,9}, -1, 50, 3000ms, 100ms, 200ms, 50ms}
	};
	std::vector<std::pair<DataSet, DataSet>> checkDataSets = {		
		 { {1,4,8,9}, {1,2,3,4,5,6,7,8,9}}
		 , { {1,2,3,4,5,6,7,8,9}, {1,2,3,4,5,6,7,8,9}}
	};

	for (size_t i = 0; i < inputDataSets.size(); i++) {

		cout << "********* test_PutGetWithDrop: I = " << i << endl;

		start();

		const auto &[data, putDelayMsec, getDelayMsec, testTime, sleepPut, sleepGet1, sleepGet2] = inputDataSets[i];

		auto streamerResult = startStreamerFromDataSet(data, putDelayMsec, sleepPut);		

		auto client1Result = startClientForDataResult(ids[0], getDelayMsec, sleepGet1);

		auto client2Result = startClientForDataResult(ids[1], getDelayMsec, sleepGet2);

		this_thread::sleep_for(testTime);

		stop();

		auto maxPutDelay = streamerResult.get();

		if (putDelayMsec != -1) {
			cout << "MaxPutDelay = " << maxPutDelay << endl;
			ASSERT_LE(maxPutDelay, putDelayMsec + putDelayMsec * 0.1);
		}

		auto[dataList1, maxGetDelay1] = client1Result.get();
		auto[dataList2, maxGetDelay2] = client2Result.get();

		cout << "Client " << ids[0] << ": ";
		for (const auto & data : dataList1) {
			cout << getDataAsInt(data) << " ";
		}
		cout << endl;

		cout << "Client " << ids[1] << ": ";
		for (const auto & data : dataList2) {
			cout << getDataAsInt(data) << " ";
		}
		cout << endl;

		if (getDelayMsec != -1) {
			cout << "MaxGetDelay1 = " << maxGetDelay1 << endl;
			ASSERT_LE(maxGetDelay1, getDelayMsec + getDelayMsec * 0.1);
			cout << "MaxGetDelay2 = " << maxGetDelay2 << endl;
			ASSERT_LE(maxGetDelay2, getDelayMsec + getDelayMsec * 0.1);
		}		

		ASSERT_EQ(dataList1.size(), checkDataSets[i].first.size());
		for (size_t j = 0; j < dataList1.size(); j++) {
			ASSERT_EQ(getDataAsInt(dataList1[j]), checkDataSets[i].first[j]);
		}
		ASSERT_EQ(dataList2.size(), checkDataSets[i].second.size());
		for (size_t j = 0; j < dataList2.size(); j++) {
			ASSERT_EQ(getDataAsInt(dataList2[j]), checkDataSets[i].second[j]);
		}

		size_t latency;
		size_t dropped;
		res = mSplitter->ClientGetById(ids[0], &latency, &dropped);
		ASSERT_TRUE(res);
		ASSERT_EQ(latency, 0);

		res = mSplitter->ClientGetById(ids[1], &latency, &dropped);
		ASSERT_TRUE(res);
		ASSERT_EQ(latency, 0);
	}
}


TEST_F(TestISplitterMain, test_PutGetWithDrop_fromTask)
{
	mSplitter = ISplitter::Create(2, 3);
	size_t maxBuffers;
	size_t maxClients;
	mSplitter->InfoGet(&maxBuffers, &maxClients);

	ASSERT_EQ(maxBuffers, 2);
	ASSERT_EQ(maxClients, 3);

	ClientIds ids;
	uint32_t id;
	bool res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);
	res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);

	res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);

	std::vector<InputData> inputDataSets = {
		{ {1,2,3,4,5,6,7,8,9,10}, 50, 50, 100ms, 50ms, 0ms, 0ms}
	};
	std::vector<std::tuple<DataSet, DataSet, DataSet>> checkDataSets = {
		 { {1,2,3,7,8,9,10}, {1,2,3,4,5,6,7,8,9,10}, {1,2,3,4,5,6,7,8,9,10}}
	};

	for (size_t i = 0; i < inputDataSets.size(); i++) {

		start();

		const auto & [data, putDelayMsec, getDelayMsec, sleepPut, sleepGet1, sleepGet2, notUsed] = inputDataSets[i];

		auto streamerResult = startStreamerFromDataSet(data, putDelayMsec, sleepPut);	

		auto client1Result = std::async(std::launch::async, [this](uint32_t clientId, int32_t waitGetDataMsec) {
			DataPtrList dataList;

			Timer tm, tm2;
			int maxDelayMsec = 0;
			
			DataPtr data;

			int time = 0;
			
			while (!mStop && time < 300) {				

				tm2.start();

				tm.start();
				auto res = mSplitter->Get(clientId, data, waitGetDataMsec);
				tm.elapsed();
				auto delayMsec = tm.elapsed();
				if (delayMsec > maxDelayMsec)
					maxDelayMsec = (int)delayMsec;				

				if (res != (int)ISplitter::Error::NoNewData) {
					dataList.push_back(data);
				}

				this_thread::sleep_for(100ms);

				time+= (int)tm2.elapsed();    
				
			}	
			
			if (mStop)
				return std::pair<DataPtrList, int>(dataList, maxDelayMsec);

			this_thread::sleep_for(600ms);

			while (!mStop) {

				tm.start();
				auto res = mSplitter->Get(clientId, data, waitGetDataMsec);
				tm.elapsed();
				auto delayMsec = tm.elapsed();
				if (delayMsec > maxDelayMsec)
					maxDelayMsec = (int)delayMsec;

				if (res != (int)ISplitter::Error::NoNewData) {
					dataList.push_back(data);
				}
			}

			return std::pair<DataPtrList, int>(dataList, maxDelayMsec);

		}, ids[0], getDelayMsec);


		auto client2Result = startClientForDataResult(ids[1], getDelayMsec, sleepGet1);
		auto client3Result = startClientForDataResult(ids[2], getDelayMsec, sleepGet2);

		this_thread::sleep_for(2000ms);

		stop();

		auto maxPutDelay = streamerResult.get();
		cout << "MaxPutDelay = " << maxPutDelay << endl;
		ASSERT_LE(maxPutDelay, putDelayMsec + putDelayMsec * 0.1);

		auto[dataList1, maxGetDelay1] = client1Result.get();
		auto[dataList2, maxGetDelay2] = client2Result.get();
		auto[dataList3, maxGetDelay3] = client3Result.get();

		cout << "Client " << ids[0] << ": ";
		for (const auto & data : dataList1) {
			cout << getDataAsInt(data) << " ";
		}
		cout << endl;

		cout << "Client " << ids[1] << ": ";
		for (const auto & data : dataList2) {
			cout << getDataAsInt(data) << " ";
		}
		cout << endl;

		cout << "Client " << ids[2] << ": ";
		for (const auto & data : dataList3) {
			cout << getDataAsInt(data) << " ";
		}
		cout << endl;

		/*cout << "MaxGetDelay1 = " << maxGetDelay1 << endl;
		ASSERT_LE(maxGetDelay1, getDelayMsec + getDelayMsec * 0.1);
		cout << "MaxGetDelay2 = " << maxGetDelay2 << endl;
		ASSERT_LE(maxGetDelay2, getDelayMsec + getDelayMsec * 0.1);
		cout << "MaxGetDelay3 = " << maxGetDelay2 << endl;
		ASSERT_LE(maxGetDelay3, getDelayMsec + getDelayMsec * 0.1);*/

		const auto &[checkSet1, checkSet2, checkSet3] = checkDataSets[i];

		ASSERT_EQ(dataList1.size(), checkSet1.size());
		for (size_t j = 0; j < dataList1.size(); j++) {
			ASSERT_EQ(getDataAsInt(dataList1[j]), checkSet1[j]);
		}
		ASSERT_EQ(dataList2.size(), checkSet2.size());
		for (size_t j = 0; j < dataList2.size(); j++) {
			ASSERT_EQ(getDataAsInt(dataList2[j]), checkSet2[j]);
		}
		ASSERT_EQ(dataList3.size(), checkSet3.size());
		for (size_t j = 0; j < dataList3.size(); j++) {
			ASSERT_EQ(getDataAsInt(dataList3[j]), checkSet3[j]);
		}

		size_t latency;
		size_t dropped;
		res = mSplitter->ClientGetById(ids[0], &latency, &dropped);
		ASSERT_TRUE(res);
		ASSERT_EQ(latency, 0);

		res = mSplitter->ClientGetById(ids[1], &latency, &dropped);
		ASSERT_TRUE(res);
		ASSERT_EQ(latency, 0);

		res = mSplitter->ClientGetById(ids[2], &latency, &dropped);
		ASSERT_TRUE(res);
		ASSERT_EQ(latency, 0);
	}
}


TEST_F(TestISplitterMain, test_Flush)
{
	mSplitter = ISplitter::Create(2, 2);

	ClientIds ids;
	uint32_t id;
	bool res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);
	res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);	

	using InputData2 = std::tuple<int, int, milliseconds, milliseconds, milliseconds>;
	
	std::vector<InputData2> inputDataSets = {
		{ 100, 100, 100ms, 150ms, 100ms}
	  , {200, 200, 50ms, 310ms, 310ms}	
	};
	std::vector<std::pair<DataSet, DataSet>> checkDataSets = {
		{{1,2,3, 5}, {1,2,3,4,5,6}}
	  , { {1,2}, {1,2}}	
	};

	for (size_t i = 0; i < inputDataSets.size(); i++) {

		cout << "----------test_Flush: " << i << endl;

		start();

		const auto &[putDelayMsec, getDelayMsec, sleepPut, sleepGet1, sleepGet2] = inputDataSets[i];

		auto streamerResult = startStreamerAsync(20, putDelayMsec, sleepPut);		

		auto client1Result = startClientForDataResult(ids[0], getDelayMsec, sleepGet1);

		auto client2Result = startClientForDataResult(ids[1], getDelayMsec, sleepGet2);

		this_thread::sleep_for(350ms);

		auto error = mSplitter->Flush();	
		ASSERT_EQ(error, static_cast<int>(ISplitter::Error::DataFlushed));

		size_t latency;
		size_t dropped;
		res = mSplitter->ClientGetById(ids[0], &latency, &dropped);
		ASSERT_TRUE(res);
		ASSERT_EQ(dropped, 0);
		ASSERT_EQ(latency, 0);

		res = mSplitter->ClientGetById(ids[1], &latency, &dropped);
		ASSERT_TRUE(res);
		ASSERT_EQ(dropped, 0);
		ASSERT_EQ(latency, 0);		

		this_thread::sleep_for(200ms);

		stop();		

		auto[dataList1, maxGetDelay1] = client1Result.get();
		auto[dataList2, maxGetDelay2] = client2Result.get();

		streamerResult.get();

		cout << "Client " << ids[0] << ": ";
		for (const auto & data : dataList1) {
			cout << getDataAsInt(data) << " ";
		}
		cout << endl;

		cout << "Client " << ids[1] << ": ";
		for (const auto & data : dataList2) {
			cout << getDataAsInt(data) << " ";
		}
		cout << endl;		

		ASSERT_EQ(dataList1.size(), checkDataSets[i].first.size());
		for (size_t j = 0; j < dataList1.size(); j++) {
			ASSERT_EQ(getDataAsInt(dataList1[j]), checkDataSets[i].first[j]);
		}
		ASSERT_EQ(dataList2.size(), checkDataSets[i].second.size());
		for (size_t j = 0; j < dataList2.size(); j++) {
			ASSERT_EQ(getDataAsInt(dataList2[j]), checkDataSets[i].second[j]);
		}

		mSplitter->Flush();
		
		res = mSplitter->ClientGetById(ids[0], &latency, &dropped);
		ASSERT_TRUE(res);
		ASSERT_EQ(latency, 0);

		res = mSplitter->ClientGetById(ids[1], &latency, &dropped);
		ASSERT_TRUE(res);
		ASSERT_EQ(latency, 0);		
	}//for
}

TEST_F(TestISplitterMain, test_Close)
{
	mSplitter = ISplitter::Create(2, 2);

	ClientIds ids;
	uint32_t id;
	bool res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);
	res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);

	using InputData2 = std::tuple<int, int, milliseconds, milliseconds, milliseconds>;

	std::vector<InputData2> inputDataSets = {
		{ 100, 100, 100ms, 150ms, 100ms}	 
	};
	std::vector<std::pair<DataSet, DataSet>> checkDataSets = {
		{{1,2,3}, {1,2,3,4}}	  
	};

	for (size_t i = 0; i < inputDataSets.size(); i++) {		

		start();

		const auto &[putDelayMsec, getDelayMsec, sleepPut, sleepGet1, sleepGet2] = inputDataSets[i];

		auto streamerResult = startStreamerAsync(20, putDelayMsec, sleepPut);

		auto client1Result = startClientForDataResult(ids[0], getDelayMsec, sleepGet1);

		auto client2Result = startClientForDataResult(ids[1], getDelayMsec, sleepGet2);

		this_thread::sleep_for(350ms);

		mSplitter->Close();

		size_t latency;
		size_t dropped;
		res = mSplitter->ClientGetById(ids[0], &latency, &dropped);
		ASSERT_FALSE(res);
		ASSERT_EQ(dropped, 0);
		ASSERT_EQ(latency, 0);

		res = mSplitter->ClientGetById(ids[1], &latency, &dropped);
		ASSERT_FALSE(res);
		ASSERT_EQ(dropped, 0);
		ASSERT_EQ(latency, 0);

		size_t clientCount;
		res = mSplitter->ClientGetCount(&clientCount);
		ASSERT_TRUE(res);
		ASSERT_EQ(clientCount, 0);

		this_thread::sleep_for(200ms);

		stop();

		auto[dataList1, maxGetDelay1] = client1Result.get();
		auto[dataList2, maxGetDelay2] = client2Result.get();

		streamerResult.get();

		cout << "Client " << ids[0] << ": ";
		for (const auto & data : dataList1) {
			cout << getDataAsInt(data) << " ";
		}
		cout << endl;

		cout << "Client " << ids[1] << ": ";
		for (const auto & data : dataList2) {
			cout << getDataAsInt(data) << " ";
		}
		cout << endl;

		ASSERT_EQ(dataList1.size(), checkDataSets[i].first.size());
		for (size_t j = 0; j < dataList1.size(); j++) {
			ASSERT_EQ(getDataAsInt(dataList1[j]), checkDataSets[i].first[j]);
		}
		ASSERT_EQ(dataList2.size(), checkDataSets[i].second.size());
		for (size_t j = 0; j < dataList2.size(); j++) {
			ASSERT_EQ(getDataAsInt(dataList2[j]), checkDataSets[i].second[j]);
		}		
	}//for
}


TEST_F(TestISplitterMain, test_RemoveAddClients)
{
	mSplitter = ISplitter::Create(5, 2);
	size_t maxBuffers;
	size_t maxClients;
	mSplitter->InfoGet(&maxBuffers, &maxClients);

	ASSERT_EQ(maxBuffers, 5);
	ASSERT_EQ(maxClients, 2);

	ClientIds ids;
	uint32_t id;
	bool res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);
	res = mSplitter->ClientAdd(&id);
	ASSERT_TRUE(res);
	ids.push_back(id);
	res = mSplitter->ClientAdd(&id);
	ASSERT_FALSE(res);	

	using InputData2 = std::tuple<int, int, milliseconds, milliseconds, milliseconds, milliseconds, milliseconds>;

	std::vector<InputData2> inputDataSets = {
		{ 100, 100, 2500ms, 350ms, 100ms, 100ms, 150ms}
	};
	std::vector<std::tuple<DataSet, DataSet, DataSet>> checkDataSets = {
		{{1,2,3,4,5,6,7,8,9,10}, {1,2,3}, {5,6,7,8,9,10}}
	};

	for (size_t i = 0; i < inputDataSets.size(); i++) {

		start();

		const auto &[putDelayMsec, getDelayMsec, testTime, sleepStart, sleepPut, sleepGet1, sleepGet2] = inputDataSets[i];

		auto streamerResult = startStreamerAsync(10, putDelayMsec, sleepPut);

		auto client1Result = startClientForDataResult(ids[0], getDelayMsec, sleepGet1);

		auto client2Result = startClientForDataResult(ids[1], getDelayMsec, sleepGet2);

		this_thread::sleep_for(sleepStart);

		res = mSplitter->ClientRemove(ids[1]);
		ASSERT_TRUE(res);

		size_t latency;
		size_t dropped;
		res = mSplitter->ClientGetById(ids[0], &latency, &dropped);
		ASSERT_TRUE(res);		

		res = mSplitter->ClientGetById(ids[1], &latency, &dropped);
		ASSERT_FALSE(res);	

		size_t clientCount;
		res = mSplitter->ClientGetCount(&clientCount);
		ASSERT_TRUE(res);
		ASSERT_EQ(clientCount, 1);

		res = mSplitter->ClientAdd(&id);
		ASSERT_TRUE(res);		
		ids.push_back(id);

		this_thread::sleep_for(50ms);

		auto client3Result = startClientForDataResult(ids[2], getDelayMsec, sleepGet2);

		this_thread::sleep_for(testTime);

		stop();

		auto[dataList1, maxGetDelay1] = client1Result.get();
		auto[dataList2, maxGetDelay2] = client2Result.get();
		auto[dataList3, maxGetDelay3] = client3Result.get();

		streamerResult.get();

		cout << "Client " << ids[0] << ": ";
		for (const auto & data : dataList1) {
			cout << getDataAsInt(data) << " ";
		}
		cout << endl;

		cout << "Client " << ids[1] << ": ";
		for (const auto & data : dataList2) {
			cout << getDataAsInt(data) << " ";
		}
		cout << endl;

		cout << "Client " << ids[2] << ": ";
		for (const auto & data : dataList3) {
			cout << getDataAsInt(data) << " ";
		}
		cout << endl;

		const auto &[checkSet1, checkSet2, checkSet3] = checkDataSets[i];

		ASSERT_EQ(dataList1.size(), checkSet1.size());
		for (size_t j = 0; j < dataList1.size(); j++) {
			ASSERT_EQ(getDataAsInt(dataList1[j]), checkSet1[j]);
		}
		ASSERT_EQ(dataList2.size(), checkSet2.size());
		for (size_t j = 0; j < dataList2.size(); j++) {
			ASSERT_EQ(getDataAsInt(dataList2[j]), checkSet2[j]);
		}
		ASSERT_EQ(dataList3.size(), checkSet3.size());
		for (size_t j = 0; j < dataList3.size(); j++) {
			ASSERT_EQ(getDataAsInt(dataList3[j]), checkSet3[j]);
		}
	}//for
}