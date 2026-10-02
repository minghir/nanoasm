; Test pentru pointeri în NanoASM v2.0
print "Test Pointeri:"
newline

; Alocăm 64 de octeți folosind malloc
mov rax, 2          ; Syscall malloc
mov rdi, 64         ; 64 octeți
int 0x80            ; Rax conține adresa din heap

; SALVĂM IMEDIAT POINTERUL DIN RAX ÎNTR-UN REGISTRU SIGUR (ex: rsi)
mov rsi, rax        

print "Memorie alocata cu succes!"
newline

; Acum putem folosi rsi pentru a scrie/citi din memorie folosind pointerul tău nou!
; De exemplu, ca să testăm un store:
mov rax, 42
mov [rsi], rax      ; Scriem valoarea 42 la adresa din rsi
print_rax
newline
exit