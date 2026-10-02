; Test complet cu variabile în NanoASM
print "Test Variabile Globale:"
newline

; 1. Citim valoarea inițială a variabilei 'scor' în rax
mov rax, [scor]
print_rax
newline

; 2. Modificăm valoarea (adunăm 10)
add rax, 10

; 3. Salvăm înapoi în variabilă
mov [scor], rax

; 4. Citim din nou variabila să vedem schimbarea
mov rax, [scor]
print_rax
newline

print "Succes total cu variabilele!"
newline

print [label]
newline

mov rax, [label2]
print_rax
newline

exit

; --- SECȚIUNEA DE DATE ---
scor:
    quad 42          ; Declarăm o variabilă cu valoarea inițială 42

label:
	data "Mimi"
	
label2:
	data 22.3