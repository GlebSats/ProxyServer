#ifndef WEBSOCKETSERVER_H
#define WEBSOCKETSERVER_H

#include "ProxyConnection.h"
#include <winhttp.h>

class WebSocketServer : public ProxyConnection {
public:
	WebSocketServer(LPCWSTR serverIP, INTERNET_PORT serverPort);
	~WebSocketServer();
	void Initialization() override;
	void Connection() override;
	void Handler() override;
	int sendData(const char* pData, int length) override;
	void receiveData() override;
	void closeConnection() override;
private:
	void WaitResponseFromServer();
private:
	bool SendResponseStatus;
	bool ReceiveResponseStatus;
	HANDLE serverSendResponse;
	HINTERNET SessionHandle;
	HINTERNET ConnectionHandle;
	HINTERNET RequestHandle;
	HINTERNET WebSocketHandle;
	LPCWSTR serverIP;
	INTERNET_PORT serverPort;
};
#endif

