#include <windows.h>
#include <strsafe.h>
#include <processthreadsapi.h>
#include <dbt.h>

#include "messages.h"
#include "service.h"

#pragma comment(lib, "advapi32.lib")

SERVICE_STATUS          gSvcStatus; 
SERVICE_STATUS_HANDLE   gSvcStatusHandle; 
BOOL                    keep_running = TRUE;
DWORD                   main_thread_id;

VOID SvcInstall(void);
VOID SvcUninstall(void);
VOID WINAPI SvcCtrlHandler( DWORD, DWORD ); 
VOID WINAPI SvcMain(); 

VOID ReportSvcStatus( DWORD, DWORD, DWORD );
VOID SvcInit(); 
VOID SvcReportEvent( LPTSTR );

//
// Purpose: 
//   Entry point for the process
//
// Parameters:
//   None
// 
// Return value:
//   None, defaults to 0 (zero)
//
int __cdecl wmain(int argc, WCHAR *argv[]) 
{ 
    if (argc < 1) 
        return; 

    // If command-line parameter is "install", install the service.
    // If command-line parameter is "uninstall", uninstall the service.
    // If command-line parameter is "debug", start service in console. 
    // Otherwise, the service is probably being started by the SCM.

    if( lstrcmpi( argv[1], L"install") == 0 )
    {
        SvcInstall();
        return;
    }
    else if( lstrcmpi( argv[1], L"uninstall") == 0 )
    {
        SvcUninstall();
        return;
    }
    else if ( lstrcmpi( argv[1], L"debug") == 0 )
    {
        int init_result = service_init(NULL);
        if (init_result != 0)
        {
            printf("Init error (%x)\n", init_result);
            return;
        }
        
        while (service_loop(TRUE) == 0) {;}

        service_quit();
    }

    SERVICE_TABLE_ENTRY DispatchTable[] = 
    { 
        { SVCNAME, (LPSERVICE_MAIN_FUNCTION) SvcMain }, 
        { NULL, NULL } 
    }; 
 
    // This call returns when the service has stopped. 
    // The process should simply terminate when the call returns.

    if (!StartServiceCtrlDispatcher( DispatchTable )) 
    { 
        SvcReportEvent(L"StartServiceCtrlDispatcher"); 
    } 
} 

//
// Purpose: 
//   Installs the service in the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
VOID SvcInstall()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    WCHAR szPath[MAX_PATH];

    if(!GetModuleFileName(NULL, szPath, MAX_PATH ) )
    {
        printf("Cannot install service (%d)\n", GetLastError());
        return;
    }

    // Get a handle to the SCM database. 
 
    schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager) 
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    // Create the service

    schService = CreateService( 
        schSCManager,              // SCM database 
        SVCNAME,                   // name of service 
        SVCNAME,                   // service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        SERVICE_AUTO_START,        // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        szPath,                    // path to service's binary 
        NULL,                      // no load ordering group 
        NULL,                      // no tag identifier 
        NULL,                      // no dependencies 
        NULL,                      // LocalSystem account 
        NULL);                     // no password 
 
    if (schService == NULL) 
    {
        printf("CreateService failed (%d)\n", GetLastError()); 
        CloseServiceHandle(schSCManager);
        return;
    }

    SERVICE_DESCRIPTION sd;
    LPTSTR szDesc = L"Manages Stadia controllers attached to the system and processes their input into virtual Xbox 360 controllers.";

    sd.lpDescription = szDesc;

    if( !ChangeServiceConfig2(
        schService,                 // handle to service
        SERVICE_CONFIG_DESCRIPTION, // change: description
        &sd) )                      // new description
    {
        printf("ChangeServiceConfig2 failed\n");
    }

    printf("Service installed successfully\n"); 

    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Uninstalls the service from the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
VOID SvcUninstall()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    WCHAR szPath[MAX_PATH];

    if(!GetModuleFileName(NULL, szPath, MAX_PATH ) )
    {
        printf("Cannot install service (%d)\n", GetLastError());
        return;
    }

    // Get a handle to the SCM database. 
 
    schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager) 
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }
    
    // Get a handle to the service.

    schService = OpenService( 
        schSCManager,       // SCM database 
        SVCNAME,          // name of service 
        DELETE);            // need delete access 
 
    if (schService == NULL)
    { 
        printf("OpenService failed (%d)\n", GetLastError()); 
        CloseServiceHandle(schSCManager);
        return;
    }

    // Delete the service.
 
    if (! DeleteService(schService) ) 
    {
        printf("DeleteService failed (%d)\n", GetLastError()); 
    }
    else printf("Service deleted successfully\n"); 
}

//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//
VOID WINAPI SvcMain()
{
    // Register the handler function for the service

    gSvcStatusHandle = RegisterServiceCtrlHandlerEx( 
        SVCNAME, 
        (LPHANDLER_FUNCTION_EX)SvcCtrlHandler, 0);

    if( !gSvcStatusHandle )
    { 
        SvcReportEvent(L"RegisterServiceCtrlHandler"); 
        return; 
    } 

    // These SERVICE_STATUS members remain as set here

    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
    gSvcStatus.dwServiceSpecificExitCode = 0;    

    // Report initial status to the SCM

    ReportSvcStatus( SERVICE_START_PENDING, NO_ERROR, 3000 );

    // Perform service-specific initialization and work.

    SvcInit();
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None
//
VOID SvcInit()
{
    main_thread_id = GetCurrentThreadId();

    int init_result = service_init(gSvcStatusHandle);
    if (init_result != 0)
    {
        ReportSvcStatus( SERVICE_STOPPED, init_result, 0 );
        return;
    }

    // Report running status when initialization is complete.

    ReportSvcStatus( SERVICE_RUNNING, NO_ERROR, 0 );

    while(service_loop(TRUE) == 0) {;}

    service_quit();

    ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
    return;
}

//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation, 
//     in milliseconds
// 
// Return value:
//   None
//
VOID ReportSvcStatus( DWORD dwCurrentState,
                      DWORD dwWin32ExitCode,
                      DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ( (dwCurrentState == SERVICE_RUNNING) ||
           (dwCurrentState == SERVICE_STOPPED) )
        gSvcStatus.dwCheckPoint = 0;
    else gSvcStatus.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus( gSvcStatusHandle, &gSvcStatus );
}

//
// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwCtrl - control code
// 
// Return value:
//   None
//
VOID WINAPI SvcCtrlHandler(DWORD dwCtrl, DWORD evtype)
{
   // Handle the requested control code. 

   switch(dwCtrl) 
   {  
      case SERVICE_CONTROL_STOP: 
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        // Signal the service to stop.

        PostThreadMessage(main_thread_id, WM_QUIT, 0, 0);
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
         
        return;

      case SERVICE_CONTROL_DEVICEEVENT:
        switch (evtype)
        {
            case DBT_DEVICEREMOVECOMPLETE:
            {
                PostThreadMessage(main_thread_id, WM_DEVICECHANGE, 0, 0);
            }
            break;
            case DBT_DEVICEARRIVAL:
            {
                PostThreadMessage(main_thread_id, WM_DEVICECHANGE, 0, 0);
            }
            break;
        }
        break;
 
      case SERVICE_CONTROL_INTERROGATE: 
        break; 
 
      default: 
        break;
   } 
   
}

//
// Purpose: 
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
// 
// Return value:
//   None
//
// Remarks:
//   The service must have an entry in the Application event log.
//
VOID SvcReportEvent(LPTSTR szFunction) 
{ 
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    WCHAR Buffer[80];

    hEventSource = RegisterEventSource(NULL, SVCNAME);

    if( NULL != hEventSource )
    {
        StringCchPrintf(Buffer, 80, L"%s failed with %d", szFunction, GetLastError());

        lpszStrings[0] = SVCNAME;
        lpszStrings[1] = Buffer;

        ReportEvent(hEventSource,        // event log handle
                    EVENTLOG_ERROR_TYPE, // event type
                    0,                   // event category
                    SVC_ERROR,           // event identifier
                    NULL,                // no security identifier
                    2,                   // size of lpszStrings array
                    0,                   // no binary data
                    lpszStrings,         // array of strings
                    NULL);               // no binary data

        DeregisterEventSource(hEventSource);
    }
}