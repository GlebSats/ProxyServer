#ifndef TCPSERVER_H
#define TCPSERVER_H
#include "ProxyConnection.h"
#include <WinSock2.h>
#include <WS2tcpip.h>

class TCPserver : public ProxyConnection
{
public:
	TCPserver(const char* listeningPort); // Constructor
	~TCPserver();
	void Initialization() override;
	void Connection() override;
	void Handler() override;
	int sendData(const char* pData, const int length) override;
	void receiveData() override;
	void subtractData(const int send_data) override;
	void closeConnection() override;
private:
	void initSockets(); // Function initiates use of the Winsock DLL
	void bindSocket(); // Function associates a local address with a socket
	void listenState(); // Set socket to listen state
	void createSockInfo(const char* ip, const char* port, addrinfo** sockInfo); // Create addrinfo and translate host name to address
	void createNewSocket(SOCKET& new_socket, addrinfo* sockInfo); // Create socket with addrinfo parameters
	TCPserver(const TCPserver&) = delete; // Copy not allowed
	void operator=(const TCPserver&) = delete; // Assignment not allowed
private:
	HANDLE bufferEmpty;
	WSADATA wsData;
	const char* listeningPort;
	addrinfo* lisSockInfo;
	SOCKET lis_socket;
	SOCKET client_socket;
	WSAEVENT clientConnectionRequest;
	WSAEVENT clientEvent;
	int errState;
};

#endif
