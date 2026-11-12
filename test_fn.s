    jmp main
main:
    mov rax, 100
    mov [var_x], rax
    mov rax, 50
    mov [var_y], rax
    mov rax, [var_x]
    push rax
    mov rax, [var_y]
    push rax
    pop rsi
    pop rdi
    call adunare
    mov [var_rezultat], rax
    mov rax, [var_rezultat]
    print_rax
    newline
    mov rax, 0
    exit
    exit
adunare:
    mov [var_a], rdi
    mov [var_b], rsi
    mov rax, [var_a]
    push rax
    mov rax, [var_b]
    mov rbx, rax
    pop rax
    add rax, rbx
    ret
    ret
var_a: data 0
var_b: data 0
var_x: data 0
var_y: data 0
var_rezultat: data 0
