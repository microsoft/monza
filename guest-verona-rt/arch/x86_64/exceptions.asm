; Copyright Microsoft and Project Monza Contributors.
; SPDX-License-Identifier: MIT

extern print_exception
extern shutdown

extern wakeup_handler
extern hv_handler
extern page_fault_handler

extern kernel_pagetable

global setup_idt_generic
global idtr

%include "macros.asm"

%macro install_exception_gate 2
    mov rax, exception_gate_%2
    mov [edi + %1 * 16], ax             ; Low-word of address
    mov [edi + %1 * 16 + 2], si         ; Selector
    mov byte [edi + %1 * 16 + 4], 1     ; IST index
    mov byte [edi + %1 * 16 + 5], 0x8f  ; Type Exception Gate
    shr rax, 16
    mov [edi + %1 * 16 + 6], ax         ; High-word of address
    shr rax, 16
    mov [edi + %1 * 16 + 8], eax        ; Upper 32-bits of address
    mov dword [edi + %1 * 16 + 12], 0   ; Must be 0
%endmacro

%macro install_interrupt_gate 1
    mov rax, %1
    mov [edi + ecx], ax                 ; Low-word of address
    mov [edi + ecx + 2], si             ; Selector
    mov byte [edi + ecx+ 4], 1          ; IST index
    mov byte [edi + ecx + 5], 0x8e      ; Type Interrupt Gate
    shr rax, 16
    mov [edi + ecx + 6], ax             ; High-word of address
    shr rax, 16
    mov [edi + ecx + 8], eax            ; Upper 32-bits of address
    mov dword [edi + ecx + 12], 0       ; Must be 0
%endmacro

%macro enable_gate_for_compartments 1
    mov al, [edi + %1 * 16 + 5]         ; Type field
    or al, 0x60                         ; 0x11 for protection level
    mov [edi + %1 * 16 + 5], al
%endmacro

%macro print_and_halt 1
    set_kernel_cr3 INT_CS_OFFSET
    mov rdi, %1
    mov rsi, [rsp + INT_ERR_OFFSET]
    mov rdx, [rsp + INT_RIP_OFFSET]
    call print_exception
    jmp shutdown
%endmacro

%macro print_and_halt_no_status 1
    push qword 0
    print_and_halt %1
%endmacro

section .text
; void setup_idt_generic()
setup_idt_generic:
    ; Find code segment in GDT
    sgdt [gdtr]
    mov esi, [gdtr + 2]     ; Set rsi to the base address of the GDT
.loop_gdt_code_search:
    mov al, [esi + 5]
    test al, 0x08
    jnz .end_gdt_code_search
    add esi, 8
    jmp .loop_gdt_code_search
.end_gdt_code_search:
    ; At this point esi points to the GDT entry corresponding to the first code segment
    sub esi, [gdtr + 2]     ; Correct esi to be the offset in the GDT instead

    ; Install vectors into custom table
    mov rdi, idt            ; Set rdi to the base of the IDT

    install_exception_gate 00, 00
    install_exception_gate 01, 01
    install_exception_gate 02, 02
    install_exception_gate 03, breakpoint
    enable_gate_for_compartments 03
    install_exception_gate 04, 04
    install_exception_gate 05, 05
    install_exception_gate 06, 06
    install_exception_gate 07, 07
    install_exception_gate 08, 08
    install_exception_gate 09, 09
    install_exception_gate 10, 10
    install_exception_gate 11, 11
    install_exception_gate 12, 12
    install_exception_gate 13, 13
    install_exception_gate 14, pagefault
    install_exception_gate 15, reserved
    install_exception_gate 16, 16
    install_exception_gate 17, 17
    install_exception_gate 18, 18
    install_exception_gate 19, 19
    install_exception_gate 20, 20
    install_exception_gate 21, reserved
    install_exception_gate 22, reserved
    install_exception_gate 23, reserved
    install_exception_gate 24, reserved
    install_exception_gate 25, reserved
    install_exception_gate 26, reserved
    install_exception_gate 27, reserved
    install_exception_gate 28, hv
    install_exception_gate 29, reserved
    install_exception_gate 30, 30
    install_exception_gate 31, reserved

    mov ecx, 256 * 16
.loop_install_interrupt_gates:
    sub ecx, 16
    install_interrupt_gate nop_interrupt_gate
    cmp ecx, 32 * 16
    jge .loop_install_interrupt_gates

    ; Add custom interrupt handlers
    mov ecx, 0x80 * 16
    install_interrupt_gate wakeup_handler

    lidt [idtr]			    ; Load the content of IDT register with the newly set up table

    sti                     ; Enable interruts

    ret

exception_gate_00:
    print_and_halt_no_status ex_string_00

exception_gate_01:
    print_and_halt_no_status ex_string_01

exception_gate_02:
    print_and_halt_no_status ex_string_02

exception_gate_04:
    print_and_halt_no_status ex_string_04

exception_gate_05:
    print_and_halt_no_status ex_string_05

exception_gate_06:
    print_and_halt_no_status ex_string_06

exception_gate_07:
    print_and_halt_no_status ex_string_07

exception_gate_08:
    print_and_halt ex_string_08

exception_gate_09:
    print_and_halt_no_status ex_string_09

exception_gate_10:
    print_and_halt ex_string_10

exception_gate_11:
    print_and_halt ex_string_11

exception_gate_12:
    print_and_halt ex_string_12

exception_gate_13:
    print_and_halt ex_string_13

exception_gate_16:
    print_and_halt_no_status ex_string_16

exception_gate_17:
    print_and_halt ex_string_17

exception_gate_18:
    print_and_halt_no_status ex_string_18

exception_gate_19:
    print_and_halt_no_status ex_string_19

exception_gate_20:
    print_and_halt_no_status ex_string_20

exception_gate_30:
    print_and_halt ex_string_30

exception_gate_reserved:
    print_and_halt_no_status ex_string_res

exception_gate_breakpoint:
    interrupt_prelude_no_status
    mov rdi, ex_string_03
    mov rsi, [rsp + TRAP_REGS_END + INT_ERR_OFFSET]
    mov rdx, [rsp + TRAP_REGS_END + INT_RIP_OFFSET]
    call print_exception
    interrupt_conclusion

exception_gate_pagefault:
    interrupt_prelude
    mov rdi, cr2        ; Faulting address as first argument
    mov rsi, rbx        ; Page table root as second argument
    mov rdx, rsp        ; Trap frame as third argument
    call page_fault_handler
    interrupt_conclusion

exception_gate_hv:
    print_and_halt_no_status ex_string_28

nop_interrupt_gate:
    iretq

ex_string_00    db "Exception Raised: Divide by Zero", 0
ex_string_01    db "Exception Raised: Debug", 0
ex_string_02    db "Exception Raised: Non-maskable Interrupt", 0
ex_string_03    db "Exception Raised: Breakpoint", 0
ex_string_04    db "Exception Raised: Overflow", 0
ex_string_05    db "Exception Raised: Bound Range Exceeded", 0
ex_string_06    db "Exception Raised: Invalid Opcode", 0
ex_string_07    db "Exception Raised: Device Not Available", 0
ex_string_08    db "Exception Raised: Double Fault", 0
ex_string_09    db "Exception Raised: Coprocessor Segment Overrun", 0
ex_string_10    db "Exception Raised: Invalid TSS", 0
ex_string_11    db "Exception Raised: Segment Not Present", 0
ex_string_12    db "Exception Raised: Stack-Segment Fault", 0
ex_string_13    db "Exception Raised: General Protection Fault", 0
ex_string_14    db "Exception Raised: Page Fault", 0
ex_string_16    db "Exception Raised: x87 Floating-Point Exception", 0
ex_string_17    db "Exception Raised: Alignment Check", 0
ex_string_18    db "Exception Raised: Machine Check", 0
ex_string_19    db "Exception Raised: SIMD Floating-Point Exception", 0
ex_string_20    db "Exception Raised: Virtualization Exception", 0
ex_string_28    db "Exception Raised: HV", 0
ex_string_30    db "Exception Raised: Security Exception", 0
ex_string_res   db "Exception Raised: Reserved", 0

section .bss
alignb 16
gdtr:                           ; Global Descriptors Table Register
	            resw 1          ; Size (minus one)
	            resq 1          ; Base address

alignb 16
idt:            resb 256 * 16   ; IDT reserved area

section .data
align 16
idtr:                           ; Interrupt Descriptor Table Register
                dw 256 * 16 - 1 ; Size (minus one)
                dq idt          ; Base address
ip_string:      db "RIP: 0x"
                times 17 db 0
code_string:    db "Code: 0x"
                times 17 db 0
