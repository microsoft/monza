; Copyright Microsoft and Project Monza Contributors.
; SPDX-License-Identifier: MIT

global start
global triple_fault

extern __early_stack_start
extern __early_stack_end
extern __stack_start
extern __stack_end
extern __bss_start
extern __bss_end
extern ap_reset_handler
extern setup_hypervisor
extern startcc
extern shutdown
extern mxcsr;
extern fp_control;

bits 64

%include "macros.asm"

section .text.start
; Assuming Linux direct boot setup
; RDI is core id
; RSI points to the Zero Page
start:
    test rdi, rdi
    jnz ap_reset_handler

    ; Use the loaded early stack for the stack pointer at the start
    mov rsp, __early_stack_end

    ; Save RSI in RBX to avoid clobbering on function call
    mov rbx, rsi

    ; Stop ring 0 from writing to read-protected addresses
    mov rax, cr0
    or eax, 1 << 16 ; Set write protect  CR0.WP
    mov cr0, rax

    ; Enable FSGSBASE
    mov rax, cr4
    or eax, 1 << 16	; Set CR4.FSGSBASE
    mov cr4, rax

    ; Init FPU
    fninit
    fldcw [fp_control]

    ; Enable SSE
    mov rax, cr0
    and ax, 0xFFFB  ; Clear coprocessor emulation CR0.EM
    or ax, 0x2      ; Set coprocessor monitoring CR0.MP
    mov cr0, rax
    mov rax, cr4
    or eax, 0x40600 ; Set CR4.OSXSAVE, CR4.OSFXSR, CR4.OSXMMEXCPT bits
    mov cr4, rax
    ldmxcsr [mxcsr]

    ; Update XCR0 with the enabled bits for FPU/SSE/AVX
    ; Can enable AVX in the future by also setting bit 2
    xor rcx, rcx
    xgetbv         ; Load XCR0 register
    or eax, 3      ; Set SSE, X87 bits
    xsetbv         ; Save back to XCR0

    ; Call the hypervisor here using the early stack
    ; This will guarantee that the regular stack and BSS are available
    ; This function should not use any BSS elements or too much stack
    call setup_hypervisor

    ; Set up the stack, including setting it to 0
    mov rdi, __stack_start  ; Start address
    mov rcx, __stack_end    ; Count
    sub rcx, rdi
    shr rcx, 3              ; Count = size / sizeof(uint64_t)
    xor rax, rax            ; Value to write (0)
    rep stosq
    mov rsp, __stack_end

    ; Set up the BSS, including setting it to 0
    mov rdi, __bss_start    ; Start address
    mov rcx, __bss_end      ; Count
    sub rcx, rdi
    shr rcx, 3              ; Count = size / sizeof(uint64_t)
    xor rax, rax            ; Value to write (0)
    rep stosq

    mov rdi, rbx
    call startcc

    jmp [shutdown]

; Trigger triple fault
triple_fault:
    lidt [fake_idtr]
    int 0
    ; Should never reach this point
    ret

align 16
fake_idtr:                      ; Fake Interrupt Descriptor Table Register
                dw 1            ; Size (minus one), size of 0 makes it unusable
                dq 0            ; Base address
