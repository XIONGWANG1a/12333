// language: C, file: driver.c, runtime: Windows Kernel (WDK x64, Win10 22H2 19045), compile: MSVC /kernel
#include <ntddk.h>
#include <intrin.h>
#include "sp_common.h"

// ================ 22H2 x64 EPROCESS offsets ================
#define OFF_EPROCESS_DIRECTORYTABLEBASE  0x28

// ================ MSRs ================
#define IA32_FEATURE_CONTROL     0x0000003A
#define IA32_VMX_BASIC           0x00000480
#define IA32_VMX_PINBASED_CTLS   0x00000481
#define IA32_VMX_PROCBASED_CTLS  0x00000482
#define IA32_VMX_EXIT_CTLS       0x00000483
#define IA32_VMX_ENTRY_CTLS      0x00000484
#define IA32_VMX_MISC            0x00000485
#define IA32_VMX_CR0_FIXED0      0x00000486
#define IA32_VMX_CR0_FIXED1      0x00000487
#define IA32_VMX_CR4_FIXED0      0x00000488
#define IA32_VMX_CR4_FIXED1      0x00000489
#define IA32_VMX_PROCBASED_CTLS2 0x0000048B
#define IA32_VMX_EPT_VPID_CAP    0x0000048C
#define IA32_VMX_TRUE_PIN        0x0000048D
#define IA32_VMX_TRUE_PROC       0x0000048E
#define IA32_VMX_TRUE_EXIT       0x0000048F
#define IA32_VMX_TRUE_ENTRY      0x00000490
#define IA32_FS_BASE             0xC0000100
#define IA32_GS_BASE             0xC0000101

#define VMXON_REGION_SIZE        4096
#define VMCS_REGION_SIZE         4096
#define HOST_STACK_SIZE          0x8000
#define MSR_BITMAP_SIZE          4096
#define MAX_SHADOW_TABLES        64

#define EPT_MEMTYPE_WB           0x06
#define EPT_FLAG_RWX             (1ULL | (1ULL<<1) | (1ULL<<2))
#define EPT_FLAG_LARGE_PAGE      (1ULL << 7)
#define EPT_PTE_R                (1ULL << 0)
#define EPT_PTE_W                (1ULL << 1)
#define EPT_PTE_X                (1ULL << 2)

// ================ VMCS encodings — SDM vol 3C App B ================
#define VMCS_HOST_CR0             0x6C00
#define VMCS_HOST_CR3             0x6C02
#define VMCS_HOST_CR4             0x6C04
#define VMCS_HOST_FS_BASE         0x6C06
#define VMCS_HOST_GS_BASE         0x6C08
#define VMCS_HOST_TR_BASE         0x6C0A
#define VMCS_HOST_GDTR_BASE       0x6C0C
#define VMCS_HOST_IDTR_BASE       0x6C0E
#define VMCS_HOST_RSP             0x6C14
#define VMCS_HOST_RIP             0x6C16
#define VMCS_HOST_CS_SEL          0x0C02
#define VMCS_HOST_SS_SEL          0x0C04
#define VMCS_HOST_DS_SEL          0x0C06
#define VMCS_HOST_ES_SEL          0x0C08
#define VMCS_HOST_FS_SEL          0x0C0A
#define VMCS_HOST_GS_SEL          0x0C0C
#define VMCS_HOST_TR_SEL          0x0C0E

#define VMCS_GUEST_CR0            0x6800
#define VMCS_GUEST_CR3            0x6802
#define VMCS_GUEST_CR4            0x6804
#define VMCS_GUEST_FS_BASE        0x6806
#define VMCS_GUEST_GS_BASE        0x6808
#define VMCS_GUEST_TR_BASE        0x680A
#define VMCS_GUEST_GDTR_BASE      0x6816
#define VMCS_GUEST_IDTR_BASE      0x6818
#define VMCS_GUEST_RSP            0x681C
#define VMCS_GUEST_RIP            0x681E
#define VMCS_GUEST_RFLAGS         0x6820
#define VMCS_GUEST_DR7            0x681A
#define VMCS_GUEST_SYSENTER_ESP   0x6824
#define VMCS_GUEST_SYSENTER_EIP   0x6826
#define VMCS_GUEST_ES_SEL         0x0800
#define VMCS_GUEST_CS_SEL         0x0802
#define VMCS_GUEST_SS_SEL         0x0804
#define VMCS_GUEST_DS_SEL         0x0806
#define VMCS_GUEST_FS_SEL         0x0808
#define VMCS_GUEST_GS_SEL         0x080A
#define VMCS_GUEST_LDTR_SEL       0x080C
#define VMCS_GUEST_TR_SEL         0x080E
#define VMCS_GUEST_ES_LIMIT       0x4800
#define VMCS_GUEST_CS_LIMIT       0x4802
#define VMCS_GUEST_SS_LIMIT       0x4804
#define VMCS_GUEST_DS_LIMIT       0x4806
#define VMCS_GUEST_FS_LIMIT       0x4808
#define VMCS_GUEST_GS_LIMIT       0x480A
#define VMCS_GUEST_LDTR_LIMIT     0x480C
#define VMCS_GUEST_TR_LIMIT       0x480E
#define VMCS_GUEST_GDTR_LIMIT     0x4810
#define VMCS_GUEST_IDTR_LIMIT     0x4812
#define VMCS_GUEST_ES_AR          0x4814
#define VMCS_GUEST_CS_AR          0x4816
#define VMCS_GUEST_SS_AR          0x4818
#define VMCS_GUEST_DS_AR          0x481A
#define VMCS_GUEST_FS_AR          0x481C
#define VMCS_GUEST_GS_AR          0x481E
#define VMCS_GUEST_LDTR_AR        0x4820
#define VMCS_GUEST_TR_AR          0x4822
#define VMCS_GUEST_INT_STATE      0x4824
#define VMCS_GUEST_ACTIVITY_STATE 0x4826
#define VMCS_GUEST_SMBASE         0x4828

#define VMCS_CTRL_PIN             0x4000
#define VMCS_CTRL_EXEC            0x4002
#define VMCS_CTRL_EXC_BITMAP      0x4004
#define VMCS_CTRL_EXEC2           0x401E
#define VMCS_CTRL_EXIT            0x400C
#define VMCS_CTRL_ENTRY           0x4012
#define VMCS_CTRL_EPTP            0x201A
#define VMCS_CTRL_VPID            0x0000
#define VMCS_CTRL_MSR_BITMAP      0x2004
#define VMCS_CTRL_TSC_OFFSET      0x2010
#define VMCS_GUEST_CR0_MASK       0x6000
#define VMCS_GUEST_CR0_SHADOW     0x6004
#define VMCS_GUEST_CR4_MASK       0x6002
#define VMCS_GUEST_CR4_SHADOW     0x6006

#define VMCS_EXIT_REASON          0x4402
#define VMCS_EXIT_QUAL            0x4404
#define VMCS_GUEST_LINEAR         0x440A
#define VMCS_GUEST_PHYSICAL       0x2400
#define VMCS_EXIT_INSTR_LEN       0x440C
#define VMCS_EXIT_INFO            0x440E

#define VMX_EXIT_EPT_VIOLATION    48
#define VMX_EXIT_VMCALL           18
#define VMX_EXIT_CPUID            10
#define VMX_EXIT_RDMSR            31
#define VMX_EXIT_WRMSR            32

#define EXIT_QUAL_DATA_READ       (1ULL << 0)
#define EXIT_QUAL_DATA_WRITE      (1ULL << 1)
#define EXIT_QUAL_LIN_VALID       (1ULL << 2)
#define EXIT_QUAL_INSTR_FETCH     (1ULL << 3)
#define EXIT_QUAL_GPA_VALID       (1ULL << 7)

// ================ VMCALL protocol ================
#define VT_MAGIC                  0x50524F54ULL   // 'PROT'
#define VT_OP_PING                1
#define VT_OP_READ                2
#define VT_OP_WRITE               3
#define VT_OP_PROTECT             4
#define VT_OP_TEARDOWN            0xFF

// ================ structures ================
typedef struct _DESCRIPTOR_TABLE { UINT16 Limit; UINT64 Base; } DESCRIPTOR_TABLE;
typedef struct _INVEPT_DESC      { UINT64 Eptp;  UINT64 Reserved; } INVEPT_DESC;

#pragma pack(push, 1)
typedef union _EPT_PTE {
    struct {
        UINT64 Read:1, Write:1, Execute:1, MemType:3, IgnorePAT:1,
               Accessed:1, Dirty:1, UserExec:1, Reserved:2,
               PhysAddr:40, Ignored:11, NoExecute:1;
    } Fields; UINT64 All;
} EPT_PTE;

typedef union _EPT_PDE_2MB {
    struct {
        UINT64 Read:1, Write:1, Execute:1, MemType:3, IgnorePAT:1,
               Accessed:1, Dirty:1, UserExec:1, Reserved:9,
               PhysAddr:31, Ignored:12, NoExecute:1;
    } Fields; UINT64 All;
} EPT_PDE_2MB;

typedef union _EPT_PDPTE {
    struct {
        UINT64 Read:1, Write:1, Execute:1, MemType:3, IgnorePAT:1,
               Accessed:1, Dirty:1, UserExec:1, Reserved:3,
               PhysAddr:40, Ignored:11, NoExecute:1;
    } Fields; UINT64 All;
} EPT_PDPTE;

typedef union _EPT_PML4E {
    struct {
        UINT64 Read:1, Write:1, Execute:1, MemType:3, IgnorePAT:1,
               Accessed:1, Dirty:1, UserExec:1, Reserved:3,
               PhysAddr:40, Ignored:11, NoExecute:1;
    } Fields; UINT64 All;
} EPT_PML4E;
#pragma pack(pop)

typedef struct _EPT_STATE {
    PVOID   Pml4, Pdpt, Pd;
    UINT64  Pml4Phys, PdptPhys, PdPhys;
    UINT64  Eptp;
} EPT_STATE;

typedef struct _SHADOW_TABLE {
    PVOID   PtVa;
    UINT64  PtPhys;
    UINT64  GpaBase;
    PVOID   ShadowVa;
    UINT64  ShadowPhys;
    BOOLEAN Active;
} SHADOW_TABLE, *PSHADOW_TABLE;

typedef struct _VMX_STATE {
    PVOID   VmxonVa, VmcsVa, HostStack, MsrBitmapVa;
    UINT64  VmxonPhys, VmcsPhys;
    BOOLEAN Launched, Initialized;
} VMX_STATE;

typedef struct _GUEST_REGS {
    ULONG64 rax, rbx, rcx, rdx, rsi, rdi, rbp;
    ULONG64 r8, r9, r10, r11, r12, r13, r14, r15;
} GUEST_REGS, *PGUEST_REGS;

extern "C" VOID       VmExitHandlerC(PGUEST_REGS regs);
extern "C" VOID       VmxExitEntry(VOID);
extern "C" ULONG_PTR  VmxLaunchEntry(VOID);
extern "C" VOID       VmxResumePoint(VOID);
extern "C" ULONG_PTR  VmxVmcallWrapper(UINT64 magic, UINT64 op, UINT64 cr3,
                                       UINT64 va, PVOID buf, UINT64 size);

// ================ globals ================
static VMX_STATE*     g_VmxArray  = nullptr;
static ULONG          g_VmxCount  = 0;
static PDEVICE_OBJECT g_Device    = nullptr;
static ULONG64        g_TargetCr3 = 0;
static ULONG          g_TargetPid = 0;

static EPT_STATE      g_GlobalEpt = {};
static SHADOW_TABLE   g_Shadows[MAX_SHADOW_TABLES] = {};
static ULONG          g_ShadowCount = 0;
static UINT64         g_PhysWindowBase = 0;

// teardown handshake with asm — serialized per-CPU, single-slot is correct
extern "C" volatile UCHAR g_TeardownDone = 0;
extern "C" UINT64 g_TeardownRsp = 0;
extern "C" UINT64 g_TeardownRip = 0;

// ================ capability cache (read pre-launch, before any intercept) ================
static UINT64 g_CachedFc        = 0;
static UINT64 g_CachedPinCtl    = 0;
static UINT64 g_CachedProcCtl   = 0;
static UINT64 g_CachedExitCtl   = 0;
static UINT64 g_CachedEntryCtl  = 0;
static UINT64 g_CachedProcCtl2  = 0;
static UINT64 g_CachedEptCap    = 0;
static UINT64 g_CachedCr0F0     = 0, g_CachedCr0F1 = 0;
static UINT64 g_CachedCr4F0     = 0, g_CachedCr4F1 = 0;

// ================ C: spoofed MSR table ================
typedef struct _MSR_SPOOF_RANGE { UINT32 Lo, Hi; UINT64 FakeValue; } MSR_SPOOF_RANGE;
static const MSR_SPOOF_RANGE g_SpoofedMsrs[] = {
    // FEATURE_CONTROL: lock=1, VMX-outside-SMX=0 → jt sees "VMX disabled"
    { 0x000003A, 0x000003A, 0x1 },
    // entire VMX capability block → 0 = "VMX not supported"
    { 0x0000480, 0x0000492, 0x0 },
};

// ================ VmxAdjustCtrl — corrected function ================
static BOOLEAN VmxAdjustCtrl(
    UINT64 cachedMsr,
    UINT32 desired,
    _Out_ UINT32* adjusted,
    _Out_opt_ UINT32* droppedBits)
{
    UINT32 allowed0 = (UINT32)(cachedMsr & 0xFFFFFFFFULL);
    UINT32 allowed1 = (UINT32)(cachedMsr >> 32);
    UINT32 value = (desired | allowed0) & allowed1;
    if (adjusted)    *adjusted = value;
    if (droppedBits) *droppedBits = desired & ~value & ~allowed0;
    return (desired & ~value) == 0;
}

// ================ physical window ================
static NTSTATUS InitPhysWindowBase(void) {
    PVOID probe = ExAllocatePoolWithTag(NonPagedPoolNx, PAGE_SIZE, 'PROT');
    if (!probe) return STATUS_INSUFFICIENT_RESOURCES;
    PHYSICAL_ADDRESS pa = MmGetPhysicalAddress(probe);
    g_PhysWindowBase = (UINT64)probe - pa.QuadPart;
    ExFreePoolWithTag(probe, 'PROT');
    return STATUS_SUCCESS;
}

static PVOID PaToVa(UINT64 pa) { return (PVOID)(g_PhysWindowBase + pa); }

// pre-launch physical read (EPT alloc sanity only)
static NTSTATUS VtReadPhysical(UINT64 pa, PVOID buffer, SIZE_T size) {
    PHYSICAL_ADDRESS p = { (LONGLONG)pa };
    PVOID mapped = MmMapIoSpaceEx(p, size, PAGE_READWRITE | PAGE_NOCACHE);
    if (!mapped) return STATUS_UNSUCCESSFUL;
    RtlCopyMemory(buffer, mapped, size);
    MmUnmapIoSpace(mapped, size);
    return STATUS_SUCCESS;
}

// ================ root-mode VA → PA (PaToVa, zero MmMapIoSpaceEx) ================
static UINT64 RootTranslateVa(UINT64 cr3, UINT64 va) {
    cr3 &= 0x000FFFFFFFFFF000ULL;
    UINT64 i4 = (va >> 39) & 0x1FF, i3 = (va >> 30) & 0x1FF,
           i2 = (va >> 21) & 0x1FF, i1 = (va >> 12) & 0x1FF;
    volatile UINT64* t; UINT64 e;
    t = (volatile UINT64*)PaToVa(cr3);                          e = t[i4];
    if (!(e & 1)) return 0;
    t = (volatile UINT64*)PaToVa(e & 0x000FFFFFFFFFF000ULL);    e = t[i3];
    if (!(e & 1)) return 0;
    if (e & (1ULL << 7)) return (e & 0x000FFFFFC0000000ULL) | (va & 0x3FFFFFFFULL);
    t = (volatile UINT64*)PaToVa(e & 0x000FFFFFFFFFF000ULL);    e = t[i2];
    if (!(e & 1)) return 0;
    if (e & (1ULL << 7)) return (e & 0x000FFFFFFFE00000ULL) | (va & 0x1FFFFFULL);
    t = (volatile UINT64*)PaToVa(e & 0x000FFFFFFFFFF000ULL);    e = t[i1];
    if (!(e & 1)) return 0;
    return (e & 0x000FFFFFFFFFF000ULL) | (va & 0xFFFULL);
}

// ================ global EPT ================
static VOID EptDestroy(void) {
    if (g_GlobalEpt.Pml4) MmFreeContiguousMemory(g_GlobalEpt.Pml4);
    if (g_GlobalEpt.Pdpt) MmFreeContiguousMemory(g_GlobalEpt.Pdpt);
    if (g_GlobalEpt.Pd)   MmFreeContiguousMemory(g_GlobalEpt.Pd);
    RtlZeroMemory(&g_GlobalEpt, sizeof(g_GlobalEpt));
}

static NTSTATUS EptBuildIdentityMap(void) {
    PHYSICAL_ADDRESS max = { .QuadPart = (LONGLONG)-1 };
    PHYSICAL_ADDRESS low = { .QuadPart = 0 };
    g_GlobalEpt.Pml4 = MmAllocateContiguousMemorySpecifyCache(PAGE_SIZE, low, max, low, MmCached);
    g_GlobalEpt.Pdpt = MmAllocateContiguousMemorySpecifyCache(PAGE_SIZE, low, max, low, MmCached);
    g_GlobalEpt.Pd   = MmAllocateContiguousMemorySpecifyCache(PAGE_SIZE * 512, low, max, low, MmCached);
    if (!g_GlobalEpt.Pml4 || !g_GlobalEpt.Pdpt || !g_GlobalEpt.Pd) {
        EptDestroy(); return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(g_GlobalEpt.Pml4, PAGE_SIZE);
    RtlZeroMemory(g_GlobalEpt.Pdpt, PAGE_SIZE);
    RtlZeroMemory(g_GlobalEpt.Pd,   PAGE_SIZE * 512);
    g_GlobalEpt.Pml4Phys = MmGetPhysicalAddress(g_GlobalEpt.Pml4).QuadPart;
    g_GlobalEpt.PdptPhys = MmGetPhysicalAddress(g_GlobalEpt.Pdpt).QuadPart;
    g_GlobalEpt.PdPhys   = MmGetPhysicalAddress(g_GlobalEpt.Pd).QuadPart;

    EPT_PML4E pml4e = {};
    pml4e.Fields.Read = pml4e.Fields.Write = pml4e.Fields.Execute = 1;
    pml4e.Fields.MemType = EPT_MEMTYPE_WB;
    pml4e.Fields.PhysAddr = g_GlobalEpt.PdptPhys >> 12;
    ((UINT64*)g_GlobalEpt.Pml4)[0] = pml4e.All;

    for (int i = 0; i < 512; i++) {
        EPT_PDPTE pdpte = {};
        pdpte.Fields.Read = pdpte.Fields.Write = pdpte.Fields.Execute = 1;
        pdpte.Fields.MemType = EPT_MEMTYPE_WB;
        pdpte.Fields.PhysAddr = (g_GlobalEpt.PdPhys + (UINT64)i * PAGE_SIZE) >> 12;
        ((UINT64*)g_GlobalEpt.Pdpt)[i] = pdpte.All;
    }
    for (UINT64 idx = 0; idx < 512ULL * 512ULL; idx++) {
        EPT_PDE_2MB pde = {};
        pde.Fields.Read = pde.Fields.Write = pde.Fields.Execute = 1;
        pde.Fields.MemType = EPT_MEMTYPE_WB;
        pde.Fields.PhysAddr = (idx * 0x200000ULL) >> 21;
        ((UINT64*)g_GlobalEpt.Pd)[idx] = pde.All | EPT_FLAG_LARGE_PAGE;
    }
    g_GlobalEpt.Eptp = (g_GlobalEpt.Pml4Phys & ~0xFFFULL) | (EPT_MEMTYPE_WB << 3) | (3 << 0);
    return STATUS_SUCCESS;
}

static volatile UINT64* EptLocatePdeSlot(UINT64 gpa) {
    UINT64 twoMb = gpa & ~0x1FFFFFULL;
    UINT64 i4 = (twoMb >> 39) & 0x1FF;
    UINT64 i3 = (twoMb >> 30) & 0x1FF;
    UINT64 i2 = (twoMb >> 21) & 0x1FF;
    if (i4 != 0) return nullptr;
    UINT64 pdpteVal = ((volatile UINT64*)g_GlobalEpt.Pdpt)[i3];
    if (!(pdpteVal & 1)) return nullptr;
    return &((volatile UINT64*)g_GlobalEpt.Pd)[i3 * 512 + i2];
}

static VOID EptInvalidateSingle(void) {
    INVEPT_DESC d = { g_GlobalEpt.Eptp, 0 };
    __invept(1, &d);
}

static PSHADOW_TABLE FindShadowForGpa(UINT64 gpa) {
    UINT64 twoMb = gpa & ~0x1FFFFFULL;
    for (ULONG i = 0; i < g_ShadowCount; i++)
        if (g_Shadows[i].Active && g_Shadows[i].GpaBase == twoMb)
            return &g_Shadows[i];
    return nullptr;
}

static NTSTATUS EptSplit2MbPage(UINT64 gpa, ULONG flags) {
    volatile UINT64* pdeSlot = EptLocatePdeSlot(gpa);
    if (!pdeSlot) return STATUS_NOT_FOUND;
    UINT64 pde = *pdeSlot;
    if (!(pde & EPT_FLAG_LARGE_PAGE)) return STATUS_ALREADY_REGISTERED;

    PVOID ptVa = ExAllocatePoolWithTag(NonPagedPoolNx, PAGE_SIZE, 'SPPT');
    if (!ptVa) return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(ptVa, PAGE_SIZE);

    UINT64 ptPhys = MmGetPhysicalAddress(ptVa).QuadPart;
    UINT64 twoMbBase = pde & 0x000FFFFFFFE00000ULL;

    for (UINT64 i = 0; i < 512; i++) {
        EPT_PTE pte = {};
        pte.Fields.Read = 1;
        pte.Fields.Write = 1;
        pte.Fields.Execute = 1;
        pte.Fields.MemType = EPT_MEMTYPE_WB;
        pte.Fields.PhysAddr = (twoMbBase + i * 0x1000ULL) >> 12;
        if (flags & (1 << 3)) {
            pte.Fields.Write   = 0;
            pte.Fields.Execute = 0;
        }
        ((UINT64*)ptVa)[i] = pte.All;
    }

    EPT_PDE_2MB newPde = {};
    newPde.Fields.Read = newPde.Fields.Write = newPde.Fields.Execute = 1;
    newPde.Fields.MemType = EPT_MEMTYPE_WB;
    newPde.Fields.PhysAddr = ptPhys >> 12;
    *pdeSlot = newPde.All;
    EptInvalidateSingle();

    if (g_ShadowCount >= MAX_SHADOW_TABLES) {
        ExFreePoolWithTag(ptVa, 'SPPT');
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    PSHADOW_TABLE st = &g_Shadows[g_ShadowCount++];
    st->PtVa = ptVa;
    st->PtPhys = ptPhys;
    st->GpaBase = twoMbBase;
    st->ShadowVa = nullptr;
    st->ShadowPhys = 0;
    st->Active = TRUE;
    return STATUS_SUCCESS;
}

// root-side protect (invoked via VMCALL VT_OP_PROTECT)
static NTSTATUS EptProtectRoot(UINT64 cr3, UINT64 va, ULONG flags) {
    UINT64 pa = RootTranslateVa(cr3, va);
    if (!pa) return STATUS_NOT_FOUND;
    NTSTATUS st = EptSplit2MbPage(pa, flags);
    if (st == STATUS_ALREADY_REGISTERED) return STATUS_SUCCESS;
    if (NT_SUCCESS(st)) return STATUS_SUCCESS;
    // legacy PDE fallback
    volatile UINT64* pdeSlot = EptLocatePdeSlot(pa);
    if (!pdeSlot) return STATUS_NOT_FOUND;
    UINT64 old = *pdeSlot, nv = old;
    if (flags & (1 << 3)) { nv &= ~EPT_PTE_X; nv &= ~EPT_PTE_W; }
    if (nv != old) { *pdeSlot = nv; EptInvalidateSingle(); }
    return STATUS_SUCCESS;
}

// ================ C: MSR bitmap helpers ================
static VOID MsrBitmapSetRead(PVOID bitmap, UINT32 msr) {
    PUINT8 b = (PUINT8)bitmap;
    if (msr < 0x2000) {
        b[msr >> 3] |= (UINT8)(1 << (msr & 7));
    } else if (msr >= 0xC0000000 && msr < 0xC0002000) {
        b[0x800 + ((msr - 0xC0000000) >> 3)] |= (UINT8)(1 << (msr & 7));
    }
}

static VOID MsrBitmapBuild(PVOID bitmap) {
    RtlZeroMemory(bitmap, MSR_BITMAP_SIZE);
    for (UINT32 m = g_SpoofedMsrs[0].Lo; m <= g_SpoofedMsrs[1].Hi; m++) {
        // set read-intercept for both ranges (0x3A and 0x480..0x492)
        MsrBitmapSetRead(bitmap, m);
    }
}

static BOOLEAN RootTryFakeMsr(UINT32 msr, _Out_ UINT64* value) {
    for (ULONG i = 0; i < RTL_NUMBER_OF(g_SpoofedMsrs); i++) {
        if (msr >= g_SpoofedMsrs[i].Lo && msr <= g_SpoofedMsrs[i].Hi) {
            *value = g_SpoofedMsrs[i].FakeValue;
            return TRUE;
        }
    }
    return FALSE;
}

// ================ host state ================
static VOID VmxFillHostState(UINT32 cpuIdx) {
    __vmx_vmwrite(VMCS_HOST_CS_SEL,  __readcs());
    __vmx_vmwrite(VMCS_HOST_SS_SEL,  __readss());
    __vmx_vmwrite(VMCS_HOST_DS_SEL,  __readds());
    __vmx_vmwrite(VMCS_HOST_ES_SEL,  __reades());
    __vmx_vmwrite(VMCS_HOST_FS_SEL,  __readfs());
    __vmx_vmwrite(VMCS_HOST_GS_SEL,  __readgs());
    __vmx_vmwrite(VMCS_HOST_TR_SEL,  __readtr());

    __vmx_vmwrite(VMCS_HOST_FS_BASE, __readmsr(IA32_FS_BASE));
    __vmx_vmwrite(VMCS_HOST_GS_BASE, __readmsr(IA32_GS_BASE));

    DESCRIPTOR_TABLE gdtr = {}, idtr = {};
    _sgdt(&gdtr); _sidt(&idtr);
    __vmx_vmwrite(VMCS_HOST_GDTR_BASE, gdtr.Base);
    __vmx_vmwrite(VMCS_HOST_IDTR_BASE, idtr.Base);

    USHORT trSel = __readtr();
    UINT64 trBase = 0;
    if (gdtr.Base && (trSel & 0xFFF8) != 0) {
        PUCHAR desc = (PUCHAR)(gdtr.Base + ((UINT64)(trSel & 0xFFF8)));
        trBase = *(PUINT64)(desc + 2)
               | ((UINT64)(*(PUINT16)(desc + 8)) << 16)
               | ((UINT64)(*(PUINT8)(desc + 11)) << 24);
    }
    __vmx_vmwrite(VMCS_HOST_TR_BASE, trBase);

    __vmx_vmwrite(VMCS_HOST_CR0, __readcr0());
    __vmx_vmwrite(VMCS_HOST_CR3, __readcr3());
    __vmx_vmwrite(VMCS_HOST_CR4, __readcr4());
    __vmx_vmwrite(VMCS_HOST_RSP,
        (ULONG64)g_VmxArray[cpuIdx].HostStack + HOST_STACK_SIZE - 0x100);
    __vmx_vmwrite(VMCS_HOST_RIP, (ULONG64)VmxExitEntry);
}

// ================ per-CPU init ================
static NTSTATUS VmxEnableOnCpu(UINT32 cpuIdx) {
    VMX_STATE* s = &g_VmxArray[cpuIdx];
    RtlZeroMemory(s, sizeof(*s));

    // FEATURE_CONTROL from cache — never RDMSR here (would self-trap post-launch)
    if (!(g_CachedFc & 0x1)) {
        __writemsr(IA32_FEATURE_CONTROL, g_CachedFc | 0x1 | 0x4);
        g_CachedFc = __readmsr(IA32_FEATURE_CONTROL);   // first CPU pre-launch: safe
    }
    if (!(g_CachedFc & 0x1) || !(g_CachedFc & 0x4)) return STATUS_NOT_SUPPORTED;

    // EPT capability from cache
    if (!(g_CachedEptCap & (1ULL << 6)) ||
        !(g_CachedEptCap & (1ULL << 14)) ||
        !(g_CachedEptCap & (1ULL << 16)))
        return STATUS_NOT_SUPPORTED;

    PHYSICAL_ADDRESS max = { .QuadPart = (LONGLONG)-1 };
    PHYSICAL_ADDRESS low = { .QuadPart = 0 };
    s->VmxonVa = MmAllocateContiguousMemorySpecifyCache(VMXON_REGION_SIZE, low, max, low, MmCached);
    if (!s->VmxonVa) return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(s->VmxonVa, VMXON_REGION_SIZE);
    s->VmxonPhys = MmGetPhysicalAddress(s->VmxonVa).QuadPart;
    *(UINT32*)s->VmxonVa = (UINT32)__readmsr(IA32_VMX_BASIC);   // pre-launch on this CPU: safe

    if (__vmx_on(&s->VmxonPhys)) { MmFreeContiguousMemory(s->VmxonVa); return STATUS_UNSUCCESSFUL; }

    s->VmcsVa = MmAllocateContiguousMemorySpecifyCache(VMCS_REGION_SIZE, low, max, low, MmCached);
    if (!s->VmcsVa) { __vmx_off(); return STATUS_INSUFFICIENT_RESOURCES; }
    RtlZeroMemory(s->VmcsVa, VMCS_REGION_SIZE);
    s->VmcsPhys = MmGetPhysicalAddress(s->VmcsVa).QuadPart;
    *(UINT32*)s->VmcsVa = (UINT32)__readmsr(IA32_VMX_BASIC);    // we are root on this CPU now

    if (__vmx_vmclear(&s->VmcsPhys) || __vmx_vmptrld(&s->VmcsPhys)) {
        __vmx_off(); return STATUS_UNSUCCESSFUL;
    }

    s->HostStack = ExAllocatePoolWithTag(NonPagedPoolNx, HOST_STACK_SIZE, 'PROT');
    if (!s->HostStack) { __vmx_off(); return STATUS_INSUFFICIENT_RESOURCES; }

    // MSR bitmap — must be < 4GB (SDM: bits 31:0), 4KB aligned
    PHYSICAL_ADDRESS max4g = { .QuadPart = 0xFFFFFFFF };
    s->MsrBitmapVa = MmAllocateContiguousMemorySpecifyCache(MSR_BITMAP_SIZE, low, max4g, low, MmCached);
    if (!s->MsrBitmapVa) { __vmx_off(); return STATUS_INSUFFICIENT_RESOURCES; }
    MsrBitmapBuild(s->MsrBitmapVa);
    UINT64 msrBitmapPhys = MmGetPhysicalAddress(s->MsrBitmapVa).QuadPart;

    // Controls — cached MSRs, adjusted per allowed-0/allowed-1
    {
        UINT32 pin = 0, exec = 0, exec2 = 0, exit_ = 0, entry = 0;
        VmxAdjustCtrl(g_CachedPinCtl,   0,                          &pin,   nullptr);
        // C: enable USE_MSR_BITMAPS (bit 15) + USE_TSC_OFFSET (bit 28)
        VmxAdjustCtrl(g_CachedProcCtl,  (1 << 15) | (1 << 28),      &exec,  nullptr);
        VmxAdjustCtrl(g_CachedProcCtl2, (1 << 1) | (1 << 20),       &exec2, nullptr);
        VmxAdjustCtrl(g_CachedExitCtl,  (1 << 9) | (1 << 20) | (1 << 21), &exit_, nullptr);
        VmxAdjustCtrl(g_CachedEntryCtl, (1 << 9) | (1 << 14),       &entry, nullptr);
        __vmx_vmwrite(VMCS_CTRL_PIN,   pin);
        __vmx_vmwrite(VMCS_CTRL_EXEC,  exec);
        __vmx_vmwrite(VMCS_CTRL_EXEC2, exec2);
        __vmx_vmwrite(VMCS_CTRL_EXIT,  exit_);
        __vmx_vmwrite(VMCS_CTRL_ENTRY, entry);
    }
    __vmx_vmwrite(VMCS_CTRL_EPTP,       g_GlobalEpt.Eptp);   // global, shared
    __vmx_vmwrite(VMCS_CTRL_VPID,       1);
    __vmx_vmwrite(VMCS_CTRL_EXC_BITMAP, 0);
    __vmx_vmwrite(VMCS_CTRL_TSC_OFFSET, 0);
    __vmx_vmwrite(VMCS_CTRL_MSR_BITMAP, msrBitmapPhys);      // C: bound

    // CR0/CR4 fixed-bit sanity from cache, mask = 0 (no CR interception)
    {
        UINT64 curCr0 = __readcr0();
        UINT64 curCr4 = __readcr4();
        if ((curCr0 & g_CachedCr0F0) != g_CachedCr0F0)      { __vmx_off(); return STATUS_INVALID_PARAMETER; }
        if ((~curCr0 & ~g_CachedCr0F1) != 0)                { __vmx_off(); return STATUS_INVALID_PARAMETER; }
        if ((curCr4 & g_CachedCr4F0) != g_CachedCr4F0)      { __vmx_off(); return STATUS_INVALID_PARAMETER; }
        if ((~curCr4 & ~g_CachedCr4F1) != 0)                { __vmx_off(); return STATUS_INVALID_PARAMETER; }
        __vmx_vmwrite(VMCS_GUEST_CR0_MASK,   0);
        __vmx_vmwrite(VMCS_GUEST_CR0_SHADOW, 0);
        __vmx_vmwrite(VMCS_GUEST_CR4_MASK,   0);
        __vmx_vmwrite(VMCS_GUEST_CR4_SHADOW, 0);
    }

    VmxFillHostState(cpuIdx);

    __vmx_vmwrite(VMCS_GUEST_CR0, __readcr0());
    __vmx_vmwrite(VMCS_GUEST_CR3, __readcr3());
    __vmx_vmwrite(VMCS_GUEST_CR4, __readcr4());
    __vmx_vmwrite(VMCS_GUEST_DR7, __readdr(7));

    __vmx_vmwrite(VMCS_GUEST_ES_SEL, __reades());
    __vmx_vmwrite(VMCS_GUEST_CS_SEL, __readcs());
    __vmx_vmwrite(VMCS_GUEST_SS_SEL, __readss());
    __vmx_vmwrite(VMCS_GUEST_DS_SEL, __readds());
    __vmx_vmwrite(VMCS_GUEST_FS_SEL, __readfs());
    __vmx_vmwrite(VMCS_GUEST_GS_SEL, __readgs());
    __vmx_vmwrite(VMCS_GUEST_TR_SEL, __readtr());
    __vmx_vmwrite(VMCS_GUEST_LDTR_SEL, 0);

    __vmx_vmwrite(VMCS_GUEST_FS_BASE, __readmsr(IA32_FS_BASE));
    __vmx_vmwrite(VMCS_GUEST_GS_BASE, __readmsr(IA32_GS_BASE));

    DESCRIPTOR_TABLE gdtr = {}, idtr = {};
    _sgdt(&gdtr); _sidt(&idtr);
    __vmx_vmwrite(VMCS_GUEST_GDTR_BASE,  gdtr.Base);
    __vmx_vmwrite(VMCS_GUEST_GDTR_LIMIT, gdtr.Limit);
    __vmx_vmwrite(VMCS_GUEST_IDTR_BASE,  idtr.Base);
    __vmx_vmwrite(VMCS_GUEST_IDTR_LIMIT, idtr.Limit);

    USHORT trSel = __readtr();
    UINT64 trBase = 0;
    UINT16 trLimit = 0x67;
    if (gdtr.Base && (trSel & 0xFFF8) != 0) {
        PUCHAR desc = (PUCHAR)(gdtr.Base + ((UINT64)(trSel & 0xFFF8)));
        trBase  = *(PUINT64)(desc + 2)
                | ((UINT64)(*(PUINT16)(desc + 8)) << 16)
                | ((UINT64)(*(PUINT8)(desc + 11)) << 24);
        trLimit = *(PUINT16)(desc);
    }
    __vmx_vmwrite(VMCS_GUEST_TR_BASE,  trBase);
    __vmx_vmwrite(VMCS_GUEST_TR_LIMIT, trLimit);

    __vmx_vmwrite(VMCS_GUEST_ES_LIMIT,   0xFFFFFFFF);
    __vmx_vmwrite(VMCS_GUEST_CS_LIMIT,   0xFFFFFFFF);
    __vmx_vmwrite(VMCS_GUEST_SS_LIMIT,   0xFFFFFFFF);
    __vmx_vmwrite(VMCS_GUEST_DS_LIMIT,   0xFFFFFFFF);
    __vmx_vmwrite(VMCS_GUEST_FS_LIMIT,   0xFFFFFFFF);
    __vmx_vmwrite(VMCS_GUEST_GS_LIMIT,   0xFFFFFFFF);
    __vmx_vmwrite(VMCS_GUEST_LDTR_LIMIT, 0);

    __vmx_vmwrite(VMCS_GUEST_ES_AR,   0xC0F3);
    __vmx_vmwrite(VMCS_GUEST_CS_AR,   0xA09B);
    __vmx_vmwrite(VMCS_GUEST_SS_AR,   0xC0F3);
    __vmx_vmwrite(VMCS_GUEST_DS_AR,   0xC0F3);
    __vmx_vmwrite(VMCS_GUEST_FS_AR,   0xC0F3);
    __vmx_vmwrite(VMCS_GUEST_GS_AR,   0xC0F3);
    __vmx_vmwrite(VMCS_GUEST_LDTR_AR, 0x10000);
    __vmx_vmwrite(VMCS_GUEST_TR_AR,   0x008B);

    __vmx_vmwrite(VMCS_GUEST_INT_STATE,      0);
    __vmx_vmwrite(VMCS_GUEST_ACTIVITY_STATE, 0);
    __vmx_vmwrite(VMCS_GUEST_SMBASE,         0);
    __vmx_vmwrite(VMCS_GUEST_SYSENTER_ESP,   __readmsr(0x175));
    __vmx_vmwrite(VMCS_GUEST_SYSENTER_EIP,   __readmsr(0x176));

    ULONG_PTR lr = VmxLaunchEntry();
    if (lr == 0) {
        s->Launched    = TRUE;
        s->Initialized = TRUE;
        return STATUS_SUCCESS;
    }
    if (s->MsrBitmapVa) MmFreeContiguousMemory(s->MsrBitmapVa);
    ExFreePoolWithTag(s->HostStack, 'PROT');
    __vmx_vmclear(&s->VmcsPhys);
    __vmx_off();
    return STATUS_UNSUCCESSFUL;
}

static VOID VmxFreeCpu(UINT32 cpuIdx) {
    VMX_STATE* s = &g_VmxArray[cpuIdx];
    if (!s->Initialized && !s->VmxonVa) return;
    if (s->MsrBitmapVa) MmFreeContiguousMemory(s->MsrBitmapVa);
    if (s->VmcsVa)      MmFreeContiguousMemory(s->VmcsVa);
    if (s->VmxonVa)     MmFreeContiguousMemory(s->VmxonVa);
    if (s->HostStack)   ExFreePoolWithTag(s->HostStack, 'PROT');
    RtlZeroMemory(s, sizeof(*s));
}

static NTSTATUS VmxInitAllCpus(void) {
    NTSTATUS finalStatus = STATUS_SUCCESS;
    GROUP_AFFINITY oldAffinity = {}, newAffinity = {};
    for (ULONG idx = 0; idx < g_VmxCount; idx++) {
        PROCESSOR_NUMBER procNum = {};
        KeGetProcessorNumberFromIndex(idx, &procNum);
        newAffinity.Group = procNum.Group;
        newAffinity.Mask  = 1ULL << procNum.Number;
        KeSetSystemGroupAffinityThread(&newAffinity, &oldAffinity);
        NTSTATUS st = VmxEnableOnCpu(KeGetCurrentProcessorNumber());
        if (!NT_SUCCESS(st) && NT_SUCCESS(finalStatus)) finalStatus = st;
        KeRevertToUserGroupAffinityThread(&oldAffinity);
    }
    return finalStatus;
}

// ================ VM exit handler ================
extern "C" VOID VmExitHandlerC(PGUEST_REGS regs) {
    UINT64 reason = 0, qual = 0, gpa = 0, rip = 0;
    __vmx_vmread(VMCS_EXIT_REASON, &reason);
    reason &= 0xFFFF;
    __vmx_vmread(VMCS_EXIT_QUAL,   &qual);

    switch (reason) {
    case VMX_EXIT_EPT_VIOLATION:
        __vmx_vmread(VMCS_GUEST_PHYSICAL, &gpa);
        if (qual & EXIT_QUAL_GPA_VALID) {
            PSHADOW_TABLE shadow = FindShadowForGpa(gpa);
            if (shadow) {
                UINT64 pteIdx = (gpa & 0x1FF000ULL) >> 12;
                volatile UINT64* pte = &((volatile UINT64*)shadow->PtVa)[pteIdx];
                UINT64 old = *pte, nv = old;
                if (qual & EXIT_QUAL_INSTR_FETCH) nv |= EPT_PTE_X;
                if (qual & EXIT_QUAL_DATA_WRITE)  nv |= EPT_PTE_W;
                if (qual & EXIT_QUAL_DATA_READ)   nv |= EPT_PTE_R;
                if (nv != old) { *pte = nv; EptInvalidateSingle(); }
            } else {
                volatile UINT64* pdeSlot = EptLocatePdeSlot(gpa);
                if (pdeSlot) {
                    UINT64 old = *pdeSlot, nv = old | EPT_FLAG_RWX;
                    if (nv != old) { *pdeSlot = nv; EptInvalidateSingle(); }
                }
            }
        }
        break;   // NO RIP ADVANCE — re-execute

    case VMX_EXIT_VMCALL: {
        UINT64 instrLen = 0;
        __vmx_vmread(VMCS_EXIT_INSTR_LEN, &instrLen);
        __vmx_vmread(VMCS_GUEST_RIP, &rip);
        __vmx_vmwrite(VMCS_GUEST_RIP, rip + instrLen);

        if (regs->rax != VT_MAGIC) break;

        if (regs->rcx == VT_OP_TEARDOWN) {
            UINT64 grsp = 0, gip = 0;
            __vmx_vmread(VMCS_GUEST_RSP, &grsp);
            __vmx_vmread(VMCS_GUEST_RIP, &gip);   // already advanced
            g_TeardownRsp = grsp;
            g_TeardownRip = gip;
            regs->rax = (ULONG64)STATUS_SUCCESS;  // becomes wrapper return after jump
            __vmx_off();                           // legal — root mode
            g_TeardownDone = 1;                    // asm: skip vmresume, jmp to Rip
            return;
        }

        NTSTATUS st = STATUS_SUCCESS;
        switch (regs->rcx) {
        case VT_OP_PING:
            break;
        case VT_OP_READ: {
            UINT64 pa = RootTranslateVa(regs->rdx, regs->r8);
            if (!pa) { st = STATUS_NOT_FOUND; break; }
            RtlCopyMemory((PVOID)regs->r9, PaToVa(pa), (SIZE_T)regs->r10);
            break;
        }
        case VT_OP_WRITE: {
            UINT64 pa = RootTranslateVa(regs->rdx, regs->r8);
            if (!pa) { st = STATUS_NOT_FOUND; break; }
            RtlCopyMemory(PaToVa(pa), (PVOID)regs->r9, (SIZE_T)regs->r10);
            break;
        }
        case VT_OP_PROTECT:
            st = EptProtectRoot(regs->rdx, regs->r8, (ULONG)regs->r10);
            break;
        default:
            st = STATUS_INVALID_PARAMETER;
        }
        regs->rax = (ULONG64)st;
        break;
    }

    case VMX_EXIT_CPUID: {
        int info[4] = {};
        __cpuidex(info, (int)regs->rax, (int)regs->rcx);
        regs->rax = (ULONG64)info[0];
        regs->rbx = (ULONG64)info[1];
        regs->rcx = (ULONG64)info[2];
        regs->rdx = (ULONG64)info[3];
        __vmx_vmread(VMCS_GUEST_RIP, &rip);
        __vmx_vmwrite(VMCS_GUEST_RIP, rip + 2);
        break;
    }

    // ================ C: RDMSR interception ================
    case VMX_EXIT_RDMSR: {
        UINT32 msr = (UINT32)regs->rcx;
        UINT64 value = 0;
        if (!RootTryFakeMsr(msr, &value)) {
            value = __readmsr(msr);   // real read, root mode — no bitmap consult
        }
        regs->rax = value & 0xFFFFFFFFULL;
        regs->rdx = value >> 32;
        __vmx_vmread(VMCS_GUEST_RIP, &rip);
        __vmx_vmwrite(VMCS_GUEST_RIP, rip + 2);   // RDMSR = 2 bytes (0F 32)
        break;
    }

    default:
        // WRMSR (32) never fires — bitmap write bits are clear.
        // Unknown exits: advance nothing; loud bugcheck in debug builds.
        break;
    }
}

// ================ dispatch ================
static NTSTATUS DispatchCreateClose(PDEVICE_OBJECT, PIRP irp) {
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

#define VmCall(op, cr3, va, buf, sz) \
    VmxVmcallWrapper(VT_MAGIC, (op), (cr3), (UINT64)(va), (PVOID)(buf), (UINT64)(sz))

static NTSTATUS DispatchIoControl(PDEVICE_OBJECT, PIRP irp) {
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG bytesIO = 0;
    ULONG inLen  = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = stack->Parameters.DeviceIoControl.OutputBufferLength;
    PVOID sysBuf = irp->AssociatedIrp.SystemBuffer;

    switch (stack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_SP_PING: {
        if (outLen < sizeof(ULONG)) { status = STATUS_BUFFER_TOO_SMALL; break; }
        *(ULONG*)sysBuf = 0x50524F54;
        bytesIO = sizeof(ULONG);
        break;
    }
    case IOCTL_SP_OPEN: {
        if (inLen < sizeof(SP_OPEN_REQ) || outLen < sizeof(SP_OPEN_RES)) { status = STATUS_BUFFER_TOO_SMALL; break; }
        PSP_OPEN_REQ req = (PSP_OPEN_REQ)sysBuf;
        PSP_OPEN_RES res = (PSP_OPEN_RES)sysBuf;
        PEPROCESS proc = nullptr;
        status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)req->Pid, &proc);
        if (NT_SUCCESS(status)) {
            g_TargetCr3 = *(UINT64*)((PUCHAR)proc + OFF_EPROCESS_DIRECTORYTABLEBASE);
            g_TargetPid = req->Pid;
            res->Cookie = g_TargetCr3;
            res->Status = STATUS_SUCCESS;
            ObDereferenceObject(proc);
            bytesIO = sizeof(SP_OPEN_RES);
        }
        break;
    }
    case IOCTL_SP_READ: {
        if (inLen < sizeof(SP_MEM_REQ) || outLen < sizeof(SP_MEM_RES)) { status = STATUS_BUFFER_TOO_SMALL; break; }
        PSP_MEM_REQ req = (PSP_MEM_REQ)sysBuf;
        PSP_MEM_RES res = (PSP_MEM_RES)sysBuf;
        if (req->Size > SP_MAX_XFER) { status = STATUS_INVALID_PARAMETER; break; }
        UINT64 cr3 = req->Cookie ? req->Cookie : g_TargetCr3;
        // VMCALL → root does translate + PaToVa copy. No MmMapIoSpaceEx on path.
        ULONG_PTR rc = VmCall(VT_OP_READ, cr3, req->Address, res->Data, req->Size);
        status = (NTSTATUS)rc;
        res->Status = status;
        res->Returned = NT_SUCCESS(status) ? req->Size : 0;
        bytesIO = NT_SUCCESS(status) ? FIELD_OFFSET(SP_MEM_RES, Data) + req->Size : sizeof(SP_MEM_RES);
        break;
    }
    case IOCTL_SP_WRITE: {
        if (inLen < sizeof(SP_WRITE_REQ) || outLen < sizeof(NTSTATUS)) { status = STATUS_BUFFER_TOO_SMALL; break; }
        PSP_WRITE_REQ req = (PSP_WRITE_REQ)sysBuf;
        UINT64 cr3 = req->Cookie ? req->Cookie : g_TargetCr3;
        ULONG_PTR rc = VmCall(VT_OP_WRITE, cr3, req->Address, req->Data, req->Size);
        status = (NTSTATUS)rc;
        *(NTSTATUS*)sysBuf = status;
        bytesIO = sizeof(NTSTATUS);
        break;
    }
    case IOCTL_SP_QUERY_CR3: {
        if (outLen < sizeof(SP_CR3_RES)) { status = STATUS_BUFFER_TOO_SMALL; break; }
        PSP_CR3_RES res = (PSP_CR3_RES)sysBuf;
        res->Cr3 = g_TargetCr3;
        res->DirectoryTableBase = g_TargetCr3;
        bytesIO = sizeof(SP_CR3_RES);
        break;
    }
    case IOCTL_SP_QUERY_EPT: {
        if (outLen < sizeof(SP_EPT_RES)) { status = STATUS_BUFFER_TOO_SMALL; break; }
        PSP_EPT_RES res = (PSP_EPT_RES)sysBuf;
        UINT32 idx = KeGetCurrentProcessorNumber();
        res->Eptp = g_GlobalEpt.Eptp;
        res->Pml4Phys = g_GlobalEpt.Pml4Phys;
        res->PageSizeMask = 0x1FFFFFULL;
        res->VmxOnline = g_VmxArray[idx].Launched;
        bytesIO = sizeof(SP_EPT_RES);
        break;
    }
    case IOCTL_SP_PROTECT_REG: {
        if (inLen < sizeof(SP_PROT_REQ)) { status = STATUS_BUFFER_TOO_SMALL; break; }
        PSP_PROT_REQ req = (PSP_PROT_REQ)sysBuf;
        ULONG_PTR rc = VmCall(VT_OP_PROTECT, g_TargetCr3, req->Address, nullptr, req->Flags);
        status = (NTSTATUS)rc;
        break;
    }
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
    }

    irp->IoStatus.Status = status;
    irp->IoStatus.Information = bytesIO;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

// ================ real unload via VMCALL teardown ================
static VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    GROUP_AFFINITY oldAffinity = {}, newAffinity = {};

    // Serialize: one CPU at a time. On each, VMCALL TEARDOWN → root vmx_off
    // → asm jmp back into the wrapper → wrapper returns.
    for (ULONG idx = 0; idx < g_VmxCount; idx++) {
        if (!g_VmxArray[idx].Launched) { VmxFreeCpu(idx); continue; }
        PROCESSOR_NUMBER procNum = {};
        KeGetProcessorNumberFromIndex(idx, &procNum);
        newAffinity.Group = procNum.Group;
        newAffinity.Mask  = 1ULL << procNum.Number;
        KeSetSystemGroupAffinityThread(&newAffinity, &oldAffinity);

        g_TeardownDone = 0;
        // Return value is whatever wrapper carries back; the jump path
        // restores RAX = STATUS_SUCCESS as set by the handler.
        VmCall(VT_OP_TEARDOWN, 0, 0, nullptr, 0);

        KeRevertToUserGroupAffinityThread(&oldAffinity);
        VmxFreeCpu(idx);
    }

    for (ULONG i = 0; i < g_ShadowCount; i++)
        if (g_Shadows[i].PtVa) ExFreePoolWithTag(g_Shadows[i].PtVa, 'SPPT');
    g_ShadowCount = 0;
    EptDestroy();

    UNICODE_STRING sym;
    RtlInitUnicodeString(&sym, SP_SYMBOLIC_LINK);
    IoDeleteSymbolicLink(&sym);
    if (g_Device) IoDeleteDevice(g_Device);
    if (g_VmxArray) ExFreePoolWithTag(g_VmxArray, 'PROT');
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
    NTSTATUS st;
    DriverObject->MajorFunction[IRP_MJ_CREATE]         = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIoControl;
    DriverObject->DriverUnload = DriverUnload;   // real unload — teardown via VMCALL

    // ---- capability cache: MUST run before any vmlaunch, otherwise
    // ---- post-launch init on CPU>0 would self-trap on intercepted MSRs
    g_CachedFc       = __readmsr(IA32_FEATURE_CONTROL);
    g_CachedPinCtl   = __readmsr(IA32_VMX_PINBASED_CTLS);
    g_CachedProcCtl  = __readmsr(IA32_VMX_PROCBASED_CTLS);
    g_CachedExitCtl  = __readmsr(IA32_VMX_EXIT_CTLS);
    g_CachedEntryCtl = __readmsr(IA32_VMX_ENTRY_CTLS);
    g_CachedProcCtl2 = __readmsr(IA32_VMX_PROCBASED_CTLS2);
    g_CachedEptCap   = __readmsr(IA32_VMX_EPT_VPID_CAP);
    g_CachedCr0F0    = __readmsr(IA32_VMX_CR0_FIXED0);
    g_CachedCr0F1    = __readmsr(IA32_VMX_CR0_FIXED1);
    g_CachedCr4F0    = __readmsr(IA32_VMX_CR4_FIXED0);
    g_CachedCr4F1    = __readmsr(IA32_VMX_CR4_FIXED1);

    st = InitPhysWindowBase();
    if (!NT_SUCCESS(st)) return st;

    UNICODE_STRING devName, symName;
    RtlInitUnicodeString(&devName, SP_DEVICE_NAME);
    RtlInitUnicodeString(&symName, SP_SYMBOLIC_LINK);
    st = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &g_Device);
    if (!NT_SUCCESS(st)) return st;
    st = IoCreateSymbolicLink(&symName, &devName);
    if (!NT_SUCCESS(st)) { IoDeleteDevice(g_Device); return st; }

    st = EptBuildIdentityMap();
    if (!NT_SUCCESS(st)) {
        IoDeleteSymbolicLink(&symName);
        IoDeleteDevice(g_Device);
        return st;
    }

    g_VmxCount = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    g_VmxArray = (VMX_STATE*)ExAllocatePoolWithTag(NonPagedPoolNx, g_VmxCount * sizeof(VMX_STATE), 'PROT');
    if (!g_VmxArray) {
        EptDestroy();
        IoDeleteSymbolicLink(&symName);
        IoDeleteDevice(g_Device);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(g_VmxArray, g_VmxCount * sizeof(VMX_STATE));

    st = VmxInitAllCpus();
    if (!NT_SUCCESS(st)) {
        for (ULONG i = 0; i < g_VmxCount; i++) VmxFreeCpu(i);
        ExFreePoolWithTag(g_VmxArray, 'PROT');
        EptDestroy();
        IoDeleteSymbolicLink(&symName);
        IoDeleteDevice(g_Device);
        return st;
    }

    DbgPrint("[12333] simple_prot VMX online on %u CPUs, MSR spoof active, unload enabled.\n", g_VmxCount);
    return STATUS_SUCCESS;
}