WebSocketConnection::WebSocketConnection(const char* listeningPort, LPCWSTR serverIP, INTERNET_PORT serverPort) : StartServer(listeningPort)
{
	this->serverIP = serverIP;
	this->serverPort = serverPort;
	SendResponseStatus = FALSE;
	ResponseStatus = FALSE;
	serverSendResponse = NULL;
	SessionHandle = NULL;
	ConnectionHandle = NULL;
	RequestHandle = NULL;
	WebSocketHandle = NULL;
}

WebSocketConnection::~WebSocketConnection()
{
	stopServer();
}

void WebSocketConnection::serverInitialization()
{
	StartServer::serverInitialization();
	SessionHandle = WinHttpOpen(L"ProxyServer", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (SessionHandle == NULL) {
		throw ServException("WinHTTP initialization error: ", GetLastError());
	}
}

void WebSocketConnection::serverHandler()
{
	acceptConnection();
	connectToServer();
}

void WebSocketConnection::WaitResponseFromServer()
{
	SendResponseStatus = WinHttpSendRequest(RequestHandle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0);
	ResponseStatus = WinHttpReceiveResponse(RequestHandle, NULL);
	SetEvent(serverSendResponse);
}

void WebSocketConnection::connectToServer() {
	serverSendResponse = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (serverSendResponse == NULL) {
		throw ServException("Create Event Error: ", GetLastError());
	}

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

	std::thread WaitingForResponse([&]() {
		WaitResponseFromServer();
	});
	WaitingForResponse.detach();
	
	HANDLE eventArr[2] = { *serviceStopEvent, serverSendResponse };
	int eventResult = WaitForMultipleObjects(2, eventArr, FALSE, 5000);
	if (eventResult == WAIT_FAILED) {
		CloseHandle(serverSendResponse);
		throw ServException("Error while waiting for events: ", GetLastError());
	}

	if (eventResult == WAIT_TIMEOUT) {
		CloseHandle(serverSendResponse);
		throw ServException("Response timeout expired: ");
	}
	
	if (eventResult == WAIT_OBJECT_0) {
		CloseHandle(serverSendResponse);
		throw ServException("Service stopped by SCM: ");
	}

	if (eventResult == WAIT_OBJECT_0 + 1) {
		if (SendResponseStatus == FALSE) {
			throw ServException("Sending request to server error: ", GetLastError());
		}

		if (ResponseStatus == FALSE) {
			throw ServException("Request response error: ", GetLastError());
		}

		WebSocketHandle = WinHttpWebSocketCompleteUpgrade(RequestHandle, NULL);
		if (WebSocketHandle == NULL) {
			throw ServException("Handshake completion error: ", GetLastError());
		}

		writeLog("Connection to Web Socket server successful");
	}
}

void WebSocketConnection::stopServer()
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
