#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include "ProxyConnection.h"
#include <WinSock2.h>
#include <WS2tcpip.h>

class TCPclient : public ProxyConnection {
public:
	TCPclient(const char* serverIP, const char* serverPort);
	~TCPclient();
	void Initialization() override {}
	void Connection() override;
	void Handler() override;
	int sendData(const char* pData, const int length) override;
	void receiveData() override;
	void subtractData(const int send_data) override;
	void closeConnection() override;
private:
	void createSockInfo(const char* ip, const char* port, addrinfo** sockInfo); // Create addrinfo and translate host name to address
	void createNewSocket(SOCKET& new_socket, addrinfo* sockInfo); // Create socket with addrinfo parameters
	TCPclient(const TCPclient&) = delete; // Copy not allowed
	void operator=(const TCPclient&) = delete; // Assignment not allowed
private:
	HANDLE bufferEmpty;
	const char* serverIP;
	const char* serverPort;
	addrinfo* serverSockInfo;
	SOCKET server_socket;
	WSAEVENT serverEvent;
	int errState;
};

#endif
