[bits 64]

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
    PUSH_ALL

    ; pass RSP to have a pointer to all the registers pushed
    mov rdi, rsp       

    call isr_handler_0

    POP_ALL
    
    add rsp, 16        
    iretq              