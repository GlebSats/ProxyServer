#include "ProxyConnection.h"

ProxyConnection::ProxyConnection() :
	stopEvent(nullptr),
	disconnect(NULL),
	readySend(NULL),
	dataToSend(NULL),
	dataInReceiveBuffer(0),
	indexForRecData(0),
	readyRecv(false)
{
	ZeroMemory(&receiveBuffer, sizeof(receiveBuffer));
}

ProxyConnection::~ProxyConnection()
{
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

	dataToSend = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (dataToSend == NULL) {
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
	if (disconnect != NULL) {
		CloseHandle(disconnect);
		disconnect = NULL;
	}
}
