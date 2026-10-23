main:
    mov rax, 0
    mov [var_tmp_float], rax
    movss xmm0, [var_tmp_float]
    movss [var_f], xmm0
    mov rax, 1065353216
    mov [var_tmp_float], rax
    movss xmm0, [var_tmp_float]
    movss [var_f], xmm0
for_cond_0:
    movss xmm0, [var_f]
    mov rax, 1080033280
    mov [var_tmp_float], rax
    movss xmm1, [var_tmp_float]
    ucomiss xmm0, xmm1
    ja for_end_0
    movss xmm0, [var_f]
    print_xmm
    newline
    movss xmm0, [var_f]
    movss [var_tmp_f2], xmm0
    mov rax, 1056964608
    mov [var_tmp_float], rax
    movss xmm0, [var_tmp_float]
    movss xmm1, xmm0
    movss xmm0, [var_tmp_f2]
    addss xmm0, xmm1
    movss [var_f], xmm0
    jmp for_cond_0
for_end_0:
    exit
var_f: data 0
var_tmp_float: data 0
var_tmp_f2: data 0
