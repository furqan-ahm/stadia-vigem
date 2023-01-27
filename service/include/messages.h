 // The following are message definitions.
//
//  Values are 32 bit values laid out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//
#define FACILITY_SYSTEM                  0x0
#define FACILITY_RUNTIME                 0x2
#define FACILITY_STUBS                   0x3
#define FACILITY_IO_ERROR_CODE           0x4


//
// Define the severity codes
//
#define STATUS_SEVERITY_SUCCESS          0x0
#define STATUS_SEVERITY_INFORMATIONAL    0x1
#define STATUS_SEVERITY_WARNING          0x2
#define STATUS_SEVERITY_ERROR            0x3


//
// MessageId: SVC_ERROR
//
// MessageText:
//
// An error has occurred (%2).
//
#define SVC_ERROR                        ((DWORD)0xC0020001L)

//
// MessageId: ERROR_VIGEMBUS_MISSING
//
// MessageText:
//
// ViGEmBus was not found.
//
#define ERROR_VIGEMBUS_MISSING           ((DWORD)0xC0020002L)

//
// MessageId: ERROR_VIGEMBUS_INCOMPATIBLE
//
// MessageText:
//
// Incompatible version of ViGEmBus installed.
//
#define ERROR_VIGEMBUS_INCOMPATIBLE      ((DWORD)0xC0020003L)

//
// MessageId: ERROR_VIGEMBUS_UNKNOWN
//
// MessageText:
//
// Encountered unknown error while connecting to ViGEmBus.
//
#define ERROR_VIGEMBUS_UNKNOWN           ((DWORD)0xC0020004L)

//
// MessageId: INFO_CONTROLLER_LIMIT_REACHED
//
// MessageText:
//
// Unable to create virtual Xbox 360 controller because max active device count reached.
//
#define INFO_CONTROLLER_LIMIT_REACHED    ((DWORD)0x40020005L)

//
// MessageId: SUCCESS_CONTROLLER_CREATED
//
// MessageText:
//
// Stadia controller sucessfully added as a virtual Xbox 360 controller.
//
#define SUCCESS_CONTROLLER_CREATED       ((DWORD)0x00020006L)

//
// MessageId: ERROR_CONTROLLER_OPEN_FAILED
//
// MessageText:
//
// Failed to open connected Stadia controller.
//
#define ERROR_CONTROLLER_OPEN_FAILED     ((DWORD)0xC0020007L)

//
// MessageId: ERROR_CONTROLLER_INITIALIZE_FAILED
//
// MessageText:
//
// Failed to initialize connected Stadia controller.
//
#define ERROR_CONTROLLER_INITIALIZE_FAILED ((DWORD)0xC0020008L)

//
// MessageId: INFO_CONTROLLER_CONNECTED
//
// MessageText:
//
// A Stadia controller was connected.
//
#define INFO_CONTROLLER_CONNECTED        ((DWORD)0x40020009L)

//
// MessageId: INFO_CONTROLLER_DISCONNECTED
//
// MessageText:
//
// A Stadia controller was disconnected.
//
#define INFO_CONTROLLER_DISCONNECTED     ((DWORD)0x40020010L)

//
// MessageId: ERROR_CONTROLLER_DATA_READ
//
// MessageText:
//
// Failed to read data from Stadia controller.
//
#define ERROR_CONTROLLER_DATA_READ       ((DWORD)0xC0020011L)

//
// MessageId: ERROR_CONTROLLER_DATA_WRITE
//
// MessageText:
//
// Failed to write data to Stadia controller.
//
#define ERROR_CONTROLLER_DATA_WRITE      ((DWORD)0xC0020012L)

//
// MessageId: ERROR_CONTROLLER_UNKNOWN
//
// MessageText:
//
// A Stadia controller has encountered an unknown error.
//
#define ERROR_CONTROLLER_UNKNOWN         ((DWORD)0xC0020013L)

//
// MessageId: ERROR_SERVICE_HANDLE_NULL
//
// MessageText:
//
// Service handle is NULL. Stadia-ViGEm will not automatically detect plugged in controllers.
//
#define ERROR_SERVICE_HANDLE_NULL        ((DWORD)0xC0020014L)

 // A message file must end with a period on its own line
 // followed by a blank line.