#pragma once

#include "threadsafe_queue.h"

#include <memory>
#include <vector>
#include <deque>
#include <shared_mutex>

using ClientIds = std::vector<uint32_t>;
using DataArray = std::vector<uint8_t>;
using DataPtr = std::shared_ptr<DataArray>;
using DataPtrList = std::vector<DataPtr>;
using Queue = threadsafe_queue<DataPtr>;
using QueuePtr = std::unique_ptr<Queue>;

class ISplitter;

using ISplitterPtr = std::shared_ptr<ISplitter>;

class ISplitter 
{
public: 
	enum class Error{ NoError = 0, MaxClientsReached, DataDropped, DataFlushed, NoNewData, NoClientFound, NoClients, Count };

public:
	ISplitter(size_t maxBuffers, size_t maxClients);
	virtual ~ISplitter();

	static std::string GetErrorText(int32_t errorId);

public:	
	static ISplitterPtr Create(size_t maxBuffers, size_t maxClients);

	bool InfoGet(size_t* pMaxBuffers, size_t* pMaxClients) const;

	bool ClientAdd(uint32_t* pClientID);
	bool ClientRemove(uint32_t clientID);

	bool ClientGetCount(size_t* pCount) const;
	bool ClientGetByIndex(size_t index, uint32_t* pClientID, size_t* pLatency, size_t* pDropped) const;
	bool ClientGetById(uint32_t clientID, size_t* pLatency, size_t* pDropped) const;

	int32_t Put(const DataPtr& data, int32_t nWaitForBuffersFreeTimeOutMsec);
	int32_t Get(uint32_t nClientID, DataPtr& data, int32_t nWaitForNewDataTimeOutMsec);

	int32_t Flush();
	int32_t Close();

private:
	ISplitter(const ISplitter& other) = delete;
	ISplitter& operator=(const ISplitter& other) = delete;		

	size_t GetClientCountImpl() const;

	class DataClient final {
	public:
		explicit DataClient(size_t maxBuffers);
		~DataClient();

	    static std::shared_ptr<DataClient> Create(size_t maxBuffers);		

	public:
		uint32_t GetClientId() const;
		size_t GetDroppedCount() const;
		size_t GetLatencyCount() const;

		int32_t PutData(const DataPtr& data, int32_t nWaitForBuffersFreeTimeOutMsec);
		int32_t GetData(DataPtr& data, int32_t nWaitForNewDataTimeOutMsec);

		void FlushData();
	private:
		static uint32_t GenerateId();

		DataClient(const DataClient& other) = delete;
		DataClient& operator=(const DataClient& other) = delete;

	private:
		const uint32_t mClientId = 0;

		mutable std::mutex mClientInfoMutex;
		size_t mDropped = 0;		

		QueuePtr mDataQueue;

		static const std::string TAG;
	};

	using DataClientPtr = std::shared_ptr<DataClient>;

private:
	const size_t mMaxBuffers;
	size_t mMaxClients;
	
	mutable std::shared_mutex mDataClientListMutex;
	std::deque<DataClientPtr> mDataClientList;

	static const std::string TAG;
};



