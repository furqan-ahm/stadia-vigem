/*
 * stadia.c -- Routines for interacting with a Stadia controller.
 */

#include "stadia.h"

#include "hid.h"

#include <stdio.h>
#include <synchapi.h>
#include <tchar.h>
#include <windows.h>

#pragma comment(lib, "kernel32.lib")

#define STADIA_READ_TIMEOUT 100

/*
 * Stadia controller vibration output report identifier.
 */
#define STADIA_VIBRATION_IDENTIFIER 0x05

static const BYTE init_vibration[5] = {STADIA_VIBRATION_IDENTIFIER, 0x00, 0x00, 0x00, 0x00};

static const DWORD dpad_map[8] =
    {
        STADIA_BUTTON_UP,
        STADIA_BUTTON_UP | STADIA_BUTTON_RIGHT,
        STADIA_BUTTON_RIGHT,
        STADIA_BUTTON_RIGHT | STADIA_BUTTON_DOWN,
        STADIA_BUTTON_DOWN,
        STADIA_BUTTON_DOWN | STADIA_BUTTON_LEFT,
        STADIA_BUTTON_LEFT,
        STADIA_BUTTON_LEFT | STADIA_BUTTON_UP};

static DWORD WINAPI _stadia_input_thread(LPVOID lparam)
{
    struct stadia_controller *controller = (struct stadia_controller *)lparam;
    INT bytes_read = 0;
    BYTE break_reason = STADIA_BREAK_REASON_UNKNOWN;

    while (controller->active)
    {
        while ((bytes_read = hid_get_input_report(controller->device, STADIA_READ_TIMEOUT)) == 0);

        if (!controller->active || bytes_read < 0) {
            break_reason = !controller->active ? STADIA_BREAK_REASON_REQUESTED : STADIA_BREAK_REASON_READ_ERROR;
            break;
        }

        // check packet header
        if (controller->device->input_buffer[0] != 0x03)
            continue;

        AcquireSRWLockExclusive(&controller->state_lock);

        controller->state.buttons = STADIA_BUTTON_NONE;

        controller->state.buttons |= controller->device->input_buffer[1] < 8 ? dpad_map[controller->device->input_buffer[1]] : 0;

        controller->state.buttons |= (controller->device->input_buffer[2] & (1 << 7)) != 0 ? STADIA_BUTTON_RS : 0;
        controller->state.buttons |= (controller->device->input_buffer[2] & (1 << 6)) != 0 ? STADIA_BUTTON_OPTIONS : 0;
        controller->state.buttons |= (controller->device->input_buffer[2] & (1 << 5)) != 0 ? STADIA_BUTTON_MENU : 0;
        controller->state.buttons |= (controller->device->input_buffer[2] & (1 << 4)) != 0 ? STADIA_BUTTON_STADIA_BTN : 0;
        controller->state.buttons |= (controller->device->input_buffer[2] & (1 << 1)) != 0 ? STADIA_BUTTON_ASSISTANT : 0;
        controller->state.buttons |= (controller->device->input_buffer[2] & (1 << 0)) != 0 ? STADIA_BUTTON_SHARE : 0;

        controller->state.buttons |= (controller->device->input_buffer[3] & (1 << 6)) != 0 ? STADIA_BUTTON_A : 0;
        controller->state.buttons |= (controller->device->input_buffer[3] & (1 << 5)) != 0 ? STADIA_BUTTON_B : 0;
        controller->state.buttons |= (controller->device->input_buffer[3] & (1 << 4)) != 0 ? STADIA_BUTTON_X : 0;
        controller->state.buttons |= (controller->device->input_buffer[3] & (1 << 3)) != 0 ? STADIA_BUTTON_Y : 0;
        controller->state.buttons |= (controller->device->input_buffer[3] & (1 << 2)) != 0 ? STADIA_BUTTON_LB : 0;
        controller->state.buttons |= (controller->device->input_buffer[3] & (1 << 1)) != 0 ? STADIA_BUTTON_RB : 0;
        controller->state.buttons |= (controller->device->input_buffer[3] & (1 << 0)) != 0 ? STADIA_BUTTON_LS : 0;

        controller->state.left_stick_x = controller->device->input_buffer[4];
        controller->state.left_stick_y = controller->device->input_buffer[5];

        controller->state.right_stick_x = controller->device->input_buffer[6];
        controller->state.right_stick_y = controller->device->input_buffer[7];

        controller->state.left_trigger = controller->device->input_buffer[8];
        controller->state.right_trigger = controller->device->input_buffer[9];

        ReleaseSRWLockExclusive(&controller->state_lock);

        stadia_update_callback(controller, &controller->state);
    }

    stadia_controller_destroy(controller, break_reason);

    return 0;
}

static DWORD WINAPI _stadia_output_thread(LPVOID lparam)
{
    struct stadia_controller *controller = (struct stadia_controller *)lparam;

    BYTE vibration[5] = {STADIA_VIBRATION_IDENTIFIER, 0x0, 0x0, 0x0, 0x0};

    HANDLE wait_events[2] = {controller->output_event, controller->stopping_event};

    while (controller->active)
    {
        DWORD wait_result = WaitForMultipleObjects(2, wait_events, FALSE, INFINITE);

        AcquireSRWLockShared(&controller->vibration_lock);

        vibration[2] = controller->big_motor;
        vibration[4] = controller->small_motor;

        ReleaseSRWLockShared(&controller->vibration_lock);

        hid_send_output_report(controller->device, vibration, sizeof(vibration), STADIA_READ_TIMEOUT);

        ResetEvent(controller->output_event);
    }

    hid_send_output_report(controller->device, init_vibration, sizeof(init_vibration), STADIA_READ_TIMEOUT);

    return 0;
}

struct stadia_controller *stadia_controller_create(struct hid_device *device)
{
    if (!hid_send_output_report(device, init_vibration, sizeof(init_vibration), STADIA_READ_TIMEOUT) <= 0)
    {
        // Don't error out for now.
        // TODO: Fix vibration for bluetooth controllers.
        //return NULL;
    }

    SECURITY_ATTRIBUTES security = {.nLength = sizeof(SECURITY_ATTRIBUTES),
                                    .lpSecurityDescriptor = NULL,
                                    .bInheritHandle = TRUE};

    struct stadia_controller *controller = (struct stadia_controller *)malloc(sizeof(struct stadia_controller));
    controller->device = device;
    controller->active = TRUE;

    // Create locks.
    InitializeSRWLock(&controller->state_lock);
    InitializeSRWLock(&controller->vibration_lock);

    // Create events.
    controller->stopping_event = CreateEvent(&security, TRUE, FALSE, NULL);
    controller->output_event = CreateEvent(&security, TRUE, FALSE, NULL);

    // Create threads.
    controller->input_thread = CreateThread(&security, 0, _stadia_input_thread, controller, CREATE_SUSPENDED, NULL);
    controller->output_thread = CreateThread(&security, 0, _stadia_output_thread, controller, CREATE_SUSPENDED, NULL);

    if (controller->input_thread == NULL || controller->output_thread == NULL)
    {
        stadia_controller_destroy(controller, STADIA_BREAK_REASON_INIT_ERROR);
        return NULL;
    }

    ResumeThread(controller->input_thread);
    ResumeThread(controller->output_thread);

    return controller;
}

void stadia_controller_set_vibration(struct stadia_controller *controller, BYTE small_motor, BYTE big_motor)
{
    AcquireSRWLockExclusive(&controller->vibration_lock);

    controller->small_motor = small_motor;
    controller->big_motor = big_motor;

    ReleaseSRWLockExclusive(&controller->vibration_lock);

    SetEvent(controller->output_event);
}

void stadia_controller_destroy(struct stadia_controller *controller, BYTE break_reason)
{
    CancelIoEx(controller->device->handle, &controller->device->input_ol);

    controller->active = FALSE;
    SetEvent(controller->stopping_event);

    INT thread_count = 0;
    HANDLE threads[2];

    WaitForMultipleObjects(2, threads, TRUE, INFINITE);

    if (controller->input_thread != NULL)
    {
        threads[thread_count++] = controller->input_thread;
    }

    if (controller->output_thread != NULL)
    {
        threads[thread_count++] = controller->output_thread;
    }

    CloseHandle(controller->stopping_event);
    CloseHandle(controller->output_event);

    for (INT i = 0; i < thread_count; i++)
    {
        CloseHandle(threads[i]);
    }

    stadia_destroy_callback(controller, break_reason);

    free(controller);
}