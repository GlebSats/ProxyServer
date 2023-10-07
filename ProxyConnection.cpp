#include "ProxyConnection.h"

ProxyConnection::ProxyConnection() :
	stopEvent(nullptr),
	disconnect(NULL),
	readySend(NULL),
	readyReceive(NULL),
	dataToSend(NULL),
	endOfWaiting(NULL),
	dataInReceiveBuffer(0),
	indexForRecData(0),
	waitingResult(false)
{
	ZeroMemory(&receiveBuffer, sizeof(receiveBuffer));
}

ProxyConnection::~ProxyConnection()
{
}

void ProxyConnection::subtractData(const int send_data)
{
	dataInReceiveBuffer -= send_data;
	indexForRecData += send_data;

	if (dataInReceiveBuffer != 0) {
		SetEvent(dataToSend);
	}
	else {
		ResetEvent(dataToSend);
	}
}

void ProxyConnection::WaitingToSend(){
	HANDLE eventArr[3] = { *stopEvent, disconnect, readySend };
	int eventResult = WaitForMultipleObjects(3, eventArr, FALSE, INFINITE);

	if (eventResult == WAIT_FAILED) {
		writeLog("Error while waiting for events: ", GetLastError());
		SetEvent(disconnect);
		waitingResult = false;
	}
	else if ((eventResult == WAIT_OBJECT_0) || (eventResult == WAIT_OBJECT_0 + 1)) {
		waitingResult = false;
	}
	else {
		waitingResult = true;
		SetEvent(endOfWaiting);
	}
}

void ProxyConnection::eventsCreation()
{
	disconnect = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (disconnect == NULL) {
		throw ServException("Create Event failed: ", GetLastError());
	}

	readySend = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (readySend == NULL) {
		throw ServException("Create Event failed: ", GetLastError());
	}

	readyReceive = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (readyReceive == NULL) {
		throw ServException("Create Event failed: ", GetLastError());
	}

	dataToSend = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (dataToSend == NULL) {
		throw ServException("Create Event failed: ", GetLastError());
	}

	endOfWaiting = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (endOfWaiting == NULL) {
		throw ServException("Create Event failed: ", GetLastError());
	}
}
void ProxyConnection::eventsDeleting()
{
	if (dataToSend != NULL) {
		CloseHandle(dataToSend);
		dataToSend = NULL;
	}
	if (readySend != NULL) {
		CloseHandle(readySend);
		readySend = NULL;
	}
	if (readyReceive != NULL) {
		CloseHandle(readyReceive);
		readyReceive = NULL;
	}
	if (disconnect != NULL) {
		CloseHandle(disconnect);
		disconnect = NULL;
	}
	if (endOfWaiting != NULL) {
		CloseHandle(endOfWaiting);
		endOfWaiting = NULL;
	}
}