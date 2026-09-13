// language: C++, file: sp_client.h, toolchain: MSVC /std:c++20 /W4 /permissive-
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include <cstring>
#include "sp_common.h"

namespace sp_client
{
    inline HANDLE    g_Handle     = INVALID_HANDLE_VALUE;
    inline ULONG_PTR g_Cookie     = 0;   // target CR3 returned by driver
    inline uint32_t  g_TargetPid  = 0;

    inline bool init()
    {
        if (g_Handle != INVALID_HANDLE_VALUE) return true;
        HANDLE h = CreateFileW(
            SP_USER_PATH,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        HANDLE old = (HANDLE)InterlockedCompareExchangePointer(
            (PVOID volatile*)&g_Handle, (PVOID)h, (PVOID)INVALID_HANDLE_VALUE);
        if (old != INVALID_HANDLE_VALUE) CloseHandle(h);
        return true;
    }

    inline void shutdown()
    {
        if (g_Handle != INVALID_HANDLE_VALUE) {
            SP_CLOSE_REQ req{ g_Cookie };
            DWORD bytes = 0;
            DeviceIoControl(g_Handle, IOCTL_SP_CLOSE,
                            &req, sizeof(req), &bytes, sizeof(bytes), nullptr, nullptr);
            CloseHandle(g_Handle);
            g_Handle = INVALID_HANDLE_VALUE;
        }
        g_Cookie = 0;
        g_TargetPid = 0;
    }

    inline bool ping()
    {
        if (!init()) return false;
        ULONG magic = 0;
        DWORD bytes = 0;
        if (!DeviceIoControl(g_Handle, IOCTL_SP_PING,
                             nullptr, 0, &magic, sizeof(magic),
                             &bytes, nullptr)) return false;
        return magic == 0x50524F54;   // 'PROT'
    }

    inline bool openProcess(uint32_t pid, uint32_t /*access — driver ignores*/)
    {
        if (!init()) return false;
        SP_OPEN_REQ req{ pid, 0 };
        SP_OPEN_RES res{};
        DWORD bytes = 0;
        if (!DeviceIoControl(g_Handle, IOCTL_SP_OPEN,
                             &req, sizeof(req), &res, sizeof(res),
                             &bytes, nullptr)) return false;
        if (!NT_SUCCESS(res.Status)) return false;
        g_Cookie = res.Cookie;         // driver stores target CR3
        g_TargetPid = pid;
        return true;
    }

    inline bool readRaw(void* buffer, uintptr_t address, uint32_t size)
    {
        if (!init() || !buffer || !size || size > SP_MAX_XFER) return false;
        SP_MEM_REQ req{ g_Cookie, address, size };
        SP_MEM_RES res{};
        DWORD bytes = 0;
        if (!DeviceIoControl(g_Handle, IOCTL_SP_READ,
                             &req, sizeof(req), &res, sizeof(res),
                             &bytes, nullptr)) return false;
        if (!NT_SUCCESS(res.Status) || res.Returned != size) return false;
        std::memcpy(buffer, res.Data, size);
        return true;
    }

    inline bool writeRaw(uintptr_t address, const void* data, uint32_t size)
    {
        if (!init() || !data || !size || size > SP_MAX_XFER) return false;
        SP_WRITE_REQ req{};
        req.Cookie  = g_Cookie;
        req.Address = address;
        req.Size    = size;
        std::memcpy(req.Data, data, size);
        DWORD bytes = 0;
        return DeviceIoControl(g_Handle, IOCTL_SP_WRITE,
                               &req, sizeof(SP_WRITE_REQ) - SP_MAX_XFER + size,
                               nullptr, 0, &bytes, nullptr);
    }

    template <typename T>
    inline bool read(uintptr_t address, T& out)
    {
        static_assert(std::is_trivially_copyable_v<T>, "sp read requires trivially copyable");
        out = T{};
        return readRaw(&out, address, sizeof(T));
    }

    template <typename T>
    inline T read(uintptr_t address)
    {
        T v{};
        read(address, v);
        return v;
    }

    template <typename T>
    inline bool write(uintptr_t address, const T& v)
    {
        static_assert(std::is_trivially_copyable_v<T>, "sp write requires trivially copyable");
        return writeRaw(address, &v, sizeof(T));
    }
}