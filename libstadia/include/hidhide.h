/*
 * hidhide.h -- HidHide driver integration for automatic device hiding.
 * 
 * This module provides functions to interact with the HidHide driver
 * to automatically hide Stadia controllers from other applications,
 * preventing double-input issues.
 */

#ifndef HIDHIDE_H
#define HIDHIDE_H

#include <wtypes.h>

/*
 * Check if the HidHide driver is installed and available.
 * Returns TRUE if available, FALSE otherwise.
 */
BOOL hidhide_available(void);

/*
 * Initialize the HidHide integration.
 * This opens a handle to the HidHide control device, adds this application
 * to the whitelist (so we can still see hidden devices), and enables hiding.
 * 
 * Returns TRUE on success, FALSE on failure.
 * If HidHide is not installed, returns FALSE gracefully.
 */
BOOL hidhide_init(void);

/*
 * Clean up the HidHide integration.
 * This unhides all devices that were hidden during this session and closes
 * the handle to the control device.
 */
void hidhide_cleanup(void);

/*
 * Hide a device from other applications.
 * The device will be added to the HidHide blacklist.
 * 
 * @param device_instance_path The Device Instance ID path of the device to hide.
 * @return TRUE on success, FALSE on failure.
 */
BOOL hidhide_hide_device(LPCWSTR device_instance_path);

/*
 * Unhide a device, making it visible to other applications again.
 * The device will be removed from the HidHide blacklist.
 * 
 * @param device_instance_path The Device Instance ID path of the device to unhide.
 * @return TRUE on success, FALSE on failure.
 */
BOOL hidhide_unhide_device(LPCWSTR device_instance_path);

/*
 * Check if a device is currently hidden by us.
 * 
 * @param device_instance_path The Device Instance ID path of the device.
 * @return TRUE if hidden by this session, FALSE otherwise.
 */
BOOL hidhide_is_device_hidden(LPCWSTR device_instance_path);

#endif /* HIDHIDE_H */
