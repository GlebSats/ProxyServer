#include "TCPserver.h"

TCPserver::TCPserver(const char* listeningPort) :
	bufferEmpty(NULL),
	listeningPort(listeningPort),
	lisSockInfo(nullptr),
	lis_socket(INVALID_SOCKET),
	client_socket(INVALID_SOCKET),
	clientConnectionRequest(WSA_INVALID_EVENT),
	clientEvent(WSA_INVALID_EVENT),
	errState(0)
{
}

TCPserver::~TCPserver()
{
	freeaddrinfo(lisSockInfo);

	if (client_socket != INVALID_SOCKET) {
		closesocket(client_socket);
	}
	if (lis_socket != INVALID_SOCKET) {
		closesocket(lis_socket);
	}

	WSACleanup();

	writeLog("TCPserver memory freed");
}

void TCPserver::Initialization()
{
	initSockets();
	createSockInfo("127.0.0.1", listeningPort, &lisSockInfo);
	createNewSocket(lis_socket, lisSockInfo);
	bindSocket();
	listenState();
}

void TCPserver::Connection()
{
	eventsCreation();
	clientConnectionRequest = WSACreateEvent();
	if (clientConnectionRequest == WSA_INVALID_EVENT) {
		throw ServException("Server: Create WSA Event failed: ", WSAGetLastError());
	}

	if (WSAEventSelect(lis_socket, clientConnectionRequest, FD_ACCEPT) != 0) {
		WSACloseEvent(clientConnectionRequest);
		throw ServException("Server: WSAEventSelect function failed: ", WSAGetLastError());
	}

	char ipStr[INET_ADDRSTRLEN];
	sockaddr_in clientSockInfo;
	ZeroMemory(&clientSockInfo, sizeof(clientSockInfo));
	int clientSize = sizeof(clientSockInfo);

	HANDLE eventArr[2] = { *stopEvent, clientConnectionRequest };
	int eventResult = WSAWaitForMultipleEvents(2, eventArr, FALSE, INFINITE, FALSE);
	if (eventResult == WSA_WAIT_FAILED) {
		WSACloseEvent(clientConnectionRequest);
		throw ServException("Server: Error while waiting for events: ", WSAGetLastError());
	}

	if (eventResult == WSA_WAIT_EVENT_0) {
		WSACloseEvent(clientConnectionRequest);
		throw ServException("Service stopped by SCM: ");
	}

	if (eventResult == WSA_WAIT_EVENT_0 + 1) {
		client_socket = accept(lis_socket, (sockaddr*)&clientSockInfo, &clientSize);
		if (client_socket == INVALID_SOCKET) {
			throw ServException("Server: Client connection error: ", WSAGetLastError());
		}
	}
}

void TCPserver::Handler()
{
	WSANETWORKEVENTS eventType;

	bufferEmpty = CreateEvent(NULL, TRUE, TRUE, NULL);
	if (bufferEmpty == NULL) {
		writeLog("Server: Create Event Error: ", GetLastError());
		SetEvent(disconnect);
		return;
	}

	clientEvent = WSACreateEvent();
	if (clientEvent == WSA_INVALID_EVENT) {
		writeLog("Server: Create WSA Event failed: ", WSAGetLastError());
		SetEvent(disconnect);
		return;
	}

	if (WSAEventSelect(client_socket, clientEvent, FD_READ | FD_CLOSE) != 0) {
		writeLog("Server: WSAEventSelect function failed: ", WSAGetLastError());
		SetEvent(disconnect);
		return;
	}

	HANDLE eventArr[3] = { *stopEvent, disconnect, clientEvent };
	HANDLE eventArr2[3] = { *stopEvent, disconnect, bufferEmpty };

	while (true) {

		int eventResult2 = WaitForMultipleObjects(3, eventArr2, FALSE, INFINITE);
		if (eventResult2 == WAIT_FAILED) {
			writeLog("Server: Error while waiting for events: ", GetLastError());
			SetEvent(disconnect);
			return;
		}

		if ((eventResult2 == WAIT_OBJECT_0) || (eventResult2 == WAIT_OBJECT_0 + 1)) {
			return;
		}

		int eventResult = WSAWaitForMultipleEvents(3, eventArr, FALSE, INFINITE, FALSE);
		if (eventResult == WSA_WAIT_FAILED) {
			writeLog("Server: Error while waiting for events: ", WSAGetLastError());
			SetEvent(disconnect);
			return;
		}

		if ((eventResult == WSA_WAIT_EVENT_0) || (eventResult == WSA_WAIT_EVENT_0 + 1)) {
			return;
		}

		errState = WSAEnumNetworkEvents(client_socket, clientEvent, &eventType);
		if (errState == SOCKET_ERROR) {
			writeLog("Server: Error while getting information about events: ", WSAGetLastError());
			SetEvent(disconnect);
			return;
		}

		if (eventType.lNetworkEvents & FD_CLOSE) {
			writeLog("Server: Connection with the client has been severed: ", WSAGetLastError());
			SetEvent(disconnect);
			return;
		}

		if (eventType.lNetworkEvents & FD_READ) {
			SetEvent(readyReceive);
		}

		if (eventType.lNetworkEvents & FD_WRITE) {
			if (WSAEventSelect(client_socket, clientEvent, FD_READ | FD_CLOSE) != 0) {
				writeLog("Server: WSAEventSelect function failed: ", WSAGetLastError());
				SetEvent(disconnect);
				return;
			}
			SetEvent(readySend);
		}
	}
}

int TCPserver::sendData(const char* pData, const int length)
{
	int send_data = send(client_socket, pData, length, 0);
	if (send_data == SOCKET_ERROR) {
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			throw ServException("Server: Connection with the client has been severed: ", WSAGetLastError());
		}

		ResetEvent(readySend);

		if (WSAEventSelect(client_socket, clientEvent, FD_READ | FD_CLOSE | FD_WRITE) != 0) {
			throw ServException("Server: WSAEventSelect function failed: ", WSAGetLastError());
		}

		send_data = 0;
	}
	//
	writeLogHex("Client< S:" + std::to_string(send_data) + " D: ", pData[0], pData[1], pData[2]);
	//
	return send_data;
}

void TCPserver::receiveData()
{
	if (dataInReceiveBuffer != 0) {
		return;
	}

	int rec_data = recv(client_socket, receiveBuffer, BUFFER_SIZE, 0);
	if (rec_data == SOCKET_ERROR) {
		throw ServException("Server: Connection with the client has been severed: ", WSAGetLastError());
	}
	dataInReceiveBuffer = rec_data;
	indexForRecData = 0;

	if (dataInReceiveBuffer != 0) {
		SetEvent(dataToSend);
		ResetEvent(readyReceive);
		ResetEvent(bufferEmpty);
	}
}

void TCPserver::subtractData(const int send_data)
{
	dataInReceiveBuffer -= send_data;
	indexForRecData += send_data;

	if (dataInReceiveBuffer != 0) {
		SetEvent(dataToSend);
	}
	else {
		SetEvent(bufferEmpty);
		ResetEvent(dataToSend);
	}
}

void TCPserver::closeConnection()
{
	SetEvent(disconnect);

	if (clientConnectionRequest != WSA_INVALID_EVENT) {
		WSACloseEvent(clientConnectionRequest);
		clientConnectionRequest = WSA_INVALID_EVENT;
	}

	if (clientEvent != WSA_INVALID_EVENT) {
		WSACloseEvent(clientEvent);
		clientEvent = WSA_INVALID_EVENT;
	}

	if (bufferEmpty != NULL) {
		CloseHandle(bufferEmpty);
		bufferEmpty = NULL;
	}

	if (client_socket != INVALID_SOCKET) {
		shutdown(client_socket, SD_BOTH);
		closesocket(client_socket);
		client_socket = INVALID_SOCKET;
	}

	eventsDeleting();
}

void TCPserver::initSockets()
{
	errState = WSAStartup(MAKEWORD(2, 2), &wsData);
	if (errState != 0) {
		throw ServException("Initialization version error: ", WSAGetLastError());
	}
}

void TCPserver::createSockInfo(const char* ip, const char* port, addrinfo** sockInfo)
{
	addrinfo addrInfo;
	ZeroMemory(&addrInfo, sizeof(addrInfo));
	addrInfo.ai_family = AF_INET;
	addrInfo.ai_socktype = SOCK_STREAM;
	addrInfo.ai_protocol = IPPROTO_TCP;
	errState = getaddrinfo(ip, port, &addrInfo, sockInfo);
	if (errState != 0) {
		throw ServException("Server: Error getting address information: ", WSAGetLastError());
	}
}

void TCPserver::createNewSocket(SOCKET& new_socket, addrinfo* sockInfo)
{
	new_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (new_socket == INVALID_SOCKET) {
		throw ServException("Server: Socket initialization error: ", WSAGetLastError());
	}
}

void TCPserver::bindSocket()
{
	errState = bind(lis_socket, lisSockInfo->ai_addr, lisSockInfo->ai_addrlen);
	if (errState != 0) {
		throw ServException("Server: Binding error: ", WSAGetLastError());
	}
}

void TCPserver::listenState()
{
	errState = listen(lis_socket, SOMAXCONN);
	if (errState != 0) {
		throw ServException("Server: Listening error: ", WSAGetLastError());
	}
	writeLog("Server in listening state...");
}
