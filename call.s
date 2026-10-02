; Test de apeluri de funcții în NanoASM
print "Inainte de apel..."
newline

call afiseaza_mesaj  ; Apelăm funcția

print "Dupa apel, totul e ok!"
newline
exit

; Definiția funcției
afiseaza_mesaj:
    print "Salut din interiorul functiei!"
    newline
    ret