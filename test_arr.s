main:
    mov rax, 0
    mov [var_i], rax
    mov rax, 0
    mov [var_i], rax
for_cond_0:
    mov rdi, [var_i]
    cmp rdi, 5
    jge for_end_0
    mov rax, [var_i]
    mov rbx, 8
    imul rbx
    lea rbx, [var_arr]
    add rax, rbx
    push rax
    mov rax, [var_i]
    push rax
    mov rax, 1
    mov rbx, rax
    pop rax
    add rax, rbx
    push rax
    mov rax, 10
    mov rbx, rax
    pop rax
    imul rbx
    pop rbx
    mov [rbx], rax
    mov rax, [var_i]
    push rax
    mov rax, 1
    mov rbx, rax
    pop rax
    add rax, rbx
    mov [var_i], rax
    jmp for_cond_0
for_end_0:
    mov rax, 0
    mov [var_i], rax
for_cond_1:
    mov rdi, [var_i]
    cmp rdi, 5
    jge for_end_1
    mov rax, [var_i]
    mov rbx, 8
    imul rbx
    lea rbx, [var_arr]
    add rax, rbx
    mov rax, [rax]
    print_rax
    newline
    mov rax, [var_i]
    push rax
    mov rax, 1
    mov rbx, rax
    pop rax
    add rax, rbx
    mov [var_i], rax
    jmp for_cond_1
for_end_1:
    print "------------\n"
    newline
    mov rax, 0
    mov rbx, 8
    imul rbx
    lea rbx, [var_farr]
    add rax, rbx
    push rax
    mov rax, 1066192077
    mov [var_tmp_float], rax
    movss xmm0, [var_tmp_float]
    pop rbx
    movss [rbx], xmm0
    mov rax, 1
    mov rbx, 8
    imul rbx
    lea rbx, [var_farr]
    add rax, rbx
    push rax
    mov rax, 1074580685
    mov [var_tmp_float], rax
    movss xmm0, [var_tmp_float]
    pop rbx
    movss [rbx], xmm0
    mov rax, 2
    mov rbx, 8
    imul rbx
    lea rbx, [var_farr]
    add rax, rbx
    push rax
    mov rax, 0
    mov rbx, 8
    imul rbx
    lea rbx, [var_farr]
    add rax, rbx
    movss xmm0, [rax]
    movss [var_tmp_f2], xmm0
    mov rax, 1
    mov rbx, 8
    imul rbx
    lea rbx, [var_farr]
    add rax, rbx
    movss xmm0, [rax]
    movss xmm1, xmm0
    movss xmm0, [var_tmp_f2]
    addss xmm0, xmm1
    pop rbx
    movss [rbx], xmm0
    mov rax, 2
    mov rbx, 8
    imul rbx
    lea rbx, [var_farr]
    add rax, rbx
    movss xmm0, [rax]
    print_xmm
    newline
    lea rax, [var_farr]
    mov [var_ff], rax
    print "------------\n"
    newline
    mov rax, [var_ff]
    movss xmm0, [rax]
    print_xmm
    newline
    mov rax, 1
    mov rbx, 8
    imul rbx
    lea rbx, [var_ff]
    add rax, rbx
    mov rax, [rax]
    print_rax
    newline
    exit
var_main: data 0
var_arr: data 0
    data 0
    data 0
    data 0
    data 0
var_i: data 0
var_farr: data 0
    data 0
    data 0
var_tmp_float: data 0
var_tmp_f2: data 0
var_ff: data 0
