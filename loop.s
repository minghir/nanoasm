; Program de test pentru bucle și etichete în NanoASM v2.0
print "Incepem numaratoarea inversa:"
newline

mov rdi, 5          ; Setăm contorul la 5

loop_start:
cmp rdi, 0          ; Comparăm rdi cu 0
je loop_end         ; Dacă rdi == 0, ieșim din buclă

mov rax, rdi        ; Pregătim valoarea pentru afișare
print_rax           ; Afișăm valoarea curentă din rax
newline

dec rdi             ; Decrementăm contorul (rdi--)
jmp loop_start      ; Sărim înapoi la începutul buclei

loop_end:
print "Bum! Am terminat bucla cu succes!"
newline
exit