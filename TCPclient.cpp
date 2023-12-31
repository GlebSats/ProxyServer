﻿#include "TCPclient.h"

TCPclient::TCPclient(const char* serverIP, const char* serverPort) :
	bufferEmpty(NULL),
	serverSockInfo(nullptr),
	server_socket(INVALID_SOCKET),
	serverEvent(WSA_INVALID_EVENT),
	errState(0)
{
	this->serverIP = std::string(serverIP);
	this->serverPort = std::string(serverPort);
}

TCPclient::~TCPclient()
{
	freeaddrinfo(serverSockInfo);

	if (server_socket != INVALID_SOCKET) {
		closesocket(server_socket);
	}

	writeLog("TCPclient memory freed");
}

void TCPclient::Connection()
{
	createSockInfo(serverIP.c_str(), serverPort.c_str(), &serverSockInfo);
	createNewSocket(server_socket, serverSockInfo);

	errState = connect(server_socket, serverSockInfo->ai_addr, serverSockInfo->ai_addrlen);
	if (errState != 0) {
		throw ServException("Client: Connection to Target Server failed: ", WSAGetLastError());
	}
	writeLog("Client: Connection to Target Server successful");
	eventsCreation();
}

void TCPclient::Handler()
{
	WSANETWORKEVENTS eventType;

	bufferEmpty = CreateEvent(NULL, TRUE, TRUE, NULL);
	if (bufferEmpty == NULL) {
		writeLog("Client: Create Event Error: ", GetLastError());
		SetEvent(disconnect);
		return;
	}

	serverEvent = WSACreateEvent();
	if (serverEvent == WSA_INVALID_EVENT) {
		writeLog("Client: Create WSA Event failed: ", WSAGetLastError());
		SetEvent(disconnect);
		return;
	}

	if (WSAEventSelect(server_socket, serverEvent, FD_READ | FD_CLOSE) != 0) {
		writeLog("Client: WSAEventSelect function failed: ", WSAGetLastError());
		SetEvent(disconnect);
		return;
	}

	HANDLE eventArr[3] = { *stopEvent, disconnect, serverEvent };
	HANDLE eventArr2[3] = { *stopEvent, disconnect, bufferEmpty };

	while (true) {

		int eventResult2 = WaitForMultipleObjects(3, eventArr2, FALSE, INFINITE);
		if (eventResult2 == WAIT_FAILED) {
			writeLog("Client: Error while waiting for events: ", GetLastError());
			SetEvent(disconnect);
			return;
		}

		if ((eventResult2 == WAIT_OBJECT_0) || (eventResult2 == WAIT_OBJECT_0 + 1)) {
			return;
		}

		int eventResult = WSAWaitForMultipleEvents(3, eventArr, FALSE, INFINITE, FALSE);
		if (eventResult == WSA_WAIT_FAILED) {
			writeLog("Client: Error while waiting for events: ", WSAGetLastError());
			SetEvent(disconnect);
			return;
		}

		if ((eventResult == WSA_WAIT_EVENT_0) || (eventResult == WSA_WAIT_EVENT_0 + 1)) {
			return;
		}

		errState = WSAEnumNetworkEvents(server_socket, serverEvent, &eventType);
		if (errState == SOCKET_ERROR) {
			writeLog("Client: Error while getting information about events: ", WSAGetLastError());
			SetEvent(disconnect);
			return;
		}

		if (eventType.lNetworkEvents & FD_CLOSE) {
			writeLog("Client: Connection with the Target Server has been severed: ", WSAGetLastError());
			SetEvent(disconnect);
			return;
		}

		if (eventType.lNetworkEvents & FD_READ) {
			SetEvent(readyReceive);
		}

		if (eventType.lNetworkEvents & FD_WRITE) {
			if (WSAEventSelect(server_socket, serverEvent, FD_READ | FD_CLOSE) != 0) {
				writeLog("Client: WSAEventSelect function failed: ", WSAGetLastError());
				SetEvent(disconnect);
				return;
			}
			SetEvent(readySend);
		}

	}
}

int TCPclient::sendData(const char* pData, const int length)
{
	int send_data = send(server_socket, pData, length, 0);
	if (send_data == SOCKET_ERROR) {
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			writeLog("Client: Connection with the server has been severed: ", WSAGetLastError());
			SetEvent(disconnect);
			return -1;
		}

		ResetEvent(readySend);

		if (WSAEventSelect(server_socket, serverEvent, FD_READ | FD_CLOSE | FD_WRITE) != 0) {
			writeLog("Client: WSAEventSelect function failed: ", WSAGetLastError());
			SetEvent(disconnect);
			return -1;
		}

		send_data = 0;
	}
	//
	writeLogHex("Target< S:" + std::to_string(send_data) + " D: ",pData[0] ,pData[1], pData[2]);
	//
	return send_data;
}

void TCPclient::receiveData()
{
	if (dataInReceiveBuffer != 0) {
		return;
	}

	int rec_data = recv(server_socket, receiveBuffer, BUFFER_SIZE, 0);
	if (rec_data == SOCKET_ERROR) {
		writeLog("Client: Connection with the server has been severed: ", WSAGetLastError());
		SetEvent(disconnect);
		return;
	}
	dataInReceiveBuffer = rec_data;
	indexForRecData = 0;

	if (dataInReceiveBuffer != 0) {
		SetEvent(dataToSend);
		ResetEvent(readyReceive);
		ResetEvent(bufferEmpty);
	}
}

void TCPclient::subtractData(const int send_data)
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

void TCPclient::closeConnection()
{
	SetEvent(disconnect);

	if (serverEvent != WSA_INVALID_EVENT) {
		WSACloseEvent(serverEvent);
		serverEvent = WSA_INVALID_EVENT;
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

void TCPclient::createSockInfo(const char* ip, const char* port, addrinfo** sockInfo)
{
	addrinfo addrInfo;
	ZeroMemory(&addrInfo, sizeof(addrInfo));
	addrInfo.ai_family = AF_INET;
	addrInfo.ai_socktype = SOCK_STREAM;
	addrInfo.ai_protocol = IPPROTO_TCP;
	errState = getaddrinfo(ip, port, &addrInfo, sockInfo);
	if (errState != 0) {
		throw ServException("Client: Error getting address information: ", WSAGetLastError());
	}
}

void TCPclient::createNewSocket(SOCKET& new_socket, addrinfo* sockInfo)
{
	new_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (new_socket == INVALID_SOCKET) {
		throw ServException("Client: Socket initialization error: ", WSAGetLastError());
	}
}
