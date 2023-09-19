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
	void closeConnection() override;
private:
	void WaitResponseFromServer();
	void trySendData(const char* pData, const int length);
private:
	char tempReceiveBuffer[BUFFER_SIZE];
	DWORD dataInTempBuffer;
	WINHTTP_WEB_SOCKET_BUFFER_TYPE tempBufferType;
	bool SendResponseStatus;
	bool ReceiveResponseStatus;
	HANDLE serverSendResponse;
	HANDLE tempBufferEmpty;
	HINTERNET SessionHandle;
	HINTERNET ConnectionHandle;
	HINTERNET RequestHandle;
	HINTERNET WebSocketHandle;
	LPCWSTR serverIP;
	INTERNET_PORT serverPort;
	int errState;
	int errorCode;
	int send_data;
};
#endif