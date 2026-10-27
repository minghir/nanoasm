main:
    mov rax, 65
    mov [var_c], rax
    lea rax, [var_c]
    mov [var_ch], rax
    mov rax, [var_ch]
    print_rax
    newline
    print "------------\n"
    newline
    mov rax, [var_ch]
    mov rax, [rax]
    print_char
    newline
    mov rax, 90
    push rax
    mov rbx, [var_ch]
    pop rax
    mov [rbx], rax
    mov rax, [var_c]
    print_char
    newline
    mov rax, [var_ch]
    mov rax, [rax]
    print_char
    newline
    exit
var_main: data 0
var_c: data 0
var_ch: data 0
