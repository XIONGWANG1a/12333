#ifndef _SIMPLE_PROT_COMMON_H_
#define _SIMPLE_PROT_COMMON_H_

#ifndef _DRIVER_
#include <windows.h>
#else
#include <ntddk.h>
#endif

#define SP_DEVICE_NAME       L"\\Device\\SimpleProt"
#define SP_SYMBOLIC_LINK     L"\\DosDevices\\SimpleProt"
#define SP_USER_PATH         L"\\\\.\\SimpleProt"

#define SP_MAX_XFER          4096
#define SP_FILE_TYPE         0x8001

#define IOCTL_SP_OPEN        CTL_CODE(SP_FILE_TYPE, 0x01, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SP_READ        CTL_CODE(SP_FILE_TYPE, 0x02, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_SP_WRITE       CTL_CODE(SP_FILE_TYPE, 0x03, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SP_CLOSE       CTL_CODE(SP_FILE_TYPE, 0x04, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SP_PING        CTL_CODE(SP_FILE_TYPE, 0x05, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_SP_QUERY_CR3   CTL_CODE(SP_FILE_TYPE, 0x06, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_SP_QUERY_EPT   CTL_CODE(SP_FILE_TYPE, 0x07, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_SP_PROTECT_REG CTL_CODE(SP_FILE_TYPE, 0x08, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SP_UNPROTECT   CTL_CODE(SP_FILE_TYPE, 0x09, METHOD_BUFFERED, FILE_WRITE_ACCESS)

typedef struct _SP_OPEN_REQ  { ULONG Pid;   ULONG Access; } SP_OPEN_REQ,  *PSP_OPEN_REQ;
typedef struct _SP_OPEN_RES  { NTSTATUS Status; ULONG_PTR Cookie; } SP_OPEN_RES, *PSP_OPEN_RES;

typedef struct _SP_MEM_REQ   { ULONG_PTR Cookie; ULONG_PTR Address; ULONG Size; } SP_MEM_REQ, *PSP_MEM_REQ;
typedef struct _SP_MEM_RES   { NTSTATUS Status; ULONG Returned; UCHAR Data[SP_MAX_XFER]; } SP_MEM_RES, *PSP_MEM_RES;

typedef struct _SP_WRITE_REQ { ULONG_PTR Cookie; ULONG_PTR Address; ULONG Size;
                               UCHAR Data[SP_MAX_XFER]; } SP_WRITE_REQ, *PSP_WRITE_REQ;

typedef struct _SP_CR3_RES   { ULONG_PTR Cr3; ULONG_PTR DirectoryTableBase; } SP_CR3_RES, *PSP_CR3_RES;

typedef struct _SP_EPT_RES   { ULONG_PTR Eptp; ULONG_PTR Pml4Phys;
                               ULONG64 PageSizeMask; ULONG VmxOnline; } SP_EPT_RES, *PSP_EPT_RES;

typedef struct _SP_PROT_REQ  { ULONG_PTR Address; ULONG Size;
                               ULONG Flags; /* bit0=R bit1=W bit2=X bit3=hide */ } SP_PROT_REQ, *PSP_PROT_REQ;

typedef struct _SP_CLOSE_REQ { ULONG_PTR Cookie; } SP_CLOSE_REQ, *PSP_CLOSE_REQ;

#endif