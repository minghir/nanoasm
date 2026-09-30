mov rdi, 5
print "Contor curent:\n"
bucla:
cmp rdi, 0
je gata
mov rax, rdi
print_rax
newline
dec rdi
jmp bucla
gata:
print "Gata!\n"
exit