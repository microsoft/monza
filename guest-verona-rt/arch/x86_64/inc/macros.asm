; Copyright Microsoft and Project Monza Contributors.
; SPDX-License-Identifier: MIT

extern local_apic_mapping
extern thread_execution_context
extern per_core_tss
extern per_core_data
extern acquire_semaphore.loop_suspend
extern acquire_semaphore.loop_hlt

; Sets the TLS base address (FS).
; Does not clobber any registers.
; Can be used in environments without the stack being set up.
%macro set_tls_base_macro 1
    wrfsbase %1
%endmacro

; Gets the TLS base address (FS).
; Does not clobber any other registers.
; Can be used in environments without the stack being set up.
%macro get_tls_base_macro 1
    rdfsbase %1
%endmacro

; Gets a per-core data for the current core.
; Takes two argumetns:
;   Register to store the result.
;   Constant representing field offset in struct.
; Does not clobber any other registers.
; Can be used in environments without the stack being set up.
%macro get_per_core_data_macro 2
    mov %1, [gs:%2]
%endmacro

; Gets the address to the per-core data for the current core.
; Takes two argumetns:
;   Register to store the result.
;   Constant representing field offset in struct.
; Does not clobber any other registers.
; Can be used in environments without the stack being set up.
%macro get_per_core_data_address_macro 2
    mov %1, [gs:PCD_SELF_OFFSET]
    add %1, %2
%endmacro


; Sets RAX to the core id of the core running it.
; Does not clobber any other registers.
; Can be used in environments without the stack being set up.
%macro get_current_core_id_macro 0
    get_per_core_data_macro rax, PCD_CORE_ID_OFFSET
%endmacro

; Gets the address to the per-core ThreadExecutionContext entry.
%macro get_thread_execution_context_entry 1
    get_per_core_data_address_macro %1, PCD_TEC_OFFSET
%endmacro

; Get a pointer to a given TaskStateSegment entry.
; Takes 3 register parameters (which must be different):
;   Register to contain the pointer at the end.
;   Register containing the core id (will be clobbered by the macro).
;   Temporary register that will be clobbered by the macro.
%macro get_tss_entry 3
    mov %1, per_core_tss
    shl %2, 3                          ; Entry to be used is at offset Core ID * 104 since entries are 104 bytes long
    mov %3, %2
    shl %2, 2
    add %3, %2
    shl %2, 1
    add %3, %2
    add %1, %3
%endmacro

%macro save_regs 0
    sub rsp, TRAP_REGS_END
    mov [rsp + TRAP_RDI_OFFSET], rdi
    mov [rsp + TRAP_RSI_OFFSET], rsi
    mov [rsp + TRAP_RDX_OFFSET], rdx
    mov [rsp + TRAP_RCX_OFFSET], rcx
    mov [rsp + TRAP_R8_OFFSET],  r8 
    mov [rsp + TRAP_R9_OFFSET],  r9 
    mov [rsp + TRAP_R10_OFFSET], r10
    mov [rsp + TRAP_R11_OFFSET], r11
    mov [rsp + TRAP_RBX_OFFSET], rbx
    mov [rsp + TRAP_RBP_OFFSET], rbp
    mov [rsp + TRAP_R12_OFFSET], r12
    mov [rsp + TRAP_R13_OFFSET], r13
    mov [rsp + TRAP_R14_OFFSET], r14
    mov [rsp + TRAP_R15_OFFSET], r15
    mov [rsp + TRAP_RAX_OFFSET], rax
%endmacro

%macro restore_regs 0
    mov rdi, [rsp + TRAP_RDI_OFFSET]
    mov rsi, [rsp + TRAP_RSI_OFFSET]
    mov rdx, [rsp + TRAP_RDX_OFFSET]
    mov rcx, [rsp + TRAP_RCX_OFFSET]
    mov r8, [rsp + TRAP_R8_OFFSET]
    mov r9, [rsp + TRAP_R9_OFFSET]
    mov r10, [rsp + TRAP_R10_OFFSET]
    mov r11, [rsp + TRAP_R11_OFFSET]
    mov rbx, [rsp + TRAP_RBX_OFFSET]
    mov rbp, [rsp + TRAP_RBP_OFFSET]
    mov r12, [rsp + TRAP_R12_OFFSET]
    mov r13, [rsp + TRAP_R13_OFFSET]
    mov r14, [rsp + TRAP_R14_OFFSET]
    mov r15, [rsp + TRAP_R15_OFFSET]
    mov rax, [rsp + TRAP_RAX_OFFSET]
    add rsp, TRAP_REGS_END
%endmacro

%macro set_kernel_cr3 1
    mov rax, [rsp + %1]
    and rax, 0x3
    test rax, rax
    jz .skip_cr3_set
    mov rax, [kernel_pagetable]
    mov cr3, rax
.skip_cr3_set:
%endmacro

%macro swap_kernel_cr3 2
    mov %1, [rsp + %2]
    and %1, 0x3
    test %1, %1
    jz .skip_cr3_swap
    mov %1, cr3
    mov rax, [kernel_pagetable]
    mov cr3, rax
    jmp .done_cr3_swap
.skip_cr3_swap:
    xor %1, %1
.done_cr3_swap:
%endmacro

%macro restore_swapped_cr3 1
    test %1, %1
    jz .skip_cr3_restore
    mov cr3, %1
.skip_cr3_restore:
%endmacro

; Ensure that the per-core data pointer is correct
; Clobbers the RAX register
%macro reset_per_core_pointer 0
    str rax
    sub rax, 0x33   ; Byte offset of TSS descriptors in GDT + 0x3
    shr rax, 4      ; Size of TSS descriptor
    shl rax, PCD_LOG2_SIZE
    add rax, [per_core_data]
    wrgsbase rax
%endmacro

; Check if interrupt is racing the check of the semaphore before halt.
; If it is, then force the check to restart from the top of the loop on resume.
; This enforces the atomicity of the check-and-halt sequence and avoids lost notifications.
; Takes 1 constant argument:
;   Offset of the RIP from the interrupt context relative to the current top of stack.
; Clobbers RAX.
%macro reset_suspend_check 1
    mov rax, [rsp + %1]
    cmp rax, acquire_semaphore.loop_suspend
    jl .end_reset_suspend_check
    cmp rax, acquire_semaphore.loop_hlt
    jge .end_reset_suspend_check
    mov qword [rsp + %1], acquire_semaphore.loop_suspend
.end_reset_suspend_check:
%endmacro

%macro interrupt_prelude 0
    save_regs
    swap_kernel_cr3 rbx, TRAP_REGS_END + INT_CS_OFFSET
    reset_per_core_pointer
    ; Switch the TLS pointer to the kernel one.
    ; Preserve the current TLS pointer in RBP.
    get_tls_base_macro rbp
    get_thread_execution_context_entry r10
    mov rax, [r10 + THREAD_EXECUTION_CONTEXT_TLS_OFFSET]
    set_tls_base_macro rax
%endmacro

%macro interrupt_prelude_no_status 0
    push qword 0
    interrupt_prelude
%endmacro

%macro interrupt_conclusion 0
    restore_swapped_cr3 rbx
    ; Restore the TLS pointer.
    set_tls_base_macro rbp
    reset_suspend_check TRAP_REGS_END + INT_RIP_OFFSET
    restore_regs
    ; Account for the error code in the stack.
    add rsp, 8
    iretq
%endmacro

INT_ERR_OFFSET      EQU 0
INT_RIP_OFFSET      EQU 0x8
INT_CS_OFFSET       EQU 0x10

; Matches struct TrapFrame in trap.h.
TRAP_RDI_OFFSET     EQU 0
TRAP_RSI_OFFSET     EQU 0x8
TRAP_RDX_OFFSET     EQU 0x10
TRAP_RCX_OFFSET     EQU 0x18
TRAP_R8_OFFSET      EQU 0x20
TRAP_R9_OFFSET      EQU 0x28
TRAP_R10_OFFSET     EQU 0x30
TRAP_R11_OFFSET     EQU 0x38
TRAP_RBX_OFFSET     EQU 0x40
TRAP_RBP_OFFSET     EQU 0x48
TRAP_R12_OFFSET     EQU 0x50
TRAP_R13_OFFSET     EQU 0x58
TRAP_R14_OFFSET     EQU 0x60
TRAP_R15_OFFSET     EQU 0x68
TRAP_RAX_OFFSET     EQU 0x70
TRAP_REGS_END       EQU 0x78

; Matches struct PerCoreData in per_core_data.h.
PCD_SELF_OFFSET     EQU 0
PCD_CORE_ID_OFFSET  EQU 0x8
PCD_NOT_GEN_OFFSET  EQU 0x10
PCD_TEC_OFFSET      EQU 0x18
PCD_LOG2_SIZE       EQU 7


; Matches struct ThreadExecutionContext in cores.h.
THREAD_EXECUTION_CONTEXT_CODE_OFFSET    EQU 0x0
THREAD_EXECUTION_CONTEXT_TLS_OFFSET     EQU 0x10
THREAD_EXECUTION_CONTEXT_SP_OFFSET      EQU 0x18
THREAD_EXECUTION_CONTEXT_LAST_SP_OFFSET EQU 0x28
