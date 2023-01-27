#ifndef SERVICE_H
#define SERVICE_H

#define SVCNAME TEXT("Stadia-ViGEm Controller Service")

int service_init(HANDLE service);

void service_quit();

int service_loop(BOOL blocking);

void service_register_device_notification(GUID filter);

void service_log_message(DWORD message, WORD message_type);

#endif /* SERVICE_H */