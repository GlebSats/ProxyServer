#include "ProxyServer.h"
#include "TCPclient.h"
#include "TCPserver.h"
#include "WebSocketClient.h"
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#include <vector>

TCHAR buffer[256];
std::vector <std::pair<std::shared_ptr<ProxyConnection>, std::shared_ptr<ProxyConnection>>> Connections;
void readInitFile() {
    TCHAR UnquotedPath[MAX_PATH];
    if (!GetModuleFileName(NULL, UnquotedPath, MAX_PATH))
    {
        printf("Cannot install service (%d)\n", GetLastError());
        return;
    }
    std::vector <std::wstring> Sections;

    std::wstring configPath = UnquotedPath;
    size_t i = configPath.find_last_of(L"\\");
    configPath = configPath.substr(0, i + 1) + L"config.ini";

    DWORD bytesRead = GetPrivateProfileSectionNamesW(buffer, sizeof(buffer), configPath.c_str());
    if (bytesRead > 0) {
        size_t startP = 0;
        while (startP < bytesRead) {
            std::wstring sectionName = &buffer[startP];
            Sections.push_back(sectionName);
            startP += sectionName.length() + 1;
        }
    }
    else {
        //write error
        std::cout << "read init error";
        return;
    }

    std::vector <LPCWSTR> keyNames{ L"Listening port", L"Host", L"Protocol", L"Subprotocol", L"Port" };
    for (auto& section : Sections) {
        std::vector <std::wstring> connectionOptions;
        int targetPort = 0;
        for (auto& key : keyNames) {
            if (key == L"Port") {
                targetPort = GetPrivateProfileIntW(section.c_str(), key, 0, configPath.c_str());
                if (targetPort == 0) {
                    //write error
                    std::cout << "read init error";
                    return;
                }
            }
            else {
                bytesRead = GetPrivateProfileStringW(section.c_str(), key, L"", buffer, sizeof(buffer), configPath.c_str());
                if (bytesRead > 0) {
                    connectionOptions.push_back(buffer);
                }
                else {
                    if (key != L"Subprotocol") {
                        //write error
                        std::cout << "read init error";
                        return;
                    }
                }
            }
        }
        if (connectionOptions[2] == L"websocket") {
            std::string lPort(connectionOptions[0].begin(), connectionOptions[0].end());
            std::shared_ptr<ProxyConnection> server = std::make_shared<TCPserver>(lPort.c_str());
            std::shared_ptr<ProxyConnection> client = std::make_shared<WebSocketClient>(connectionOptions[1].c_str(), targetPort);
            Connections.push_back(std::make_pair(server, client));
        }
        else if (connectionOptions[2] == L"TCP") {
            std::string lPort(connectionOptions[0].begin(), connectionOptions[0].end());
            std::string sHost(connectionOptions[1].begin(), connectionOptions[1].end());
            std::string sPort = std::to_string(targetPort);
            std::shared_ptr<ProxyConnection> server = std::make_unique<TCPserver>(lPort.c_str());
            std::shared_ptr<ProxyConnection> client = std::make_unique<TCPclient>(sHost.c_str(), sPort.c_str());
            Connections.push_back(std::make_pair(server, client));
        }
        else {
            //write error protocol
        }
    }
}

int main()
{
   readInitFile();
   HANDLE ev = CreateEvent(NULL, TRUE, FALSE, NULL);
   //TCPserver server("4444");
   //TCPclient client("127.0.0.1", "1883");
   /*std::shared_ptr<ProxyConnection> server = std::make_shared<TCPserver>("4444");
   std::shared_ptr<ProxyConnection> client = std::make_shared<TCPclient>("127.0.0.1", "1883");
   Connections.push_back(std::make_pair(std::move(server), std::move(client)));*/
   //WebSocketClient client(L"127.0.0.1", 8080);
   proxyServer(Connections[0].first, Connections[0].second, &ev);
}



