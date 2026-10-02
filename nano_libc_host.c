#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>

// --- Terminal & Ecran ---

void nano_clear_screen(void) {
    // Secvențe ANSI standard pentru curățarea ecranului și resetarea cursorului
    printf("\033[H\033[J");
    fflush(stdout);
}

void nano_print(const char* str) {
    if (str) {
        fputs(str, stdout);
        fflush(stdout);
    }
}

// --- Citire tastatură în mod "Raw" (necesar pentru editori de text) ---

char nano_read_char(void) {
    struct termios oldt, newt;
    char ch;

    // Salvăm setările terminalului curent
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    // Dezactivăm Canonical Mode (să nu aștepte Enter) și Echo (să nu afișeze tasta apăsată automat)
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    ch = getchar();

    // Restaurăm setările vechi ale terminalului
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return ch;
}

// --- Gestionare Fișiere ---

int nano_read_file(const char* name, uint8_t* buffer, uint32_t max_size) {
    FILE* f = fopen(name, "rb");
    if (!f) return -1;

    size_t bytes_read = fread(buffer, 1, max_size, f);
    fclose(f);
    return (int)bytes_read;
}

int nano_write_file(const char* name, const uint8_t* data, uint32_t size) {
    FILE* f = fopen(name, "wb");
    if (!f) return -1;

    size_t bytes_written = fwrite(data, 1, size, f);
    fclose(f);
    return (bytes_written == size) ? 0 : -1;
}

int nano_create_file(const char* name, uint32_t size) {
    FILE* f = fopen(name, "wb");
    if (!f) return 0;

    if (size > 0) {
        int fd = fileno(f);
        if (ftruncate(fd, size) != 0) {
            fclose(f);
            return 0;
        }
    }
    
    fclose(f);
    return 1;
}


// Funcție custom pentru a converti string la int64_t (fără dependențe de atoll din libc)
int64_t parse_int64(const char* str) {
    int64_t res = 0;
    int sign = 1;
    int i = 0;
    
    if (str[0] == '-') {
        sign = -1;
        i = 1;
    } else if (str[0] == '+') {
        i = 1;
    }
    
    while (str[i] >= '0' && str[i] <= '9') {
        res = res * 10 + (str[i] - '0');
        i++;
    }
    
    return res * sign;
}
