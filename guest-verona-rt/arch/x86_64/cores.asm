; Copyright Microsoft and Project Monza Contributors.
; SPDX-License-Identifier: MIT

global acquire_semaphore
global acquire_semaphore.loop_suspend
global acquire_semaphore.loop_hlt
global ap_reset
global wakeup_handler
global ap_reset_handler

extern executing_cores
extern thread_notification_semaphores
extern local_apic_mapping
extern install_gdt
extern idtr
extern current_cr0
extern current_cr3
extern current_cr4
extern current_gs
extern finished_with_current

extern ap_init

%include "macros.asm"

section .text
; void acquire_semaphore(size_t& semaphore)
acquire_semaphore:
    ; The following code is meant to be the atomic check-and-halt waiting for a notification.
    ; Since there are no primitives for check-and-halt, the interrupt handler that can update the semaphore
    ; is responsible to emulate atomicity for this section.
.loop_suspend:
    mov rcx, [rdi]
    test rcx, rcx
    jnz .end_suspend
    hlt
    ; Marker for the interrupt handler to detect if hlt has been executed already.
    ; If this instruction pointer is reached, then we finished with the atomic block and can continue as normal.
    ; If not, then reset to .loop_suspend to ensure atomicity.
.loop_hlt:
    ; At this point we have been woken up safely, but we need to recheck the semaphore.
    jmp .loop_suspend
    ; Alternatively the interrupt handler could check for .end_suspend, but this label cannot be immediately preceded by hlt.
.end_suspend:
    ; Semaphore value positive, try to acquire it.
    mov rax, rcx
    dec rcx
    lock cmpxchg [rdi], rcx
    jnz .loop_suspend ; Failed to acquire semaphore doe to contention, restart
    ret

; Starting point for non-primary cores to wait for work to be posted to them.
ap_reset:
    call [ap_init]
.loop:
    ; Use RBX for table base address so it survives the function call
    get_thread_execution_context_entry rbx
    mov rax, [rbx + THREAD_EXECUTION_CONTEXT_CODE_OFFSET]   ; Check if the code pointer is null, go back to sleep if it is
    test rax, rax
    jnz .execute
    ;hlt
    jmp .loop

.execute:
    mov rdi, [rbx + 8]              ; Move argument to rdi

    lock inc qword [executing_cores]; Increment number of executing cores
    xor rcx, rcx                    ; Clear done flag to signal that execution is in progress
    mov [rbx + 32], rcx
    mov [rbx], rcx                  ; Clear code pointer to signal that it has been processed

    call rax
.done:
    mov rax, 0x1                    ; Set done flag to signal that execution is finished
    mov [rbx + 32], rax
    lock dec qword [executing_cores]; Decrement number of executing cores
    jmp .loop

; A simple interrupt handler that just acknowledges an IPI. Useful for getting an AP past a 'hlt' in the code.
align 8
wakeup_handler:
    push rdi
    push rax

    ; Increment generation counter to notify that at least one IPI has executed.
    get_per_core_data_address_macro rdi, PCD_NOT_GEN_OFFSET
    lock inc qword [rdi]

    reset_suspend_check 0x10

    ; Acknowledge the IPI
    mov rdi, [local_apic_mapping]
    xor eax, eax
    mov [rdi + 0xB0], eax

    pop rax
    pop rdi
    iretq

align 8
; Emulates Hyper-V core init which will also set up system registers.
ap_reset_handler:
    ; Set CR0, CR4 and GS based on the values the waker stored for us
    ; Needed before anything else since we want to set up the thread execution context
    mov rax, [current_cr0]
    mov cr0, rax
    mov rax, [current_cr4]
    mov cr4, rax
    mov rax, [current_gs]
    wrgsbase rax
    ; Set up FS and stack pointer for the core using pre-allocated region
    get_thread_execution_context_entry rsi
    mov rax, [rsi + THREAD_EXECUTION_CONTEXT_TLS_OFFSET]
    set_tls_base_macro rax
    mov rsp, [rsi + THREAD_EXECUTION_CONTEXT_SP_OFFSET]

    ; Install the GDT and TSS
    call install_gdt

    ; Install the IDT
    lidt [idtr]

    ; Set CR3 based on the values the waker stored for us
    mov rax, [current_cr3]
    mov cr3, rax
    mov qword [finished_with_current], 1

    ; Acknowledge the IPI
    mov rdi, [local_apic_mapping]
    xor eax, eax
    mov [rdi + 0xB0], eax

    sti
    jmp ap_reset        ; Start executing the actual reset sequence

ap_reset_string     db "AP reset", 0
