main:
    mov rax, 5
    mov [var_x], rax
    mov rax, 10
    mov [var_y], rax
    mov rax, 20
    mov [var_x], rax
    mov rdi, [var_x]
    cmp rdi, 0
    je else_body_0
    jmp if_body_0
if_body_0:
    mov rax, [var_x]
    print_rax
    newline
else_body_0:
if_end_0:
while_start_0:
    mov rax, 1
    mov [var_x], rax
    mov rdi, [var_x]
    cmp rdi, 0
    je while_end_0
    mov rax, [var_x]
    print_rax
    newline
    jmp while_start_0
while_end_0:
    exit
var_x: data 0
var_y: data 0
