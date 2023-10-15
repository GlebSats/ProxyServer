#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef PROXYCONNECTION_H
#define PROXYCONNECTION_H

#include <iostream>
#include "writeLog.h"
#include "ServException.h"
#include <Windows.h>

#include "writeLogHex.h"

#define BUFFER_SIZE 1024

class ProxyConnection {
	friend void proxyServer(std::shared_ptr<ProxyConnection> server, std::shared_ptr<ProxyConnection> client, HANDLE* stopEvent);

public:
	ProxyConnection();
	virtual ~ProxyConnection() = 0;
	virtual void Initialization() = 0;
	virtual void Connection() = 0;
	virtual void Handler() = 0;
	virtual int sendData(const char* pData, const int length) = 0;
	virtual void receiveData() = 0;
	virtual void closeConnection() = 0;
	virtual void subtractData(const int send_data);
	void WaitingToSend();
protected:
	void eventsCreation();
	void eventsDeleting();
public:
	HANDLE* stopEvent;
protected:
	char receiveBuffer[BUFFER_SIZE];
	int dataInReceiveBuffer;
	int indexForRecData;
	bool waitingResult;
	HANDLE disconnect;
	HANDLE readySend;
	HANDLE readyReceive;
	HANDLE dataToSend;
	HANDLE endOfWaiting;
};

#endif
