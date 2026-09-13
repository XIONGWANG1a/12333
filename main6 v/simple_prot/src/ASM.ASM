; language: asm, file: vmx_asm.asm, toolchain: ml64 (WDK x64)
; Complete: launch / resume / exit / vmcall-wrapper / teardown path
;
; GUEST_REGS layout (offsets from base pointer passed to C handler):
;   +00h rax    +08h rbx    +10h rcx    +18h rdx
;   +20h rsi    +28h rdi    +30h rbp    +38h r8
;   +40h r9     +48h r10    +50h r11    +58h r12
;   +60h r13    +68h r14    +70h r15
;
; Push order in VmxExitEntry: r15..rax. Top of stack after final
; push = rax = GUEST_REGS[0]. mov rcx,rsp passes the block pointer.

EXTERN VmExitHandlerC       : PROC
EXTERN g_TeardownDone       : BYTE     ; root sets 1 after vmx_off
EXTERN g_TeardownRsp        : QWORD    ; guest RSP stashed pre-vmx_off
EXTERN g_TeardownRip        : QWORD    ; guest RIP (post-advance) stashed

PUBLIC VmxLaunchEntry
PUBLIC VmxResumePoint
PUBLIC VmxExitEntry
PUBLIC VmxVmcallWrapper

.code

; ==========================================================================
; VmxLaunchEntry
;   C has written all guest state except GUEST_RSP/RIP/RFLAGS.
;   Writes those three via register-form vmwrite, then vmlaunch.
;   RAX=0 on success (guest resumes at VmxResumePoint), RAX=1 on vmfail.
;   Frame: 4 pushes (32) + sub 28h (40) = 72 bytes; 72 mod 16 = 8,
;   plus 8 for the inbound return address = 0 mod 16 at vmlaunch. OK.
; ==========================================================================
VmxLaunchEntry PROC
    push rbx
    push rsi
    push rdi
    push rbp
    sub  rsp, 28h

    ; GUEST_RSP (0x681C)
    mov  ecx, 681Ch
    mov  rdx, rsp
    vmwrite rcx, rdx

    ; GUEST_RIP (0x681E) -> VmxResumePoint
    mov  ecx, 681Eh
    lea  rdx, VmxResumePoint
    vmwrite rcx, rdx

    ; GUEST_RFLAGS (0x6820), bit 1 forced (reserved, must be 1)
    mov  ecx, 6820h
    pushfq
    pop  rdx
    or   rdx, 2
    vmwrite rcx, rdx

    vmlaunch
    ; ---- vmfail path ----
    add  rsp, 28h
    pop  rbp
    pop  rdi
    pop  rsi
    pop  rbx
    mov  eax, 1
    ret
VmxLaunchEntry ENDP

; ==========================================================================
; VmxResumePoint
;   Guest entry after successful vmlaunch. Stack identical to inside
;   VmxLaunchEntry — unwind and return 0.
; ==========================================================================
VmxResumePoint PROC
    add  rsp, 28h
    pop  rbp
    pop  rdi
    pop  rsi
    pop  rbx
    xor  eax, eax
    ret
VmxResumePoint ENDP

; ==========================================================================
; VmxExitEntry
;   Entered on VM exit via HOST_RIP. No swapgs anywhere: vmlaunch was
;   issued from kernel mode, host and guest GS_BASE both = KPCR.
;
;   Teardown path: C handler (root) executes vmx_off, stashes guest
;   RSP/RIP, sets RAX=STATUS_SUCCESS in the GPR block, sets
;   g_TeardownDone=1. Asm pops the GPR block (restoring wrapper
;   register state at the VMCALL), then loads stashed RSP and jumps
;   to stashed RIP — landing right after the VMCALL inside
;   VmxVmcallWrapper, which unwinds normally and returns to its
;   caller with RAX = STATUS_SUCCESS. VMX is off; execution is
;   plain ring-0.
; ==========================================================================
VmxExitEntry PROC
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax                              ; top of stack = GUEST_REGS[0]

    mov  rcx, rsp                         ; arg0 = PGUEST_REGS
    sub  rsp, 28h                         ; shadow(20h) + align(8h)
    call VmExitHandlerC
    add  rsp, 28h

    ; teardown? (RAX currently dead — true guest RAX is on the stack)
    mov  al, byte ptr [g_TeardownDone]
    test al, al
    jnz  teardown_path

    ; ---- normal path: restore, vmresume ----
    pop  rax
    pop  rbx
    pop  rcx
    pop  rdx
    pop  rsi
    pop  rdi
    pop  rbp
    pop  r8
    pop  r9
    pop  r10
    pop  r11
    pop  r12
    pop  r13
    pop  r14
    pop  r15

    vmresume
    ud2                                   ; vmresume failed — loud trap

teardown_path:
    ; ---- VMX off; restore GPRs, jump back into the wrapper ----
    pop  rax                              ; includes RAX=STATUS_SUCCESS
    pop  rbx
    pop  rcx
    pop  rdx
    pop  rsi
    pop  rdi
    pop  rbp
    pop  r8
    pop  r9
    pop  r10
    pop  r11
    pop  r12
    pop  r13
    pop  r14
    pop  r15

    mov  rsp, qword ptr [g_TeardownRsp]
    jmp  qword ptr [g_TeardownRip]
VmxExitEntry ENDP

; ==========================================================================
; VmxVmcallWrapper
;   Win x64 ABI in:  rcx=magic rdx=op r8=cr3 r9=va [rsp+28h]=buf [rsp+30h]=size
;   VMCALL regs:     rax=magic rcx=op rdx=cr3 r8=va r9=buf r10=size
;   Win x64 ABI out: rax = NTSTATUS written by root handler
;
;   Stack args are read BEFORE any prologue, so future prologue edits
;   cannot desync the offsets. Entry layout (no frame yet):
;     [rsp+00h] ret   [rsp+28h] buf   [rsp+30h] size
;   Frame: 3 pushes (24) + sub 28h (40) = 64; 64+8(ret) = 0 mod 16
;   at vmcall. OK.
; ==========================================================================
VmxVmcallWrapper PROC
    mov  r10, [rsp+30h]                   ; size  — before prologue
    mov  r11, [rsp+28h]                   ; buf   — before prologue

    push rbx
    push rsi
    push rdi
    sub  rsp, 28h

    mov  rax, rcx                         ; magic
    mov  rcx, rdx                         ; op
    mov  rdx, r8                          ; cr3
    mov  r8,  r9                          ; va
    mov  r9,  r11                         ; buf
    ; r10 = size already in place

    vmcall                                ; root writes guest RAX

    add  rsp, 28h
    pop  rdi
    pop  rsi
    pop  rbx
    ret
VmxVmcallWrapper ENDP

END