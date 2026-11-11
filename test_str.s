main:
    lea rax, [str_lit_0]
    mov [var_s], rax
    mov rax, [var_s]
    print_str
    newline
    exit
var_s: data 0
str_lit_0: data "Mimi"
    data 0
