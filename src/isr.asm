[bits 64]

; --- The Macros (Think of these as templates) ---

%macro PUSH_ALL 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro POP_ALL 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

extern isr_handler_0
section .text

global isr0
isr0:  
    push 0             ; Dummy error code
    push 0             ; Interrupt vector (0 for Division Error)
    jmp isr_common

isr_common:
    PUSH_ALL           ; NASM literally pastes the 15 pushes here

    ; In 64-bit C (System V ABI), the first argument is passed in RDI
    ; We pass RSP so our C code has a pointer to all the registers we just pushed
    mov rdi, rsp       
    
    ; IMPORTANT: The 64-bit ABI requires the stack to be 16-byte aligned 
    ; before calling a C function. Pushing 15 registers + 2 numbers + RBP
    ; usually handles this, but keep an eye on it.
    call isr_handler_0

    POP_ALL            ; NASM literally pastes the 15 pops here
    
    add rsp, 16        ; Clean up the 2 numbers we pushed at the start
    iretq              ; Return from interrupt