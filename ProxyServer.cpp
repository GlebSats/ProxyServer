#include "ProxyServer.h"

void proxyServer(ProxyConnection& client, ProxyConnection& targetServer, HANDLE* stopEvent) {

	client.stopEvent = stopEvent;
	targetServer.stopEvent = stopEvent;

	try
	{
		client.Initialization();
		targetServer.Initialization();

		while (WaitForSingleObject(stopEvent, 0) != WAIT_OBJECT_0) {

			try
			{
				client.Connection();
				targetServer.Connection();

				//threads client a server handlers
				std::thread cH([&]() {
					client.Handler();
					});
				cH.detach();

				std::thread sH([&]() {
					targetServer.Handler();
					});
				sH.detach();

				while (true)
				{
					HANDLE eventArr[7] = { *stopEvent, client.disconnect, targetServer.disconnect, client.readySend, client.dataToSend, targetServer.readySend, targetServer.dataToSend };
					int eventResult = WaitForMultipleObjects(7, eventArr, FALSE, INFINITE);

					if (eventResult == WAIT_FAILED) {
						client.closeConnection();
						targetServer.closeConnection();
						writeLog("Error while waiting for events: ", GetLastError());
						break;
					}

					if (eventResult == WAIT_OBJECT_0) {
						client.closeConnection();
						targetServer.closeConnection();
						writeLog("Connection has been severed: ", GetLastError());
						break;
					}

					if (WaitForSingleObject(eventArr[1], 0) == WAIT_OBJECT_0) {
						SetEvent(targetServer.disconnect);
						client.closeConnection();
						targetServer.closeConnection();
						break;
					}

					if (WaitForSingleObject(eventArr[2], 0) == WAIT_OBJECT_0) {
						SetEvent(client.disconnect);
						targetServer.closeConnection();
						client.closeConnection();
						break;
					}

					if (WaitForSingleObject(eventArr[3], 0) == WAIT_OBJECT_0) {
						client.receiveData();
						ResetEvent(client.readySend);
					}

					if (WaitForSingleObject(eventArr[4], 0) == WAIT_OBJECT_0) {
						int send_data = targetServer.sendData(client.receiveBuffer + client.indexForRecData, client.dataInReceiveBuffer);
						client.dataInReceiveBuffer -= send_data;
						client.indexForRecData += send_data;
					}

					if (WaitForSingleObject(eventArr[5], 0) == WAIT_OBJECT_0) {
						targetServer.receiveData();
						ResetEvent(targetServer.readySend);
					}

					if (WaitForSingleObject(eventArr[6], 0) == WAIT_OBJECT_0) {
						int send_data = client.sendData(targetServer.receiveBuffer + targetServer.indexForRecData, targetServer.dataInReceiveBuffer);
						targetServer.dataInReceiveBuffer -= send_data;
						targetServer.indexForRecData += send_data;
					}

					if (client.dataInReceiveBuffer != 0) {
						SetEvent(client.dataToSend);
					}
					else {
						ResetEvent(client.dataToSend);
					}

					if (targetServer.dataInReceiveBuffer != 0) {
						SetEvent(targetServer.dataToSend);
					}
					else {
						ResetEvent(targetServer.dataToSend);
					}
				}
			}
			catch (const ServException& ex)
			{
				writeLog(ex.GetErrorType(), ex.GetErrorCode());
			}
		}
	}
	catch (const ServException& ex)
	{
		writeLog(ex.GetErrorType(), ex.GetErrorCode());
	}
}
