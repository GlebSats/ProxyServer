#include "WebSocketServer.h"

WebSocketServer::WebSocketServer(LPCWSTR serverIP, INTERNET_PORT serverPort) :
	dataInTempBuffer(0),
	serverIP(serverIP),
	serverPort(serverPort),
	SendResponseStatus(FALSE),
	ReceiveResponseStatus(FALSE),
	serverSendResponse(NULL),
	tempBufferEmpty(NULL),
	SessionHandle(NULL),
	ConnectionHandle(NULL),
	RequestHandle(NULL),
	WebSocketHandle(NULL),
	send_data(0),
	errorCode(0),
	errState(0)
{
}

WebSocketServer::~WebSocketServer()
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
	if (serverSendResponse != NULL) {
		CloseHandle(serverSendResponse);
	}
	if (tempBufferEmpty != NULL) {
		CloseHandle(tempBufferEmpty);
	}
}

void WebSocketServer::Initialization()
{
	SessionHandle = WinHttpOpen(L"ProxyConnection", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (SessionHandle == NULL) {
		throw ServException("WinHTTP initialization error: ", GetLastError());
	}
}

void WebSocketServer::WaitResponseFromServer()
{
	SendResponseStatus = WinHttpSendRequest(RequestHandle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0);
	ReceiveResponseStatus = WinHttpReceiveResponse(RequestHandle, NULL);
	SetEvent(serverSendResponse);
}

void WebSocketServer::trySendData(const char* pData, const int length)
{
	if (length == BUFFER_SIZE) {
		errState = WinHttpWebSocketSend(WebSocketHandle, WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE, (PVOID)pData, length);
		if (errState != NO_ERROR) {
			send_data = -1;
			errorCode = errState;
			SetEvent(readyReceive);
			return;
		}
	}
	else {
		errState = WinHttpWebSocketSend(WebSocketHandle, WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, (PVOID)pData, length);
		if (errState != NO_ERROR) {
			send_data = -1;
			errorCode = errState;
			SetEvent(readyReceive);
			return;
		}
	}

	send_data = length;
	SetEvent(readyReceive);
}

void WebSocketServer::Connection()
{
	eventsCreation();

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

#pragma warning(suppress : 6387)
	if (WinHttpSetOption(RequestHandle, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0) == FALSE) {
		throw ServException("Setting Internet option error: ", GetLastError());
	}

	std::thread WaitingForResponse([&]() {
		WaitResponseFromServer();
		});
	WaitingForResponse.detach();

	HANDLE eventArr[2] = { *stopEvent, serverSendResponse };
	int eventResult = WaitForMultipleObjects(2, eventArr, FALSE, 30000);
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

		if (ReceiveResponseStatus == FALSE) {
			throw ServException("Request response error: ", GetLastError());
		}

		WebSocketHandle = WinHttpWebSocketCompleteUpgrade(RequestHandle, NULL);
		if (WebSocketHandle == NULL) {
			throw ServException("Handshake completion error: ", GetLastError());
		}

		writeLog("Connection to Web Socket server successful");
	}
}

void WebSocketServer::Handler()
{
	tempBufferEmpty = CreateEvent(NULL, TRUE, TRUE, NULL);
	if (tempBufferEmpty == NULL) {
		throw ServException("Create Event Error: ", GetLastError());
	}

	HANDLE eventArr[3] = { *stopEvent, disconnect, tempBufferEmpty };

	while (true) {
		int eventResult = WaitForMultipleObjects(3, eventArr, FALSE, INFINITE);

		if (eventResult == WAIT_FAILED) {
			writeLog("Error while waiting for events: ", GetLastError());
			SetEvent(disconnect);
			return;
		}

		if ((eventResult == WAIT_OBJECT_0) || (eventResult == WAIT_OBJECT_0 + 1)) {
			return;
		}

		errState = WinHttpWebSocketReceive(WebSocketHandle, tempReceiveBuffer, BUFFER_SIZE, &dataInTempBuffer, &tempBufferType);
		if (errState != NO_ERROR) {
			writeLog("Web Socket Receive Error : ", errState);
			SetEvent(disconnect);
			return;
		}

		if (tempBufferType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
			writeLog("Connection with the server has been severed: ", GetLastError());
			SetEvent(disconnect);
			return;
		}

		SetEvent(readySend);
		ResetEvent(tempBufferEmpty);
	}
}

int WebSocketServer::sendData(const char* pData, const int length)
{
	if (send_data > 0) {
		send_data = 0;
		return length;
	}

	if (send_data == -1) {
		throw ServException("Error sending message to websocket server: ", errorCode);
	}

	std::thread SW([&]() {
		trySendData(pData, length);
		});
	SW.detach();

	HANDLE eventArr[1] = { readyReceive };
	int eventResult = WaitForMultipleObjects(1, eventArr, FALSE, 1000);

	if (eventResult == WAIT_FAILED) {
		throw ServException("Error while waiting for events: ", GetLastError());
	}

	if (eventResult == WAIT_TIMEOUT) {
		return 0;
	}

	return length;
}

void WebSocketServer::receiveData()
{
	if (dataInReceiveBuffer == 0) {
		memcpy(receiveBuffer, tempReceiveBuffer, dataInTempBuffer);
		dataInReceiveBuffer = dataInTempBuffer;
		indexForRecData = 0;
		dataInTempBuffer = 0;
		SetEvent(tempBufferEmpty);
		ResetEvent(readySend);
	}
}

void WebSocketServer::closeConnection()
{
	send_data = 0;

	SetEvent(disconnect);

	if (WebSocketHandle != NULL) {
		WinHttpWebSocketClose(WebSocketHandle, WINHTTP_WEB_SOCKET_EMPTY_CLOSE_STATUS, NULL, 0);
		WinHttpCloseHandle(WebSocketHandle);
		WebSocketHandle = NULL;
	}

	if (RequestHandle != NULL) {
		WinHttpCloseHandle(RequestHandle);
		RequestHandle = NULL;
	}

	if (ConnectionHandle != NULL) {
		WinHttpCloseHandle(ConnectionHandle);
		ConnectionHandle = NULL;
	}

	if (serverSendResponse != NULL) {
		CloseHandle(serverSendResponse);
		serverSendResponse = NULL;
	}

	if (tempBufferEmpty != NULL) {
		CloseHandle(tempBufferEmpty);
		tempBufferEmpty = NULL;
	}

	eventsDeleting();
}
