include "ProxyServer.h"

void proxyServer(ProxyConnection& server, ProxyConnection& client, HANDLE* stopEvent) {

	server.stopEvent = stopEvent;
	client.stopEvent = stopEvent;
	try
	{
		server.Initialization();
		client.Initialization();

		while (WaitForSingleObject(stopEvent, 0) != WAIT_OBJECT_0) {

			try
			{
				server.Connection();
				client.Connection();

				std::thread cH([&]() {
					server.Handler();
					});
				cH.detach();

				std::thread sH([&]() {
					client.Handler();
					});
				sH.detach();

				while (true)
				{
					HANDLE eventArr[7] = { *stopEvent, server.disconnect, client.disconnect, server.dataToSend, client.dataToSend, server.readyReceive, client.readyReceive };
					int eventResult = WaitForMultipleObjects(7, eventArr, FALSE, INFINITE);

					if (eventResult == WAIT_FAILED) {
						server.closeConnection();
						client.closeConnection();
						writeLog("Error while waiting for events: ", GetLastError());
						break;
					}

					if (eventResult == WAIT_OBJECT_0) {
						server.closeConnection();
						client.closeConnection();
						writeLog("Connection has been severed");
						break;
					}

					if (eventResult == WAIT_OBJECT_0 + 1) {
						if (server.dataInReceiveBuffer != 0) {
							// poslat data
						}

						server.closeConnection();
						client.closeConnection();
						break;
					}

					if (eventResult == WAIT_OBJECT_0 + 2) {
						if (client.dataInReceiveBuffer != 0) {
							// poslat data
						}

						client.closeConnection();
						server.closeConnection();
						break;
					}

					if (eventResult == WAIT_OBJECT_0 + 3) {
						int send_data = client.sendData(server.receiveBuffer + server.indexForRecData, server.dataInReceiveBuffer);
						if (send_data != 0) {
							server.subtractData(send_data);
						}
						else {
							ResetEvent(server.dataToSend);
							std::thread CW([&]() {
								if (client.WaitingToSend() == 0) {
									SetEvent(server.dataToSend);
								}
								});
							CW.detach();
						}
					}

					if (eventResult == WAIT_OBJECT_0 + 4) {
						int send_data = server.sendData(client.receiveBuffer + client.indexForRecData, client.dataInReceiveBuffer);
						if (send_data != 0) {
							client.subtractData(send_data);
						}
						else {
							ResetEvent(client.dataToSend);
							std::thread SW([&]() {
								if (server.WaitingToSend() == 0) {
									SetEvent(client.dataToSend);
								}
								});
							SW.detach();
						}
					}

					if (eventResult == WAIT_OBJECT_0 + 5) {
						server.receiveData();
					}

					if (eventResult == WAIT_OBJECT_0 + 6) {
						client.receiveData();
					}
				}
			}
			catch (const ServException& ex)
			{
				writeLog(ex.GetErrorType(), ex.GetErrorCode());
				server.closeConnection();
				client.closeConnection();
			}
		}
	}
	catch (const ServException& ex)
	{
		writeLog(ex.GetErrorType(), ex.GetErrorCode());
	}
}
