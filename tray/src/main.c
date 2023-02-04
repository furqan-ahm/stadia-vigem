/*
 * main.c -- Program entry point and Stadia device mapping.
 */

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <synchapi.h>

#include <ViGEm/Client.h>

#include "tray.h"
#include "hid.h"
#include "stadia.h"

#ifndef _DEBUG
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

#define MAX_ACTIVE_DEVICE_COUNT 4
#define DEVICE_COUNT_TEMPLATE TEXT("%d/4 device(s) connected")

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
static PVIGEM_CLIENT vigem_client;
static BOOL vigem_connected = FALSE;

static struct tray_menu tray_menu_device_count;

// future declarations
static void stadia_controller_update_cb(struct stadia_controller *controller, struct stadia_state *state);
static void stadia_controller_stop_cb(struct stadia_controller *controller, BYTE break_reason);
static void CALLBACK x360_notification_cb(PVIGEM_CLIENT client, PVIGEM_TARGET target, UCHAR large_motor,
                                          UCHAR small_motor, UCHAR led_number, LPVOID user_data);
static void refresh_cb(struct tray_menu *item);
static void quit_cb(struct tray_menu *item);

static const struct tray_menu tray_menu_refresh = {.text = TEXT("Refresh"), .cb = refresh_cb};
static const struct tray_menu tray_menu_quit = {.text = TEXT("Quit"), .cb = quit_cb};
static const struct tray_menu tray_menu_separator = {.text = TEXT("-")};
static const struct tray_menu tray_menu_terminator = {.text = NULL};
static struct tray tray =
    {
        .icon = TEXT("APP_ICON"),
        .tip = TEXT("Stadia Controller"),
        .menu = NULL};

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

static void rebuild_tray_menu()
{
    struct tray_menu *prev_menu = tray.menu;
    
    struct tray_menu *new_menu = (struct tray_menu *)malloc(5 * sizeof(struct tray_menu));
    int index = 0;

    AcquireSRWLockShared(&active_devices_lock);

    LPWSTR old_device_count_text = tray_menu_device_count.text;

    INT tray_text_length = _scwprintf(DEVICE_COUNT_TEMPLATE, active_device_count);
    tray_menu_device_count.text = (LPWSTR)malloc((tray_text_length + 1) * sizeof(WCHAR));
    wsprintf(tray_menu_device_count.text, DEVICE_COUNT_TEMPLATE, active_device_count);

    free(old_device_count_text);

    ReleaseSRWLockShared(&active_devices_lock);

    new_menu[index++] = tray_menu_device_count;
    new_menu[index++] = tray_menu_separator;
    new_menu[index++] = tray_menu_refresh;
    new_menu[index++] = tray_menu_quit;
    new_menu[index++] = tray_menu_terminator;

    tray.menu = new_menu;
    free(prev_menu);
}

static BOOL add_device(LPWSTR path)
{
    if (active_device_count == MAX_ACTIVE_DEVICE_COUNT)
    {
        tray_show_notification(NT_TRAY_WARNING, TEXT("Stadia Controller error"),
                               TEXT("Device count limit reached"));
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
        {
            device = hid_open_device(path, TRUE, TRUE);
        }
    }

    if (device == NULL)
    {
        tray_show_notification(NT_TRAY_WARNING, TEXT("Stadia Controller error"),
                               TEXT("Error opening new device"));
        return FALSE;
    }

    struct stadia_controller *controller = stadia_controller_create(device);
    if (controller == NULL)
    {
        tray_show_notification(NT_TRAY_WARNING, TEXT("Stadia Controller error"),
                               TEXT("Error initializing new device"));
        hid_close_device(device);
        hid_free_device(device);
        return FALSE;
    }

    struct active_device *active_device = (struct active_device *)malloc(sizeof(struct active_device));
    active_device->src_device = device;
    active_device->controller = controller;

    if (vigem_connected)
    {
        active_device->tgt_device = vigem_target_x360_alloc();
        vigem_target_add(vigem_client, active_device->tgt_device);
        XUSB_REPORT_INIT(&active_device->tgt_report);
        vigem_target_x360_register_notification(vigem_client, active_device->tgt_device, x360_notification_cb,
                                                (LPVOID)active_device);
    }

    AcquireSRWLockExclusive(&active_devices_lock);
    active_devices[active_device_count++] = active_device;
    ReleaseSRWLockExclusive(&active_devices_lock);

    rebuild_tray_menu();
    tray_update(&tray);

    if (!vigem_connected)
    {
        tray_show_notification(NT_TRAY_WARNING, TEXT("Stadia Controller error"),
                               TEXT("Device added, but emulation doesn't work due to ViGEmBus problem"));
    }

    return TRUE;
}

static BOOL remove_device(struct stadia_controller *controller)
{
    BOOL removed = FALSE;

    AcquireSRWLockExclusive(&active_devices_lock);

    for (int i = 0; i < active_device_count; i++)
    {
        if (active_devices[i]->controller == controller)
        {
            hid_close_device(active_devices[i]->src_device);
            hid_free_device(active_devices[i]->src_device);

            if (vigem_connected)
            {
                vigem_target_x360_unregister_notification(active_devices[i]->tgt_device);
                vigem_target_remove(vigem_client, active_devices[i]->tgt_device);
                vigem_target_free(active_devices[i]->tgt_device);
            }

            free(active_devices[i]);

            if (i < active_device_count - 1)
            {
                memmove(&active_devices[i], &active_devices[i + 1],
                        sizeof(struct active_device *) * (active_device_count - i - 1));
            }

            active_device_count--;
            removed = TRUE;

            break;
        }
    }

    ReleaseSRWLockExclusive(&active_devices_lock);

    return removed;
}

static void refresh_devices()
{
    LPWSTR stadia_hw_path_filters[3] = {STADIA_USB_HW_FILTER, STADIA_BLT_HW_FILTER, NULL};
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
            if (wcscmp(active_devices[i]->src_device->path, cur->path) == 0)
            {
                found = TRUE;
                break;
            }

            cur = cur->next;
        }

        if (!found)
        {
            stadia_controller_destroy(active_devices[i]->controller, STADIA_BREAK_REASON_REQUESTED);
        }
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
            if (wcscmp(cur->path, active_devices[i]->src_device->path) == 0)
            {
                found = TRUE;
                break;
            }
        }
        ReleaseSRWLockShared(&active_devices_lock);
        if (!found)
        {
            add_device(cur->path);
        }
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

static void device_change_cb()
{
    refresh_devices();
}

static void stadia_controller_update_cb(struct stadia_controller *controller, struct stadia_state *state)
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
    {
        return;
    }

    if (vigem_connected)
    {
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
}

static void stadia_controller_stop_cb(struct stadia_controller *controller, BYTE break_reason)
{
    UNREFERENCED_PARAMETER(break_reason);
    if (remove_device(controller))
    {
        rebuild_tray_menu();
        tray_update(&tray);
    }
}

static void CALLBACK x360_notification_cb(PVIGEM_CLIENT client, PVIGEM_TARGET target, UCHAR large_motor,
                                          UCHAR small_motor, UCHAR led_number, LPVOID user_data)
{
    UNREFERENCED_PARAMETER(led_number);
    UNREFERENCED_PARAMETER(target);
    UNREFERENCED_PARAMETER(client);

    struct active_device *active_device = (struct active_device *)user_data;
    stadia_controller_set_vibration(active_device->controller, small_motor, large_motor);
}

static void refresh_cb(struct tray_menu *item)
{
    (void)item;
    fflush(stdout);
    refresh_devices();
}

static void quit_cb(struct tray_menu *item)
{
    (void)item;
    fflush(stdout);
    tray_exit();
}

INT main()
{
    rebuild_tray_menu();
    if (tray_init(&tray) < 0)
    {
        printf("Failed to create tray\n");
        return 1;
    }
    vigem_client = vigem_alloc();
    VIGEM_ERROR vigem_res = vigem_connect(vigem_client);
    if (vigem_res == VIGEM_ERROR_BUS_NOT_FOUND)
    {
        tray_show_notification(NT_TRAY_ERROR, TEXT("Stadia Controller error"),
                               TEXT("ViGEmBus not installed"));
    }
    else if (vigem_res == VIGEM_ERROR_BUS_VERSION_MISMATCH)
    {
        tray_show_notification(NT_TRAY_ERROR, TEXT("Stadia Controller error"),
                               TEXT("ViGEmBus incompatible version"));
    }
    else if (vigem_res != VIGEM_ERROR_NONE)
    {
        tray_show_notification(NT_TRAY_ERROR, TEXT("Stadia Controller error"),
                               TEXT("Error connecting to ViGEmBus"));
    }
    else
    {
        vigem_connected = TRUE;
    }

    stadia_update_callback = stadia_controller_update_cb;
    stadia_destroy_callback = stadia_controller_stop_cb;

    refresh_devices();
    tray_register_device_notification(hid_get_class(), device_change_cb);

    while (tray_loop(TRUE) == 0)
    {
        ;
    }

    AcquireSRWLockExclusive(&active_devices_lock);
    for (INT i = 0; i < active_device_count; i++)
    {
        hid_close_device(active_devices[i]->src_device);
        hid_free_device(active_devices[i]->src_device);
        if (vigem_connected)
        {
            vigem_target_x360_unregister_notification(active_devices[i]->tgt_device);
            vigem_target_remove(vigem_client, active_devices[i]->tgt_device);
            vigem_target_free(active_devices[i]->tgt_device);
        }
        free(active_devices[i]);
    }
    active_device_count = 0;
    ReleaseSRWLockExclusive(&active_devices_lock);
    if (vigem_connected)
    {
        vigem_disconnect(vigem_client);
    }
    vigem_free(vigem_client);
    free(tray.menu);
    return 0;
}
