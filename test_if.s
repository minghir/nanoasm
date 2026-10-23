main:
    mov rax, 1103856927
    mov [var_tmp_float], rax
    movss xmm0, [var_tmp_float]
    movss [var_z], xmm0
    movss xmm0, [var_z]
    mov rax, 1103856927
    mov [var_tmp_float], rax
    movss xmm1, [var_tmp_float]
    ucomiss xmm0, xmm1
    jne else_body_0
    movss xmm0, [var_z]
    print_xmm
    newline
    jmp if_end_0
else_body_0:
    print "airea"
    newline
if_end_0:
    exit
var_z: data 0
var_tmp_float: data 0
