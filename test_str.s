main:
    lea rax, [str_lit_0]
    mov [var_s], rax
    mov rax, 0
    mov rbx, [var_s]
    add rbx, rax
    movzx rax, [rbx]
    print_char
    newline
    mov rax, 0
    mov rbx, [var_s]
    add rbx, rax
    push rbx
    mov rax, 78
    pop rbx
    mov [rbx], al
    mov rax, [var_s]
    print_str
    newline
    mov rax, 0
    mov [var_i], rax
for_cond_0:
    mov rdi, [var_i]
    cmp rdi, 5
    jge for_end_0
    mov rax, [var_i]
    mov rbx, [var_s]
    add rbx, rax
    movzx rax, [rbx]
    print_char
    newline
    mov rax, [var_i]
    inc rax
    mov [var_i], rax
    jmp for_cond_0
for_end_0:
    mov rax, 0
    exit
var_s: data 0
var_i: data 0
str_lit_0: data "Mimi"
    data 0
