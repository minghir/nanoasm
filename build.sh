rm -f ./asm ./out ./cc
gcc asm.c nano_libc_host.c -o asm
gcc cc.c nano_libc_host.c -o cc -Wall -Wextra
