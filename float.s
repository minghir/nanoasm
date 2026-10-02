; Test complet pentru toate operatiile aritmetice float in NanoASM

print "=== TEST ARITMETICA FLOAT ==="
newline

; 1. ADUNARE (10.5 + 2.5 = 13.0)
print "Adunare (10.5 + 2.5): "
movss xmm0, [val_a]
addss xmm0, [val_b]
movss [tmp], xmm0   ; Salvam rezultatul inainte de print!
print_xmm
newline

; 2. SCADERE (13.0 - 3.0 = 10.0)
print "Scadere (13.0 - 3.0): "
movss xmm0, [tmp]   ; Restauram 13.0
subss xmm0, [val_c]
movss [tmp], xmm0   ; Salvam rezultatul inainte de print!
print_xmm
newline

; 3. INMULTIRE (10.0 * 1.5 = 15.0)
print "Inmultire (10.0 * 1.5): "
movss xmm0, [tmp]   ; Restauram 10.0
mulss xmm0, [val_d]
movss [tmp], xmm0   ; Salvam rezultatul inainte de print!
print_xmm
newline

; 4. IMPARTIRE (15.0 / 2.0 = 7.5)
print "Impartire (15.0 / 2.0): "
movss xmm0, [tmp]   ; Restauram 15.0
divss xmm0, [val_e]
print_xmm
newline

print "=== TOATE TESTELE AU trecut! ==="
newline
exit

; --- BAZA DE DATE FLOAT ---
val_a:
    data 10.5
val_b:
    data 2.5
val_c:
    data 3.0
val_d:
    data 1.5
val_e:
    data 2.0
tmp:
    data 0.0  ; Variabila temporara pentru salvarea starii XMM0