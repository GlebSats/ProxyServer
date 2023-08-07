#pragma once
#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <winhttp.h>

class ProxyServer {
public:
	ProxyServer();
	virtual ~ProxyServer() = 0;
	virtual void proxyServerInit() = 0;
	virtual void WaitingForClients() = 0;
	virtual void connectToTargetServer() = 0;
	virtual void sendData() = 0;
	virtual void receiveData() = 0;
	virtual void closeConnection() = 0;
public:
	HANDLE* stopEvent;
	HANDLE disconnect;
	HANDLE readySend;
	HANDLE dataToSend;
};

class TCPClient: public ProxyServer
{
public:
	TCPClient(const char* listeningPort); // Constructor
	~TCPClient();
	void proxyServerInit() override;
	void WaitingForClients() override;
	void sendData() override;
	void receiveData() override;
	void closeConnection() override;
private:
	//Prohibited methods
	void connectToTargetServer() override {}
	//
	void initSockets(); // Function initiates use of the Winsock DLL
	void bindSocket(); // Function associates a local address with a socket
	void listenState(); // Set socket to listen state
	void stopServer(); // Freeing memory
	void createSockInfo(const char* ip, const char* port, addrinfo** sockInfo); // Create addrinfo and translate host name to address
	void createNewSocket(SOCKET& new_socket, addrinfo* sockInfo); // Create socket with addrinfo parameters
	void acceptConnection();
	TCPClient(const TCPClient&) = delete; // Copy not allowed
	void operator=(const TCPClient&) = delete; // Assignment not allowed
private:
	const char* listeningPort;
	addrinfo* lisSockInfo;
	SOCKET lis_socket;
	WSAEVENT clientConnectionRequest;
	int errState;
	WSADATA wsData;
	SOCKET client_socket;
};

class TCPTargetServer : public ProxyServer {
public:
	TCPTargetServer(const char* serverIP, const char* serverPort);
	~TCPTargetServer();
	void connectToTargetServer() override;
	//void serverHandler() override;
	void sendData() override;
	void receiveData() override;
	void closeConnection() override;
private:
	//Prohibited methods
	virtual void proxyServerInit() override {}
	virtual void WaitingForClients() override {}
	//
	void createSockInfo(const char* ip, const char* port, addrinfo** sockInfo); // Create addrinfo and translate host name to address
	void createNewSocket(SOCKET& new_socket, addrinfo* sockInfo); // Create socket with addrinfo parameters
	//void sockCommunication();
	//void createSocketEvents();
	//void closeConnection();
	TCPTargetServer(const TCPTargetServer&) = delete; // Copy not allowed
	void operator=(const TCPTargetServer&) = delete; // Assignment not allowed
private:
	const char* serverIP;
	const char* serverPort;
	addrinfo* serverSockInfo;
	SOCKET server_socket;
	WSAEVENT bufToClientHasData;
	WSAEVENT bufToServHasData;
	WSAEVENT clientReadySend;
	WSAEVENT serverReadySend;
	int errState;
};

class WebSocketConnection : public StartServer {
public:
	WebSocketConnection(const char* listeningPort, LPCWSTR serverIP, INTERNET_PORT serverPort);
	~WebSocketConnection();
	void serverInitialization() override;
	void serverHandler() override;
private:
	void WaitResponseFromServer();
	void connectToServer();
	void sockCommunication();
	void createSocketEvents();
	void stopServer();
private:
	bool SendResponseStatus;
	bool ResponseStatus;
	HANDLE serverSendResponse;
	HINTERNET SessionHandle;
	HINTERNET ConnectionHandle;
	HINTERNET RequestHandle;
	HINTERNET WebSocketHandle;
	LPCWSTR serverIP;
	INTERNET_PORT serverPort;
};

void proxyConnection(ProxyServer& client, ProxyServer& targetServer, HANDLE* stopEvent);
