#include "ProxyServer.h"
#include "TCPclient.h"
#include "TCPserver.h"
#include "WebSocketClient.h"
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "winhttp.lib")

int main()
{
   HANDLE ev = CreateEvent(NULL, TRUE, FALSE, NULL);
   TCPserver server("4444");
   //TCPclient client("127.0.0.1", "1883");
   WebSocketClient client(L"127.0.0.1", 8080);
   proxyServer(server, client, &ev);
}



