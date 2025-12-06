# Stadia-ViGEm

Xbox 360 controller emulation for Stadia controller. Supports controllers connected via USB & bluetooth. Supports multiple devices and vibration (wired only). Forked from Mi-ViGEm (https://github.com/grayver/Mi-ViGEm) by grayver.
Xbox 360 controller emulation driver is provided by ViGEm (https://github.com/ViGEm/ViGEmBus), by Benjamin Höglinger.

## Requirements
- Windows 11 (should work on Windows 7-10 also)
- ViGEm bus installed (can be downloaded [here](https://github.com/ViGEm/ViGEmBus/releases))

## How it works
Stadia-ViGEm program at start scans for Stadia Controllers and then proxies found Stadia Controllers to virtual Xbox 360 gamepads (with help from ViGEmBus). Also Stadia-ViGEm subscribes to system device plug/unplug notifications and rescans for devices on each notification.
All found devices are displayed in the tray icon context menu. Manual device rescan can be initiated via the tray icon context menu.

## Double input (Automatic HidHide Integration)

Stadia-ViGEm creates a virtual Xbox 360 controller which can result in double input issues when some applications read input from both the virtual and the real Stadia controller.

### Automatic Solution (Recommended)

If you have [HidHide](https://github.com/ViGEm/HidHide) installed, Stadia-ViGEm will **automatically**:
- Hide the Stadia controller when it connects (so games only see the virtual Xbox controller)
- Unhide the controller when it disconnects or when Stadia-ViGEm exits

This means you don't need to manually configure HidHide - it just works!

To use this feature:
1. Install [HidHide](https://github.com/nefarius/HidHide/releases)
2. Run Stadia-ViGEm - that's it!

### Manual Configuration (Fallback)

If automatic hiding doesn't work or you prefer manual control, you can configure HidHide manually:
 - Open HidHide Configuration Client
 - On Applications tab:
   - Click "+" button
   - Browse to the Stadia-ViGEm executable you normally use (Stadia-ViGEm-x86.exe or Stadia-ViGEm-x64.exe)
 - On Devices tab:
   - Tick box next to the Stadia controller entry (wired controllers are named "Google LLC Stadia Controller rev. A" & bluetooth controllers are named "HID-compliant game controller")
   - Tick "Enable device hiding" at the bottom of the window
 - Reboot your PC

Note: With manual configuration, the controller will not be visible to any application when Stadia-ViGEm isn't running.

## Thanks to

grayver, the developer of Mi-ViGEm that makes up 95% of this program.

This project is inspired by following projects written on C#:
- https://github.com/irungentoo/Xiaomi_gamepad
- https://github.com/dancol90/mi-360

Thanks to following libraries and resources:
- https://github.com/libusb/hidapi for HID implementation
- https://github.com/zserge/tray for lightweight tray app implementation
- https://www.flaticon.com/authors/freepik for application icon
