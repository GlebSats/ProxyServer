#pragma once
#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <winhttp.h>

class ProxyServer
{
public:
	ProxyServer(const char* listeningPort, const char* serverIP, const char* serverPort);
	virtual ~ProxyServer();
	virtual void serverInitialization();
	virtual void serverHandler();

private:
	void initSockets(); // Function initiates use of the Winsock DLL
	void createSockInfo(const char* ip, const char* port, addrinfo** sockInfo); // Create addrinfo and translate host name to address
	void createNewSocket(SOCKET& new_socket, addrinfo* sockInfo); // Create socket with addrinfo parameters
	void bindSocket(); // Function associates a local address with a socket
	void listenState(); // Set socket to listen state
	void connectToServer();
	void sockCommunication();
	void createSocketEvents();
	void closeConnection();
	void stopServer();
	ProxyServer(const ProxyServer&) = delete; // Copy not allowed
	void operator=(const ProxyServer&) = delete; // Assignment not allowed

protected:
	ProxyServer(const char* listeningPort);
	void acceptConnection();
public:
	HANDLE* serviceStopEvent;
private:
	const char* listeningPort;
	const char* serverIP;
	const char* serverPort;
	addrinfo* lisSockInfo;
	SOCKET lis_socket;
	WSAEVENT clientConnectionRequest;
	WSAEVENT serverReadySend;
	addrinfo* servSockInfo;
	SOCKET server_socket;
	int errState;
	WSADATA wsData;
protected:
	SOCKET client_socket;
	WSAEVENT bufToClientHasData;
	WSAEVENT bufToServHasData;
	WSAEVENT clientReadySend;
};

class WebSocketProxyServer : public ProxyServer {
public:
	WebSocketProxyServer(const char* listeningPort, LPCWSTR serverIP, INTERNET_PORT serverPort);
	~WebSocketProxyServer();
	void serverInitialization() override;
	void serverHandler() override;
private:
	void connectToServer();
	void CALLBACK WebSocketEvents (HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength);
	void sockCommunication();
	void createSocketEvents();
	void stopServer();
private:
	HINTERNET SessionHandle;
	HINTERNET ConnectionHandle;
	HINTERNET RequestHandle;
	HINTERNET WebSocketHandle;
	LPCWSTR serverIP;
	INTERNET_PORT serverPort;
};
