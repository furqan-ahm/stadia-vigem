/*
 * hidhide.c -- HidHide driver integration implementation.
 */

#include "hidhide.h"

#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <tchar.h>

#pragma comment(lib, "kernel32.lib")

/*
 * HidHide control device path
 */
#define HIDHIDE_DEVICE_PATH L"\\\\.\\HidHide"

/*
 * HidHide I/O control custom device type (range 32768 .. 65535)
 */
#define HIDHIDE_IOCTL_DEVICE_TYPE 32769

/*
 * HidHide IOCTL codes
 */
#define IOCTL_HIDHIDE_GET_WHITELIST CTL_CODE(HIDHIDE_IOCTL_DEVICE_TYPE, 2048, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_HIDHIDE_SET_WHITELIST CTL_CODE(HIDHIDE_IOCTL_DEVICE_TYPE, 2049, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_HIDHIDE_GET_BLACKLIST CTL_CODE(HIDHIDE_IOCTL_DEVICE_TYPE, 2050, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_HIDHIDE_SET_BLACKLIST CTL_CODE(HIDHIDE_IOCTL_DEVICE_TYPE, 2051, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_HIDHIDE_GET_ACTIVE    CTL_CODE(HIDHIDE_IOCTL_DEVICE_TYPE, 2052, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_HIDHIDE_SET_ACTIVE    CTL_CODE(HIDHIDE_IOCTL_DEVICE_TYPE, 2053, METHOD_BUFFERED, FILE_READ_DATA)

/*
 * Maximum number of devices we can track as hidden by us
 */
#define MAX_HIDDEN_DEVICES 8

/*
 * Maximum path length for device instance paths
 */
#define MAX_INSTANCE_PATH_LEN 512

/*
 * Module state
 */
static HANDLE g_hidhide_handle = INVALID_HANDLE_VALUE;
static BOOL g_initialized = FALSE;
static WCHAR g_hidden_devices[MAX_HIDDEN_DEVICES][MAX_INSTANCE_PATH_LEN];
static int g_hidden_device_count = 0;

/*
 * Convert a DOS path (C:\...) to NT device path (\Device\HarddiskVolumeN\...)
 */
static BOOL get_nt_path_from_dos_path(LPCWSTR dos_path, LPWSTR nt_path, DWORD nt_path_size)
{
    WCHAR drive[3] = { dos_path[0], dos_path[1], L'\0' };
    WCHAR device_name[MAX_PATH];
    
    if (QueryDosDeviceW(drive, device_name, MAX_PATH) == 0)
    {
        return FALSE;
    }
    
    // Combine device name with the rest of the path (skip the drive letter)
    _snwprintf(nt_path, nt_path_size, L"%s%s", device_name, dos_path + 2);
    return TRUE;
}

/*
 * Get the full path of the current executable in NT device path format
 */
static BOOL get_current_exe_nt_path(LPWSTR nt_path, DWORD nt_path_size)
{
    WCHAR dos_path[MAX_PATH];
    
    if (GetModuleFileNameW(NULL, dos_path, MAX_PATH) == 0)
    {
        return FALSE;
    }
    
    return get_nt_path_from_dos_path(dos_path, nt_path, nt_path_size);
}

/*
 * Parse a double-null-terminated multi-string into individual strings.
 * Returns the count of strings found.
 */
static int parse_multi_string(LPCWSTR multi_string, DWORD buffer_size_chars, 
                              LPWSTR* strings, int max_strings)
{
    int count = 0;
    LPCWSTR current = multi_string;
    
    while (*current != L'\0' && count < max_strings)
    {
        strings[count++] = (LPWSTR)current;
        current += wcslen(current) + 1;
        
        // Safety check to not read past buffer
        if ((DWORD)(current - multi_string) >= buffer_size_chars)
        {
            break;
        }
    }
    
    return count;
}

/*
 * Build a double-null-terminated multi-string from an array of strings.
 * Returns the total size in characters (including all null terminators).
 */
static DWORD build_multi_string(LPWSTR* strings, int count, LPWSTR buffer, DWORD buffer_size_chars)
{
    DWORD offset = 0;
    
    for (int i = 0; i < count; i++)
    {
        size_t len = wcslen(strings[i]);
        if (offset + len + 1 >= buffer_size_chars)
        {
            break;
        }
        wcscpy(buffer + offset, strings[i]);
        offset += (DWORD)(len + 1);
    }
    
    // Add final null terminator
    if (offset < buffer_size_chars)
    {
        buffer[offset] = L'\0';
        offset++;
    }
    
    return offset;
}

/*
 * Get the current whitelist from HidHide
 */
static BOOL get_whitelist(LPWSTR buffer, DWORD buffer_size_bytes, DWORD* bytes_returned)
{
    if (g_hidhide_handle == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }
    
    // First call to get required size
    DWORD needed = 0;
    DeviceIoControl(g_hidhide_handle, IOCTL_HIDHIDE_GET_WHITELIST,
                    NULL, 0, NULL, 0, &needed, NULL);
    
    if (needed > buffer_size_bytes)
    {
        *bytes_returned = needed;
        return FALSE;
    }
    
    return DeviceIoControl(g_hidhide_handle, IOCTL_HIDHIDE_GET_WHITELIST,
                           NULL, 0, buffer, buffer_size_bytes, bytes_returned, NULL);
}

/*
 * Set the whitelist in HidHide
 */
static BOOL set_whitelist(LPCWSTR buffer, DWORD buffer_size_bytes)
{
    if (g_hidhide_handle == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }
    
    DWORD bytes_returned = 0;
    return DeviceIoControl(g_hidhide_handle, IOCTL_HIDHIDE_SET_WHITELIST,
                           (LPVOID)buffer, buffer_size_bytes, NULL, 0, &bytes_returned, NULL);
}

/*
 * Get the current blacklist from HidHide
 */
static BOOL get_blacklist(LPWSTR buffer, DWORD buffer_size_bytes, DWORD* bytes_returned)
{
    if (g_hidhide_handle == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }
    
    // First call to get required size
    DWORD needed = 0;
    DeviceIoControl(g_hidhide_handle, IOCTL_HIDHIDE_GET_BLACKLIST,
                    NULL, 0, NULL, 0, &needed, NULL);
    
    if (needed > buffer_size_bytes)
    {
        *bytes_returned = needed;
        return FALSE;
    }
    
    return DeviceIoControl(g_hidhide_handle, IOCTL_HIDHIDE_GET_BLACKLIST,
                           NULL, 0, buffer, buffer_size_bytes, bytes_returned, NULL);
}

/*
 * Set the blacklist in HidHide
 */
static BOOL set_blacklist(LPCWSTR buffer, DWORD buffer_size_bytes)
{
    if (g_hidhide_handle == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }
    
    DWORD bytes_returned = 0;
    return DeviceIoControl(g_hidhide_handle, IOCTL_HIDHIDE_SET_BLACKLIST,
                           (LPVOID)buffer, buffer_size_bytes, NULL, 0, &bytes_returned, NULL);
}

/*
 * Get the active state from HidHide
 */
static BOOL get_active(BOOL* active)
{
    if (g_hidhide_handle == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }
    
    BOOLEAN value = FALSE;
    DWORD bytes_returned = 0;
    BOOL result = DeviceIoControl(g_hidhide_handle, IOCTL_HIDHIDE_GET_ACTIVE,
                                  NULL, 0, &value, sizeof(BOOLEAN), &bytes_returned, NULL);
    if (result)
    {
        *active = (value != FALSE);
    }
    return result;
}

/*
 * Set the active state in HidHide
 */
static BOOL set_active(BOOL active)
{
    if (g_hidhide_handle == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }
    
    BOOLEAN value = active ? TRUE : FALSE;
    DWORD bytes_returned = 0;
    return DeviceIoControl(g_hidhide_handle, IOCTL_HIDHIDE_SET_ACTIVE,
                           &value, sizeof(BOOLEAN), NULL, 0, &bytes_returned, NULL);
}

/*
 * Add our application to the whitelist
 */
static BOOL add_self_to_whitelist(void)
{
    WCHAR exe_path[MAX_PATH * 2];
    if (!get_current_exe_nt_path(exe_path, MAX_PATH * 2))
    {
        return FALSE;
    }
    
    // Get current whitelist
    WCHAR whitelist_buffer[8192];
    DWORD bytes_returned = 0;
    
    if (!get_whitelist(whitelist_buffer, sizeof(whitelist_buffer), &bytes_returned))
    {
        // If we can't get the whitelist, try to set just ourselves
        size_t exe_path_len = wcslen(exe_path);
        wcscpy(whitelist_buffer, exe_path);
        whitelist_buffer[exe_path_len + 1] = L'\0';  // Double null terminate
        
        return set_whitelist(whitelist_buffer, (DWORD)((exe_path_len + 2) * sizeof(WCHAR)));
    }
    
    // Parse current whitelist
    LPWSTR entries[256];
    int entry_count = parse_multi_string(whitelist_buffer, bytes_returned / sizeof(WCHAR), entries, 256);
    
    // Check if we're already in the whitelist
    for (int i = 0; i < entry_count; i++)
    {
        if (_wcsicmp(entries[i], exe_path) == 0)
        {
            return TRUE;  // Already whitelisted
        }
    }
    
    // Add ourselves to the whitelist
    entries[entry_count++] = exe_path;
    
    // Build new multi-string
    WCHAR new_whitelist[8192];
    DWORD new_size = build_multi_string(entries, entry_count, new_whitelist, 8192);
    
    return set_whitelist(new_whitelist, new_size * sizeof(WCHAR));
}

/*
 * Add a device to the blacklist
 */
static BOOL add_to_blacklist(LPCWSTR device_instance_path)
{
    // Get current blacklist
    WCHAR blacklist_buffer[8192];
    DWORD bytes_returned = 0;
    
    ZeroMemory(blacklist_buffer, sizeof(blacklist_buffer));
    get_blacklist(blacklist_buffer, sizeof(blacklist_buffer), &bytes_returned);
    
    // Parse current blacklist
    LPWSTR entries[256];
    int entry_count = 0;
    
    if (bytes_returned > 0)
    {
        entry_count = parse_multi_string(blacklist_buffer, bytes_returned / sizeof(WCHAR), entries, 256);
    }
    
    // Check if device is already in the blacklist
    for (int i = 0; i < entry_count; i++)
    {
        if (_wcsicmp(entries[i], device_instance_path) == 0)
        {
            return TRUE;  // Already blacklisted
        }
    }
    
    // Allocate space for the new path (need to copy since we're modifying the list)
    static WCHAR new_path_buffer[MAX_INSTANCE_PATH_LEN];
    wcscpy(new_path_buffer, device_instance_path);
    entries[entry_count++] = new_path_buffer;
    
    // Build new multi-string
    WCHAR new_blacklist[8192];
    DWORD new_size = build_multi_string(entries, entry_count, new_blacklist, 8192);
    
    return set_blacklist(new_blacklist, new_size * sizeof(WCHAR));
}

/*
 * Remove a device from the blacklist
 */
static BOOL remove_from_blacklist(LPCWSTR device_instance_path)
{
    // Get current blacklist
    WCHAR blacklist_buffer[8192];
    DWORD bytes_returned = 0;
    
    if (!get_blacklist(blacklist_buffer, sizeof(blacklist_buffer), &bytes_returned))
    {
        return FALSE;
    }
    
    if (bytes_returned == 0)
    {
        return TRUE;  // Blacklist is empty, nothing to remove
    }
    
    // Parse current blacklist
    LPWSTR entries[256];
    int entry_count = parse_multi_string(blacklist_buffer, bytes_returned / sizeof(WCHAR), entries, 256);
    
    // Find and remove the device
    BOOL found = FALSE;
    for (int i = 0; i < entry_count; i++)
    {
        if (_wcsicmp(entries[i], device_instance_path) == 0)
        {
            // Shift remaining entries
            for (int j = i; j < entry_count - 1; j++)
            {
                entries[j] = entries[j + 1];
            }
            entry_count--;
            found = TRUE;
            break;
        }
    }
    
    if (!found)
    {
        return TRUE;  // Device wasn't in the blacklist
    }
    
    // Build new multi-string
    WCHAR new_blacklist[8192];
    DWORD new_size = build_multi_string(entries, entry_count, new_blacklist, 8192);
    
    // Handle empty list
    if (entry_count == 0)
    {
        new_blacklist[0] = L'\0';
        new_size = 1;
    }
    
    return set_blacklist(new_blacklist, new_size * sizeof(WCHAR));
}

/*
 * Track a device as hidden by us
 */
static void track_hidden_device(LPCWSTR device_instance_path)
{
    if (g_hidden_device_count >= MAX_HIDDEN_DEVICES)
    {
        return;
    }
    
    // Check if already tracked
    for (int i = 0; i < g_hidden_device_count; i++)
    {
        if (_wcsicmp(g_hidden_devices[i], device_instance_path) == 0)
        {
            return;
        }
    }
    
    wcsncpy(g_hidden_devices[g_hidden_device_count], device_instance_path, MAX_INSTANCE_PATH_LEN - 1);
    g_hidden_devices[g_hidden_device_count][MAX_INSTANCE_PATH_LEN - 1] = L'\0';
    g_hidden_device_count++;
}

/*
 * Untrack a device
 */
static void untrack_hidden_device(LPCWSTR device_instance_path)
{
    for (int i = 0; i < g_hidden_device_count; i++)
    {
        if (_wcsicmp(g_hidden_devices[i], device_instance_path) == 0)
        {
            // Shift remaining entries
            for (int j = i; j < g_hidden_device_count - 1; j++)
            {
                wcscpy(g_hidden_devices[j], g_hidden_devices[j + 1]);
            }
            g_hidden_device_count--;
            return;
        }
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

BOOL hidhide_available(void)
{
    HANDLE handle = CreateFileW(
        HIDHIDE_DEVICE_PATH,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (handle == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }
    
    CloseHandle(handle);
    return TRUE;
}

BOOL hidhide_init(void)
{
    if (g_initialized)
    {
        return TRUE;
    }
    
    // Open handle to HidHide control device
    g_hidhide_handle = CreateFileW(
        HIDHIDE_DEVICE_PATH,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (g_hidhide_handle == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }
    
    // Add ourselves to the whitelist so we can still see hidden devices
    if (!add_self_to_whitelist())
    {
        CloseHandle(g_hidhide_handle);
        g_hidhide_handle = INVALID_HANDLE_VALUE;
        return FALSE;
    }
    
    // Ensure hiding is active
    BOOL is_active = FALSE;
    if (get_active(&is_active) && !is_active)
    {
        set_active(TRUE);
    }
    
    g_initialized = TRUE;
    g_hidden_device_count = 0;
    
    return TRUE;
}

void hidhide_cleanup(void)
{
    if (!g_initialized)
    {
        return;
    }
    
    // Unhide all devices we hid
    for (int i = g_hidden_device_count - 1; i >= 0; i--)
    {
        remove_from_blacklist(g_hidden_devices[i]);
    }
    g_hidden_device_count = 0;
    
    // Close handle
    if (g_hidhide_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_hidhide_handle);
        g_hidhide_handle = INVALID_HANDLE_VALUE;
    }
    
    g_initialized = FALSE;
}

BOOL hidhide_hide_device(LPCWSTR device_instance_path)
{
    if (!g_initialized || device_instance_path == NULL)
    {
        return FALSE;
    }
    
    if (!add_to_blacklist(device_instance_path))
    {
        return FALSE;
    }
    
    track_hidden_device(device_instance_path);
    return TRUE;
}

BOOL hidhide_unhide_device(LPCWSTR device_instance_path)
{
    if (!g_initialized || device_instance_path == NULL)
    {
        return FALSE;
    }
    
    if (!remove_from_blacklist(device_instance_path))
    {
        return FALSE;
    }
    
    untrack_hidden_device(device_instance_path);
    return TRUE;
}

BOOL hidhide_is_device_hidden(LPCWSTR device_instance_path)
{
    if (device_instance_path == NULL)
    {
        return FALSE;
    }
    
    for (int i = 0; i < g_hidden_device_count; i++)
    {
        if (_wcsicmp(g_hidden_devices[i], device_instance_path) == 0)
        {
            return TRUE;
        }
    }
    
    return FALSE;
}
