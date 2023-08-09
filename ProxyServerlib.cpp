#include "ProxyServerlib.h"
#include "ServException.h"
#include "writeLog.h"
#include <string>
#include <thread>

ProxyServer::ProxyServer(): 
stopEvent(nullptr), 
disconnect(NULL),
readySend(NULL),
dataToSend(NULL),
dataInReceiveBuffer(0),
indexForRecData(0)
{
	ZeroMemory(&receiveBuffer, sizeof(receiveBuffer));
}

ProxyServer::~ProxyServer()
{
}

// TCP Client 

TCPClient::TCPClient(const char* listeningPort) :
listeningPort(listeningPort),
lisSockInfo(nullptr),
lis_socket(INVALID_SOCKET),
client_socket(INVALID_SOCKET),
clientConnectionRequest(WSA_INVALID_EVENT),
clientReadySend(WSA_INVALID_EVENT),
errState(0)
{
}

TCPClient::~TCPClient()
{
	freeaddrinfo(lisSockInfo);

	if (client_socket != INVALID_SOCKET) {
		closesocket(client_socket);
	}
	if (lis_socket != INVALID_SOCKET) {
		closesocket(lis_socket);
	}

	WSACleanup();

	writeLog("TCPClient memory freed");
}

void TCPClient::proxyServerInit()
{
	initSockets();
	createSockInfo("127.0.0.1", listeningPort, &lisSockInfo);
	createNewSocket(lis_socket, lisSockInfo);
	bindSocket();
	listenState();
}

void TCPClient::WaitingForClients()
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

	HANDLE eventArr[2] = { *stopEvent, clientConnectionRequest };
	int eventResult = WSAWaitForMultipleEvents(2, eventArr, FALSE, INFINITE, FALSE);
	if (eventResult == WSA_WAIT_FAILED) {
		WSACloseEvent(clientConnectionRequest);
		throw ServException("Error while waiting for events: ", WSAGetLastError());
	}

	if (eventResult == WSA_WAIT_EVENT_0) {
		WSACloseEvent(clientConnectionRequest);
		throw ServException("Service stopped by SCM: ");
	}

	if (eventResult == WSA_WAIT_EVENT_0 + 1) {
		client_socket = accept(lis_socket, (sockaddr*)&clientSockInfo, &clientSize);
		if (client_socket == INVALID_SOCKET) {
			throw ServException("Client connection error: ", WSAGetLastError());
		}
	}
}

void TCPClient::Handler()
{
	WSANETWORKEVENTS clientEvents;

	clientReadySend = WSACreateEvent();
	if (clientReadySend == WSA_INVALID_EVENT) {
		writeLog("Create WSA Event failed: ", WSAGetLastError());
		SetEvent(disconnect);
		return;
	}

	HANDLE eventArr[2] = { *stopEvent, clientReadySend };
	while (true) {

		int eventResult = WSAWaitForMultipleEvents(2, eventArr, FALSE, INFINITE, FALSE);
		if (eventResult == WSA_WAIT_FAILED) {
			writeLog("Error while waiting for events: ", WSAGetLastError());
			SetEvent(disconnect);
			return;
		}

		if (eventResult == WSA_WAIT_EVENT_0) {
			return;
		}

		errState = WSAEnumNetworkEvents(client_socket, clientReadySend, &clientEvents);
		if (errState == SOCKET_ERROR) {
			writeLog("Error while getting information about events: ", WSAGetLastError());
			SetEvent(disconnect);
			return;
		}

		if (clientEvents.lNetworkEvents & FD_CLOSE) { // poslat zbýtek dat?
			writeLog("Connection with the client has been severed: ", WSAGetLastError());
			SetEvent(disconnect);
			return;
		}

		if (clientEvents.lNetworkEvents & FD_READ) {
			SetEvent(readySend);
			return;
		}

	}
}

int TCPClient::sendData(const char* pData, int length)
{
	if (length == 0) {
		return 0;
	}

	int send_data = send(client_socket, pData, length, 0);
	if (send_data == SOCKET_ERROR) {
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			closeConnection();
			throw ServException("Connection with the client has been severed: ", WSAGetLastError());
		}
		send_data = 0;
	}
	return send_data;
}

void TCPClient::receiveData()
{
	if (dataInReceiveBuffer != 0) {
		return;
	}

	int rec_data = recv(client_socket, receiveBuffer, BUFFER_SIZE, 0);
	if (rec_data == SOCKET_ERROR) {
		closeConnection();
		throw ServException("Connection with the client has been severed: ", WSAGetLastError());
	}
	dataInReceiveBuffer = rec_data;
	indexForRecData = 0;
}

void TCPClient::closeConnection()
{
	WSACloseEvent(clientConnectionRequest);
	shutdown(client_socket, SD_BOTH);
	closesocket(client_socket);
}

void TCPClient::initSockets()
{
	errState = WSAStartup(MAKEWORD(2, 2), &wsData);
	if (errState != 0) {
		throw ServException("Initialization version error: ", WSAGetLastError());
	}
}

void TCPClient::createSockInfo(const char* ip, const char* port, addrinfo** sockInfo)
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

void TCPClient::createNewSocket(SOCKET& new_socket, addrinfo* sockInfo)
{
	new_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (new_socket == INVALID_SOCKET) {
		throw ServException("Socket initialization error: ", WSAGetLastError());
	}
}

void TCPClient::bindSocket()
{
	errState = bind(lis_socket, lisSockInfo->ai_addr, lisSockInfo->ai_addrlen);
	if (errState != 0) {
		throw ServException("Binding error: ", WSAGetLastError());
	}
}

void TCPClient::listenState()
{
	errState = listen(lis_socket, SOMAXCONN);
	if (errState != 0) {
		throw ServException("Listening error: ", WSAGetLastError());
	}
	writeLog("Server in listening state...");
}

// TCP Target Server

TCPTargetServer::TCPTargetServer(const char* serverIP, const char* serverPort): 
	serverIP (serverIP),
	serverPort (serverPort),
	serverSockInfo(nullptr),
	server_socket(INVALID_SOCKET),
	targetServerReadySend(WSA_INVALID_EVENT),
	errState(0)
{
}

TCPTargetServer::~TCPTargetServer()
{
	freeaddrinfo(serverSockInfo);

	if (server_socket != INVALID_SOCKET) {
		closesocket(server_socket);
	}
	
	writeLog("TCPTargetServer memory freed");
}

void TCPTargetServer::connectToTargetServer()
{
	createSockInfo(serverIP, serverPort, &serverSockInfo);
	createNewSocket(server_socket, serverSockInfo);

	errState = connect(server_socket, serverSockInfo->ai_addr, serverSockInfo->ai_addrlen);
	if (errState != 0) {
		throw ServException("Connection to Target Server failed: ", WSAGetLastError());
	}
	writeLog("Connection to Target Server successful");
}

void TCPTargetServer::Handler()
{
	WSANETWORKEVENTS targetServerEvents;

	targetServerReadySend = WSACreateEvent();
	if (targetServerReadySend == WSA_INVALID_EVENT) {
		writeLog("Create WSA Event failed: ", WSAGetLastError());
		SetEvent(disconnect);
		return;
	}

	HANDLE eventArr[2] = { *stopEvent, targetServerReadySend };
	while (true) {

		int eventResult = WSAWaitForMultipleEvents(2, eventArr, FALSE, INFINITE, FALSE);
		if (eventResult == WSA_WAIT_FAILED) {
			writeLog("Error while waiting for events: ", WSAGetLastError());
			SetEvent(disconnect);
			return;
		}

		if (eventResult == WSA_WAIT_EVENT_0) {
			return;
		}

		errState = WSAEnumNetworkEvents(server_socket, targetServerReadySend, &targetServerEvents);
		if (errState == SOCKET_ERROR) {
			writeLog("Error while getting information about events: ", WSAGetLastError());
			SetEvent(disconnect);
			return;
		}

		if (targetServerEvents.lNetworkEvents & FD_CLOSE) { // poslat zbýtek dat?
			writeLog("Connection with the target server has been severed: ", WSAGetLastError());
			SetEvent(disconnect);
			return;
		}

		if (targetServerEvents.lNetworkEvents & FD_READ) {
			SetEvent(readySend);
		}

	}
}

int TCPTargetServer::sendData(const char* pData, int length)
{
	if (length == 0) {
		return 0;
	}

	int send_data = send(server_socket, pData, length, 0);
	if (send_data == SOCKET_ERROR) {
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			closeConnection();
			throw ServException("Connection with the server has been severed: ", WSAGetLastError());
		}
		send_data = 0;
	}
	return send_data;
}

void TCPTargetServer::receiveData()
{
	if (dataInReceiveBuffer != 0) {
		return;
	}

	int rec_data = recv(server_socket, receiveBuffer, BUFFER_SIZE, 0);
	if (rec_data == SOCKET_ERROR) {
		closeConnection();
		throw ServException("Connection with the server has been severed: ", WSAGetLastError());
	}
	dataInReceiveBuffer = rec_data;
	indexForRecData = 0;
}

void TCPTargetServer::closeConnection()
{
	WSACloseEvent(targetServerReadySend);
	shutdown(server_socket, SD_BOTH);
	closesocket(server_socket);
}

void TCPTargetServer::createSockInfo(const char* ip, const char* port, addrinfo** sockInfo)
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

void TCPTargetServer::createNewSocket(SOCKET& new_socket, addrinfo* sockInfo)
{
	new_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (new_socket == INVALID_SOCKET) {
		throw ServException("Socket initialization error: ", WSAGetLastError());
	}
}

//void TCPConnection::sockCommunication()
//{
//	WSANETWORKEVENTS clientEvents;
//	WSANETWORKEVENTS serverEvents;
//	char bufToClient[BUFFER_SIZE];
//	char bufToServer[BUFFER_SIZE];
//	ZeroMemory(&bufToClient, sizeof(bufToClient));
//	ZeroMemory(&bufToServer, sizeof(bufToServer));
//	int dataForClient = 0;
//	int dataForServer = 0;
//	int indexForClient = 0;
//	int indexForServer = 0;
//
//	createSocketEvents();
//	HANDLE eventArr[5] = { *serviceStopEvent, clientReadySend, serverReadySend, bufToServHasData, bufToClientHasData };
//
//	while (true) {
//
//		int eventResult = WSAWaitForMultipleEvents(5, eventArr, FALSE, INFINITE, FALSE);
//		if (eventResult == WSA_WAIT_FAILED) {
//			closeConnection();
//			throw ServException("Error while waiting for events: ", WSAGetLastError());
//		}
//
//		if (eventResult == WSA_WAIT_EVENT_0) {
//			closeConnection();
//			throw ServException("Connection has been severed: ", WSAGetLastError());
//		}
//
//		errState = WSAEnumNetworkEvents(client_socket, clientReadySend, &clientEvents);
//		if (errState == SOCKET_ERROR) {
//			closeConnection();
//			throw ServException("Error while getting information about events: ", WSAGetLastError());
//		}
//
//		errState = WSAEnumNetworkEvents(server_socket, serverReadySend, &serverEvents);
//		if (errState == SOCKET_ERROR) {
//			closeConnection();
//			throw ServException("Error while getting information about events: ", WSAGetLastError());
//		}
//
//		if (clientEvents.lNetworkEvents & FD_CLOSE) { // poslat zbýtek dat?
//			closeConnection();
//			throw ServException("Connection with the client has been severed: ", WSAGetLastError());
//		}
//
//		if (serverEvents.lNetworkEvents & FD_CLOSE) { // poslat zbýtek dat?
//			closeConnection();
//			throw ServException("Connection with the server has been severed: ", WSAGetLastError());
//		}
//
//		if ((clientEvents.lNetworkEvents & FD_READ) && (dataForServer == 0)) {
//			int rec_data = recv(client_socket, bufToServer, BUFFER_SIZE, 0);
//			if (rec_data == SOCKET_ERROR) {
//				closeConnection();
//				throw ServException("Connection with the client has been severed: ", WSAGetLastError());
//			}
//			dataForServer = rec_data;
//			indexForServer = 0;
//		}
//
//		if ((serverEvents.lNetworkEvents & FD_READ) && (dataForClient == 0)) {
//			int rec_data = recv(server_socket, bufToClient, BUFFER_SIZE, 0);
//			if (rec_data == SOCKET_ERROR) {
//				closeConnection();
//				throw ServException("Connection with the server has been severed: ", WSAGetLastError());
//			}
//			dataForClient = rec_data;
//			indexForClient = 0;
//		}
//
//		if (dataForServer != 0) {
//			int send_data = send(server_socket, bufToServer + indexForServer, dataForServer, 0);
//			if (send_data == SOCKET_ERROR) {
//				if (WSAGetLastError() != WSAEWOULDBLOCK) {
//					closeConnection();
//					throw ServException("Connection with the server has been severed: ", WSAGetLastError());
//				}
//			}
//			else {
//				dataForServer -= send_data;
//				indexForServer += send_data;
//			}
//		}
//
//		if (dataForClient != 0) {
//			int send_data = send(client_socket, bufToClient + indexForClient, dataForClient, 0);
//			if (send_data == SOCKET_ERROR) {
//				if (WSAGetLastError() != WSAEWOULDBLOCK) {
//					closeConnection();
//					throw ServException("Connection with the client has been severed: ", WSAGetLastError());
//				}
//			}
//			else {
//				dataForClient -= send_data;
//				indexForClient += send_data;
//			}
//		}
//
//		if (dataForClient != 0) {
//			WSASetEvent(bufToClientHasData);
//		}
//		else {
//			WSAResetEvent(bufToClientHasData);
//		}
//
//		if (dataForServer != 0) {
//			WSASetEvent(bufToServHasData);
//		}
//		else {
//			WSAResetEvent(bufToServHasData);
//		}
//	}
//}

//void TCPConnection::createSocketEvents()
//{
//	bufToClientHasData = WSACreateEvent();
//	if (bufToClientHasData == WSA_INVALID_EVENT) {
//		closeConnection();
//		throw ServException("Create WSA Event failed: ", WSAGetLastError());
//	}
//
//	bufToServHasData = WSACreateEvent();
//	if (bufToServHasData == WSA_INVALID_EVENT) {
//		closeConnection();
//		throw ServException("Create WSA Event failed: ", WSAGetLastError());
//	}
//
//	clientReadySend = WSACreateEvent();
//	if (clientReadySend == WSA_INVALID_EVENT) {
//		closeConnection();
//		throw ServException("Create WSA Event failed: ", WSAGetLastError());
//	}
//
//	serverReadySend = WSACreateEvent();
//	if (serverReadySend == WSA_INVALID_EVENT) {
//		closeConnection();
//		throw ServException("Create WSA Event failed: ", WSAGetLastError());
//	}
//
//	if (WSAEventSelect(client_socket, clientReadySend, FD_READ | FD_CLOSE) != 0) {
//		closeConnection();
//		throw ServException("WSAEventSelect function failed: ", WSAGetLastError());
//	}
//
//	if (WSAEventSelect(server_socket, serverReadySend, FD_READ | FD_CLOSE) != 0) {
//		closeConnection();
//		throw ServException("WSAEventSelect function failed: ", WSAGetLastError());
//	}
//}

//

//WebSocketConnection::WebSocketConnection(const char* listeningPort, LPCWSTR serverIP, INTERNET_PORT serverPort) : StartServer(listeningPort)
//{
//	this->serverIP = serverIP;
//	this->serverPort = serverPort;
//	SendResponseStatus = FALSE;
//	ResponseStatus = FALSE;
//	serverSendResponse = NULL;
//	SessionHandle = NULL;
//	ConnectionHandle = NULL;
//	RequestHandle = NULL;
//	WebSocketHandle = NULL;
//}
//
//WebSocketConnection::~WebSocketConnection()
//{
//	stopServer();
//}
//
//void WebSocketConnection::serverInitialization()
//{
//	StartServer::serverInitialization();
//	SessionHandle = WinHttpOpen(L"ProxyServer", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
//	if (SessionHandle == NULL) {
//		throw ServException("WinHTTP initialization error: ", GetLastError());
//	}
//}
//
//void WebSocketConnection::serverHandler()
//{
//	acceptConnection();
//	connectToServer();
//}
//
//void WebSocketConnection::WaitResponseFromServer()
//{
//	SendResponseStatus = WinHttpSendRequest(RequestHandle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0);
//	ResponseStatus = WinHttpReceiveResponse(RequestHandle, NULL);
//	SetEvent(serverSendResponse);
//}
//
//void WebSocketConnection::connectToServer() {
//	serverSendResponse = CreateEvent(NULL, TRUE, FALSE, NULL);
//	if (serverSendResponse == NULL) {
//		throw ServException("Create Event Error: ", GetLastError());
//	}
//
//	ConnectionHandle = WinHttpConnect(SessionHandle, serverIP, serverPort, 0);
//	if (ConnectionHandle == NULL) {
//		throw ServException("Target server initialization error: ", GetLastError());
//	}
//
//	RequestHandle = WinHttpOpenRequest(ConnectionHandle, L"GET", NULL, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
//	if (RequestHandle == NULL) {
//		throw ServException("HTTP request creation error: ", GetLastError());
//	}
//
//#pragma warning(suppress : 6387)
//	if (WinHttpSetOption(RequestHandle, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0) == FALSE) {
//		throw ServException("Setting Internet option error: ", GetLastError());
//	}
//
//	std::thread WaitingForResponse([&]() {
//		WaitResponseFromServer();
//	});
//	WaitingForResponse.detach();
//	
//	HANDLE eventArr[2] = { *serviceStopEvent, serverSendResponse };
//	int eventResult = WaitForMultipleObjects(2, eventArr, FALSE, 5000);
//	if (eventResult == WAIT_FAILED) {
//		CloseHandle(serverSendResponse);
//		throw ServException("Error while waiting for events: ", GetLastError());
//	}
//
//	if (eventResult == WAIT_TIMEOUT) {
//		CloseHandle(serverSendResponse);
//		throw ServException("Response timeout expired: ");
//	}
//	
//	if (eventResult == WAIT_OBJECT_0) {
//		CloseHandle(serverSendResponse);
//		throw ServException("Service stopped by SCM: ");
//	}
//
//	if (eventResult == WAIT_OBJECT_0 + 1) {
//		if (SendResponseStatus == FALSE) {
//			throw ServException("Sending request to server error: ", GetLastError());
//		}
//
//		if (ResponseStatus == FALSE) {
//			throw ServException("Request response error: ", GetLastError());
//		}
//
//		WebSocketHandle = WinHttpWebSocketCompleteUpgrade(RequestHandle, NULL);
//		if (WebSocketHandle == NULL) {
//			throw ServException("Handshake completion error: ", GetLastError());
//		}
//
//		writeLog("Connection to Web Socket server successful");
//	}
//}
//
//void WebSocketConnection::sockCommunication()
//{
//
//}

//void WebSocketConnection::createSocketEvents()
//{
//	bufToClientHasData = WSACreateEvent();
//	if (bufToClientHasData == WSA_INVALID_EVENT) {
//		closeConnection();
//		throw ServException("Create WSA Event failed: ", WSAGetLastError());
//	}
//
//	bufToServHasData = WSACreateEvent();
//	if (bufToServHasData == WSA_INVALID_EVENT) {
//		closeConnection();
//		throw ServException("Create WSA Event failed: ", WSAGetLastError());
//	}
//
//	clientReadySend = WSACreateEvent();
//	if (clientReadySend == WSA_INVALID_EVENT) {
//		closeConnection();
//		throw ServException("Create WSA Event failed: ", WSAGetLastError());
//	}
//
//	serverReadySend = WSACreateEvent();
//	if (serverReadySend == WSA_INVALID_EVENT) {
//		closeConnection();
//		throw ServException("Create WSA Event failed: ", WSAGetLastError());
//	}
//
//	if (WSAEventSelect(client_socket, clientReadySend, FD_ACCEPT | FD_READ | FD_CLOSE) != 0) {
//		closeConnection();
//		throw ServException("WSAEventSelect function failed: ", WSAGetLastError());
//	}
//
//}

//void WebSocketConnection::stopServer()
//{
//	if (SessionHandle != NULL) {
//		WinHttpCloseHandle(SessionHandle);
//	}
//	if (ConnectionHandle != NULL) {
//		WinHttpCloseHandle(ConnectionHandle);
//	}
//	if (RequestHandle != NULL) {
//		WinHttpCloseHandle(RequestHandle);
//	}
//	if (WebSocketHandle != NULL) {
//		WinHttpCloseHandle(WebSocketHandle);
//	}
//}

// Proxy server connection function

void proxyConnection(ProxyServer& client, ProxyServer& targetServer, HANDLE* stopEvent) {

	client.stopEvent = stopEvent;
	targetServer.stopEvent = stopEvent;

	try
	{
		client.proxyServerInit();

		while (WaitForSingleObject(stopEvent, 0) != WAIT_OBJECT_0) {

			try
			{
				client.WaitingForClients();
				targetServer.connectToTargetServer();

				//threads client a server handlers
				std::thread cH([&]() {
					client.Handler();
					});
				cH.detach();

				std::thread sH([&]() {
					targetServer.Handler();
					});
				sH.detach();

				while (true)
				{
					HANDLE eventArr[7] = { *stopEvent, client.disconnect, targetServer.disconnect, client.readySend, client.dataToSend, targetServer.readySend, targetServer.dataToSend };
					int eventResult = WaitForMultipleObjects(7, eventArr, FALSE, INFINITE);

					if (eventResult == WAIT_FAILED) {
						client.closeConnection();
						targetServer.closeConnection();
						writeLog("Error while waiting for events: ", GetLastError());
						break;
					}

					if (eventResult == WAIT_OBJECT_0) {
						client.closeConnection();
						targetServer.closeConnection();
						writeLog("Connection has been severed: ", GetLastError());
						break;
					}

					if (WaitForSingleObject(eventArr[1], 0) == WAIT_OBJECT_0) {
						client.closeConnection();
						targetServer.closeConnection();
						break;
					}

					if (WaitForSingleObject(eventArr[2], 0) == WAIT_OBJECT_0) {
						targetServer.closeConnection();
						client.closeConnection();
						break;
					}

					if (WaitForSingleObject(eventArr[3], 0) == WAIT_OBJECT_0) {
						client.receiveData();
					}

					if (WaitForSingleObject(eventArr[4], 0) == WAIT_OBJECT_0) {
						int send_data = targetServer.sendData(client.receiveBuffer + client.indexForRecData, client.dataInReceiveBuffer);
						client.dataInReceiveBuffer -= send_data;
						client.indexForRecData += send_data;
					}

					if (WaitForSingleObject(eventArr[5], 0) == WAIT_OBJECT_0) {
						targetServer.receiveData();
					}

					if (WaitForSingleObject(eventArr[6], 0) == WAIT_OBJECT_0) {
						int send_data = client.sendData(targetServer.receiveBuffer + targetServer.indexForRecData, targetServer.dataInReceiveBuffer);
						targetServer.dataInReceiveBuffer -= send_data;
						targetServer.indexForRecData += send_data;
					}

					if (client.dataInReceiveBuffer != 0) {
						WSASetEvent(client.dataToSend);
					}
					else {
						WSAResetEvent(client.dataToSend);
					}

					if (targetServer.dataInReceiveBuffer != 0) {
						WSASetEvent(targetServer.dataToSend);
					}
					else {
						WSAResetEvent(targetServer.dataToSend);
					}
				}
			}
			catch (const ServException& ex)
			{
				writeLog(ex.GetErrorType(), ex.GetErrorCode());
			}
		}
	}
	catch (const ServException& ex)
	{
		writeLog(ex.GetErrorType(), ex.GetErrorCode());
	}
}
