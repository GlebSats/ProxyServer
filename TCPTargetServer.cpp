#include "TCPTargetServer.h"

TCPTargetServer::TCPTargetServer(const char* serverIP, const char* serverPort) :
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

		readyRecv = false;

		if (WSAEventSelect(server_socket, targetServerReadySend, FD_READ | FD_CLOSE | FD_WRITE) != 0) {
			closeConnection();
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
