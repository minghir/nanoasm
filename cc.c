#include "nano_libc.h"

// Tipuri de Tokenuri pentru Subsetul C
typedef enum {
    TOKEN_EOF = 0,
    TOKEN_INT_KW,     // int
    TOKEN_PRINT_KW,   // print
    TOKEN_IDENTIFIER, // ex: x, main
    TOKEN_NUMBER,     // ex: 42
    TOKEN_STRING,     // ex: "Hello!"
    TOKEN_ASSIGN,     // =
    TOKEN_SEMICOLON,  // ;
    TOKEN_LPAREN,     // (
    TOKEN_RPAREN,     // )
    TOKEN_LBRACE,     // {
    TOKEN_RBRACE      // }
} TokenType;

typedef struct {
    TokenType type;
    char text[64];
} Token;

static const char* src_ptr = 0;

void init_tokenizer(const char* src) {
    src_ptr = src;
}

int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

int is_digit(char c) {
    return c >= '0' && c <= '9';
}

int is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

Token get_next_token() {
    Token token;
    token.type = TOKEN_EOF;
    token.text[0] = '\0';

    if (!src_ptr || *src_ptr == '\0') return token;

    // Ignorăm spațiile albe
    while (*src_ptr == ' ' || *src_ptr == '\t' || *src_ptr == '\r' || *src_ptr == '\n') {
        src_ptr++;
    }

    if (*src_ptr == '\0') return token;

    char c = *src_ptr;

    // Identificatori și cuvinte cheie
    if (is_alpha(c)) {
        int i = 0;
        while (is_alnum(*src_ptr) && i < 63) {
            token.text[i++] = *src_ptr++;
        }
        token.text[i] = '\0';

        if (strcmp(token.text, "int") == 0) token.type = TOKEN_INT_KW;
        else if (strcmp(token.text, "print") == 0) token.type = TOKEN_PRINT_KW;
        else token.type = TOKEN_IDENTIFIER;
        return token;
    }

    // Numere
    if (is_digit(c)) {
        int i = 0;
        while (is_digit(*src_ptr) && i < 63) {
            token.text[i++] = *src_ptr++;
        }
        token.text[i] = '\0';
        token.type = TOKEN_NUMBER;
        return token;
    }

    // Șiruri între ghilimele
    if (c == '"') {
        src_ptr++;
        int i = 0;
        while (*src_ptr != '"' && *src_ptr != '\0' && i < 63) {
            token.text[i++] = *src_ptr++;
        }
        token.text[i] = '\0';
        if (*src_ptr == '"') src_ptr++;
        token.type = TOKEN_STRING;
        return token;
    }

    // Simboluri unice
    src_ptr++;
    token.text[0] = c;
    token.text[1] = '\0';

    switch (c) {
        case '=': token.type = TOKEN_ASSIGN; break;
        case ';': token.type = TOKEN_SEMICOLON; break;
        case '(': token.type = TOKEN_LPAREN; break;
        case ')': token.type = TOKEN_RPAREN; break;
        case '{': token.type = TOKEN_LBRACE; break;
        case '}': token.type = TOKEN_RBRACE; break;
        default: token.type = TOKEN_EOF; break;
    }

    return token;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        nano_print("Utilizare: cc <sursa.c> <iesire.s>\n");
        return 1;
    }

    nano_print("[nanoc] Compilator C nativ pornit pe host...\n");

    uint8_t src_buffer[8192];
    int bytes_read = nano_read_file(argv[1], src_buffer, 8191);
    if (bytes_read <= 0) {
        nano_print("[nanoc] Eroare la citirea sursei C!\n");
        return 1;
    }
    src_buffer[bytes_read] = '\0';

    init_tokenizer((char*)src_buffer);

    // Deschidem fișierul de ieșire .s
    if (nano_create_file(argv[2], 0) != 0) {
        nano_print("[nanoc] Eroare la crearea fisierului .s de iesire!\n");
        return 1;
    }

    char asm_out[4096];
    int asm_idx = 0;

    // Header pentru NanoASM
    const char* header = "main:\n";
    for (int i = 0; header[i] != '\0'; i++) asm_out[asm_idx++] = header[i];

    // Parser simplu secvențial (Recursive Descent simplificat)
    Token t = get_next_token();
    
    // Așteptăm "int main () {"
    // Pentru simplitate, sărim direct la căutarea instrucțiunilor print
    while (t.type != TOKEN_EOF) {
        if (t.type == TOKEN_PRINT_KW) {
            // Așteptăm '('
            t = get_next_token(); // ar trebui să fie '('
            t = get_next_token(); // ar trebui să fie TOKEN_STRING (mesajul)
            
            if (t.type == TOKEN_STRING) {
                // Generăm linia de ASM: print "mesaj" + newline
                const char* p_cmd = "    print \"";
                for (int i = 0; p_cmd[i] != '\0'; i++) asm_out[asm_idx++] = p_cmd[i];
                
                for (int i = 0; t.text[i] != '\0'; i++) asm_out[asm_idx++] = t.text[i];
                
                const char* p_end = "\"\n    newline\n";
                for (int i = 0; p_end[i] != '\0'; i++) asm_out[asm_idx++] = p_end[i];
            }
        }
        t = get_next_token();
    }

    // Adăugăm exit la sfârșit
    const char* footer = "    exit\n";
    for (int i = 0; footer[i] != '\0'; i++) asm_out[asm_idx++] = footer[i];
    asm_out[asm_idx] = '\0';

    // Scriem în fișierul .s
    nano_write_file(argv[2], (const uint8_t*)asm_out, asm_idx);

    nano_print("[nanoc] Generare cod ASM (.s) finalizata cu succes!\n");
    return 0;
}