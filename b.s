main:
    mov rax, 10
    mov [var_x], rax
    print "Valoarea initiala a lui x:\n"
    newline
    mov rax, [var_x]
    print_rax
    mov rax, 50
    mov [var_x], rax
    print "Valoarea lui x dupa atribuire:\n"
    newline
    mov rax, [var_x]
    print_rax
    mov rax, [var_x]
    add rax, 15
    mov [var_x], rax
    print "Valoarea lui x dupa x = x + 15:\n"
    newline
    mov rax, [var_x]
    print_rax
    exit
var_x: data 0
