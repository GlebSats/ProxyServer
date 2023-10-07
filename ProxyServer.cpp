#include "ProxyServer.h"

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

				std::thread sH([&]() {
					server.Handler();
					});
				std::thread ServerWaitingSend;

				std::thread cH([&]() {
					client.Handler();
					});
				std::thread ClientWaitingSend;

				while (true)
				{
					HANDLE eventArr[9] = { *stopEvent, server.disconnect, client.disconnect, server.dataToSend, client.dataToSend,
					server.readyReceive, client.readyReceive, server.endOfWaiting, client.endOfWaiting };

					int eventResult = WaitForMultipleObjects(9, eventArr, FALSE, INFINITE);

					if (eventResult == WAIT_FAILED) {
						server.closeConnection();
						client.closeConnection();
						writeLog("Error while waiting for events: ", GetLastError());
						sH.join();
						cH.join();
						break;
					}

					if (eventResult == WAIT_OBJECT_0) {
						server.closeConnection();
						client.closeConnection();
						writeLog("Connection has been severed");
						sH.join();
						cH.join();
						break;
					}

					if (eventResult == WAIT_OBJECT_0 + 1) {
						if (server.dataInReceiveBuffer != 0) {
							while (server.dataInReceiveBuffer != 0) {
								int send_data = client.sendData(server.receiveBuffer + server.indexForRecData, server.dataInReceiveBuffer);
								if (send_data <= 0) {
									break;
								}
								server.dataInReceiveBuffer -= send_data;
							}
						}
						server.closeConnection();
						client.closeConnection();
						sH.join();
						cH.join();
						break;
					}

					if (eventResult == WAIT_OBJECT_0 + 2) {
						if (client.dataInReceiveBuffer != 0) {
							while (client.dataInReceiveBuffer != 0) {
								int send_data = server.sendData(client.receiveBuffer + client.indexForRecData, client.dataInReceiveBuffer);
								if (send_data <= 0) {
									break;
								}
								client.dataInReceiveBuffer -= send_data;
							}
						}

						client.closeConnection();
						server.closeConnection();
						sH.join();
						cH.join();
						break;
					}

					if (eventResult == WAIT_OBJECT_0 + 3) {
						int send_data = client.sendData(server.receiveBuffer + server.indexForRecData, server.dataInReceiveBuffer);
						if (send_data > 0) {
							server.subtractData(send_data);
						}
						else if (send_data == -1) {
							SetEvent(client.disconnect);
						}
						else {
							ResetEvent(server.dataToSend);
							ClientWaitingSend = std::thread(&ProxyConnection::WaitingToSend, &client);
						}
					}

					if (eventResult == WAIT_OBJECT_0 + 4) {
						int send_data = server.sendData(client.receiveBuffer + client.indexForRecData, client.dataInReceiveBuffer);
						if (send_data > 0) {
							client.subtractData(send_data);
						}
						else if (send_data == -1) {
							SetEvent(server.disconnect);
						}
						else {
							ResetEvent(client.dataToSend);
							ServerWaitingSend = std::thread(&ProxyConnection::WaitingToSend, &server);
						}
					}

					if (eventResult == WAIT_OBJECT_0 + 5) {
						server.receiveData();
					}

					if (eventResult == WAIT_OBJECT_0 + 6) {
						client.receiveData();
					}

					if (eventResult == WAIT_OBJECT_0 + 7) {
						if (server.waitingResult) {
							SetEvent(client.dataToSend);
						}
						ResetEvent(server.endOfWaiting);
						ServerWaitingSend.join();
					}

					if (eventResult == WAIT_OBJECT_0 + 8) {
						if (client.waitingResult) {
							SetEvent(server.dataToSend);
						}
						ResetEvent(client.endOfWaiting);
						ClientWaitingSend.join();
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
