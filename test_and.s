main:
    mov rax, 5
    mov [var_x], rax
    mov rax, 10
    mov [var_y], rax
    mov rdi, [var_x]
    cmp rdi, 10
    jg else_body_0
    mov rdi, [var_y]
    cmp rdi, 5
    jle else_body_0
    jmp if_body_0
if_body_0:
    print "Ambele conditii sunt adevarate!\n"
    newline
    jmp if_end_0
else_body_0:
    print "Aiurea!\n"
    newline
if_end_0:
    mov rdi, [var_x]
    cmp rdi, 20
    jle else_body_1
    mov rdi, [var_y]
    cmp rdi, 5
    jle else_body_1
    jmp if_body_1
if_body_1:
    print "Eroare!\n"
    newline
    jmp if_end_1
else_body_1:
    print "Short-circuit a functionat: prima fiind falsa, a srit direct aici!\n"
    newline
if_end_1:
    exit
var_x: data 0
var_y: data 0
