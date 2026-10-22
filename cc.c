#include "nano_libc.h"

// Am adăugat TOKEN_PLUS, TOKEN_MINUS și TOKEN_UNKNOWN pentru stabilitate
typedef enum {
    TOKEN_EOF = 0, TOKEN_INT_KW, TOKEN_PRINT_KW, TOKEN_IF_KW, TOKEN_ELSE_KW, TOKEN_WHILE_KW, TOKEN_IDENTIFIER, 
    TOKEN_NUMBER, TOKEN_STRING, TOKEN_ASSIGN, 
    TOKEN_EQ, TOKEN_NE, TOKEN_GT, TOKEN_LT, TOKEN_GE, TOKEN_LE,
    TOKEN_SEMICOLON, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
	TOKEN_AND, TOKEN_OR, // && și ||
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_UNKNOWN
} TokenType;

typedef struct {
    TokenType type;
    char text[256];
} Token;

static const char* src_ptr = 0;

void init_tokenizer(const char* src) { src_ptr = src; }
int is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
int is_digit(char c) { return c >= '0' && c <= '9'; }
int is_alnum(char c) { return is_alpha(c) || is_digit(c); }

void append_str(char* dest, int* idx, const char* src) {
    for (int i = 0; src[i] != '\0'; i++) dest[(*idx)++] = src[i];
}

void append_int(char* dest, int* idx, int val) {
    char buf[32];
    itoa(val, buf, 10);
    append_str(dest, idx, buf);
}

// Funcții utilitare robuste adăugate pentru cc.c
int str_eq(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return 0;
        i++;
    }
    return s1[i] == s2[i];
}

void str_cpy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0') { dest[i] = src[i]; i++; }
    dest[i] = '\0';
}

int str_to_int(const char* str) {
    int val = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (is_digit(str[i])) val = val * 10 + (str[i] - '0');
    }
    return val;
}

const char* get_inverse_jump(TokenType op) {
    if (op == TOKEN_EQ) return "jne"; 
    if (op == TOKEN_NE) return "je";  
    if (op == TOKEN_GT) return "jle"; // Dacă e >, sărim la else dacă e <=
    if (op == TOKEN_LT) return "jg";  // Dacă e <, sărim la else dacă e >= (în loc de jle)
    if (op == TOKEN_GE) return "jl";  
    if (op == TOKEN_LE) return "jge"; 
    return "jne"; 
}

const char* get_direct_jump(TokenType op) {
    if (op == TOKEN_EQ) return "je";
    if (op == TOKEN_NE) return "jne";
    if (op == TOKEN_GT) return "jg";  // Corectat pentru salt direct la >
    if (op == TOKEN_LT) return "jl";  // Corectat pentru salt direct la <
    if (op == TOKEN_GE) return "jge"; 
    if (op == TOKEN_LE) return "jle"; 
    return "je";
}

const char* token_type_to_str(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_INT_KW: return "INT_KW";
        case TOKEN_PRINT_KW: return "PRINT_KW";
        case TOKEN_IF_KW: return "IF_KW";
        case TOKEN_ELSE_KW: return "ELSE_KW";
        case TOKEN_WHILE_KW: return "WHILE_KW";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_ASSIGN: return "ASSIGN=";
        case TOKEN_EQ: return "EQ (==)";
        case TOKEN_NE: return "NE (!=)";
        case TOKEN_GT: return "GT (>)";
        case TOKEN_LT: return "LT (<)";
        case TOKEN_GE: return "GE (>=)";
        case TOKEN_LE: return "LE (<=)";
        case TOKEN_AND: return "AND (&&)"; // <--- Adăugat
        case TOKEN_OR: return "OR (||)";   // <--- Adăugat
        case TOKEN_SEMICOLON: return "SEMICOLON (;)";
        case TOKEN_LPAREN: return "LPAREN (()";
        case TOKEN_RPAREN: return "RPAREN ())";
        case TOKEN_LBRACE: return "LBRACE ({)";
        case TOKEN_RBRACE: return "RBRACE (})";
        case TOKEN_PLUS: return "PLUS (+)";
        case TOKEN_MINUS: return "MINUS (-)";
        default: return "UNKNOWN";
    }
}


Token get_next_token() {
    Token token;
    token.type = TOKEN_EOF;
    token.text[0] = '\0';

    if (!src_ptr || *src_ptr == '\0') return token;

    while (1) {
        // Ignorăm spațiile albe, tab-urile, newlines
        while (*src_ptr == ' ' || *src_ptr == '\t' || *src_ptr == '\r' || *src_ptr == '\n') {
            src_ptr++;
        }
        if (*src_ptr == '\0') return token;

        // Ignorăm comentariile C++ style (// ...)
        if (*src_ptr == '/' && *(src_ptr + 1) == '/') {
            while (*src_ptr != '\n' && *src_ptr != '\0') {
                src_ptr++;
            }
            continue; // Reluăm bucla după comentariu
        }
        break;
    }

    if (*src_ptr == '\0') return token;

    char c = *src_ptr;

    if (is_alpha(c)) {
        int i = 0;
        while (is_alnum(*src_ptr) && i < 63) {
            token.text[i++] = *src_ptr++;
        }
        token.text[i] = '\0';

        if (str_eq(token.text, "int")) token.type = TOKEN_INT_KW;
        else if (str_eq(token.text, "print")) token.type = TOKEN_PRINT_KW;
        else if (str_eq(token.text, "if")) token.type = TOKEN_IF_KW;
        else if (str_eq(token.text, "else")) token.type = TOKEN_ELSE_KW;
		else if (str_eq(token.text, "while")) token.type = TOKEN_WHILE_KW;
        else token.type = TOKEN_IDENTIFIER;
        return token;
    }

    if (is_digit(c)) {
        int i = 0;
        while (is_digit(*src_ptr) && i < 63) {
            token.text[i++] = *src_ptr++;
        }
        token.text[i] = '\0';
        token.type = TOKEN_NUMBER;
        return token;
    }

    if (c == '"') {
        src_ptr++;
        int i = 0;
        while (*src_ptr != '"' && *src_ptr != '\0' && i < 255) {
            token.text[i++] = *src_ptr++;
        }
        token.text[i] = '\0';
        if (*src_ptr == '"') src_ptr++;
        token.type = TOKEN_STRING;
        return token;
    }

    src_ptr++;
    token.text[0] = c;
    token.text[1] = '\0';

    switch (c) {
        case '=': 
            if (*src_ptr == '=') { src_ptr++; token.type = TOKEN_EQ; str_cpy(token.text, "=="); } 
            else { token.type = TOKEN_ASSIGN; }
            break;
        case '!':
            if (*src_ptr == '=') { src_ptr++; token.type = TOKEN_NE; str_cpy(token.text, "!="); }
            else { token.type = TOKEN_UNKNOWN; }
            break;
        case '>':
            if (*src_ptr == '=') { src_ptr++; token.type = TOKEN_GE; str_cpy(token.text, ">="); }
            else { token.type = TOKEN_GT; str_cpy(token.text, ">"); }
            break;
        case '<':
            if (*src_ptr == '=') { src_ptr++; token.type = TOKEN_LE; str_cpy(token.text, "<="); }
            else { token.type = TOKEN_LT; str_cpy(token.text, "<"); }
            break;
        case ';': token.type = TOKEN_SEMICOLON; break;
        case '(': token.type = TOKEN_LPAREN; break;
        case ')': token.type = TOKEN_RPAREN; break;
        case '{': token.type = TOKEN_LBRACE; break;
        case '}': token.type = TOKEN_RBRACE; break;
        case '+': token.type = TOKEN_PLUS; break;
        case '-': token.type = TOKEN_MINUS; break;
		case '&':
			if (*src_ptr == '&') { src_ptr++; token.type = TOKEN_AND; str_cpy(token.text, "&&"); }
			else { token.type = TOKEN_UNKNOWN; }
			break;
		case '|':
			if (*src_ptr == '|') { src_ptr++; token.type = TOKEN_OR; str_cpy(token.text, "||"); }
			else { token.type = TOKEN_UNKNOWN; }
			break;
        default:  token.type = TOKEN_UNKNOWN; break;
    }
	
	
    if (token.type != TOKEN_EOF) {
        nano_print("  [LEXER] Token gasit -> Tip: ");
        nano_print((char*)token_type_to_str(token.type));
        nano_print(" | Text: '");
        nano_print(token.text);
        nano_print("'\n");
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

    if (nano_create_file(argv[2], 0) != 0) {
        nano_print("[nanoc] Eroare la crearea fisierului .s de iesire!\n");
        return 1;
    }

    char asm_out[4096];
    int asm_idx = 0;

    const char* header = "main:\n";
    for (int i = 0; header[i] != '\0'; i++) asm_out[asm_idx++] = header[i];

    char var_names[32][32];
    int var_values[32];
    int var_count = 0;

	int if_counter = 0;       // Numărător pentru a crea etichete unice (if_body_0, if_end_0, etc.)
    int block_stack[32];      // Stivă pentru a ști ce if se închide când dăm de '}'
    int block_depth = 0;
	
	int while_counter = 0;

    Token t = get_next_token();
    
    while (t.type != TOKEN_EOF) {
        if (t.type == TOKEN_INT_KW) {
            Token name_tok = get_next_token();
            Token assign_tok = get_next_token();
            
            if (name_tok.type == TOKEN_IDENTIFIER && assign_tok.type == TOKEN_ASSIGN) {
                Token val1_tok = get_next_token();
                int final_val = 0; // Folosit temporar ca fallback
                
                // PASUL 1: Punem prima valoare în registrul RAX
                if (val1_tok.type == TOKEN_NUMBER) {
                    final_val = str_to_int(val1_tok.text);
                    append_str(asm_out, &asm_idx, "    mov rax, ");
                    append_str(asm_out, &asm_idx, val1_tok.text);
                    append_str(asm_out, &asm_idx, "\n");
                } else if (val1_tok.type == TOKEN_IDENTIFIER) {
                    for (int v = 0; v < var_count; v++) {
                        if (str_eq(var_names[v], val1_tok.text)) { final_val = var_values[v]; break; }
                    }
                    append_str(asm_out, &asm_idx, "    mov rax, [var_");
                    append_str(asm_out, &asm_idx, val1_tok.text);
                    append_str(asm_out, &asm_idx, "]\n");
                }
                
                // PASUL 2: Look-ahead pentru operații matematice (+ sau -)
                const char* saved_ptr = src_ptr;
                Token op_tok = get_next_token();
                
                if (op_tok.type == TOKEN_PLUS || op_tok.type == TOKEN_MINUS) {
                    Token val2_tok = get_next_token();
                    int val2 = 0;
                    
                    if (val2_tok.type == TOKEN_NUMBER) {
                        val2 = str_to_int(val2_tok.text);
                        append_str(asm_out, &asm_idx, (op_tok.type == TOKEN_PLUS) ? "    add rax, " : "    sub rax, ");
                        append_str(asm_out, &asm_idx, val2_tok.text);
                        append_str(asm_out, &asm_idx, "\n");
                        final_val = (op_tok.type == TOKEN_PLUS) ? (final_val + val2) : (final_val - val2);
                    } else if (val2_tok.type == TOKEN_IDENTIFIER) {
                        for (int v = 0; v < var_count; v++) {
                            if (str_eq(var_names[v], val2_tok.text)) { val2 = var_values[v]; break; }
                        }
                        append_str(asm_out, &asm_idx, (op_tok.type == TOKEN_PLUS) ? "    add rax, " : "    sub rax, ");
                        append_int(asm_out, &asm_idx, val2);
                        append_str(asm_out, &asm_idx, "\n");
                        final_val = (op_tok.type == TOKEN_PLUS) ? (final_val + val2) : (final_val - val2);
                    }
                } else {
                    src_ptr = saved_ptr; // Refacem cursorul dacă nu e operație
                }
                
                // PASUL 3: Salvăm valoarea din RAX în memoria variabilei
                append_str(asm_out, &asm_idx, "    mov [var_");
                append_str(asm_out, &asm_idx, name_tok.text);
                append_str(asm_out, &asm_idx, "], rax\n");
                
                str_cpy(var_names[var_count], name_tok.text);
                var_values[var_count] = final_val; 
                var_count++;
            }
        }
		else if (t.type == TOKEN_IDENTIFIER) {
            // Salvăm token-ul curent (numele variabilei) și cursorul
            Token name_tok = t;
            const char* saved_ptr = src_ptr;
            Token assign_tok = get_next_token();
            
            // Verificăm dacă urmează un '=' (adică facem reatribuire: x = ...)
            if (assign_tok.type == TOKEN_ASSIGN) {
                Token val1_tok = get_next_token();
                
                // PASUL 1: Punem prima valoare în registrul RAX
                if (val1_tok.type == TOKEN_NUMBER) {
                    append_str(asm_out, &asm_idx, "    mov rax, ");
                    append_str(asm_out, &asm_idx, val1_tok.text);
                    append_str(asm_out, &asm_idx, "\n");
                } else if (val1_tok.type == TOKEN_IDENTIFIER) {
                    append_str(asm_out, &asm_idx, "    mov rax, [var_");
                    append_str(asm_out, &asm_idx, val1_tok.text);
                    append_str(asm_out, &asm_idx, "]\n");
                }
                
                // PASUL 2: Look-ahead pentru operații matematice (+ sau -)
                const char* saved_ptr2 = src_ptr;
                Token op_tok = get_next_token();
                
                if (op_tok.type == TOKEN_PLUS || op_tok.type == TOKEN_MINUS) {
                    Token val2_tok = get_next_token();
                    if (val2_tok.type == TOKEN_NUMBER) {
                        append_str(asm_out, &asm_idx, (op_tok.type == TOKEN_PLUS) ? "    add rax, " : "    sub rax, ");
                        append_str(asm_out, &asm_idx, val2_tok.text);
                        append_str(asm_out, &asm_idx, "\n");
                    } else if (val2_tok.type == TOKEN_IDENTIFIER) {
                        append_str(asm_out, &asm_idx, (op_tok.type == TOKEN_PLUS) ? "    add rax, [var_" : "    sub rax, [var_");
                        append_str(asm_out, &asm_idx, val2_tok.text);
                        append_str(asm_out, &asm_idx, "]\n");
                    }
                } else {
                    src_ptr = saved_ptr2; // Refacem cursorul dacă nu e operație
                }
                
                // PASUL 3: Salvăm noul rezultat din RAX în memoria variabilei existente
                append_str(asm_out, &asm_idx, "    mov [var_");
                append_str(asm_out, &asm_idx, name_tok.text);
                append_str(asm_out, &asm_idx, "], rax\n");
            } else {
                // Dacă nu e atribuire, dăm cursorul înapoi (fallback)
                src_ptr = saved_ptr;
            }
        }
        else if (t.type == TOKEN_PRINT_KW) {
            get_next_token(); // Trecem peste '('
            Token arg_tok = get_next_token();
            
            if (arg_tok.type == TOKEN_STRING) {
                const char* p_cmd = "    print \"";
                for (int i = 0; p_cmd[i] != '\0'; i++) asm_out[asm_idx++] = p_cmd[i];
                for (int i = 0; arg_tok.text[i] != '\0'; i++) asm_out[asm_idx++] = arg_tok.text[i];
                const char* p_end = "\"\n    newline\n";
                for (int i = 0; p_end[i] != '\0'; i++) asm_out[asm_idx++] = p_end[i];
            } 
            else if (arg_tok.type == TOKEN_IDENTIFIER) {
				
				append_str(asm_out, &asm_idx, "    mov rax, [var_");
                append_str(asm_out, &asm_idx, arg_tok.text);
                append_str(asm_out, &asm_idx, "]\n");
                
                append_str(asm_out, &asm_idx, "    print_rax\n");

            }
        }
		else if (t.type == TOKEN_IF_KW) {
            get_next_token(); // Trece peste '('
            
            Token var1_tok = get_next_token(); // e.g. 'x'
            Token op1_tok = get_next_token();  // e.g. '=' sau '<'
            
            int current_if = if_counter++;
            
            if (op1_tok.type == TOKEN_ASSIGN) {
                // --- ATRIBUIRE ÎN CONDIȚIE (ex: if (x = 20)) ---
                Token val1_tok = get_next_token(); // e.g. '20'
                get_next_token(); // Trece peste ')'
                
                // 1. Facem atribuirea în memorie (efect lateral)
                append_str(asm_out, &asm_idx, "    mov rax, ");
                append_str(asm_out, &asm_idx, val1_tok.text);
                append_str(asm_out, &asm_idx, "\n    mov [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "], rax\n");
                
                // 2. Încărcăm valoarea și o comparăm cu 0
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, 0\n");
                
                // Dacă este 0 (fals), sărim direct la else_body
                append_str(asm_out, &asm_idx, "    je else_body_");
                append_int(asm_out, &asm_idx, current_if);
                append_str(asm_out, &asm_idx, "\n");
                
            } else {
                // --- COMPARAȚIE CLASICĂ (inclusiv && și ||) ---
                Token val1_tok = get_next_token(); // e.g. '10'
                
                // Generăm cod pentru Condiția 1
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, ");
                append_str(asm_out, &asm_idx, val1_tok.text);
                append_str(asm_out, &asm_idx, "\n");
                
                // Verificăm operatorul logic următor: '&&', '||' sau ')'
                Token next_after_cond1 = get_next_token();
                
                if (next_after_cond1.type == TOKEN_AND) {
                    // --- LOGICA AND (&&) ---
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, get_inverse_jump(op1_tok.type));
                    append_str(asm_out, &asm_idx, " else_body_");
                    append_int(asm_out, &asm_idx, current_if);
                    append_str(asm_out, &asm_idx, "\n");
                    
                    Token var2_tok = get_next_token();
                    Token op2_tok = get_next_token();
                    Token val2_tok = get_next_token();
                    get_next_token(); // ')'
                    
                    append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                    append_str(asm_out, &asm_idx, var2_tok.text);
                    append_str(asm_out, &asm_idx, "]\n    cmp rdi, ");
                    append_str(asm_out, &asm_idx, val2_tok.text);
                    append_str(asm_out, &asm_idx, "\n");
                    
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, get_inverse_jump(op2_tok.type));
                    append_str(asm_out, &asm_idx, " else_body_");
                    append_int(asm_out, &asm_idx, current_if);
                    append_str(asm_out, &asm_idx, "\n");
                    
                } else if (next_after_cond1.type == TOKEN_OR) {
                    // --- LOGICA OR (||) ---
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, get_direct_jump(op1_tok.type));
                    append_str(asm_out, &asm_idx, " if_body_");
                    append_int(asm_out, &asm_idx, current_if);
                    append_str(asm_out, &asm_idx, "\n");
                    
                    Token var2_tok = get_next_token();
                    Token op2_tok = get_next_token();
                    Token val2_tok = get_next_token();
                    get_next_token(); // ')'
                    
                    append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                    append_str(asm_out, &asm_idx, var2_tok.text);
                    append_str(asm_out, &asm_idx, "]\n    cmp rdi, ");
                    append_str(asm_out, &asm_idx, val2_tok.text);
                    append_str(asm_out, &asm_idx, "\n");
                    
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, get_direct_jump(op2_tok.type));
                    append_str(asm_out, &asm_idx, " if_body_");
                    append_int(asm_out, &asm_idx, current_if);
                    append_str(asm_out, &asm_idx, "\n");
                    
                    append_str(asm_out, &asm_idx, "    jmp else_body_");
                    append_int(asm_out, &asm_idx, current_if);
                    append_str(asm_out, &asm_idx, "\n");
                    
                } else {
                    // --- O SINGURĂ CONDIȚIE ---
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, get_inverse_jump(op1_tok.type));
                    append_str(asm_out, &asm_idx, " else_body_");
                    append_int(asm_out, &asm_idx, current_if);
                    append_str(asm_out, &asm_idx, "\n");
                }
            }
            
            // Salt direct la corpul de adevăr
            append_str(asm_out, &asm_idx, "    jmp if_body_");
            append_int(asm_out, &asm_idx, current_if);
            append_str(asm_out, &asm_idx, "\n");
            
            // Eticheta corpului TRUE
            append_str(asm_out, &asm_idx, "if_body_");
            append_int(asm_out, &asm_idx, current_if);
            append_str(asm_out, &asm_idx, ":\n");
            
            // Verificăm dacă există acoladă sau o singură instrucțiune
            Token next_tok = get_next_token();
            if (next_tok.type == TOKEN_LBRACE) {
                // Cazul 1: Bloc cu acolade (folosește stiva)
                block_stack[block_depth++] = current_if;
            } else {
                // Cazul 2: Fără acolade -> O singură instrucțiune (ex: print)
                if (next_tok.type == TOKEN_PRINT_KW) {
                    get_next_token(); // Trecem peste '('
                    Token arg_tok = get_next_token();
                    get_next_token(); // Trecem peste ')'
                    get_next_token(); // Trecem peste ';'
                    
                    if (arg_tok.type == TOKEN_STRING) {
                        append_str(asm_out, &asm_idx, "    print \"");
                        append_str(asm_out, &asm_idx, arg_tok.text);
                        append_str(asm_out, &asm_idx, "\"\n    newline\n");
                    } else if (arg_tok.type == TOKEN_IDENTIFIER) {
                        append_str(asm_out, &asm_idx, "    mov rax, [var_");
                        append_str(asm_out, &asm_idx, arg_tok.text);
                        append_str(asm_out, &asm_idx, "]\n    print_rax\n    newline\n");
                    }
                }
                
                // Închidem imediat etichetele pentru if-ul fără acolade
                append_str(asm_out, &asm_idx, "else_body_");
                append_int(asm_out, &asm_idx, current_if);
                append_str(asm_out, &asm_idx, ":\n");
                
                append_str(asm_out, &asm_idx, "if_end_");
                append_int(asm_out, &asm_idx, current_if);
                append_str(asm_out, &asm_idx, ":\n");
            }
        }
		else if (t.type == TOKEN_WHILE_KW) {
            get_next_token(); // Trece peste '('
            
            Token var1_tok = get_next_token(); // e.g. 'x'
            Token op1_tok = get_next_token();  // e.g. '=' sau '<'
            
            int current_while = while_counter++;
            block_stack[block_depth++] = current_while + 2000; // Marcat ca WHILE
            
            // 1. Eticheta de început a buclei
            append_str(asm_out, &asm_idx, "while_start_");
            append_int(asm_out, &asm_idx, current_while);
            append_str(asm_out, &asm_idx, ":\n");
            
            if (op1_tok.type == TOKEN_ASSIGN) {
                // --- ATRIBUIRE ÎN CONDIȚIE WHILE (ex: while (x = 1)) ---
                Token val1_tok = get_next_token(); // e.g. '1'
                get_next_token(); // Trece peste ')'
                
                // 1. Efectul lateral: facem atribuirea în memorie
                append_str(asm_out, &asm_idx, "    mov rax, ");
                append_str(asm_out, &asm_idx, val1_tok.text);
                append_str(asm_out, &asm_idx, "\n    mov [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "], rax\n");
                
                // 2. Evaluarea: încărcăm valoarea și o comparăm cu 0
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, 0\n");
                
                // Dacă este 0 (fals), ieșim din buclă
                append_str(asm_out, &asm_idx, "    je while_end_");
                append_int(asm_out, &asm_idx, current_while);
                append_str(asm_out, &asm_idx, "\n");
                
            } else {
                // --- COMPARAȚIE CLASICĂ WHILE (inclusiv && și ||) ---
                Token val1_tok = get_next_token(); // e.g. '5'
                
                // Generăm cod pentru Condiția 1
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, ");
                append_str(asm_out, &asm_idx, val1_tok.text);
                append_str(asm_out, &asm_idx, "\n");
                
                // Verificăm operatorul logic următor: '&&', '||' sau ')'
                Token next_after_cond1 = get_next_token();
                
                if (next_after_cond1.type == TOKEN_AND) {
                    // --- WHILE CU AND (&&) ---
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, get_inverse_jump(op1_tok.type));
                    append_str(asm_out, &asm_idx, " while_end_");
                    append_int(asm_out, &asm_idx, current_while);
                    append_str(asm_out, &asm_idx, "\n");
                    
                    Token var2_tok = get_next_token();
                    Token op2_tok = get_next_token();
                    Token val2_tok = get_next_token();
                    get_next_token(); // ')'
                    
                    append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                    append_str(asm_out, &asm_idx, var2_tok.text);
                    append_str(asm_out, &asm_idx, "]\n    cmp rdi, ");
                    append_str(asm_out, &asm_idx, val2_tok.text);
                    append_str(asm_out, &asm_idx, "\n");
                    
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, get_inverse_jump(op2_tok.type));
                    append_str(asm_out, &asm_idx, " while_end_");
                    append_int(asm_out, &asm_idx, current_while);
                    append_str(asm_out, &asm_idx, "\n");
                    
                } else if (next_after_cond1.type == TOKEN_OR) {
                    // --- WHILE CU OR (||) ---
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, get_direct_jump(op1_tok.type));
                    append_str(asm_out, &asm_idx, " while_body_");
                    append_int(asm_out, &asm_idx, current_while);
                    append_str(asm_out, &asm_idx, "\n");
                    
                    Token var2_tok = get_next_token();
                    Token op2_tok = get_next_token();
                    Token val2_tok = get_next_token();
                    get_next_token(); // ')'
                    
                    append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                    append_str(asm_out, &asm_idx, var2_tok.text);
                    append_str(asm_out, &asm_idx, "]\n    cmp rdi, ");
                    append_str(asm_out, &asm_idx, val2_tok.text);
                    append_str(asm_out, &asm_idx, "\n");
                    
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, get_direct_jump(op2_tok.type));
                    append_str(asm_out, &asm_idx, " while_body_");
                    append_int(asm_out, &asm_idx, current_while);
                    append_str(asm_out, &asm_idx, "\n");
                    
                    append_str(asm_out, &asm_idx, "    jmp while_end_");
                    append_int(asm_out, &asm_idx, current_while);
                    append_str(asm_out, &asm_idx, "\n");
                    
                    append_str(asm_out, &asm_idx, "while_body_");
                    append_int(asm_out, &asm_idx, current_while);
                    append_str(asm_out, &asm_idx, ":\n");
                    
                } else {
                    // --- O SINGURĂ CONDIȚIE ---
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, get_inverse_jump(op1_tok.type));
                    append_str(asm_out, &asm_idx, " while_end_");
                    append_int(asm_out, &asm_idx, current_while);
                    append_str(asm_out, &asm_idx, "\n");
                }
            }
            
            // Verificăm dacă există acoladă sau o singură instrucțiune
            Token next_tok = get_next_token();
            if (next_tok.type == TOKEN_LBRACE) {
                // Cazul 1: Bloc cu acolade (folosește stiva și este închis de TOKEN_RBRACE)
            } else {
                // Cazul 2: Fără acolade -> O singură instrucțiune
                if (next_tok.type == TOKEN_PRINT_KW) {
                    get_next_token(); // '('
                    Token arg_tok = get_next_token();
                    get_next_token(); // ')'
                    get_next_token(); // ';'
                    
                    if (arg_tok.type == TOKEN_STRING) {
                        append_str(asm_out, &asm_idx, "    print \"");
                        append_str(asm_out, &asm_idx, arg_tok.text);
                        append_str(asm_out, &asm_idx, "\"\n    newline\n");
                    } else if (arg_tok.type == TOKEN_IDENTIFIER) {
                        append_str(asm_out, &asm_idx, "    mov rax, [var_");
                        append_str(asm_out, &asm_idx, arg_tok.text);
                        append_str(asm_out, &asm_idx, "]\n    print_rax\n    newline\n");
                    }
                }
                
                // Salt înapoi la începutul buclei pentru repetare
                append_str(asm_out, &asm_idx, "    jmp while_start_");
                append_int(asm_out, &asm_idx, current_while);
                append_str(asm_out, &asm_idx, "\n");
                
                // Eticheta de ieșire din buclă
                append_str(asm_out, &asm_idx, "while_end_");
                append_int(asm_out, &asm_idx, current_while);
                append_str(asm_out, &asm_idx, ":\n");
                
                // Scoatem blocul de pe stivă
                block_depth--;
            }
        }
        else if (t.type == TOKEN_RBRACE) {
            if (block_depth > 0) {
                block_depth--;
                int closed_block = block_stack[block_depth];
                
                if (closed_block >= 2000) {
                    // Se închide un bloc WHILE
                    int original_while = closed_block - 2000;
                    
                    append_str(asm_out, &asm_idx, "    jmp while_start_");
                    append_int(asm_out, &asm_idx, original_while);
                    append_str(asm_out, &asm_idx, "\n");
                    
                    append_str(asm_out, &asm_idx, "while_end_");
                    append_int(asm_out, &asm_idx, original_while);
                    append_str(asm_out, &asm_idx, ":\n");
                } 
                else if (closed_block < 1000) {
                    // Se închide ramura TRUE a unui IF
                    // Verificăm dacă urmează direct un 'else' fără să stricăm src_ptr
                    const char* check_ptr = src_ptr;
                    Token next_tok = get_next_token();
                    
                    if (next_tok.type == TOKEN_ELSE_KW) {
                        // Consumăm acolada deschisă '{' de la else
                        get_next_token(); 
                        
                        append_str(asm_out, &asm_idx, "    jmp if_end_");
                        append_int(asm_out, &asm_idx, closed_block);
                        append_str(asm_out, &asm_idx, "\n");
                        
                        append_str(asm_out, &asm_idx, "else_body_");
                        append_int(asm_out, &asm_idx, closed_block);
                        append_str(asm_out, &asm_idx, ":\n");
                        
                        block_stack[block_depth++] = closed_block + 1000;
                    } else {
                        // Nu există else, restaurăm cursorul exact unde era
                        src_ptr = check_ptr;
                        
                        append_str(asm_out, &asm_idx, "else_body_");
                        append_int(asm_out, &asm_idx, closed_block);
                        append_str(asm_out, &asm_idx, ":\n");
                        
                        append_str(asm_out, &asm_idx, "if_end_");
                        append_int(asm_out, &asm_idx, closed_block);
                        append_str(asm_out, &asm_idx, ":\n");
                    }
                } else {
                    // Se închide ramura ELSE
                    int original_if = closed_block - 1000;
                    append_str(asm_out, &asm_idx, "if_end_");
                    append_int(asm_out, &asm_idx, original_if);
                    append_str(asm_out, &asm_idx, ":\n");
                }
            }
        }
        t = get_next_token();
    }

    // Adăugăm instrucțiunea de ieșire
    append_str(asm_out, &asm_idx, "    exit\n");

    // ALOCARE MEMORIE: Definim etichetele pentru fiecare variabilă
    for (int v = 0; v < var_count; v++) {
        append_str(asm_out, &asm_idx, "var_");
        append_str(asm_out, &asm_idx, var_names[v]);
        append_str(asm_out, &asm_idx, ": data 0\n");
    }

    asm_out[asm_idx] = '\0';

    nano_write_file(argv[2], (const uint8_t*)asm_out, asm_idx);
    nano_print("[nanoc] Generare cod ASM (.s) finalizata cu succes!\n");
    return 0;
}