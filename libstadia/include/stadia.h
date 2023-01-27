/*
 * stadia.h -- Routines for interacting with a Stadia controller.
 */

#ifndef STADIA_H
#define STADIA_H

#include <wtypes.h>

#define STADIA_USB_HW_VENDOR_ID             0x18D1
#define STADIA_USB_HW_PRODUCT_ID            0x9400
#define STADIA_USB_HW_FILTER                TEXT("VID_18D1&PID_9400")

#define STADIA_BLT_HW_VENDOR_ID             0x18D1
#define STADIA_BLT_HW_PRODUCT_ID            0x9400
#define STADIA_BLT_HW_FILTER                TEXT("vid&0218d1_pid&9400")

#define STADIA_BUTTON_NONE                  0b00000000000000000
#define STADIA_BUTTON_A                     0b00000000000000001
#define STADIA_BUTTON_B                     0b00000000000000010
#define STADIA_BUTTON_X                     0b00000000000000100
#define STADIA_BUTTON_Y                     0b00000000000001000
#define STADIA_BUTTON_LB                    0b00000000000010000
#define STADIA_BUTTON_RB                    0b00000000000100000
#define STADIA_BUTTON_LS                    0b00000000001000000
#define STADIA_BUTTON_RS                    0b00000000010000000
#define STADIA_BUTTON_UP                    0b00000000100000000
#define STADIA_BUTTON_DOWN                  0b00000001000000000
#define STADIA_BUTTON_LEFT                  0b00000010000000000
#define STADIA_BUTTON_RIGHT                 0b00000100000000000
#define STADIA_BUTTON_OPTIONS               0b00001000000000000
#define STADIA_BUTTON_MENU                  0b00010000000000000
#define STADIA_BUTTON_STADIA_BTN            0b00100000000000000
#define STADIA_BUTTON_ASSISTANT             0b01000000000000000
#define STADIA_BUTTON_SHARE                 0b10000000000000000

#define STADIA_BREAK_REASON_UNKNOWN         0x0000
#define STADIA_BREAK_REASON_REQUESTED       0x0001
#define STADIA_BREAK_REASON_INIT_ERROR      0x0002
#define STADIA_BREAK_REASON_READ_ERROR      0x0004
#define STADIA_BREAK_REASON_WRITE_ERROR     0x0008

struct stadia_state
{
    DWORD buttons;

    BYTE left_stick_x;
    BYTE left_stick_y;

    BYTE right_stick_x;
    BYTE right_stick_y;

    BYTE left_trigger;
    BYTE right_trigger;
};

struct stadia_controller
{
    struct hid_device *device;

    SRWLOCK state_lock;
    struct stadia_state state;

    BOOL active;
    HANDLE stopping_event;
    HANDLE output_event;

    SRWLOCK vibration_lock;
    BYTE small_motor;
    BYTE big_motor;

    HANDLE input_thread;
    HANDLE output_thread;
};

void (*stadia_update_callback)(struct stadia_controller *, struct stadia_state *);
void (*stadia_destroy_callback)(struct stadia_controller *, BYTE break_reason);

struct stadia_controller *stadia_controller_create(struct hid_device *device);
void stadia_controller_set_vibration(struct stadia_controller *controller, BYTE small_motor, BYTE big_motor);
void stadia_controller_destroy(struct stadia_controller *controller, BYTE break_reason);

#endif // STADIA_H