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
					HANDLE eventArr[7] = { *stopEvent, client.disconnect, targetServer.disconnect, client.dataToSend, targetServer.dataToSend, client.readySend, targetServer.readySend  };
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

					if (eventResult == WAIT_OBJECT_0 + 1) {
						SetEvent(targetServer.disconnect);
						client.closeConnection();
						targetServer.closeConnection();
						break;
					}

					if (eventResult == WAIT_OBJECT_0 + 2) {
						SetEvent(client.disconnect);
						targetServer.closeConnection();
						client.closeConnection();
						break;
					}

					if (eventResult == WAIT_OBJECT_0 + 3) {
						int send_data = targetServer.sendData(client.receiveBuffer + client.indexForRecData, client.dataInReceiveBuffer);
						client.dataInReceiveBuffer -= send_data;
						client.indexForRecData += send_data;
					}

					if (eventResult == WAIT_OBJECT_0 + 4) {
						int send_data = client.sendData(targetServer.receiveBuffer + targetServer.indexForRecData, targetServer.dataInReceiveBuffer);
						targetServer.dataInReceiveBuffer -= send_data;
						targetServer.indexForRecData += send_data;
					}

					if (eventResult == WAIT_OBJECT_0 + 5) {
						client.receiveData();
					}

					if (eventResult == WAIT_OBJECT_0 + 6) {
						targetServer.receiveData();
					}

					if ((client.dataInReceiveBuffer != 0) && targetServer.readyRecv) { //ready receive true false
						SetEvent(client.dataToSend);
					}
					else {
						ResetEvent(client.dataToSend);
					}

					if (targetServer.dataInReceiveBuffer != 0 && client.readyRecv) { //ready receive true false
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
				client.closeConnection();
				targetServer.closeConnection();
			}
		}
	}
	catch (const ServException& ex)
	{
		writeLog(ex.GetErrorType(), ex.GetErrorCode());
	}
}
