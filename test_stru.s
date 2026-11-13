    jmp main
sum_list:
    push rbp
    mov rbp, rsp
    sub rsp, 1024
    mov [rbp-8], rdi
    mov rax, 0
    mov [rbp-16], rax
    mov rax, [rbp-8]
    mov [rbp-24], rax
while_cond_0:
    mov rax, [rbp-24]
    push rax
    mov rax, 0
    mov rbx, rax
    pop rax
    cmp rax, rbx
    jne eq_true_1
    mov rax, 0
    jmp eq_end_1
eq_true_1:
    mov rax, 1
eq_end_1:
    cmp rax, 0
    je while_end_0
    mov rax, [rbp-16]
    push rax
    lea rax, [rbp-24]
    mov rax, [rax]
    mov rax, [rax]
    mov rbx, rax
    pop rax
    add rax, rbx
    mov [rbp-16], rax
    lea rax, [rbp-24]
    mov rax, [rax]
    mov rbx, 8
    add rax, rbx
    mov rax, [rax]
    mov [rbp-24], rax
    jmp while_cond_0
while_end_0:
    mov rax, [rbp-16]
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret
scale_array:
    push rbp
    mov rbp, rsp
    sub rsp, 1024
    mov [rbp-8], rdi
    mov [rbp-16], rsi
    mov [rbp-24], rdx
    mov rax, 0
    mov [rbp-32], rax
for_cond_2:
    mov rax, [rbp-32]
    push rax
    mov rax, [rbp-16]
    mov rbx, rax
    pop rax
    cmp rax, rbx
    jl cmp_true_3
    mov rax, 0
    jmp cmp_end_3
cmp_true_3:
    mov rax, 1
cmp_end_3:
    cmp rax, 0
    je for_end_2
    mov rax, [rbp-32]
    mov rbx, 16
    imul rbx
    mov rbx, [rbp-8]
    add rax, rbx
    push rax
    mov rax, [rbp-32]
    mov rbx, 16
    imul rbx
    mov rbx, [rbp-8]
    add rax, rbx
    mov rax, [rax]
    push rax
    mov rax, [rbp-24]
    mov rbx, rax
    pop rax
    imul rbx
    pop rbx
    mov [rbx], rax
for_step_2:
    mov rax, [rbp-32]
    inc rax
    mov [rbp-32], rax
    jmp for_cond_2
for_end_2:
    mov rsp, rbp
    pop rbp
    ret
main:
    push rbp
    mov rbp, rsp
    sub rsp, 1024
    mov rax, 0
    mov [rbp-72], rax
for_cond_4:
    mov rax, [rbp-72]
    push rax
    mov rax, 4
    mov rbx, rax
    pop rax
    cmp rax, rbx
    jl cmp_true_5
    mov rax, 0
    jmp cmp_end_5
cmp_true_5:
    mov rax, 1
cmp_end_5:
    cmp rax, 0
    je for_end_4
    mov rax, [rbp-72]
    mov rbx, 16
    imul rbx
    lea rbx, [rbp-64]
    add rax, rbx
    push rax
    mov rax, [rbp-72]
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
    mov rax, [rbp-72]
    push rax
    mov rax, 3
    mov rbx, rax
    pop rax
    cmp rax, rbx
    jl cmp_true_7
    mov rax, 0
    jmp cmp_end_7
cmp_true_7:
    mov rax, 1
cmp_end_7:
    cmp rax, 0
    je else_body_6
    mov rax, [rbp-72]
    mov rbx, 16
    imul rbx
    lea rbx, [rbp-64]
    add rax, rbx
    mov rbx, 8
    add rax, rbx
    push rax
    mov rax, [rbp-72]
    push rax
    mov rax, 1
    mov rbx, rax
    pop rax
    add rax, rbx
    mov rbx, 16
    imul rbx
    lea rbx, [rbp-64]
    add rax, rbx
    pop rbx
    mov [rbx], rax
    jmp if_end_6
else_body_6:
    mov rax, [rbp-72]
    mov rbx, 16
    imul rbx
    lea rbx, [rbp-64]
    add rax, rbx
    mov rbx, 8
    add rax, rbx
    push rax
    mov rax, 0
    pop rbx
    mov [rbx], rax
if_end_6:
for_step_4:
    mov rax, [rbp-72]
    inc rax
    mov [rbp-72], rax
    jmp for_cond_4
for_end_4:
    lea rax, [rbp-64]
    push rax
    mov rax, 4
    push rax
    mov rax, 2
    push rax
    pop rdx
    pop rsi
    pop rdi
    call scale_array
    print "Elemente dupa scale_array(nodes, 4, 2):"
    newline
    mov rax, 0
    mov rbx, 16
    imul rbx
    lea rbx, [rbp-64]
    add rax, rbx
    mov rax, [rax]
    print_rax
    newline
    mov rax, 1
    mov rbx, 16
    imul rbx
    lea rbx, [rbp-64]
    add rax, rbx
    mov rax, [rax]
    print_rax
    newline
    mov rax, 2
    mov rbx, 16
    imul rbx
    lea rbx, [rbp-64]
    add rax, rbx
    mov rax, [rax]
    print_rax
    newline
    mov rax, 3
    mov rbx, 16
    imul rbx
    lea rbx, [rbp-64]
    add rax, rbx
    mov rax, [rax]
    print_rax
    newline
    print "Parcurgere prin pointeri (nodes[0].next->next->val):"
    newline
    mov rax, 0
    mov rbx, 16
    imul rbx
    lea rbx, [rbp-64]
    add rax, rbx
    mov rbx, 8
    add rax, rbx
    mov rax, [rax]
    mov rbx, 8
    add rax, rbx
    mov rax, [rax]
    mov rax, [rax]
    print_rax
    newline
    print "Suma parcursa prin lista inlantuita:"
    newline
    mov rax, 0
    mov rbx, 16
    imul rbx
    lea rbx, [rbp-64]
    add rax, rbx
    push rax
    pop rdi
    call sum_list
    print_rax
    newline
    mov rax, 0
    exit
    exit
