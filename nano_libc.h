#ifndef NANO_LIBC_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Prototypes pentru funcțiile nano folosite de nanovi
void nano_clear_screen(void);
void nano_print(const char* str);
char nano_read_char(void);
int nano_read_file(const char* name, uint8_t* buffer, uint32_t max_size);
int nano_write_file(const char* name, const uint8_t* data, uint32_t size);
int nano_create_file(const char* name, uint32_t size);

// Implementare helper pentru itoa (deoarece Linux nu o are nativ în stdlib.h)
static inline void itoa(int n, char* buf, int radix) {
    if (radix == 10) {
        sprintf(buf, "%d", n);
    } else {
        sprintf(buf, "%x", n);
    }
}

#endif