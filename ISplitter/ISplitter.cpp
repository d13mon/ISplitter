#include "ISplitter.h"
#include "threadsafe_queue.h"

#include <cassert>
#include <algorithm>
#include <iomanip>

using namespace std;

const std::string ISplitter::TAG = "ISplitter: ";

ISplitter::ISplitter(size_t maxBuffers, size_t maxClients)
	: mMaxBuffers(maxBuffers)
	, mMaxClients(maxClients)
{

}

ISplitter::~ISplitter()
{
	Close();
}

std::string ISplitter::GetErrorText(int32_t errorId)
{
	if (errorId < 0 || errorId >= (int)Error::Count)
		return "Error not found";

	Error error = static_cast<Error>(errorId);

	switch (error) {
	case Error::NoError: return "No error";
	case Error::MaxClientsReached: return "Max clients number has reached.";
	case Error::DataFlushed: return "All data buffers were flushed.";
	case Error::DataDropped: return "Some data was dropped.";
	case Error::NoNewData: return "No new data received.";
	case Error::NoClientFound: return "The client with this ID not found .";
	case Error::NoClients: return "Clients list empty.";
		
	default:
		assert(0);
	}

	return std::string();
}

std::shared_ptr<ISplitter> ISplitter::Create(size_t maxBuffers, size_t maxClients)
{
	return std::make_shared<ISplitter>(maxBuffers, maxClients);
}

bool ISplitter::InfoGet(size_t* pMaxBuffers, size_t* pMaxClients) const
{
	if (!pMaxBuffers || !pMaxClients)
		return false;

	*pMaxBuffers = mMaxBuffers;
	*pMaxClients = mMaxClients;

	return true;
}

bool ISplitter::ClientAdd(uint32_t* pClientID)
{
	if (!pClientID)
		return false;

	unique_lock lock(mDataClientListMutex);
	if (mDataClientList.size() == mMaxClients)
		return false;

	lock.unlock();
	
	auto client = DataClient::Create(mMaxBuffers);
	*pClientID = client->GetClientId();

	lock.lock();
	mDataClientList.push_back(std::move(client));

	return true;
}

bool ISplitter::ClientRemove(uint32_t clientID)
{
	unique_lock lock(mDataClientListMutex);
	for (auto it = begin(mDataClientList); it != end(mDataClientList); ++it) {
		if ( (*it)->GetClientId() == clientID) {
			(*it)->FlushData();
			mDataClientList.erase(it);
			return true;
		}
	}	

	return false;
}

bool ISplitter::ClientGetCount(size_t* pCount) const
{
	if (!pCount)
		return false;

	*pCount = GetClientCountImpl();

	return true;
}

size_t ISplitter::GetClientCountImpl() const
{
	shared_lock lock(mDataClientListMutex);
	return  mDataClientList.size();
}

bool ISplitter::ClientGetByIndex(size_t index, uint32_t* pClientID, size_t* pLatency, size_t* pDropped) const
{
	if (!pLatency || !pDropped || !pClientID || index >= GetClientCountImpl())
		return false;		

	shared_lock lock(mDataClientListMutex);
	const auto & client = mDataClientList[index];

	*pClientID = client->GetClientId();
	*pLatency = client->GetLatencyCount();
	*pDropped = client->GetDroppedCount();

	return true;
}

bool ISplitter::ClientGetById(uint32_t clientID, size_t* pLatency, size_t* pDropped) const
{
	if (!pLatency || !pDropped)
		return false;	

	*pLatency = 0;
	*pDropped = 0;

	shared_lock lock(mDataClientListMutex);
	for (auto it = begin(mDataClientList); it != end(mDataClientList); ++it) {
		if ((*it)->GetClientId() == clientID) {
			
			*pLatency = (*it)->GetLatencyCount();
			*pDropped = (*it)->GetDroppedCount();

			return true;
		}
	}

	return false;
}

int32_t ISplitter::Put(const DataPtr& data, int32_t nWaitForBuffersFreeTimeOutMsec)
{
	int32_t error = 0;	

	shared_lock lock(mDataClientListMutex);

	if (!mDataClientList.size())
		return static_cast<int32_t>(Error::NoClients);

	for (auto it = begin(mDataClientList); it != end(mDataClientList); ++it) {
		auto err = (*it)->PutData(data, nWaitForBuffersFreeTimeOutMsec);
		if (err) error = err;
	}

	return error;
}

int32_t ISplitter::Get(uint32_t nClientID, DataPtr& data, int32_t nWaitForNewDataTimeOutMsec)
{
	shared_lock lock(mDataClientListMutex);
	for (auto it = begin(mDataClientList); it != end(mDataClientList); ++it) {
		if ((*it)->GetClientId() == nClientID) {			
			int32_t error = (*it)->GetData(data, nWaitForNewDataTimeOutMsec);
			return error;
		}
	}

	return static_cast<int32_t>(Error::NoClientFound);
}

int32_t ISplitter::Flush()
{
#if _DEBUG
	cout << TAG << "FLUSH" << endl;
#endif

	unique_lock lock(mDataClientListMutex);
	for (auto it = begin(mDataClientList); it != end(mDataClientList); ++it) {		
		(*it)->FlushData();			
	}

	return static_cast<int>(Error::DataFlushed);
}

int32_t ISplitter::Close()
{
	auto errorId = Flush();

	unique_lock lock(mDataClientListMutex);
	mDataClientList.clear();

	return errorId;
}


//////////////////////// DATA CLIENT ///////////////////////////////////////////////////////

const std::string ISplitter::DataClient::TAG = "ISplitter::DataClient: ";

ISplitter::DataClient::DataClient(size_t maxBuffers)
	: mClientId(GenerateId())	
{	
	mDataQueue.reset(new Queue(maxBuffers));
}

ISplitter::DataClient::~DataClient()
{

}

ISplitter::DataClientPtr ISplitter::DataClient::Create(size_t maxBuffers)
{
	return std::make_shared<DataClient>(maxBuffers);
}

uint32_t ISplitter::DataClient::GenerateId()
{
	static uint32_t sId = 0;	
	return ++sId;
}

uint32_t ISplitter::DataClient::GetClientId() const
{
	return mClientId;
}

size_t ISplitter::DataClient::GetDroppedCount() const
{
	scoped_lock lock(mClientInfoMutex);
	return mDropped;
}

size_t ISplitter::DataClient::GetLatencyCount() const
{	
	return mDataQueue->size();
}

int32_t ISplitter::DataClient::PutData(const DataPtr& data, int32_t nWaitForBuffersFreeTimeOutMsec)
{
#if _DEBUG
	cout << TAG  << "Client: " << std::setw(2) << mClientId << ": PUT " << (int)data->at(0) << endl;
#endif

	if (!mDataQueue->push(data, nWaitForBuffersFreeTimeOutMsec)) {
		{
			scoped_lock lock(mClientInfoMutex);
			mDropped++;
#if _DEBUG					
		    cout << TAG << "Client: " << std::setw(2) << mClientId << ": TIMEOUT: droppedCount =" << mDropped << endl;
#endif
		}

		return static_cast<int32_t>(Error::DataDropped);
	}

	return 0;
}

int32_t ISplitter::DataClient::GetData(DataPtr& data, int32_t nWaitForNewDataTimeOutMsec)
{
#if _DEBUG
	cout << TAG << "Client: " << std::setw(2) << mClientId << ": GET... " << endl;
#endif

	if (!mDataQueue->try_pop(data)) {
		if (!mDataQueue->wait_and_pop(data, nWaitForNewDataTimeOutMsec)) {
			return static_cast<int32_t>(Error::NoNewData);
		}
	}	

#if _DEBUG
	cout << TAG << "Client: " << std::setw(2) << mClientId << ": GET " << (data && data->size() ? (int)data->at(0) : -1) << endl;
#endif

    return 0;
}

void ISplitter::DataClient::FlushData()
{
#if _DEBUG
	cout << TAG << "Client: " << std::setw(2) << mClientId << ": FLUSH " << endl;
#endif

	auto maxLength = mDataQueue->max_length();
	mDataQueue->flush();
	mDataQueue.reset(new Queue(maxLength));

	scoped_lock lock(mClientInfoMutex);
	mDropped = 0;
}


