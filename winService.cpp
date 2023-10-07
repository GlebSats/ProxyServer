#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <tchar.h>
#include <strsafe.h>
#include "ServException.h"
#include "ProxyServer.h"
#include "TCPserver.h"
#include "WebSocketClient.h"
#include <Windows.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "winhttp.lib")

// Global Variables
TCHAR SVCNAME[] = L"webSocketProxy";
SERVICE_STATUS serviceStatus;
SERVICE_STATUS_HANDLE serviceStatusHandle;
HANDLE serviceStopEvent = NULL;
// Windows Service Functions
VOID WINAPI winServiceMain(DWORD Argc, LPTSTR* Argv);
VOID WINAPI winServiceHandler(DWORD controlCode);
VOID ReportWinServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);
VOID InitWinService(DWORD Argc, LPTSTR* Argv);
VOID SvcInstall();
VOID SvcUninstall();
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

    serviceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (serviceStopEvent == NULL) {
        ReportWinServiceStatus(SERVICE_STOPPED, GetLastError(), 0);
    }
    else {
        TCPserver server("4444");
        WebSocketClient client(L"127.0.0.1", 8080);
        ReportWinServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);
        proxyServer(server, client, &serviceStopEvent);
    }

    ReportWinServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

VOID SvcInstall() {
    SC_HANDLE hSCManager, hService;
    TCHAR UnquotedPath[MAX_PATH];

    if (!GetModuleFileName(NULL, UnquotedPath, MAX_PATH))
    {
        printf("Cannot install service (%d)\n", GetLastError());
        return;
    }

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
    
