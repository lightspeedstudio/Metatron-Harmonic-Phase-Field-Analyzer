#include "metatron_api.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mt_port {
    HANDLE handle;
    char name[16];
};

uint64_t mt_now_unix_ms(void)
{
    FILETIME ft;
    ULARGE_INTEGER ticks;
    GetSystemTimePreciseAsFileTime(&ft);
    ticks.LowPart = ft.dwLowDateTime;
    ticks.HighPart = ft.dwHighDateTime;
    return (ticks.QuadPart - 116444736000000000ULL) / 10000ULL;
}

int mt_port_path(const char *name, char *out, size_t cap)
{
    const char *number;
    size_t i;
    if (name == NULL || out == NULL || cap < 8) {
        return MT_ERR_BAD_ARG;
    }
    if (strncmp(name, "\\\\.\\", 4) == 0) {
        name += 4;
    }
    if (!((name[0] == 'C' || name[0] == 'c') &&
          (name[1] == 'O' || name[1] == 'o') &&
          (name[2] == 'M' || name[2] == 'm'))) {
        return MT_ERR_BAD_ARG;
    }
    number = name + 3;
    if (number[0] < '1' || number[0] > '9') {
        return MT_ERR_BAD_ARG;
    }
    for (i = 1; number[i] != '\0'; i++) {
        if (number[i] < '0' || number[i] > '9' || i > 2) {
            return MT_ERR_BAD_ARG;
        }
    }
    if ((size_t)snprintf(out, cap, "\\\\.\\COM%s", number) >= cap) {
        return MT_ERR_BAD_ARG;
    }
    return MT_OK;
}

int mt_port_list(char names[][16], int cap, int *count)
{
    HKEY key;
    DWORD index;
    LONG result;
    int n = 0;
    if (names == NULL || count == NULL || cap < 1) {
        return MT_ERR_BAD_ARG;
    }
    *count = 0;
    result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key);
    if (result != ERROR_SUCCESS) {
        return MT_OK;
    }
    for (index = 0; n < cap; index++) {
        char value_name[256];
        BYTE data[32];
        DWORD name_len = sizeof(value_name);
        DWORD data_len = sizeof(data);
        DWORD type = 0;
        size_t text_len;
        result = RegEnumValueA(key, index, value_name, &name_len, NULL, &type, data, &data_len);
        if (result == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (result != ERROR_SUCCESS || type != REG_SZ || data_len == 0 || data_len >= sizeof(data)) {
            continue;
        }
        if (data[data_len - 1] == 0) {
            text_len = data_len - 1;
        } else {
            text_len = data_len;
            data[text_len] = 0;
        }
        if (text_len == 0 || text_len >= 16) {
            continue;
        }
        memcpy(names[n], data, text_len);
        names[n][text_len] = '\0';
        n++;
    }
    RegCloseKey(key);
    *count = n;
    return MT_OK;
}

int mt_port_open(const char *name, mt_port **out)
{
    char path[32];
    HANDLE handle;
    DCB dcb;
    COMMTIMEOUTS timeouts;
    mt_port *port;
    DWORD error;
    int rc;
    if (name == NULL || out == NULL) {
        return MT_ERR_BAD_ARG;
    }
    rc = mt_port_path(name, path, sizeof(path));
    if (rc != MT_OK) {
        return rc;
    }
    handle = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION) {
            return MT_ERR_PORT_IN_USE;
        }
        return MT_ERR_IO;
    }
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle, &dcb)) {
        CloseHandle(handle);
        return MT_ERR_IO;
    }
    dcb.BaudRate = 115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    if (!SetCommState(handle, &dcb)) {
        CloseHandle(handle);
        return MT_ERR_IO;
    }
    EscapeCommFunction(handle, CLRDTR);
    EscapeCommFunction(handle, CLRRTS);
    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 1000;
    if (!SetCommTimeouts(handle, &timeouts)) {
        CloseHandle(handle);
        return MT_ERR_IO;
    }
    port = (mt_port *)calloc(1, sizeof(*port));
    if (port == NULL) {
        CloseHandle(handle);
        return MT_ERR_IO;
    }
    port->handle = handle;
    snprintf(port->name, sizeof(port->name), "%s", name);
    *out = port;
    return MT_OK;
}

void mt_port_close(mt_port *port)
{
    if (port == NULL) {
        return;
    }
    if (port->handle != INVALID_HANDLE_VALUE && port->handle != NULL) {
        CloseHandle(port->handle);
    }
    free(port);
}

int mt_port_write(mt_port *port, const uint8_t *data, size_t len)
{
    DWORD written = 0;
    if (port == NULL || data == NULL || len == 0) {
        return MT_ERR_BAD_ARG;
    }
    if (len >= 3 && data[0] == 0xAA && (data[2] == 0x04 || data[2] == 0x09)) {
        return MT_ERR_BAD_ARG;
    }
    if (!WriteFile(port->handle, data, (DWORD)len, &written, NULL) || written != len) {
        return MT_ERR_IO;
    }
    return MT_OK;
}

int mt_port_read(mt_port *port, uint8_t *buf, size_t cap, size_t *nread, int timeout_ms)
{
    COMMTIMEOUTS timeouts;
    DWORD got = 0;
    if (port == NULL || buf == NULL || nread == NULL || cap == 0 || timeout_ms < 0) {
        return MT_ERR_BAD_ARG;
    }
    *nread = 0;
    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = (DWORD)timeout_ms;
    timeouts.WriteTotalTimeoutConstant = 1000;
    if (!SetCommTimeouts(port->handle, &timeouts)) {
        return MT_ERR_IO;
    }
    if (!ReadFile(port->handle, buf, (DWORD)cap, &got, NULL)) {
        return MT_ERR_IO;
    }
    *nread = got;
    if (got == 0) {
        return MT_ERR_TIMEOUT;
    }
    return MT_OK;
}
