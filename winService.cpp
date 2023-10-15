#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <tchar.h>
#include <strsafe.h>
#include "ServException.h"
#include "ProxyServer.h"
#include "TCPserver.h"
#include "TCPclient.h"
#include "WebSocketClient.h"
#include <vector>
#include <string>
#include <Windows.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "winhttp.lib")

// Global Variables
TCHAR UnquotedPath[MAX_PATH];
TCHAR SVCNAME[] = L"webSocketProxy";
SERVICE_STATUS serviceStatus;
SERVICE_STATUS_HANDLE serviceStatusHandle;
HANDLE serviceStopEvent = NULL;
std::vector <std::pair<std::shared_ptr<ProxyConnection>, std::shared_ptr<ProxyConnection>>> Connections;
// Windows Service Functions
VOID WINAPI winServiceMain(DWORD Argc, LPTSTR* Argv);
VOID WINAPI winServiceHandler(DWORD controlCode);
VOID ReportWinServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);
VOID InitWinService(DWORD Argc, LPTSTR* Argv);
VOID SvcInstall();
VOID SvcUninstall();
VOID readInitFile();
//
int _tmain(int argc, TCHAR* argv[])
{
    if (lstrcmpi(argv[1], TEXT("install")) == 0)
    {
        SvcInstall();
        return 0;
    }
    else if (lstrcmpi(argv[1], TEXT("uninstall")) == 0) {
        SvcUninstall();
        return 0;
    }
    else {
        SERVICE_TABLE_ENTRY DispatchTable[] =
        {
            { SVCNAME, (LPSERVICE_MAIN_FUNCTION)winServiceMain},
            { NULL, NULL }
        };

        if (!StartServiceCtrlDispatcher(DispatchTable))
        {
            writeLog("StartServiceCtrlDispatcher function error: " + GetLastError());
        }
        return 0;
    }
}

VOID WINAPI winServiceMain(DWORD Argc, LPTSTR* Argv) {
    serviceStatusHandle = RegisterServiceCtrlHandlerW(SVCNAME, winServiceHandler);
    if (serviceStatusHandle == 0) {
        writeLog("RegisterServiceCtrlHandler function error: " + GetLastError());
        return;
    }

    serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    serviceStatus.dwServiceSpecificExitCode = 0;

    ReportWinServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    InitWinService(Argc, Argv);
}

VOID WINAPI winServiceHandler(DWORD controlCode) {

    switch (controlCode)
    {

    case SERVICE_CONTROL_STOP:
        
        SetEvent(serviceStopEvent);
        ReportWinServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
        //ReportWinServiceStatus(SERVICE_STOPPED, NO_ERROR, 3000);

    default:
        break;
    }
}

VOID ReportWinServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint) {
    static DWORD dwCheckPoint = 1;

    serviceStatus.dwCurrentState = dwCurrentState;
    serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
    serviceStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING) {
        serviceStatus.dwControlsAccepted = 0;
    }
    else {
        serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    }

    if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED)) {
        serviceStatus.dwCheckPoint = 0;
    }
    else {
        serviceStatus.dwCheckPoint = dwCheckPoint++;
    }

    SetServiceStatus(serviceStatusHandle, &serviceStatus);
}

VOID InitWinService(DWORD Argc, LPTSTR* Argv) {
    //здесь нужно грузить конфиг
    serviceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (serviceStopEvent == NULL) {
        ReportWinServiceStatus(SERVICE_STOPPED, GetLastError(), 0);
    }
    else {
        ReportWinServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);
        proxyServer(Connections[0].first, Connections[0].second, &serviceStopEvent);
    }

    ReportWinServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

VOID SvcInstall() {
    SC_HANDLE hSCManager, hService;

    if (!GetModuleFileName(NULL, UnquotedPath, MAX_PATH))
    {
        printf("Cannot install service (%d)\n", GetLastError());
        return;
    }
    //
    readInitFile();
    //
    TCHAR szPath[MAX_PATH];
    StringCbPrintf(szPath, MAX_PATH, TEXT("\"%s\""), UnquotedPath);

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }
    // nebo SERVICE_DEMAND_START
    hService = CreateService(hSCManager, SVCNAME, SVCNAME, SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, szPath, NULL, NULL, NULL, NULL, NULL);
    if (hService == NULL) {
        printf("CreateService failed (%d)\n", GetLastError());
        CloseServiceHandle(hSCManager);
        return;
    }
    else {
        printf("Service installed successfully\n");
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
}

VOID SvcUninstall() {
    SC_HANDLE hSCManager, hService;

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    hService = OpenService(hSCManager, SVCNAME, SERVICE_ALL_ACCESS);
    if (hService == NULL) {
        printf("OpenService failed (%d)\n", GetLastError());
        CloseServiceHandle(hSCManager);
        return;
    }

    if (!DeleteService(hService)) {
        printf("DeleteService failed (%d)\n", GetLastError());
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        return;
    }
    else {
        printf("Service uninstalled successfully\n");
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        return;
    }
}

VOID readInitFile() {
    WCHAR buffer[256];
    std::vector <std::wstring> Sections;

    std::wstring configPath = UnquotedPath;
    size_t i = configPath.find_last_of(L"\\");
    configPath = configPath.substr(0, i+1) + L"config.ini";

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
            std::shared_ptr<ProxyConnection> server = std::make_shared<TCPserver>(lPort.c_str());
            std::shared_ptr<ProxyConnection> client = std::make_shared<TCPclient>(sHost.c_str(), sPort.c_str());
            Connections.push_back(std::make_pair(server, client));
        }
        else {
            //write error protocol
        }
    }
}
