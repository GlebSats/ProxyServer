#include "TCPTargetServer.h"

TCPTargetServer::TCPTargetServer(const char* serverIP, const char* serverPort) :
	bufferEmpty(NULL),
	serverIP(serverIP),
	serverPort(serverPort),
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

void TCPTargetServer::Connection()
{
	eventsCreation();
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

	bufferEmpty = CreateEvent(NULL, TRUE, TRUE, NULL);
	if (bufferEmpty == NULL) {
		writeLog("Create Event Error: ", GetLastError());
		SetEvent(disconnect);
		return;
	}

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

	HANDLE eventArr[3] = { *stopEvent, disconnect, targetServerReadySend };
	HANDLE eventArr2[3] = { *stopEvent, disconnect, bufferEmpty };

	while (true) {

		int eventResult2 = WaitForMultipleObjects(3, eventArr2, FALSE, INFINITE);
		if (eventResult2 == WAIT_FAILED) {
			writeLog("Error while waiting for events: ", GetLastError());
			SetEvent(disconnect);
			return;
		}

		if ((eventResult2 == WAIT_OBJECT_0) || (eventResult2 == WAIT_OBJECT_0 + 1)) {
			return;
		}

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

		if (targetServerEvents.lNetworkEvents & FD_CLOSE) { 
			writeLog("Connection with the target server has been severed: ", WSAGetLastError());
			SetEvent(disconnect);
			return;
		}

		if ((targetServerEvents.lNetworkEvents & FD_READ) && (dataInReceiveBuffer == 0)) {
			SetEvent(readySend);
		}

		if (targetServerEvents.lNetworkEvents & FD_WRITE) {
			if (WSAEventSelect(server_socket, targetServerReadySend, FD_READ | FD_CLOSE) != 0) {
				writeLog("WSAEventSelect function failed: ", WSAGetLastError());
				SetEvent(disconnect);
				return;
			}
			SetEvent(readyReceive);
		}

	}
}

int TCPTargetServer::sendData(const char* pData, const int length)
{
	int send_data = send(server_socket, pData, length, 0);
	if (send_data == SOCKET_ERROR) {
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			throw ServException("Connection with the server has been severed: ", WSAGetLastError());
		}

		if (WSAEventSelect(server_socket, targetServerReadySend, FD_READ | FD_CLOSE | FD_WRITE) != 0) {
			throw ServException("WSAEventSelect function failed: ", WSAGetLastError());
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
		throw ServException("Connection with the server has been severed: ", WSAGetLastError());
	}
	dataInReceiveBuffer = rec_data;
	indexForRecData = 0;

	if (dataInReceiveBuffer != 0) {
		SetEvent(dataToSend);
		ResetEvent(readySend);
		ResetEvent(bufferEmpty);
	}
}

void TCPTargetServer::subtractData(const int send_data)
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

void TCPTargetServer::closeConnection()
{ 
	SetEvent(disconnect);

	if (targetServerReadySend != WSA_INVALID_EVENT) {
		WSACloseEvent(targetServerReadySend);
		targetServerReadySend = WSA_INVALID_EVENT;
	}

	if (bufferEmpty != NULL) {
		CloseHandle(bufferEmpty);
		bufferEmpty = NULL;
	}

	if (server_socket != INVALID_SOCKET) {
		shutdown(server_socket, SD_BOTH);
		closesocket(server_socket);
		server_socket = INVALID_SOCKET;
	}

	eventsDeleting();
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
