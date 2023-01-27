#include <stdio.h>
#include <string.h>
#include <tchar.h>
#include <windows.h>
#include <shellapi.h>
#include <synchapi.h>
#include <dbt.h>

#include <ViGEm/Client.h>

#include "hid.h"
#include "messages.h"
#include "stadia.h"

#include "service.h"

#define MAX_ACTIVE_DEVICE_COUNT 4
#define DEVICE_UNKNOWN_CHANGE   0
#define DEVICE_ATTACHED         1
#define DEVICE_REMOVED          2

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

struct active_device
{
    struct hid_device *src_device;
    struct stadia_controller *controller;
    PVIGEM_TARGET tgt_device;
    XUSB_REPORT tgt_report;
};

static int active_device_count = 0;
static struct active_device *active_devices[MAX_ACTIVE_DEVICE_COUNT];
static SRWLOCK active_devices_lock = SRWLOCK_INIT;
PVIGEM_CLIENT vigem_client;
HANDLE service_handle;
HDEVNOTIFY device_notification_handle;

// Forward declarations:
BOOL remove_device(struct stadia_controller *);

SHORT FORCEINLINE _map_byte_to_short(BYTE value, BOOL inverted)
{
    CHAR centered = value - 128;
    if (centered < -127)
    {
        centered = -127;
    }
    if (inverted)
    {
        centered = -centered;
    }
    return (SHORT)(32767 * centered / 127);
}

// Callbacks:

static void CALLBACK x360_notification_callback(PVIGEM_CLIENT client, PVIGEM_TARGET target, UCHAR large_motor, UCHAR small_motor, UCHAR led_number, LPVOID user_data)
{
    (void)client;
    (void)target;
    (void)led_number;

    struct active_device *active_device = (struct active_device *)user_data;
    stadia_controller_set_vibration(active_device->controller, small_motor, large_motor);
}

static void stadia_controller_update_callback(struct stadia_controller *controller, struct stadia_state *state)
{
    struct active_device *active_device = NULL;

    AcquireSRWLockShared(&active_devices_lock);

    for (int i = 0; i < active_device_count; i++)
    {
        if (active_devices[i]->controller == controller)
        {
            active_device = active_devices[i];
            break;
        }
    }

    ReleaseSRWLockShared(&active_devices_lock);

    if (active_device == NULL)
        return;

    active_device->tgt_report.wButtons = 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_UP) != 0 ? XUSB_GAMEPAD_DPAD_UP : 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_DOWN) != 0 ? XUSB_GAMEPAD_DPAD_DOWN : 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_LEFT) != 0 ? XUSB_GAMEPAD_DPAD_LEFT : 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_RIGHT) != 0 ? XUSB_GAMEPAD_DPAD_RIGHT : 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_MENU) != 0 ? XUSB_GAMEPAD_START : 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_OPTIONS) != 0 ? XUSB_GAMEPAD_BACK : 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_LS) != 0 ? XUSB_GAMEPAD_LEFT_THUMB : 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_RS) != 0 ? XUSB_GAMEPAD_RIGHT_THUMB : 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_LB) != 0 ? XUSB_GAMEPAD_LEFT_SHOULDER : 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_RB) != 0 ? XUSB_GAMEPAD_RIGHT_SHOULDER : 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_A) != 0 ? XUSB_GAMEPAD_A : 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_B) != 0 ? XUSB_GAMEPAD_B : 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_X) != 0 ? XUSB_GAMEPAD_X : 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_Y) != 0 ? XUSB_GAMEPAD_Y : 0;
    active_device->tgt_report.wButtons |= (state->buttons & STADIA_BUTTON_STADIA_BTN) != 0 ? XUSB_GAMEPAD_GUIDE : 0;
    active_device->tgt_report.bLeftTrigger = state->left_trigger;
    active_device->tgt_report.bRightTrigger = state->right_trigger;
    active_device->tgt_report.sThumbLX = _map_byte_to_short(state->left_stick_x, FALSE);
    active_device->tgt_report.sThumbLY = _map_byte_to_short(state->left_stick_y, TRUE);
    active_device->tgt_report.sThumbRX = _map_byte_to_short(state->right_stick_x, FALSE);
    active_device->tgt_report.sThumbRY = _map_byte_to_short(state->right_stick_y, TRUE);
    vigem_target_x360_update(vigem_client, active_device->tgt_device, active_device->tgt_report);
}

static void stadia_controller_destroy_callback(struct stadia_controller *controller, BYTE break_reason)
{
    if (remove_device(controller))
    {
        switch (break_reason)
        {
            case STADIA_BREAK_REASON_REQUESTED:
                break;
            case STADIA_BREAK_REASON_INIT_ERROR:
                service_log_message(ERROR_CONTROLLER_INITIALIZE_FAILED, EVENTLOG_ERROR_TYPE);
                break;
            case STADIA_BREAK_REASON_READ_ERROR:
                service_log_message(ERROR_CONTROLLER_DATA_READ, EVENTLOG_ERROR_TYPE);
                break;
            case STADIA_BREAK_REASON_WRITE_ERROR:
                service_log_message(ERROR_CONTROLLER_DATA_WRITE, EVENTLOG_ERROR_TYPE);
                break;
            default:
                service_log_message(ERROR_CONTROLLER_UNKNOWN, EVENTLOG_ERROR_TYPE);
                break;
        }

        service_log_message(INFO_CONTROLLER_DISCONNECTED, EVENTLOG_INFORMATION_TYPE);
    }
}

BOOL add_device(LPTSTR path)
{
    service_log_message(INFO_CONTROLLER_CONNECTED, EVENTLOG_INFORMATION_TYPE);

    if (active_device_count == MAX_ACTIVE_DEVICE_COUNT)
    {
        service_log_message(INFO_CONTROLLER_LIMIT_REACHED, EVENTLOG_INFORMATION_TYPE);
        return FALSE;
    }

    struct hid_device *device = hid_open_device(path, TRUE, FALSE);
    if (device == NULL)
    {
        if (hid_reenable_device(path))
        {
            device = hid_open_device(path, TRUE, FALSE);
            if (device == NULL)
            {
                device = hid_open_device(path, TRUE, TRUE);
            }
        }
        else
            device = hid_open_device(path, TRUE, TRUE);
    }

    if (device == NULL)
    {
        service_log_message(ERROR_CONTROLLER_OPEN_FAILED, EVENTLOG_ERROR_TYPE);
        return FALSE;
    }

    struct stadia_controller *controller = stadia_controller_create(device);
    if (controller == NULL)
    {
        service_log_message(ERROR_CONTROLLER_INITIALIZE_FAILED, EVENTLOG_ERROR_TYPE);

        hid_close_device(device);
        hid_free_device(device);
        return FALSE;
    }

    struct active_device *active_device = (struct active_device *)malloc(sizeof(struct active_device));
    active_device->src_device = device;
    active_device->controller = controller;
    
    active_device->tgt_device = vigem_target_x360_alloc();
    vigem_target_add(vigem_client, active_device->tgt_device);
    XUSB_REPORT_INIT(&active_device->tgt_report);
    vigem_target_x360_register_notification(vigem_client, active_device->tgt_device, x360_notification_callback, (LPVOID)active_device);

    AcquireSRWLockExclusive(&active_devices_lock);
    active_devices[active_device_count++] = active_device;
    ReleaseSRWLockExclusive(&active_devices_lock);

    service_log_message(SUCCESS_CONTROLLER_CREATED, EVENTLOG_INFORMATION_TYPE);

    return TRUE;
}

BOOL remove_device(struct stadia_controller *controller)
{
    BOOL removed = FALSE;

    AcquireSRWLockExclusive(&active_devices_lock);

    for (int i = 0; i < active_device_count; i++)
    {
        if (active_devices[i]->controller == controller)
        {
            hid_close_device(active_devices[i]->src_device);
            hid_free_device(active_devices[i]->src_device);

            vigem_target_x360_unregister_notification(active_devices[i]->tgt_device);
            vigem_target_remove(vigem_client, active_devices[i]->tgt_device);
            vigem_target_free(active_devices[i]->tgt_device);
            
            free(active_devices[i]);

            if (i < active_device_count - 1)
                memmove(&active_devices[i], &active_devices[i + 1], sizeof(struct active_device *) * (active_device_count - i - 1));

            active_device_count--;
            removed = TRUE;

            break;
        }
    }

    ReleaseSRWLockExclusive(&active_devices_lock);

    return removed;
}

void refresh_devices()
{
    LPTSTR stadia_hw_path_filters[3] = { STADIA_USB_HW_FILTER, STADIA_BLT_HW_FILTER, NULL };
    struct hid_device_info *device_info = hid_enumerate(stadia_hw_path_filters);
    struct hid_device_info *cur;
    BOOL found = FALSE;

    // remove missing devices
    AcquireSRWLockShared(&active_devices_lock);

    for (int i = 0; i < active_device_count; i++)
    {
        found = FALSE;
        cur = device_info;

        while (cur != NULL)
        {
            if (_tcscmp(active_devices[i]->src_device->path, cur->path) == 0)
            {
                found = TRUE;
                break;
            }
            cur = cur->next;
        }

        if (!found)
            stadia_controller_destroy(active_devices[i]->controller, STADIA_BREAK_REASON_REQUESTED);
    }

    ReleaseSRWLockShared(&active_devices_lock);

    // add new devices
    cur = device_info;
    while (cur != NULL)
    {
        found = FALSE;
        AcquireSRWLockShared(&active_devices_lock);

        for (int i = 0; i < active_device_count; i++)
        {
            if (_tcscmp(cur->path, active_devices[i]->src_device->path) == 0)
            {
                found = TRUE;
                break;
            }
        }

        ReleaseSRWLockShared(&active_devices_lock);

        if (!found)
            add_device(cur->path);

        cur = cur->next;
    }

    // free hid_device_info list
    while (device_info)
    {
        cur = device_info->next;
        hid_free_device_info(device_info);
        device_info = cur;
    }
}

int service_init(HANDLE service)
{
    service_handle = service;
    vigem_client = vigem_alloc();
    VIGEM_ERROR connect_result = vigem_connect(vigem_client);
    if (connect_result == VIGEM_ERROR_BUS_NOT_FOUND)
    {
        service_log_message(ERROR_VIGEMBUS_MISSING, EVENTLOG_ERROR_TYPE);
        return ERROR_VIGEMBUS_MISSING;
    }
    else if (connect_result == VIGEM_ERROR_BUS_VERSION_MISMATCH)
    {
        service_log_message(ERROR_VIGEMBUS_INCOMPATIBLE, EVENTLOG_ERROR_TYPE);
        return ERROR_VIGEMBUS_INCOMPATIBLE;
    }
    else if (connect_result != VIGEM_ERROR_NONE)
    {
        service_log_message(ERROR_VIGEMBUS_UNKNOWN, EVENTLOG_ERROR_TYPE);
        return ERROR_VIGEMBUS_UNKNOWN;
    }

    stadia_update_callback = stadia_controller_update_callback;
    stadia_destroy_callback = stadia_controller_destroy_callback;

    refresh_devices();
    service_register_device_notification(hid_get_class());

    return 0;
}

void service_quit()
{
    if (device_notification_handle != NULL)
    {
        UnregisterDeviceNotification(device_notification_handle);
    }
    
    AcquireSRWLockExclusive(&active_devices_lock);
    for (int i = 0; i < active_device_count; i++)
    {
        hid_close_device(active_devices[i]->src_device);
        hid_free_device(active_devices[i]->src_device);
    }
}

extern void service_register_device_notification(GUID filter)
{
    if (service_handle == NULL)
    {
        service_log_message(ERROR_SERVICE_HANDLE_NULL, EVENTLOG_ERROR_TYPE);
        return;
    }

    DEV_BROADCAST_DEVICEINTERFACE dbdi;
    memset(&dbdi, 0, sizeof(dbdi));
    dbdi.dbcc_size = sizeof(dbdi);
    dbdi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    dbdi.dbcc_classguid = filter;

    device_notification_handle = RegisterDeviceNotification(service_handle, &dbdi, DEVICE_NOTIFY_SERVICE_HANDLE);
}

int service_loop(BOOL blocking)
{
    MSG msg;
    if (blocking)
    {
        GetMessage(&msg, NULL, 0, 0);
    }
    else
    {
        PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
    }

    switch (msg.message)
    {
        case WM_QUIT:
            return -1;
        case WM_DEVICECHANGE:
            refresh_devices();
    }

    TranslateMessage(&msg);
    DispatchMessage(&msg);
    return 0;
}

void service_log_message(DWORD message, WORD message_type) 
{ 
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];

    hEventSource = RegisterEventSource(NULL, SVCNAME);

    if( NULL != hEventSource )
    {
        lpszStrings[0] = SVCNAME;

        ReportEvent(hEventSource,        // event log handle
                    message_type,        // event type
                    0,                   // event category
                    message,             // event identifier
                    NULL,                // no security identifier
                    1,                   // size of lpszStrings array
                    0,                   // no binary data
                    lpszStrings,         // array of strings
                    NULL);               // no binary data

        DeregisterEventSource(hEventSource);
    }
}