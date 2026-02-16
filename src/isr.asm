[bits 64]
global isr_common
extern isr_handler_C

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

%macro PULL_ALL 0

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

%macro ISR_NOERR 1
global isr%1
isr%1:
	push 0 ; no error code
	push %1 ; vector number
	lea rax, [rel isr_common]
	jmp rax
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
	; CPU already pushed error code
	push %1 ; vector number
    lea rax, [rel isr_common]
	jmp rax
%endmacro

section .data
;---------------------variabels---------------------;



;---------------------variabels---------------------;
section .text
;---------------------functions---------------------;



;---------------------functions---------------------;

ISR_NOERR 0
ISR_NOERR 1
ISR_ERR 8
ISR_ERR 14

isr_common:


PUSH_ALL

mov rdi, rsp ; pass pointer to stack

call isr_handler_C

PULL_ALL

add rsp, 16 ; pop vector and error code out of stack
iretq
