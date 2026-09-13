#ifndef _SIMPLE_PROT_COMMON_H_
#define _SIMPLE_PROT_COMMON_H_

#ifndef _DRIVER_
#include <windows.h>
#endif

#define SP_DEVICE_NAME   L"\\Device\\SimpleProt"
#define SP_SYMBOLIC_LINK L"\\DosDevices\\SimpleProt"
#define SP_USER_PATH     L"\\\\.\\SimpleProt"

#define SP_MAX_XFER 512

#define IOCTL_SP_OPEN  CTL_CODE(0x8001, 0x01, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SP_READ  CTL_CODE(0x8001, 0x02, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_SP_WRITE CTL_CODE(0x8001, 0x03, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SP_CLOSE CTL_CODE(0x8001, 0x04, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SP_PING  CTL_CODE(0x8001, 0x05, METHOD_BUFFERED, FILE_READ_ACCESS)

typedef struct _SP_OPEN_REQ {
    DWORD Pid;
    DWORD Access;
} SP_OPEN_REQ, *PSP_OPEN_REQ;

typedef struct _SP_OPEN_RES {
    ULONG_PTR Handle;
    NTSTATUS Status;
} SP_OPEN_RES, *PSP_OPEN_RES;

typedef struct _SP_MEM_REQ {
    ULONG_PTR Handle;
    ULONG_PTR Address;
    DWORD Size;
} SP_MEM_REQ, *PSP_MEM_REQ;

typedef struct _SP_MEM_RES {
    NTSTATUS Status;
    BYTE Data[SP_MAX_XFER];
} SP_MEM_RES, *PSP_MEM_RES;

typedef struct _SP_CLOSE_REQ {
    ULONG_PTR Handle;
} SP_CLOSE_REQ, *PSP_CLOSE_REQ;

#endif
