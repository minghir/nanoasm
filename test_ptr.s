main:
    mov rax, 10
    mov [var_a], rax
    lea rax, [var_a]
    mov [var_b], rax
    mov rax, [var_b]
    print_rax
    newline
    print "------------\n"
    newline
    mov rax, [var_b]
    mov rax, [rax]
    print_rax
    newline
    exit
var_a: data 0
var_b: data 0
