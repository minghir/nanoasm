rm ./asm ./out
cp ../nano_os/nano_libc/nanoasm.c ./
cp ../nano_os/nano_libc/*.s ./
gcc nanoasm.c nano_libc_host.c -o asm
./asm float.s 