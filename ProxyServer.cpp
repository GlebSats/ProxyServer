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
						writeLog("Connection has been severed");
						break;
					}

					if (eventResult == WAIT_OBJECT_0 + 1) { 
						if (client.dataInReceiveBuffer != 0) {
							// poslat data
						}

						client.closeConnection();
						targetServer.closeConnection();
						break;
					}

					if (eventResult == WAIT_OBJECT_0 + 2) { 
						if (targetServer.dataInReceiveBuffer != 0) {
							// poslat data
						}

						targetServer.closeConnection();
						client.closeConnection();
						break;
					}

					if (eventResult == WAIT_OBJECT_0 + 3) {
						int send_data = targetServer.sendData(client.receiveBuffer + client.indexForRecData, client.dataInReceiveBuffer);
						if (send_data != 0) {
							client.subtractData(send_data);
						}
						else {
							ResetEvent(client.dataToSend);
							std::thread CW([&]() {
								if (targetServer.WaitingToSend() == 0) {
									SetEvent(client.dataToSend);
								}
								});
							CW.detach();
						}
					}

					if (eventResult == WAIT_OBJECT_0 + 4) {
						int send_data = client.sendData(targetServer.receiveBuffer + targetServer.indexForRecData, targetServer.dataInReceiveBuffer);
						if (send_data != 0) {
							targetServer.subtractData(send_data);
						}
						else {
							ResetEvent(targetServer.dataToSend);
							std::thread SW([&]() {
								if (client.WaitingToSend() == 0) {
									SetEvent(targetServer.dataToSend);
								}
								});
							SW.detach();
						}
					}

					if (eventResult == WAIT_OBJECT_0 + 5) {
						client.receiveData();
					}

					if (eventResult == WAIT_OBJECT_0 + 6) {
						targetServer.receiveData();
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
