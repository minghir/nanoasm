main:
    mov rax, 42
    mov [var_x], rax
    mov rax, [var_x]
    add rax, 18
    mov [var_y], rax
    print "Testam logica compilatorului...\n"
    newline
    mov rdi, [var_y]
    cmp rdi, 60
    jge if_body_0
    jmp else_body_0
if_body_0:
    print "MIRACOL! Matematica a functionat corect!\n"
    newline
    mov rax, [var_y]
    print_rax
    newline
    jmp if_end_0
else_body_0:
    print "Y nu e 60!\n"
    newline
if_end_0:
    mov rdi, [var_y]
    cmp rdi, 100
    jle if_body_1
    jmp else_body_1
if_body_1:
    print "Sub 100\n"
    newline
    mov rdi, [var_y]
    cmp rdi, 61
    je if_body_2
    jmp else_body_2
if_body_2:
    print "Exact 60\n"
    newline
    jmp if_end_2
else_body_2:
    print "nu e Exact 60\n"
    newline
if_end_2:
    jmp if_end_1
else_body_1:
    print "Nu e 100!\n"
    newline
if_end_1:
    exit
var_x: data 0
var_y: data 0
