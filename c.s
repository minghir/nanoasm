main:
    mov rax, 0
    mov [var_x], rax
    print "Incepem numaratoarea:\n"
    newline
while_start_0:
    mov rdi, [var_x]
    cmp rdi, 20
    jge while_end_0
    mov rdi, [var_x]
    cmp rdi, 10
    jl if_body_0
    jmp else_body_0
if_body_0:
    mov rax, [var_x]
    print_rax
    newline
    jmp if_end_0
else_body_0:
    print "Mai mare ca 10"
    newline
if_end_0:
    mov rax, [var_x]
    add rax, 1
    mov [var_x], rax
    jmp while_start_0
while_end_0:
    print "Bucla s-a terminat cu succes!\n"
    newline
    exit
var_x: data 0
