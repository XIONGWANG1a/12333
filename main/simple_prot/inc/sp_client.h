#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>

namespace sp_client
{
    inline HANDLE g_Handle = INVALID_HANDLE_VALUE;
    inline HANDLE g_TargetHandle = NULL;

    inline bool init()
    {
        if (g_Handle != INVALID_HANDLE_VALUE) return true;
        HANDLE h = CreateFileW(
            SP_USER_PATH,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            0, nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        HANDLE old = (HANDLE)InterlockedCompareExchangePointer(
            (PVOID volatile*)&g_Handle, (PVOID)h, (PVOID)INVALID_HANDLE_VALUE);
        if (old != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
        }
        return true;
    }

    inline void shutdown()
    {
        if (g_TargetHandle) {
            SP_CLOSE_REQ req = { g_TargetHandle };
            DWORD bytes = 0;
            DeviceIoControl(g_Handle, IOCTL_SP_CLOSE,
                &req, sizeof(req), &bytes, sizeof(bytes), nullptr, nullptr);
            g_TargetHandle = NULL;
        }
        if (g_Handle != INVALID_HANDLE_VALUE) {
            CloseHandle(g_Handle);
            g_Handle = INVALID_HANDLE_VALUE;
        }
    }

    inline bool openProcess(DWORD pid, DWORD access)
    {
        if (!init()) return false;
        if (g_TargetHandle) {
            SP_CLOSE_REQ req = { g_TargetHandle };
            DWORD bytes = 0;
            DeviceIoControl(g_Handle, IOCTL_SP_CLOSE,
                &req, sizeof(req), &bytes, sizeof(bytes), nullptr, nullptr);
            g_TargetHandle = NULL;
        }
        SP_OPEN_REQ req = { pid, access };
        SP_OPEN_RES res = {};
        DWORD bytes = 0;
        bool ok = DeviceIoControl(g_Handle, IOCTL_SP_OPEN,
            &req, sizeof(req),
            &res, sizeof(res),
            &bytes, nullptr);
        if (ok && NT_SUCCESS(res.Status)) {
            g_TargetHandle = (HANDLE)res.Handle;
            return true;
        }
        return false;
    }

    template<typename T>
    inline bool read(ULONG_PTR address, T& out, DWORD size)
    {
        if (!init() || !g_TargetHandle || size == 0 || size > SP_MAX_XFER) return false;
        SP_MEM_REQ req = { g_TargetHandle, address, size };
        BYTE buf[SP_MAX_XFER] = {};
        DWORD bytes = 0;
        bool ok = DeviceIoControl(g_Handle, IOCTL_SP_READ,
            &req, sizeof(req),
            buf, sizeof(buf),
            &bytes, nullptr);
        if (ok && bytes >= size) {
            memcpy(&out, buf, size);
            return true;
        }
        return false;
    }

    template<typename T>
    inline bool read(ULONG_PTR address, T& out)
    {
        if (sizeof(T) > SP_MAX_XFER) return false;
        return read(address, out, (DWORD)sizeof(T));
    }

    inline bool readRaw(void* buffer, ULONG_PTR address, DWORD size)
    {
        if (!init() || !g_TargetHandle || !buffer || size == 0 || size > SP_MAX_XFER) return false;
        SP_MEM_REQ req = { g_TargetHandle, address, size };
        BYTE buf[SP_MAX_XFER] = {};
        DWORD bytes = 0;
        bool ok = DeviceIoControl(g_Handle, IOCTL_SP_READ,
            &req, sizeof(req),
            buf, sizeof(buf),
            &bytes, nullptr);
        if (ok && bytes >= size) {
            memcpy(buffer, buf, size);
            return true;
        }
        return false;
    }

    inline bool writeRaw(ULONG_PTR address, const void* data, DWORD size)
    {
        if (!init() || !g_TargetHandle || !data || size == 0 || size > SP_MAX_XFER) return false;
        BYTE buf[sizeof(SP_MEM_REQ) + SP_MAX_XFER] = {};
        SP_MEM_REQ* req = (SP_MEM_REQ*)buf;
        req->Handle = g_TargetHandle;
        req->Address = address;
        req->Size = size;
        memcpy(buf + sizeof(SP_MEM_REQ), data, size);
        DWORD bytes = 0;
        return DeviceIoControl(g_Handle, IOCTL_SP_WRITE,
            buf, sizeof(SP_MEM_REQ) + size,
            nullptr, 0,
            &bytes, nullptr);
    }

    template<typename T>
    inline bool write(ULONG_PTR address, const T& val)
    {
        if (sizeof(T) > SP_MAX_XFER) return false;
        return writeRaw(address, &val, (DWORD)sizeof(T));
    }

    inline bool ping()
    {
        if (!init()) return false;
        NTSTATUS st = STATUS_UNSUCCESSFUL;
        DWORD bytes = 0;
        return DeviceIoControl(g_Handle, IOCTL_SP_PING,
            nullptr, 0,
            &st, sizeof(st),
            &bytes, nullptr) && NT_SUCCESS(st);
    }
}
