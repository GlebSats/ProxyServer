#ifndef TCPTARGETSERVER_H
#define TCPTARGETSERVER_H

#include "ProxyConnection.h"
#include <WinSock2.h>
#include <WS2tcpip.h>

class TCPTargetServer : public ProxyConnection {
public:
	TCPTargetServer(const char* serverIP, const char* serverPort);
	~TCPTargetServer();
	void Initialization() override {}
	void Connection() override;
	void Handler() override;
	int sendData(const char* pData, int length) override;
	void receiveData() override;
	void closeConnection() override;
private:
	void createSockInfo(const char* ip, const char* port, addrinfo** sockInfo); // Create addrinfo and translate host name to address
	void createNewSocket(SOCKET& new_socket, addrinfo* sockInfo); // Create socket with addrinfo parameters
	TCPTargetServer(const TCPTargetServer&) = delete; // Copy not allowed
	void operator=(const TCPTargetServer&) = delete; // Assignment not allowed
private:
	const char* serverIP;
	const char* serverPort;
	addrinfo* serverSockInfo;
	SOCKET server_socket;
	WSAEVENT targetServerReadySend;
	int errState;
};

#endif
