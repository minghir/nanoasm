main:
    mov rax, 1078523330
    mov [var_tmp_float], rax
    movss xmm0, [var_tmp_float]
    movss [var_pi], xmm0
    mov rax, 1073741824
    mov [var_tmp_float], rax
    movss xmm0, [var_tmp_float]
    movss [var_raza], xmm0
    movss xmm0, [var_pi]
    movss [var_tmp_f1], xmm0
    movss xmm0, [var_raza]
    movss xmm1, xmm0
    movss xmm0, [var_tmp_f1]
    mulss xmm0, xmm1
    movss [var_tmp_f1], xmm0
    movss xmm0, [var_raza]
    movss xmm1, xmm0
    movss xmm0, [var_tmp_f1]
    mulss xmm0, xmm1
    movss [var_arie], xmm0
    print "Ariea cercului este: "
    newline
    movss xmm0, [var_arie]
    print_xmm
    newline
    exit
var_pi: data 0
var_tmp_float: data 0
var_raza: data 0
var_arie: data 0
var_tmp_f1: data 0
