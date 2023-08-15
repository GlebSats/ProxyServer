#include "TCPClient.h"

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

void TCPClient::Initialization()
{
	initSockets();
	createSockInfo("127.0.0.1", listeningPort, &lisSockInfo);
	createNewSocket(lis_socket, lisSockInfo);
	bindSocket();
	listenState();
}

void TCPClient::Connection()
{
	eventsCreation();
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
	WSANETWORKEVENTS targetServerEvents;

	targetServerReadySend = WSACreateEvent();
	if (targetServerReadySend == WSA_INVALID_EVENT) {
		writeLog("Create WSA Event failed: ", WSAGetLastError());
		SetEvent(disconnect);
		return;
	}

	if (WSAEventSelect(server_socket, targetServerReadySend, FD_READ | FD_CLOSE) != 0) {
		writeLog("WSAEventSelect function failed: ", WSAGetLastError());
		SetEvent(disconnect);
		return;
	}

	readyRecv = true;

	HANDLE eventArr[3] = { *stopEvent, disconnect, targetServerReadySend };
	while (true) {

		int eventResult = WSAWaitForMultipleEvents(3, eventArr, FALSE, INFINITE, FALSE);
		if (eventResult == WSA_WAIT_FAILED) {
			writeLog("Error while waiting for events: ", WSAGetLastError());
			SetEvent(disconnect);
			return;
		}

		if ((eventResult == WSA_WAIT_EVENT_0) || (eventResult == WSA_WAIT_EVENT_0 + 1)) {
			return;
		}

		errState = WSAEnumNetworkEvents(server_socket, targetServerReadySend, &targetServerEvents);
		if (errState == SOCKET_ERROR) {
			writeLog("Server: Error while getting information about events: ", WSAGetLastError());
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

		if (targetServerEvents.lNetworkEvents & FD_WRITE) {
			readyRecv = true;
			if (WSAEventSelect(server_socket, targetServerReadySend, FD_READ | FD_CLOSE) != 0) {
				writeLog("WSAEventSelect function failed: ", WSAGetLastError());
				SetEvent(disconnect);
				return;
			}
		}

	}
}

int TCPClient::sendData(const char* pData, int length)
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

		readyRecv = false;

		if (WSAEventSelect(server_socket, targetServerReadySend, FD_READ | FD_CLOSE | FD_WRITE) != 0) {
			closeConnection();
			throw ServException("WSAEventSelect function failed: ", WSAGetLastError());
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
	eventsDeleting();
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
