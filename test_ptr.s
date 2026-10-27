main:
    mov rax, 100
    mov [var_x], rax
    lea rax, [var_x]
    mov [var_p], rax
    lea rax, [var_p]
    mov [var_pp], rax
    mov rax, 500
    push rax
    mov rbx, [var_pp]
    mov rbx, [rbx]
    pop rax
    mov [rbx], rax
    mov rax, [var_x]
    print_rax
    newline
    mov rax, [var_p]
    mov rax, [rax]
    print_rax
    newline
    mov rax, [var_pp]
    mov rax, [rax]
    mov rax, [rax]
    print_rax
    newline
    print "------------\n"
    newline
    mov rax, 1075838976
    mov [var_tmp_float], rax
    movss xmm0, [var_tmp_float]
    movss [var_f], xmm0
    lea rax, [var_f]
    mov [var_pf], rax
    lea rax, [var_pf]
    mov [var_ppf], rax
    mov rax, [var_ppf]
    mov rax, [rax]
    movss xmm0, [rax]
    movss [var_tmp_f2], xmm0
    mov rax, 1084227584
    mov [var_tmp_float], rax
    movss xmm0, [var_tmp_float]
    movss xmm1, xmm0
    movss xmm0, [var_tmp_f2]
    addss xmm0, xmm1
    mov rax, [var_ppf]
    mov rax, [rax]
    movss [rax], xmm0
    movss xmm0, [var_f]
    print_xmm
    newline
    mov rax, [var_ppf]
    mov rax, [rax]
    movss xmm0, [rax]
    print_xmm
    newline
    exit
var_main: data 0
var_x: data 0
var_p: data 0
var_pp: data 0
var_f: data 0
var_tmp_float: data 0
var_pf: data 0
var_ppf: data 0
var_tmp_f2: data 0
