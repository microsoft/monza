; Copyright Microsoft and Project Monza Contributors.
; SPDX-License-Identifier: MIT

ORG 0x100000
BITS 64

; This is the emulated Linux 64-bit boot process targeted by the Pure64 bootloader.
; The Pure64 bootloader uses the range 0x100000 to 0x108000 for a kernel of a size at most 32kB.
; The Pure64 bootloader also writes a byte-map of free 2MB pages starting at 0x400000.
; We want to put the start of the real kernel at a 2MB boundary so we can use 0x600000.
; This allows the byte-map to work up to a total RAM size of 4TB.
; We use the range 0x100000 to 0x108000 to emulate the Linux Zero Page functionality and
; redirect execution to 0x600000 where the real kernel entry-point is.

stage0_start:
    cli             ; Disable interrupts as specified by the Linux 64-bit boot protocol

; Copy E820 memory map into Zero Page format
; Pure64 keeps entries of size 32 bytes starting at BootloaderE820Entries
; 0-sized entry signals end of entries
; Zero Page uses 20 byte entries which matches the first 20 bytes in the Pure64 entries
; Separate entry count in the Zero Page
    mov rsi, BootloaderE820Entries
    mov rdi, E820Table
    xor rcx, rcx
.read_e820_entry:
    mov rax, [rsi]
    mov rbx, [rsi + 8]
    mov edx, [rsi + 16]
    test rbx, rbx
    jz .end_e820
.process_e820_entry: 
    mov [rdi], rax
    mov [rdi + 8], rbx
    mov [rdi + 16], edx
    add rsi, 32
    add rdi, 20
    inc cl
    jmp .read_e820_entry
.end_e820:
    mov [E820Entries], cl

; Install the kernel start as the destination for the AP reset interrupt in the bootloader IDT
    sidt [idtr]
    mov rdi, [idtr + 2]
    add rdi, 0x81 * 16          ; AP reset is vector 0x81
    mov rax, .KernelAPStart
    mov [rdi], ax               ; Low-word of address
    mov word [rdi + 2], 8       ; Code Selector in bootloader GDT
    mov byte [rdi + 4], 0       ; Must be 0
    mov byte [rdi + 5], 0x8e    ; Type Interrupt Gate
    shr rax, 16
    mov [rdi + 6], ax           ; High-word of address
    shr rax, 16
    mov [rdi + 8], eax          ; Upper 32-bits of address
    mov dword [rdi + 12], 0     ; Must be 0

.KernelAPStart:
; Read the core ID from the LocalAPIC
; Pure64 keepts the LocalAPIC address in BootloaderLocalAPICAddress
    mov rax, [BootloaderLocalAPICAddress]
    mov eax, [rax + 0x20]
    shr eax, 24

; Set up expected initial register state
    mov rdi, rax
    mov rsi, ZeroPage

; Jump to actual kernel start
    jmp KernelStart

section .bss
alignb 16
idtr:                           ; Interrupt Descriptor Table Register
                resw 1          ; Size (minus one)
                resq 1          ; Base address

BootloaderLocalAPICAddress: equ 0x5A28
BootloaderE820Entries:      equ 0x6000

KernelStart:                equ 0x600000

ZeroPage:                   equ 0x101000
E820Entries:                equ ZeroPage + 0x1e8
E820Table:                  equ ZeroPage + 0x2d0
