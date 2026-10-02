print "Afisam descrescator:"
newline
mov rdi, 5
bucla:
cmp rdi, 0
je gata
mov rax, rdi
print_rax
newline
dec rdi
jmp bucla
gata:
print "gata!"
newline
exit