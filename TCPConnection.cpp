TCPConnection::TCPConnection(const char* listeningPort, const char* serverIP, const char* serverPort): StartServer(listeningPort)
{
	this->serverIP = serverIP;
	this->serverPort = serverPort;
	serverSockInfo = nullptr;
	server_socket = INVALID_SOCKET;
	bufToClientHasData = WSA_INVALID_EVENT;
	bufToServHasData = WSA_INVALID_EVENT;
	clientReadySend = WSA_INVALID_EVENT;
	serverReadySend = WSA_INVALID_EVENT;
	errState = 0;
}

TCPConnection::~TCPConnection()
{
}

void TCPConnection::serverHandler()
{
	acceptConnection();
	createSockInfo(serverIP, serverPort, &serverSockInfo);
	createNewSocket(server_socket, serverSockInfo);
	connectToServer();
	sockCommunication();
}

void TCPConnection::connectToServer()
{
	errState = connect(server_socket, serverSockInfo->ai_addr, serverSockInfo->ai_addrlen);
	if (errState != 0) {
		throw ServException("Connection to Web Server failed: ", WSAGetLastError());
	}
	writeLog("Connection to Server successful");
}

void TCPConnection::sockCommunication()
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
			throw ServException("Connection has been severed: ", WSAGetLastError());
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

		if (clientEvents.lNetworkEvents & FD_CLOSE) { // poslat zbýtek dat?
			closeConnection();
			throw ServException("Connection with the client has been severed: ", WSAGetLastError());
		}

		if (serverEvents.lNetworkEvents & FD_CLOSE) { // poslat zbýtek dat?
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

void TCPConnection::createSocketEvents()
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

	if (WSAEventSelect(client_socket, clientReadySend, FD_READ | FD_CLOSE) != 0) {
		closeConnection();
		throw ServException("WSAEventSelect function failed: ", WSAGetLastError());
	}

	if (WSAEventSelect(server_socket, serverReadySend, FD_READ | FD_CLOSE) != 0) {
		closeConnection();
		throw ServException("WSAEventSelect function failed: ", WSAGetLastError());
	}
}

void TCPConnection::closeConnection()
{
	WSACloseEvent(bufToClientHasData);
	WSACloseEvent(bufToServHasData);
	WSACloseEvent(clientReadySend);
	WSACloseEvent(serverReadySend);
	shutdown(server_socket, SD_BOTH);
	shutdown(client_socket, SD_BOTH);
	closesocket(server_socket);
	closesocket(client_socket);
}
