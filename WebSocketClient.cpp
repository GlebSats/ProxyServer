#include "WebSocketClient.h"

WebSocketClient::WebSocketClient(LPCWSTR serverIP, INTERNET_PORT serverPort) :
	pDataToSend(NULL),
	sizeDataToSend(0),
	trySend(NULL),
	receivedData(0),
	serverIP(serverIP),
	serverPort(serverPort),
	SendResponseStatus(FALSE),
	ReceiveResponseStatus(FALSE),
	serverSendResponse(NULL),
	bufferEmpty(NULL),
	SessionHandle(NULL),
	ConnectionHandle(NULL),
	RequestHandle(NULL),
	WebSocketHandle(NULL),
	sentBytes(0),
	errorCode(0),
	errState(0)
{
}

WebSocketClient::~WebSocketClient()
{
	if (SessionHandle != NULL) {
		WinHttpCloseHandle(SessionHandle);
	}
}

void WebSocketClient::Initialization()
{
	SessionHandle = WinHttpOpen(L"ProxyConnection", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (SessionHandle == NULL) {
		throw ServException("Client: WinHTTP initialization error: ", GetLastError());
	}
}

void WebSocketClient::WaitResponseFromServer()
{
	LPCWSTR addHeader = L"Sec-WebSocket-Protocol: mqttv3.1\r\n";
	SendResponseStatus = WinHttpSendRequest(RequestHandle, addHeader, -1L, NULL, 0, 0, WINHTTP_FLAG_SECURE);
	ReceiveResponseStatus = WinHttpReceiveResponse(RequestHandle, NULL);
	SetEvent(serverSendResponse);
}

void WebSocketClient::Connection()
{
	serverSendResponse = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (serverSendResponse == NULL) {
		throw ServException("Client: Create Event Error: ", GetLastError());
	}

	ConnectionHandle = WinHttpConnect(SessionHandle, serverIP, serverPort, 0);
	if (ConnectionHandle == NULL) {
		throw ServException("Client: Initialization error: ", GetLastError());
	}

	RequestHandle = WinHttpOpenRequest(ConnectionHandle, L"GET", NULL, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
	if (RequestHandle == NULL) {
		throw ServException("Client: HTTP request creation error: ", GetLastError());
	}

#pragma warning(suppress : 6387)
	if (WinHttpSetOption(RequestHandle, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0) == FALSE) {
		throw ServException("Client: Setting Internet option error: ", GetLastError());
	}

	std::thread WaitingForResponse([&]() {
		WaitResponseFromServer();
		});
	WaitingForResponse.detach();

	HANDLE eventArr[2] = { *stopEvent, serverSendResponse };
	int eventResult = WaitForMultipleObjects(2, eventArr, FALSE, 30000);
	if (eventResult == WAIT_FAILED) {
		CloseHandle(serverSendResponse);
		throw ServException("Client: Error while waiting for events: ", GetLastError());
	}

	if (eventResult == WAIT_TIMEOUT) {
		CloseHandle(serverSendResponse);
		throw ServException("Client: Response timeout expired: ");
	}

	if (eventResult == WAIT_OBJECT_0) {
		CloseHandle(serverSendResponse);
		throw ServException("Service stopped by SCM: ");
	}

	if (eventResult == WAIT_OBJECT_0 + 1) {
		if (SendResponseStatus == FALSE) {
			throw ServException("Client: Sending request to server error: ", GetLastError());
		}

		if (ReceiveResponseStatus == FALSE) {
			throw ServException("Client: Request response error: ", GetLastError());
		}

		WebSocketHandle = WinHttpWebSocketCompleteUpgrade(RequestHandle, NULL);
		if (WebSocketHandle == NULL) {
			throw ServException("Client: Handshake completion error: ", GetLastError());
		}

		writeLog("Client: Connection to Web Socket server successful");
	}

	eventsCreation();
	preparation();
}

void WebSocketClient::Handler()
{
	bufferEmpty = CreateEvent(NULL, TRUE, TRUE, NULL);
	if (bufferEmpty == NULL) {
		throw ServException("Client: Create Event Error: ", GetLastError());
	}

	HANDLE eventArr[3] = { *stopEvent, disconnect, bufferEmpty };

	while (true) {
		int eventResult = WaitForMultipleObjects(3, eventArr, FALSE, INFINITE);

		if (eventResult == WAIT_FAILED) {
			writeLog("Client: Error while waiting for events: ", GetLastError());
			SetEvent(disconnect);
			return;
		}

		if ((eventResult == WAIT_OBJECT_0) || (eventResult == WAIT_OBJECT_0 + 1)) {
			return;
		}

		errState = WinHttpWebSocketReceive(WebSocketHandle, receiveBuffer, BUFFER_SIZE, &receivedData, &bufferType);
		if (errState != NO_ERROR) {
			writeLog("Client: Web Socket Receive Error : ", errState);
			SetEvent(disconnect);
			return;
		}

		if (bufferType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
			writeLog("Client: Connection with the server has been severed: ", GetLastError());
			SetEvent(disconnect);
			return;
		}

		ResetEvent(bufferEmpty);
		SetEvent(readyReceive);
	}
}

void WebSocketClient::trySendData()
{
	HANDLE eventArr[3] = { *stopEvent, disconnect, trySend };

	while (true) {

		int eventResult = WaitForMultipleObjects(3, eventArr, FALSE, INFINITE);

		if (eventResult == WAIT_FAILED) {
			writeLog("Client: Error while waiting for events: ", GetLastError());
			SetEvent(disconnect);
			return;
		}

		if ((eventResult == WAIT_OBJECT_0) || (eventResult == WAIT_OBJECT_0 + 1)) {
			return;
		}

		if (sizeDataToSend == BUFFER_SIZE) {
			errState = WinHttpWebSocketSend(WebSocketHandle, WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE, (PVOID)pDataToSend, sizeDataToSend);
			if (errState != NO_ERROR) {
				sentBytes = -1;
				errorCode = errState;
				SetEvent(readySend);
				return;
			}
		}
		else {
			errState = WinHttpWebSocketSend(WebSocketHandle, WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, (PVOID)pDataToSend, sizeDataToSend);
			if (errState != NO_ERROR) {
				sentBytes = -1;
				errorCode = errState;
				SetEvent(readySend);
				return;
			}
		}
		//
		writeLogHex("Target< S:" + std::to_string(sizeDataToSend) + " D: ", pDataToSend[0], pDataToSend[1], pDataToSend[2]);
		//
		sentBytes = sizeDataToSend;
		ResetEvent(trySend);
		SetEvent(readySend);
	}
}

void WebSocketClient::preparation()
{
	trySend = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (trySend == NULL) {
		throw ServException("Client: Create Event Error: ", GetLastError());
	}

	TS = std::thread(&WebSocketClient::trySendData, this);

	sentBytes = 0;
}

int WebSocketClient::sendData(const char* pData, const int length)
{
	if (sentBytes > 0) {
		sentBytes = 0;
		return length;
	}

	if (sentBytes == -1) {
		writeLog("Client: Error sending message to websocket server: ", errorCode);
		SetEvent(disconnect);
		return -1;
	}

	pDataToSend = pData;
	sizeDataToSend = length;
	SetEvent(trySend);

	HANDLE eventArr[1] = { readySend };
	int eventResult = WaitForMultipleObjects(1, eventArr, FALSE, 1000);

	if (eventResult == WAIT_FAILED) {
		writeLog("Client: Error while waiting for events: ", GetLastError());
		SetEvent(disconnect);
		return -1;
	}

	if (eventResult == WAIT_TIMEOUT) {
		ResetEvent(readySend);
		return 0;
	}

	sentBytes = 0;
	return length;
}

void WebSocketClient::receiveData()
{
	if (receivedData != 0) {
		dataInReceiveBuffer = receivedData;
		indexForRecData = 0;
		SetEvent(dataToSend);
		ResetEvent(readyReceive);
	}
}

void WebSocketClient::subtractData(const int send_data)
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

void WebSocketClient::closeConnection()
{
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

	if (bufferEmpty != NULL) {
		CloseHandle(bufferEmpty);
		bufferEmpty = NULL;
	}

	if (trySend != NULL) {
		CloseHandle(trySend);
		trySend = NULL;
	}

	TS.join();
	eventsDeleting();
}
