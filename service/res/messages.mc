MessageIdTypedef=DWORD

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
    Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
    Warning=0x2:STATUS_SEVERITY_WARNING
    Error=0x3:STATUS_SEVERITY_ERROR
    )


FacilityNames=(System=0x0:FACILITY_SYSTEM
    Runtime=0x2:FACILITY_RUNTIME
    Stubs=0x3:FACILITY_STUBS
    Io=0x4:FACILITY_IO_ERROR_CODE
)

LanguageNames=(English=0x409:MSG00409)

; // The following are message definitions.

MessageId=0x1
Severity=Error
Facility=Runtime
SymbolicName=SVC_ERROR
Language=English
An error has occurred (%2).
.

MessageId=0x2
Severity=Error
Facility=Runtime
SymbolicName=ERROR_VIGEMBUS_MISSING
Language=English
ViGEmBus was not found.
.

MessageId=0x3
Severity=Error
Facility=Runtime
SymbolicName=ERROR_VIGEMBUS_INCOMPATIBLE
Language=English
Incompatible version of ViGEmBus installed.
.

MessageId=0x4
Severity=Error
Facility=Runtime
SymbolicName=ERROR_VIGEMBUS_UNKNOWN
Language=English
Encountered unknown error while connecting to ViGEmBus.
.

MessageId=0x5
Severity=Informational
Facility=Runtime
SymbolicName=INFO_CONTROLLER_LIMIT_REACHED
Language=English
Unable to create virtual Xbox 360 controller because max active device count reached.
.

MessageId=0x6
Severity=Success
Facility=Runtime
SymbolicName=SUCCESS_CONTROLLER_CREATED
Language=English
Stadia controller sucessfully added as a virtual Xbox 360 controller.
.

MessageId=0x7
Severity=Error
Facility=Runtime
SymbolicName=ERROR_CONTROLLER_OPEN_FAILED
Language=English
Failed to open connected Stadia controller.
.

MessageId=0x8
Severity=Error
Facility=Runtime
SymbolicName=ERROR_CONTROLLER_INITIALIZE_FAILED
Language=English
Failed to initialize connected Stadia controller.
.

MessageId=0x9
Severity=Informational
Facility=Runtime
SymbolicName=INFO_CONTROLLER_CONNECTED
Language=English
A Stadia controller was connected.
.

MessageId=0x10
Severity=Informational
Facility=Runtime
SymbolicName=INFO_CONTROLLER_DISCONNECTED
Language=English
A Stadia controller was disconnected.
.

MessageId=0x11
Severity=Error
Facility=Runtime
SymbolicName=ERROR_CONTROLLER_DATA_READ
Language=English
Failed to read data from Stadia controller.
.

MessageId=0x12
Severity=Error
Facility=Runtime
SymbolicName=ERROR_CONTROLLER_DATA_WRITE
Language=English
Failed to write data to Stadia controller.
.

MessageId=0x13
Severity=Error
Facility=Runtime
SymbolicName=ERROR_CONTROLLER_UNKNOWN
Language=English
A Stadia controller has encountered an unknown error.
.

MessageId=0x14
Severity=Error
Facility=Runtime
SymbolicName=ERROR_SERVICE_HANDLE_NULL
Language=English
Service handle is NULL. Stadia-ViGEm will not automatically detect plugged in controllers.
.

; // A message file must end with a period on its own line
; // followed by a blank line.