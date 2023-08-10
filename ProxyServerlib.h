#pragma once
#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <winhttp.h>

#define BUFFER_SIZE 1024

class ProxyServer {
	friend void proxyConnection(ProxyServer& client, ProxyServer& targetServer, HANDLE* stopEvent);

public:
	ProxyServer();
	virtual ~ProxyServer() = 0;
	virtual void Initialization() = 0;
	virtual void WaitingForClients() = 0;
	virtual void connectToTargetServer() = 0;
	virtual void Handler() = 0;
	virtual int sendData(const char* pData, int length) = 0;
	virtual void receiveData() = 0;
	virtual void closeConnection() = 0;
protected:
	void eventsCreation();
	void eventsDeleting();
public:
	HANDLE* stopEvent;
protected:
	char receiveBuffer[BUFFER_SIZE];
	int dataInReceiveBuffer;
	int indexForRecData;
	HANDLE disconnect;
	HANDLE readySend;
	HANDLE dataToSend;
};

class TCPClient: public ProxyServer
{
public:
	TCPClient(const char* listeningPort); // Constructor
	~TCPClient();
	void Initialization() override;
	void WaitingForClients() override;
	void Handler() override;
	int sendData(const char* pData, int length) override;
	void receiveData() override;
	void closeConnection() override;
private:
	//Prohibited method
	void connectToTargetServer() override {}
	//
	void initSockets(); // Function initiates use of the Winsock DLL
	void bindSocket(); // Function associates a local address with a socket
	void listenState(); // Set socket to listen state
	void createSockInfo(const char* ip, const char* port, addrinfo** sockInfo); // Create addrinfo and translate host name to address
	void createNewSocket(SOCKET& new_socket, addrinfo* sockInfo); // Create socket with addrinfo parameters
	TCPClient(const TCPClient&) = delete; // Copy not allowed
	void operator=(const TCPClient&) = delete; // Assignment not allowed
private:
	WSADATA wsData;
	const char* listeningPort;
	addrinfo* lisSockInfo;
	SOCKET lis_socket;
	SOCKET client_socket;
	WSAEVENT clientConnectionRequest;
	WSAEVENT clientReadySend;
	int errState;
};

class TCPTargetServer : public ProxyServer {
public:
	TCPTargetServer(const char* serverIP, const char* serverPort);
	~TCPTargetServer();
	void Initialization() override {}
	void connectToTargetServer() override;
	void Handler() override;
	int sendData(const char* pData, int length) override;
	void receiveData() override;
	void closeConnection() override;
private:
	//Prohibited methods
	virtual void WaitingForClients() override {}
	//
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

//class WebSocketConnection : public StartServer {
//public:
//	WebSocketConnection(const char* listeningPort, LPCWSTR serverIP, INTERNET_PORT serverPort);
//	~WebSocketConnection();
//	void serverInitialization() override;
//	void serverHandler() override;
//private:
//	void WaitResponseFromServer();
//	void connectToServer();
//	void sockCommunication();
//	void createSocketEvents();
//	void stopServer();
//private:
//	bool SendResponseStatus;
//	bool ResponseStatus;
//	HANDLE serverSendResponse;
//	HINTERNET SessionHandle;
//	HINTERNET ConnectionHandle;
//	HINTERNET RequestHandle;
//	HINTERNET WebSocketHandle;
//	LPCWSTR serverIP;
//	INTERNET_PORT serverPort;
//};

void proxyConnection(ProxyServer& client, ProxyServer& targetServer, HANDLE* stopEvent);

