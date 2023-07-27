#include "ProxyServer.h"
#include "ServException.h"
#include "writeLog.h"
#include <string>

#define BUFFER_SIZE 1024

ProxyServer::ProxyServer(const char* listeningPort, const char* serverIP, const char* serverPort) : lisSockInfo(nullptr),
servSockInfo(nullptr), serviceStopEvent(nullptr), listeningPort(listeningPort),
serverIP(serverIP), serverPort(serverPort), lis_socket(-1), client_socket(-1),
server_socket(-1), errState(0), bufToClientHasData(WSA_INVALID_EVENT), bufToServHasData(WSA_INVALID_EVENT),
clientReadySend(WSA_INVALID_EVENT), serverReadySend(WSA_INVALID_EVENT)
{
}

ProxyServer::~ProxyServer()
{
	stopServer();
}

void ProxyServer::serverInitialization()
{
	initSockets();
	createSockInfo("127.0.0.1", listeningPort, &lisSockInfo);
	createNewSocket(lis_socket, lisSockInfo);
	bindSocket();
	listenState();
}

void ProxyServer::serverHandler()
{
	acceptConnection();
	createSockInfo(serverIP, serverPort, &servSockInfo);
	createNewSocket(server_socket, servSockInfo);
	connectToServer();
	sockCommunication();
}

void ProxyServer::initSockets()
{
	errState = WSAStartup(MAKEWORD(2, 2), &wsData);
	if (errState != 0) {
		throw ServException("Initialization version error: ", WSAGetLastError());
	}
}

void ProxyServer::createSockInfo(const char* ip, const char* port, addrinfo** sockInfo)
{
	addrinfo addrInfo;
	ZeroMemory(&addrInfo, sizeof(addrInfo));
	addrInfo.ai_family = AF_INET;
	addrInfo.ai_socktype = SOCK_STREAM;
	addrInfo.ai_protocol = IPPROTO_TCP;
	errState = getaddrinfo(ip, port, &addrInfo, sockInfo);
	if (errState != 0) {
		throw ServException("Error getting address information: ", WSAGetLastError());
	}
}

void ProxyServer::createNewSocket(SOCKET& new_socket, addrinfo* sockInfo)
{
	new_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (new_socket == INVALID_SOCKET) {
		throw ServException("Socket initialization error: ", WSAGetLastError());
	}
}

void ProxyServer::bindSocket()
{
	errState = bind(lis_socket, lisSockInfo->ai_addr, lisSockInfo->ai_addrlen);
	if (errState != 0) {
		throw ServException("Binding error: ", WSAGetLastError());
	}
}

void ProxyServer::listenState()
{
	errState = listen(lis_socket, SOMAXCONN);
	if (errState != 0) {
		throw ServException("Listening error: ", WSAGetLastError());
	}
	writeLog("Server in listening state...");
}

void ProxyServer::connectToServer()
{
	errState = connect(server_socket, servSockInfo->ai_addr, servSockInfo->ai_addrlen);
	if (errState != 0) {
		throw ServException("Connection to Web Server failed: ", WSAGetLastError());
	}
	writeLog("Connection to Server successful");
}

void ProxyServer::sockCommunication()
{
	WSANETWORKEVENTS clientEvents;
	WSANETWORKEVENTS serverEvents;
	char bufToClient[BUFFER_SIZE];
	char bufToServer[BUFFER_SIZE];
	ZeroMemory(&bufToClient, sizeof(bufToClient));
	ZeroMemory(&bufToServer, sizeof(bufToServer));
	int dataForClient = 0;
	int dataForServer = 0;
	int indexForClient = 0;
	int indexForServer = 0;

	createSocketEvents();
	HANDLE eventArr[5] = { *serviceStopEvent, clientReadySend, serverReadySend, bufToServHasData, bufToClientHasData };

	while (true) {

		int eventResult = WSAWaitForMultipleEvents(5, eventArr, FALSE, INFINITE, FALSE);
		if (eventResult == WSA_WAIT_FAILED) {
			closeConnection();
			throw ServException("Error while waiting for events: ", WSAGetLastError());
		}

		if (eventResult == WSA_WAIT_EVENT_0) {
			closeConnection();
			throw ServException("Service stopped by SCM: ");
		}

		errState = WSAEnumNetworkEvents(client_socket, clientReadySend, &clientEvents);
		if (errState == SOCKET_ERROR) {
			closeConnection();
			throw ServException("Error while getting information about events: ", WSAGetLastError());
		}

		errState = WSAEnumNetworkEvents(server_socket, serverReadySend, &serverEvents);
		if (errState == SOCKET_ERROR) {
			closeConnection();
			throw ServException("Error while getting information about events: ", WSAGetLastError());
		}

		if (clientEvents.lNetworkEvents & FD_CLOSE) {
			if (dataForServer != 0) {
				int send_data = send(server_socket, bufToServer + indexForServer, dataForServer, 0);
			}
			closeConnection();
			throw ServException("Connection with the client has been severed: ", WSAGetLastError());
		}

		if (serverEvents.lNetworkEvents & FD_CLOSE) {
			if (dataForClient != 0) {
				int send_data = send(client_socket, bufToClient + indexForClient, dataForClient, 0);
			}
			closeConnection();
			throw ServException("Connection with the server has been severed: ", WSAGetLastError());
		}

		if ((clientEvents.lNetworkEvents & FD_READ) && (dataForServer == 0)) {
			int rec_data = recv(client_socket, bufToServer, BUFFER_SIZE, 0);
			if (rec_data == SOCKET_ERROR) {
				closeConnection();
				throw ServException("Connection with the client has been severed: ", WSAGetLastError());
			}
			dataForServer = rec_data;
			indexForServer = 0;
		}

		if ((serverEvents.lNetworkEvents & FD_READ) && (dataForClient == 0)) {
			int rec_data = recv(server_socket, bufToClient, BUFFER_SIZE, 0);
			if (rec_data == SOCKET_ERROR) {
				closeConnection();
				throw ServException("Connection with the server has been severed: ", WSAGetLastError());
			}
			dataForClient = rec_data;
			indexForClient = 0;
		}

		if (dataForServer != 0) {
			int send_data = send(server_socket, bufToServer + indexForServer, dataForServer, 0);
			if (send_data == SOCKET_ERROR) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					closeConnection();
					throw ServException("Connection with the server has been severed: ", WSAGetLastError());
				}
			}
			else {
				dataForServer -= send_data;
				indexForServer += send_data;
			}
		}

		if (dataForClient != 0) {
			int send_data = send(client_socket, bufToClient + indexForClient, dataForClient, 0);
			if (send_data == SOCKET_ERROR) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					closeConnection();
					throw ServException("Connection with the client has been severed: ", WSAGetLastError());
				}
			}
			else {
				dataForClient -= send_data;
				indexForClient += send_data;
			}
		}

		if (dataForClient != 0) {
			WSASetEvent(bufToClientHasData);
		}
		else {
			WSAResetEvent(bufToClientHasData);
		}

		if (dataForServer != 0) {
			WSASetEvent(bufToServHasData);
		}
		else {
			WSAResetEvent(bufToServHasData);
		}
	}
}

void ProxyServer::createSocketEvents()
{
	bufToClientHasData = WSACreateEvent();
	if (bufToClientHasData == WSA_INVALID_EVENT) {
		closeConnection();
		throw ServException("Create WSA Event failed: ", WSAGetLastError());
	}

	bufToServHasData = WSACreateEvent();
	if (bufToServHasData == WSA_INVALID_EVENT) {
		closeConnection();
		throw ServException("Create WSA Event failed: ", WSAGetLastError());
	}

	clientReadySend = WSACreateEvent();
	if (clientReadySend == WSA_INVALID_EVENT) {
		closeConnection();
		throw ServException("Create WSA Event failed: ", WSAGetLastError());
	}

	serverReadySend = WSACreateEvent();
	if (serverReadySend == WSA_INVALID_EVENT) {
		closeConnection();
		throw ServException("Create WSA Event failed: ", WSAGetLastError());
	}

	if (WSAEventSelect(client_socket, clientReadySend, FD_ACCEPT | FD_READ | FD_CLOSE) != 0) {
		closeConnection();
		throw ServException("WSAEventSelect function failed: ", WSAGetLastError());
	}

	if (WSAEventSelect(server_socket, serverReadySend, FD_READ | FD_CLOSE) != 0) {
		closeConnection();
		throw ServException("WSAEventSelect function failed: ", WSAGetLastError());
	}
}

void ProxyServer::closeConnection()
{
	WSACloseEvent(clientConnectionRequest);
	WSACloseEvent(bufToClientHasData);
	WSACloseEvent(bufToServHasData);
	WSACloseEvent(clientReadySend);
	WSACloseEvent(serverReadySend);
	shutdown(server_socket, SD_BOTH);
	shutdown(client_socket, SD_BOTH);
	closesocket(server_socket);
	closesocket(client_socket);
}

void ProxyServer::stopServer()
{
	freeaddrinfo(servSockInfo);
	freeaddrinfo(lisSockInfo);
	if (server_socket != INVALID_SOCKET) {
		closesocket(server_socket);
	}
	if (client_socket != INVALID_SOCKET) {
		closesocket(client_socket);
	}
	if (lis_socket != INVALID_SOCKET) {
		closesocket(lis_socket);
	}
	WSACleanup();

	writeLog("Proxy server was stopped");
}

ProxyServer::ProxyServer(const char* listeningPort) : lisSockInfo(nullptr),
servSockInfo(nullptr), serviceStopEvent(nullptr), listeningPort(listeningPort),
lis_socket(-1), client_socket(-1), server_socket(-1),
errState(0), bufToClientHasData(WSA_INVALID_EVENT), bufToServHasData(WSA_INVALID_EVENT),
clientReadySend(WSA_INVALID_EVENT), serverReadySend(WSA_INVALID_EVENT)
{
}

void ProxyServer::acceptConnection()
{
	clientConnectionRequest = WSACreateEvent();
	if (clientConnectionRequest == WSA_INVALID_EVENT) {
		throw ServException("Create WSA Event failed: ", WSAGetLastError());
	}

	if (WSAEventSelect(lis_socket, clientConnectionRequest, FD_ACCEPT) != 0) {
		WSACloseEvent(clientConnectionRequest);
		throw ServException("WSAEventSelect function failed: ", WSAGetLastError());
	}

	char ipStr[INET_ADDRSTRLEN];
	sockaddr_in clientSockInfo;
	ZeroMemory(&clientSockInfo, sizeof(clientSockInfo));
	int clientSize = sizeof(clientSockInfo);

	HANDLE eventArr[2] = { *serviceStopEvent, clientConnectionRequest };
	int eventResult = WSAWaitForMultipleEvents(2, eventArr, FALSE, INFINITE, FALSE);
	if (eventResult == WSA_WAIT_FAILED) {
		closeConnection();
		throw ServException("Error while waiting for events: ", WSAGetLastError());
	}

	if (eventResult == WSA_WAIT_EVENT_0) {
		closeConnection();
		throw ServException("Service stopped by SCM: ");
	}

	if (eventResult == WSA_WAIT_EVENT_0 + 1) {
		client_socket = accept(lis_socket, (sockaddr*)&clientSockInfo, &clientSize);
		if (client_socket == INVALID_SOCKET) {
			throw ServException("Client connection error: ", WSAGetLastError());
		}
	}
}

WebSocketProxyServer::WebSocketProxyServer(const char* listeningPort, LPCWSTR serverIP, INTERNET_PORT serverPort) : ProxyServer(listeningPort)
{
	this->serverIP = serverIP;
	this->serverPort = serverPort;
	HINTERNET SessionHandle = NULL;
	HINTERNET ConnectionHandle = NULL;
	HINTERNET RequestHandle = NULL;
	HINTERNET WebSocketHandle = NULL;
}

WebSocketProxyServer::~WebSocketProxyServer()
{
	stopServer();
}

void WebSocketProxyServer::serverInitialization()
{
	ProxyServer::serverInitialization();
	SessionHandle = WinHttpOpen(L"ProxyServer", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (SessionHandle == NULL) {
		throw ServException("WinHTTP initialization error: ", GetLastError());
	}
}

void WebSocketProxyServer::serverHandler()
{
	acceptConnection();
	connectToServer();
}

void WebSocketProxyServer::connectToServer()
{
	ConnectionHandle = WinHttpConnect(SessionHandle, serverIP, serverPort, 0);
	if (ConnectionHandle == NULL) {
		throw ServException("Target server initialization error: ", GetLastError());
	}

	RequestHandle = WinHttpOpenRequest(ConnectionHandle, L"GET", NULL, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
	if (RequestHandle == NULL) {
		throw ServException("HTTP request creation error: ", GetLastError());
	}

	if (WinHttpSetOption(RequestHandle, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0) == FALSE) {
		throw ServException("Setting Internet option error: ", GetLastError());
	}

	if (WinHttpSendRequest(RequestHandle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0) == FALSE) {
		throw ServException("Sending request to server error: ", GetLastError());
	}

	if (WinHttpReceiveResponse(RequestHandle, NULL) == FALSE) {
		throw ServException("Request response error: ", GetLastError());
	}

	WebSocketHandle = WinHttpWebSocketCompleteUpgrade(RequestHandle, NULL);
	if (WebSocketHandle == NULL) {
		throw ServException("Handshake completion error: ", GetLastError());
	}

	writeLog("Connection to Web Socket server successful");
}

void WebSocketProxyServer::WebSocketEvents(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength)
{

}

void WebSocketProxyServer::sockCommunication()
{

}

void WebSocketProxyServer::createSocketEvents()
{
	bufToClientHasData = WSACreateEvent();
	if (bufToClientHasData == WSA_INVALID_EVENT) {
		closeConnection();
		throw ServException("Create WSA Event failed: ", WSAGetLastError());
	}

	bufToServHasData = WSACreateEvent();
	if (bufToServHasData == WSA_INVALID_EVENT) {
		closeConnection();
		throw ServException("Create WSA Event failed: ", WSAGetLastError());
	}

	clientReadySend = WSACreateEvent();
	if (clientReadySend == WSA_INVALID_EVENT) {
		closeConnection();
		throw ServException("Create WSA Event failed: ", WSAGetLastError());
	}

	serverReadySend = WSACreateEvent();
	if (serverReadySend == WSA_INVALID_EVENT) {
		closeConnection();
		throw ServException("Create WSA Event failed: ", WSAGetLastError());
	}

	if (WSAEventSelect(client_socket, clientReadySend, FD_ACCEPT | FD_READ | FD_CLOSE) != 0) {
		closeConnection();
		throw ServException("WSAEventSelect function failed: ", WSAGetLastError());
	}

}

void WebSocketProxyServer::stopServer()
{
	if (SessionHandle != NULL) {
		WinHttpCloseHandle(SessionHandle);
	}
	if (ConnectionHandle != NULL) {
		WinHttpCloseHandle(ConnectionHandle);
	}
	if (RequestHandle != NULL) {
		WinHttpCloseHandle(RequestHandle);
	}
	if (WebSocketHandle != NULL) {
		WinHttpCloseHandle(WebSocketHandle);
	}
}
