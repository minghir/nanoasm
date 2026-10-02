; Test complet pentru operatii aritmetice in NanoASM
print "Test Aritmetica:"
newline

; 1. Adunare: 10 + 5 = 15
mov rax, 10
add rax, 5
print_rax
newline

; 2. Scadere: 15 - 3 = 12
sub rax, 3
print_rax
newline

; 3. Inmultire: 12 * 4 = 48
mov rdx, 0
mov rdi, 4
mul rdi
print_rax
newline

; 4. Impartire: 48 / 6 = 8
mov rdx, 0
mov rsi, 6
div rsi
print_rax
newline

exit