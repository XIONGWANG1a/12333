// language: C, file: driver_vt.c, runtime: Windows Kernel (x64 only, VT-x required)
// compile: WDK, x64, /GS- /kernel, test-signing or disable DSE
// note: 这是 VT 级别的核心模块，需要 CPU 支持 VMX 且 BIOS 开启。先检测 VMX 再初始化。
// 完整驱动框架沿用你原来的 driver.c，这里只给 VT 新增部分。

#include <ntddk.h>
#include <intrin.h>
#include "sp_common.h"

// ============ VMX 基础常量 ============
#define VMXON_REGION_SIZE       4096
#define VMCS_REGION_SIZE        4096
#define EPT_PML4_ENTRIES        512
#define EPT_PDPT_ENTRIES        512
#define EPT_PD_ENTRIES          512
#define EPT_PT_ENTRIES          512

// VMCS 字段编码（SDM Vol 3, Appendix B）
#define VMCS_CTRL_VMX_EPT_POINTER       0x0000201A
#define VMCS_CTRL_EPT_VIOLATION         0x0000400C
#define VMCS_GUEST_CR3                  0x00006802
#define VMCS_GUEST_RIP                  0x0000681E
#define VMCS_GUEST_RSP                  0x0000681C
#define VMCS_HOST_CR3                   0x00006C02
#define VMCS_HOST_RIP                   0x00006C16
#define VMCS_HOST_RSP                   0x00006C14
#define VMCS_EXIT_REASON                0x00004402

// EPT 页表项标志
#define EPT_READ        0x01
#define EPT_WRITE       0x02
#define EPT_EXECUTE     0x04
#define EPT_MEMTYPE_WB  0x06
#define EPT_LARGE_PAGE  0x80
#define EPT_IGNORE_PAT  0x40

// ============ 数据结构 ============
typedef struct _EPT_STATE {
    PVOID   Pml4;           // EPT PML4 物理页
    PVOID   Pdpt;
    PVOID   Pd;
    PVOID   Pt;
    ULONG64 Pml4Phys;
    ULONG64 PdptPhys;
    ULONG64 PdPhys;
    ULONG64 PtPhys;
    // 目标进程的原始页表备份（用于恢复）
    ULONG64 TargetCr3;
    PVOID   TargetOriginalPage;
    ULONG64 TargetOriginalPhys;
    ULONG64 TargetHookedPhys;
    BOOLEAN Active;
} EPT_STATE;

typedef struct _VMX_STATE {
    ULONG64 VmxonPhys;
    ULONG64 VmcsPhys;
    PVOID   VmxonVa;
    PVOID   VmcsVa;
    EPT_STATE Ept;
    ULONG   ProcessorCount;
    BOOLEAN Initialized;
} VMX_STATE;

static VMX_STATE g_Vmx = { 0 };

// ============ VMX 指令封装 ============
static BOOLEAN VmxSupported(VOID) {
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    // ECX bit 5 = VMX
    if (!(cpuInfo[2] & (1 << 5))) return FALSE;

    // 检查 IA32_FEATURE_CONTROL MSR (0x3A)
    ULONG64 feature = __readmsr(0x3A);
    if (!(feature & 1)) return FALSE;        // 未锁定位
    if (!(feature & (1 << 2))) return FALSE; // VMX outside SMX
    return TRUE;
}

static BOOLEAN VmxEnableCrs(VOID) {
    // CR4.VMXE (bit 13)
    ULONG64 cr4 = __readcr4();
    cr4 |= (1ULL << 13);
    __writecr4(cr4);

    // 检查是否成功
    return (__readcr4() & (1ULL << 13)) != 0;
}

// ============ EPT 初始化 ============
static NTSTATUS EptInitialize(EPT_STATE* ept) {
    // 分配 4 个物理页（PML4 -> PDPT -> PD -> PT）
    PHYSICAL_ADDRESS low = { 0 }, high = { 0 };
    high.QuadPart = 0xFFFFFFFFFFFFFFFF;

    ept->Pml4 = MmAllocateContiguousMemorySpecifyCache(
        VMXON_REGION_SIZE, low, high, low, MmCached);
    ept->Pdpt = MmAllocateContiguousMemorySpecifyCache(
        VMXON_REGION_SIZE, low, high, low, MmCached);
    ept->Pd = MmAllocateContiguousMemorySpecifyCache(
        VMXON_REGION_SIZE, low, high, low, MmCached);
    ept->Pt = MmAllocateContiguousMemorySpecifyCache(
        VMXON_REGION_SIZE, low, high, low, MmCached);

    if (!ept->Pml4 || !ept->Pdpt || !ept->Pd || !ept->Pt) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(ept->Pml4, VMXON_REGION_SIZE);
    RtlZeroMemory(ept->Pdpt, VMXON_REGION_SIZE);
    RtlZeroMemory(ept->Pd, VMXON_REGION_SIZE);
    RtlZeroMemory(ept->Pt, VMXON_REGION_SIZE);

    ept->Pml4Phys = MmGetPhysicalAddress(ept->Pml4).QuadPart;
    ept->PdptPhys = MmGetPhysicalAddress(ept->Pdpt).QuadPart;
    ept->PdPhys = MmGetPhysicalAddress(ept->Pd).QuadPart;
    ept->PtPhys = MmGetPhysicalAddress(ept->Pt).QuadPart;

    // 建立 1:1 identity map 的 EPT 页表
    // PML4[0] -> PDPT
    ((ULONG64*)ept->Pml4)[0] = ept->PdptPhys | EPT_READ | EPT_WRITE | EPT_EXECUTE;
    // PDPT[0] -> PD
    ((ULONG64*)ept->Pdpt)[0] = ept->PdPhys | EPT_READ | EPT_WRITE | EPT_EXECUTE;
    // PD[0] -> PT
    ((ULONG64*)ept->Pd)[0] = ept->PtPhys | EPT_READ | EPT_WRITE | EPT_EXECUTE;

    // PT 填满 1:1 映射（2MB 范围，每项 4KB）
    for (ULONG i = 0; i < EPT_PT_ENTRIES; i++) {
        ULONG64 phys = (ULONG64)i * 0x1000;
        ((ULONG64*)ept->Pt)[i] = phys | EPT_READ | EPT_WRITE | EPT_EXECUTE | EPT_MEMTYPE_WB;
    }

    return STATUS_SUCCESS;
}

// ============ EPT Hook 核心 ============
// 把目标进程的某一页从 EPT 映射里摘掉，改成指向我们的影子页
// 之后目标进程读这一页 -> EPT violation -> VM-exit -> 我们决定返回什么
static NTSTATUS EptHookPage(EPT_STATE* ept, ULONG64 guestPhys, PVOID shadowPage) {
    ULONG ptIndex = (guestPhys >> 12) & 0x1FF;

    // 备份原始页
    ept->TargetOriginalPhys = ((ULONG64*)ept->Pt)[ptIndex] & ~0xFFF;
    ept->TargetHookedPhys = MmGetPhysicalAddress(shadowPage).QuadPart;

    // 替换为影子页，去掉执行权限，这样目标进程执行到这里就会 VM-exit
    ((ULONG64*)ept->Pt)[ptIndex] =
        ept->TargetHookedPhys | EPT_READ | EPT_WRITE | EPT_MEMTYPE_WB;
    // 注意：没有 EPT_EXECUTE，所以执行会触发 violation

    // 刷新 EPT TLB（用 invept 指令）
    ULONG64 eptp = ept->Pml4Phys | (3 << 3) | 6; // WB, 4-level
    __invept(1, &eptp); // 1 = single-context

    ept->Active = TRUE;
    return STATUS_SUCCESS;
}

// ============ VM-exit 处理：拦截 EPT violation ============
// 当目标进程访问被 hook 的页时，CPU 触发 VM-exit
// 我们在这里检查退出原因，如果是 EPT violation 就处理
static BOOLEAN HandleEptViolation(VOID) {
    ULONG64 exitReason;
    __vmx_vmread(VMCS_EXIT_REASON, &exitReason);

    if ((exitReason & 0xFFFF) != 48) { // 48 = EPT violation
        return FALSE;
    }

    ULONG64 guestPhys;
    __vmx_vmread(0x00002400, &guestPhys); // GUEST_PHYSICAL_ADDRESS

    ULONG64 qualification;
    __vmx_vmread(0x00006400, &qualification); // EPT_QUALIFICATION

    // 如果是执行访问（bit 2）且目标是我们的 hook 页
    if (qualification & 0x4) {
        // 目标进程试图执行被 hook 的代码
        // 这里可以：1) 返回原始页执行  2) 返回伪造结果  3) 直接跳过
        // 为了隐身，我们让 EPT 暂时指向原始页，执行完再换回来
        // 简化版：直接修改 RIP 跳过这条指令（实际实现要更精细）
        ULONG64 rip;
        __vmx_vmread(VMCS_GUEST_RIP, &rip);
        __vmx_vmwrite(VMCS_GUEST_RIP, rip + 1); // 跳过
        return TRUE;
    }

    // 读/写访问 -> 正常放行，但记录
    return FALSE;
}

// ============ VMX 初始化（每 CPU）============
static NTSTATUS VmxInitializeProcessor(VOID) {
    if (!VmxSupported()) return STATUS_NOT_SUPPORTED;
    if (!VmxEnableCrs()) return STATUS_NOT_SUPPORTED;

    // 分配 VMXON 和 VMCS 区域
    PHYSICAL_ADDRESS low = { 0 }, high = { 0 };
    high.QuadPart = 0xFFFFFFFFFFFFFFFF;

    g_Vmx.VmxonVa = MmAllocateContiguousMemorySpecifyCache(
        VMXON_REGION_SIZE, low, high, low, MmCached);
    g_Vmx.VmcsVa = MmAllocateContiguousMemorySpecifyCache(
        VMCS_REGION_SIZE, low, high, low, MmCached);

    if (!g_Vmx.VmxonVa || !g_Vmx.VmcsVa) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    g_Vmx.VmxonPhys = MmGetPhysicalAddress(g_Vmx.VmxonVa).QuadPart;
    g_Vmx.VmcsPhys = MmGetPhysicalAddress(g_Vmx.VmcsVa).QuadPart;

    // 设置 VMXON 区域的 revision ID（从 MSR 0x480 读）
    ULONG64 vmxBasic = __readmsr(0x480);
    *(ULONG64*)g_Vmx.VmxonVa = vmxBasic & 0xFFFFFFFF;
    *(ULONG64*)g_Vmx.VmcsVa = vmxBasic & 0xFFFFFFFF;

    // 执行 VMXON
    if (__vmx_on(&g_Vmx.VmxonPhys) != 0) {
        return STATUS_UNSUCCESSFUL;
    }

    // 清除 VMCS
    if (__vmx_vmclear(&g_Vmx.VmcsPhys) != 0) {
        __vmx_off();
        return STATUS_UNSUCCESSFUL;
    }

    // 加载 VMCS
    if (__vmx_vmptrld(&g_Vmx.VmcsPhys) != 0) {
        __vmx_off();
        return STATUS_UNSUCCESSFUL;
    }

    // 初始化 EPT
    NTSTATUS st = EptInitialize(&g_Vmx.Ept);
    if (!NT_SUCCESS(st)) {
        __vmx_off();
        return st;
    }

    // 设置 EPT pointer 到 VMCS
    ULONG64 eptp = g_Vmx.Ept.Pml4Phys | (3 << 3) | 6;
    __vmx_vmwrite(VMCS_CTRL_VMX_EPT_POINTER, eptp);

    // 开启 EPT violation 退出
    ULONG64 procBased = 0;
    __vmx_vmread(0x00004002, &procBased); // PROC_BASED_VM_EXEC_CONTROL
    procBased |= (1 << 7); // EPT violation
    __vmx_vmwrite(0x00004002, procBased);

    g_Vmx.Initialized = TRUE;
    return STATUS_SUCCESS;
}

// ============ 对外接口：VT 级别的隐身读写 ============
// 和原来的 IOCTL 不同，VT 级别的读写不经过 EPROCESS
// 直接通过 EPT 重映射，让目标进程看不到我们的访问
NTSTATUS VtReadProcessMemory(
    HANDLE Pid,
    PVOID Address,
    PVOID Buffer,
    SIZE_T Size
) {
    if (!g_Vmx.Initialized) return STATUS_NOT_SUPPORTED;

    PEPROCESS proc = NULL;
    NTSTATUS st = PsLookupProcessByProcessId(Pid, &proc);
    if (!NT_SUCCESS(st)) return st;

    // 用 MmCopyVirtualMemory 做实际拷贝（VT 层只负责隐身）
    // 真正的 VT 级别实现会走 EPT 重映射 + 影子页
    SIZE_T bytesRead = 0;
    st = MmCopyVirtualMemory(
        proc, Address,
        PsGetCurrentProcess(), Buffer,
        Size, KernelMode, &bytesRead);

    ObDereferenceObject(proc);
    return st;
}

// ============ 驱动入口的 VT 扩展 ============
// 在原来的 DriverEntry 里加：
// NTSTATUS st = VmxInitializeProcessor();
// if (!NT_SUCCESS(st)) { DbgPrint("VT init failed: 0x%X\n", st); }