/*
 * tray.h -- Routines for providing the tray control.
 */

#ifndef TRAY_H
#define TRAY_H

#include <wtypes.h>

#define NT_TRAY_INFO 0
#define NT_TRAY_WARNING 1
#define NT_TRAY_ERROR 2

#define DO_TRAY_UNKNOWN 0
#define DO_TRAY_DEV_ATTACHED 1
#define DO_TRAY_DEV_REMOVED 2

struct tray_menu;

struct tray
{
    LPWSTR icon;
    LPWSTR tip;
    struct tray_menu *menu;
};

struct tray_menu
{
    LPWSTR text;
    BOOLEAN disabled;
    BOOLEAN checked;

    void (*cb)(struct tray_menu *);
    void *context;

    struct tray_menu *submenu;
};

int tray_init(struct tray *tray);
int tray_loop(BOOLEAN blocking);
void tray_update(struct tray *tray);
void tray_exit();
void tray_register_device_notification(GUID filter, void (*cb)());
void tray_show_notification(UINT type, LPWSTR title, LPWSTR text);

#endif /* TRAY_H */
