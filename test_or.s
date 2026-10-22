main:
    mov rax, 25
    mov [var_x], rax
    mov rax, 3
    mov [var_y], rax
    mov rdi, [var_x]
    cmp rdi, 10
    jl if_body_0
    mov rdi, [var_y]
    cmp rdi, 5
    jl if_body_0
    jmp else_body_0
    jmp if_body_0
if_body_0:
    print "OR a functionat: cel putin o conditie este adevarata!\n"
    newline
    jmp if_end_0
else_body_0:
    print "Eroare la OR!\n"
    newline
if_end_0:
    exit
var_x: data 0
var_y: data 0
