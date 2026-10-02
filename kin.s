; Program de test: Citire de la tastatură și Echo (Afișare)

print "Scrie ceva și apasă Enter:"
newline

; Notă: Dacă vrei un buffer sigur, poți folosi o adresă din zona liberă 
; sau o zonă alocată. Pentru test simplu, hai să vedem cum reacționează bufferul.
; Să zic că folosim un pointer în rsi. 
; (Mai întâi afișăm promptul, apoi citim)

mov rax, 2          ; Syscall Read / Input
mov rdi, 0x00500000 ; O adresă din heap-ul tău unde Nano OS are memorie liberă
int 0x80            ; Aici introduci textul de la tastatură și apeși Enter

print "Ai scris:"
newline

; Acum afișăm ce am citit de la adresa respectivă folosind syscall 1 (Print)
mov rax, 1          ; Syscall Print
mov rdi, 0x00500000 ; Aceeași adresă de unde citim textul
int 0x80            ; Printăm textul pe ecran
newline

exit