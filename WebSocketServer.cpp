#include "WebSocketServer.h"

WebSocketServer::WebSocketServer(LPCWSTR serverIP, INTERNET_PORT serverPort):
	serverIP(serverIP),
	serverPort(serverPort),
	SendResponseStatus(FALSE),
	ReceiveResponseStatus(FALSE),
	serverSendResponse(NULL),
	SessionHandle(NULL),
	ConnectionHandle(NULL),
	RequestHandle(NULL),
	WebSocketHandle(NULL)
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
	CloseHandle(serverSendResponse);
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
