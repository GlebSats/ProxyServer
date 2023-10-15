#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include "ProxyConnection.h"
#include <winhttp.h>
#include <cstring>
#include <thread>

class WebSocketClient : public ProxyConnection {
public:
	WebSocketClient(LPCWSTR serverIP, INTERNET_PORT serverPort);
	~WebSocketClient();
	void Initialization() override;
	void Connection() override;
	void Handler() override;
	int sendData(const char* pData, const int length) override;
	void receiveData() override;
	void subtractData(const int send_data) override;
	void closeConnection() override;
private:
	void WaitResponseFromServer();
	void trySendData();
	void preparation();
private:
	const char* pDataToSend;
	int sizeDataToSend;
	HANDLE trySend;
	DWORD receivedData;
	WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType;
	bool SendResponseStatus;
	bool ReceiveResponseStatus;
	HANDLE serverSendResponse;
	HANDLE bufferEmpty;
	HINTERNET SessionHandle;
	HINTERNET ConnectionHandle;
	HINTERNET RequestHandle;
	HINTERNET WebSocketHandle;
	std::wstring serverIP;
	INTERNET_PORT serverPort;
	int errState;
	int errorCode;
	int sentBytes;
	std::thread TS;
};
#endif
