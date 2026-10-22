#include "nano_libc.h"

typedef enum {
    TOKEN_EOF = 0, TOKEN_INT_KW, TOKEN_FLOAT_KW, TOKEN_PRINT_KW, TOKEN_IF_KW, TOKEN_ELSE_KW, TOKEN_WHILE_KW, TOKEN_IDENTIFIER, 
    TOKEN_NUMBER, TOKEN_FLOAT_NUMBER, TOKEN_STRING, TOKEN_ASSIGN, 
    TOKEN_EQ, TOKEN_NE, TOKEN_GT, TOKEN_LT, TOKEN_GE, TOKEN_LE,
    TOKEN_SEMICOLON, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_AND, TOKEN_OR, TOKEN_NOT,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_MOD, TOKEN_UNKNOWN
} TokenType;

typedef struct {
    TokenType type;
    char text[256];
} Token;

typedef struct {
    char name[32];
    int is_float;
} Variable;

static const char* src_ptr = 0;
char asm_out[4096];
int asm_idx = 0;

// Tabel pentru colectarea automată a variabilelor
Variable var_table[128];
int var_count = 0;

// --- PROTOTIPURI DE FUNCȚII (pentru evitarea erorilor de declarație implicită) ---
void append_str(char* dest, int* idx, const char* src);
void append_int(char* dest, int* idx, int val);
Token get_next_token();

void parse_float_expr();
void parse_float_term();
void parse_float_factor();

void parse_expr();
void parse_term();
void parse_factor();

void register_variable(const char* name, int is_float) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) return;
    }
    strcpy(var_table[var_count].name, name);
    var_table[var_count].is_float = is_float;
    var_count++;
}

int is_var_float(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) return var_table[i].is_float;
    }
    return 0;
}

// Convertește un șir text de tip float ("3.14") în reprezentarea sa binară IEEE-754 pe 32 de biți
uint32_t float_to_u32(const char* str) {
    float f = 0.0f;
    int i = 0;
    float sign = 1.0f;
    if (str[i] == '-') { sign = -1.0f; i++; }
    while (str[i] >= '0' && str[i] <= '9') {
        f = f * 10.0f + (str[i] - '0');
        i++;
    }
    if (str[i] == '.') {
        i++;
        float div = 10.0f;
        while (str[i] >= '0' && str[i] <= '9') {
            f += (str[i] - '0') / div;
            div *= 10.0f;
            i++;
        }
    }
    f *= sign;
    union { float f_val; uint32_t u_val; } cvt;
    cvt.f_val = f;
    return cvt.u_val;
}

void parse_float_factor() {
    Token t = get_next_token();
    if (t.type == TOKEN_NUMBER || t.type == TOKEN_FLOAT_NUMBER) {
        uint32_t u = float_to_u32(t.text);
        register_variable("tmp_float", 1);
        append_str(asm_out, &asm_idx, "    mov rax, ");
        append_int(asm_out, &asm_idx, (int)u);
        append_str(asm_out, &asm_idx, "\n    mov [var_tmp_float], rax\n");
        append_str(asm_out, &asm_idx, "    movss xmm0, [var_tmp_float]\n");
    } else if (t.type == TOKEN_IDENTIFIER) {
        append_str(asm_out, &asm_idx, "    movss xmm0, [var_");
        append_str(asm_out, &asm_idx, t.text);
        append_str(asm_out, &asm_idx, "]\n");
    } else if (t.type == TOKEN_LPAREN) {
        parse_float_expr();
        get_next_token(); // Consumă ')'
    }
}

void parse_float_term() {
    parse_float_factor(); // Rezultatul ajunge în xmm0
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        if (op.type == TOKEN_STAR || op.type == TOKEN_SLASH) {
            register_variable("tmp_f1", 1);
            append_str(asm_out, &asm_idx, "    movss [var_tmp_f1], xmm0\n");
            
            parse_float_factor(); // Al doilea operand ajunge în xmm0
            
            append_str(asm_out, &asm_idx, "    movss xmm1, xmm0\n");
            append_str(asm_out, &asm_idx, "    movss xmm0, [var_tmp_f1]\n");
            
            if (op.type == TOKEN_STAR) {
                append_str(asm_out, &asm_idx, "    mulss xmm0, xmm1\n");
            } else {
                append_str(asm_out, &asm_idx, "    divss xmm0, xmm1\n");
            }
        } else {
            src_ptr = saved_ptr;
            break;
        }
    }
}

void parse_float_expr() {
    parse_float_term(); // Rezultatul ajunge în xmm0
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        if (op.type == TOKEN_PLUS || op.type == TOKEN_MINUS) {
            register_variable("tmp_f2", 1);
            append_str(asm_out, &asm_idx, "    movss [var_tmp_f2], xmm0\n");
            
            parse_float_term(); // Al doilea termen ajunge în xmm0
            
            append_str(asm_out, &asm_idx, "    movss xmm1, xmm0\n");
            append_str(asm_out, &asm_idx, "    movss xmm0, [var_tmp_f2]\n");
            
            if (op.type == TOKEN_PLUS) {
                append_str(asm_out, &asm_idx, "    addss xmm0, xmm1\n");
            } else {
                append_str(asm_out, &asm_idx, "    subss xmm0, xmm1\n");
            }
        } else {
            src_ptr = saved_ptr;
            break;
        }
    }
}

void parse_factor() {
    Token t = get_next_token();
    if (t.type == TOKEN_NUMBER) {
        append_str(asm_out, &asm_idx, "    mov rax, ");
        append_str(asm_out, &asm_idx, t.text);
        append_str(asm_out, &asm_idx, "\n");
    } else if (t.type == TOKEN_IDENTIFIER) {
        append_str(asm_out, &asm_idx, "    mov rax, [var_");
        append_str(asm_out, &asm_idx, t.text);
        append_str(asm_out, &asm_idx, "]\n");
    } else if (t.type == TOKEN_LPAREN) {
        parse_expr(); // Evaluează tot ce se află în paranteze
        get_next_token(); // Consumă paranteza de închidere ')'
    }
}

void parse_term() {
    parse_factor(); // Primul factor ajunge în rax
    
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        
        if (op.type == TOKEN_STAR || op.type == TOKEN_SLASH || op.type == TOKEN_MOD) {
            append_str(asm_out, &asm_idx, "    push rax\n"); // Salvăm operandul stâng pe stivă
            
            parse_factor(); // Evaluează operandul drept -> rezultatul ajunge în rax
            
            append_str(asm_out, &asm_idx, "    mov rbx, rax\n"); // Mutăm operandul drept în rbx
            append_str(asm_out, &asm_idx, "    pop rax\n");     // Restaurăm operandul stâng în rax
            
            if (op.type == TOKEN_STAR) {
                append_str(asm_out, &asm_idx, "    imul rbx\n");
            } else if (op.type == TOKEN_SLASH) {
                append_str(asm_out, &asm_idx, "    cqo\n    idiv rbx\n");
            } else if (op.type == TOKEN_MOD) {
                append_str(asm_out, &asm_idx, "    cqo\n    idiv rbx\n    mov rax, rdx\n");
            }
        } else {
            src_ptr = saved_ptr; // Refacem cursorul dacă nu avem operator de prioritate înaltă
            break;
        }
    }
}

void parse_expr() {
    parse_term(); // Primul termen ajunge în rax
    
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        
        if (op.type == TOKEN_PLUS || op.type == TOKEN_MINUS) {
            append_str(asm_out, &asm_idx, "    push rax\n"); // Salvăm termenul stâng pe stivă
            
            parse_term(); // Evaluează termenul drept -> rezultatul ajunge în rax
            
            append_str(asm_out, &asm_idx, "    mov rbx, rax\n"); // Mutăm termenul drept în rbx
            append_str(asm_out, &asm_idx, "    pop rax\n");     // Restaurăm termenul stâng în rax
            
            if (op.type == TOKEN_PLUS) {
                append_str(asm_out, &asm_idx, "    add rax, rbx\n");
            } else if (op.type == TOKEN_MINUS) {
                append_str(asm_out, &asm_idx, "    sub rax, rbx\n");
            }
        } else {
            src_ptr = saved_ptr; // Refacem cursorul dacă nu avem adunare/scădere
            break;
        }
    }
}

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
    if (op == TOKEN_GT) return "jle";
    if (op == TOKEN_LT) return "jg"; 
    if (op == TOKEN_GE) return "jl";  
    if (op == TOKEN_LE) return "jge"; 
    return "jne"; 
}

const char* get_direct_jump(TokenType op) {
    if (op == TOKEN_EQ) return "je";
    if (op == TOKEN_NE) return "jne";
    if (op == TOKEN_GT) return "jg"; 
    if (op == TOKEN_LT) return "jl"; 
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
        case TOKEN_AND: return "AND (&&)";
        case TOKEN_OR: return "OR (||)";
        case TOKEN_SEMICOLON: return "SEMICOLON (;)";
        case TOKEN_LPAREN: return "LPAREN (()";
        case TOKEN_RPAREN: return "RPAREN ())";
        case TOKEN_LBRACE: return "LBRACE ({)";
        case TOKEN_RBRACE: return "RBRACE (})";
        case TOKEN_PLUS: return "PLUS (+)";
        case TOKEN_MINUS: return "MINUS (-)";
        case TOKEN_NOT: return "NOT (!)";
        case TOKEN_STAR: return "STAR (*)";
        case TOKEN_SLASH: return "SLASH (/)";
        case TOKEN_MOD: return "MOD (%)";
        default: return "UNKNOWN";
    }
}

Token get_next_token() {
    Token token;
    token.type = TOKEN_EOF;
    token.text[0] = '\0';

    if (!src_ptr || *src_ptr == '\0') return token;

    while (1) {
        while (*src_ptr == ' ' || *src_ptr == '\t' || *src_ptr == '\r' || *src_ptr == '\n') {
            src_ptr++;
        }
        if (*src_ptr == '\0') return token;

        if (*src_ptr == '/' && *(src_ptr + 1) == '/') {
            while (*src_ptr != '\n' && *src_ptr != '\0') {
                src_ptr++;
            }
            continue;
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
        else if (str_eq(token.text, "float")) token.type = TOKEN_FLOAT_KW;
        else if (str_eq(token.text, "print")) token.type = TOKEN_PRINT_KW;
        else if (str_eq(token.text, "if")) token.type = TOKEN_IF_KW;
        else if (str_eq(token.text, "else")) token.type = TOKEN_ELSE_KW;
        else if (str_eq(token.text, "while")) token.type = TOKEN_WHILE_KW;
        else token.type = TOKEN_IDENTIFIER;
        return token;
    }

    if (is_digit(c)) {
        int i = 0;
        int has_dot = 0;
        while ((is_digit(*src_ptr) || *src_ptr == '.') && i < 63) {
            if (*src_ptr == '.') has_dot = 1;
            token.text[i++] = *src_ptr++;
        }
        token.text[i] = '\0';
        token.type = has_dot ? TOKEN_FLOAT_NUMBER : TOKEN_NUMBER;
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
        case '!':
            if (*src_ptr == '=') { 
                src_ptr++; 
                token.type = TOKEN_NE; 
                str_cpy(token.text, "!="); 
            } else { 
                token.type = TOKEN_NOT;
                str_cpy(token.text, "!");
            }
            break;
        case '*': token.type = TOKEN_STAR; break;
        case '/': token.type = TOKEN_SLASH; break;
        case '%': token.type = TOKEN_MOD; break;
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

    const char* header = "main:\n";
    for (int i = 0; header[i] != '\0'; i++) asm_out[asm_idx++] = header[i];

    int if_counter = 0;
    int block_stack[32];
    int block_depth = 0;
    int while_counter = 0;

    Token t = get_next_token();
    
    while (t.type != TOKEN_EOF) {
        if (t.type == TOKEN_INT_KW) {
            Token var_tok = get_next_token();
            Token next_tok = get_next_token();
            
            // Dacă este declarație de funcție (ex: int main())
            if (next_tok.type == TOKEN_LPAREN) {
                get_next_token(); // Consumă ')'
                Token brace_tok = get_next_token();
                if (brace_tok.type == TOKEN_LBRACE) {
                    // Am intrat în corpul funcției main
                }
            } 
            // Dacă este declarație de variabilă (ex: int x = 5;)
            else if (next_tok.type == TOKEN_ASSIGN) {
                register_variable(var_tok.text, 0);
                parse_expr();
                get_next_token(); // Consumă ';'
                append_str(asm_out, &asm_idx, "    mov [var_");
                append_str(asm_out, &asm_idx, var_tok.text);
                append_str(asm_out, &asm_idx, "], rax\n");
            }
            else if (next_tok.type == TOKEN_SEMICOLON) {
                register_variable(var_tok.text, 0);
            }
        }
        else if (t.type == TOKEN_FLOAT_KW) {
            Token var_tok = get_next_token();
            register_variable(var_tok.text, 1);
            Token assign_tok = get_next_token();
            if (assign_tok.type == TOKEN_ASSIGN) {
                parse_float_expr();
                get_next_token(); // Consumă ';'
                append_str(asm_out, &asm_idx, "    movss [var_");
                append_str(asm_out, &asm_idx, var_tok.text);
                append_str(asm_out, &asm_idx, "], xmm0\n");
            }
        }
        else if (t.type == TOKEN_IDENTIFIER) {
            Token var_tok = t;
            Token assign_tok = get_next_token();
            if (assign_tok.type == TOKEN_ASSIGN) {
                if (is_var_float(var_tok.text)) {
                    parse_float_expr();
                    get_next_token(); // ';'
                    append_str(asm_out, &asm_idx, "    movss [var_");
                    append_str(asm_out, &asm_idx, var_tok.text);
                    append_str(asm_out, &asm_idx, "], xmm0\n");
                } else {
                    parse_expr();
                    get_next_token(); // ';'
                    append_str(asm_out, &asm_idx, "    mov [var_");
                    append_str(asm_out, &asm_idx, var_tok.text);
                    append_str(asm_out, &asm_idx, "], rax\n");
                }
            }
        }
        else if (t.type == TOKEN_PRINT_KW) {
            get_next_token(); // '('
            Token arg_tok = get_next_token();
            get_next_token(); // ')'
            get_next_token(); // ';'
            
            if (arg_tok.type == TOKEN_STRING) {
                append_str(asm_out, &asm_idx, "    print \"");
                append_str(asm_out, &asm_idx, arg_tok.text);
                append_str(asm_out, &asm_idx, "\"\n    newline\n");
            } else if (arg_tok.type == TOKEN_IDENTIFIER) {
                if (is_var_float(arg_tok.text)) {
                    append_str(asm_out, &asm_idx, "    movss xmm0, [var_");
                    append_str(asm_out, &asm_idx, arg_tok.text);
                    append_str(asm_out, &asm_idx, "]\n    print_xmm\n    newline\n");
                } else {
                    append_str(asm_out, &asm_idx, "    mov rax, [var_");
                    append_str(asm_out, &asm_idx, arg_tok.text);
                    append_str(asm_out, &asm_idx, "]\n    print_rax\n    newline\n");
                }
            }
        }
        else if (t.type == TOKEN_IF_KW) {
            get_next_token(); // Trece peste '('
            
            int current_if = if_counter++;
            
            Token next_tok_check = get_next_token();
            int is_negated = 0;
            Token var1_tok;
            
            if (next_tok_check.type == TOKEN_NOT) {
                is_negated = 1;
                var1_tok = get_next_token();
            } else {
                var1_tok = next_tok_check;
            }
            
            Token op1_tok = get_next_token();
            
            if (op1_tok.type == TOKEN_ASSIGN) {
                Token val1_tok = get_next_token();
                get_next_token(); // ')'
                
                append_str(asm_out, &asm_idx, "    mov rax, ");
                append_str(asm_out, &asm_idx, val1_tok.text);
                append_str(asm_out, &asm_idx, "\n    mov [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "], rax\n");
                
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, 0\n");
                
                append_str(asm_out, &asm_idx, "    ");
                append_str(asm_out, &asm_idx, is_negated ? "jne" : "je");
                append_str(asm_out, &asm_idx, " else_body_");
                append_int(asm_out, &asm_idx, current_if);
                append_str(asm_out, &asm_idx, "\n");
                
            } else if (op1_tok.type == TOKEN_RPAREN) {
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, 0\n");
                
                append_str(asm_out, &asm_idx, "    ");
                append_str(asm_out, &asm_idx, is_negated ? "jne" : "je");
                append_str(asm_out, &asm_idx, " else_body_");
                append_int(asm_out, &asm_idx, current_if);
                append_str(asm_out, &asm_idx, "\n");
                
            } else {
                Token val1_tok = get_next_token(); 
                
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, ");
                append_str(asm_out, &asm_idx, val1_tok.text);
                append_str(asm_out, &asm_idx, "\n");
                
                Token next_after_cond1 = get_next_token();
                
                if (next_after_cond1.type == TOKEN_AND) {
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, is_negated ? get_direct_jump(op1_tok.type) : get_inverse_jump(op1_tok.type));
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
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, is_negated ? get_inverse_jump(op1_tok.type) : get_direct_jump(op1_tok.type));
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
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, is_negated ? get_direct_jump(op1_tok.type) : get_inverse_jump(op1_tok.type));
                    append_str(asm_out, &asm_idx, " else_body_");
                    append_int(asm_out, &asm_idx, current_if);
                    append_str(asm_out, &asm_idx, "\n");
                }
            }
            
            append_str(asm_out, &asm_idx, "    jmp if_body_");
            append_int(asm_out, &asm_idx, current_if);
            append_str(asm_out, &asm_idx, "\n");
            
            append_str(asm_out, &asm_idx, "if_body_");
            append_int(asm_out, &asm_idx, current_if);
            append_str(asm_out, &asm_idx, ":\n");
            
            Token next_tok = get_next_token();
            if (next_tok.type == TOKEN_LBRACE) {
                block_stack[block_depth++] = current_if;
            } else {
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
            
            int current_while = while_counter++;
            block_stack[block_depth++] = current_while + 2000;
            
            append_str(asm_out, &asm_idx, "while_start_");
            append_int(asm_out, &asm_idx, current_while);
            append_str(asm_out, &asm_idx, ":\n");
            
            Token next_tok_check = get_next_token();
            int is_negated = 0;
            Token var1_tok;
            
            if (next_tok_check.type == TOKEN_NOT) {
                is_negated = 1;
                var1_tok = get_next_token(); 
            } else {
                var1_tok = next_tok_check;
            }
            
            Token op1_tok = get_next_token(); 
            
            if (op1_tok.type == TOKEN_ASSIGN) {
                Token val1_tok = get_next_token(); 
                get_next_token(); // ')'
                
                append_str(asm_out, &asm_idx, "    mov rax, ");
                append_str(asm_out, &asm_idx, val1_tok.text);
                append_str(asm_out, &asm_idx, "\n    mov [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "], rax\n");
                
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, 0\n");
                
                append_str(asm_out, &asm_idx, "    ");
                append_str(asm_out, &asm_idx, is_negated ? "jne" : "je");
                append_str(asm_out, &asm_idx, " while_end_");
                append_int(asm_out, &asm_idx, current_while);
                append_str(asm_out, &asm_idx, "\n");
                
            } else if (op1_tok.type == TOKEN_RPAREN) {
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, 0\n");
                
                append_str(asm_out, &asm_idx, "    ");
                append_str(asm_out, &asm_idx, is_negated ? "jne" : "je");
                append_str(asm_out, &asm_idx, " while_end_");
                append_int(asm_out, &asm_idx, current_while);
                append_str(asm_out, &asm_idx, "\n");
                
            } else {
                Token val1_tok = get_next_token();
                
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, ");
                append_str(asm_out, &asm_idx, val1_tok.text);
                append_str(asm_out, &asm_idx, "\n");
                
                Token next_after_cond1 = get_next_token();
                
                if (next_after_cond1.type == TOKEN_AND) {
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, is_negated ? get_direct_jump(op1_tok.type) : get_inverse_jump(op1_tok.type));
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
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, is_negated ? get_inverse_jump(op1_tok.type) : get_direct_jump(op1_tok.type));
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
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, is_negated ? get_direct_jump(op1_tok.type) : get_inverse_jump(op1_tok.type));
                    append_str(asm_out, &asm_idx, " while_end_");
                    append_int(asm_out, &asm_idx, current_while);
                    append_str(asm_out, &asm_idx, "\n");
                }
            }
            
            Token next_tok = get_next_token();
            if (next_tok.type == TOKEN_LBRACE) {
                // Cazul 1: Bloc cu acolade 
            } else {
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
                
                append_str(asm_out, &asm_idx, "    jmp while_start_");
                append_int(asm_out, &asm_idx, current_while);
                append_str(asm_out, &asm_idx, "\n");
                
                append_str(asm_out, &asm_idx, "while_end_");
                append_int(asm_out, &asm_idx, current_while);
                append_str(asm_out, &asm_idx, ":\n");
                
                block_depth--;
            }
        }
        else if (t.type == TOKEN_RBRACE) {
            if (block_depth > 0) {
                block_depth--;
                int closed_block = block_stack[block_depth];
                
                if (closed_block >= 2000) {
                    int original_while = closed_block - 2000;
                    
                    append_str(asm_out, &asm_idx, "    jmp while_start_");
                    append_int(asm_out, &asm_idx, original_while);
                    append_str(asm_out, &asm_idx, "\n");
                    
                    append_str(asm_out, &asm_idx, "while_end_");
                    append_int(asm_out, &asm_idx, original_while);
                    append_str(asm_out, &asm_idx, ":\n");
                } 
                else if (closed_block < 1000) {
                    const char* check_ptr = src_ptr;
                    Token next_tok = get_next_token();
                    
                    if (next_tok.type == TOKEN_ELSE_KW) {
                        get_next_token(); // '{'
                        
                        append_str(asm_out, &asm_idx, "    jmp if_end_");
                        append_int(asm_out, &asm_idx, closed_block);
                        append_str(asm_out, &asm_idx, "\n");
                        
                        append_str(asm_out, &asm_idx, "else_body_");
                        append_int(asm_out, &asm_idx, closed_block);
                        append_str(asm_out, &asm_idx, ":\n");
                        
                        block_stack[block_depth++] = closed_block + 1000;
                    } else {
                        src_ptr = check_ptr;
                        
                        append_str(asm_out, &asm_idx, "else_body_");
                        append_int(asm_out, &asm_idx, closed_block);
                        append_str(asm_out, &asm_idx, ":\n");
                        
                        append_str(asm_out, &asm_idx, "if_end_");
                        append_int(asm_out, &asm_idx, closed_block);
                        append_str(asm_out, &asm_idx, ":\n");
                    }
                } else {
                    int original_if = closed_block - 1000;
                    append_str(asm_out, &asm_idx, "if_end_");
                    append_int(asm_out, &asm_idx, original_if);
                    append_str(asm_out, &asm_idx, ":\n");
                }
            }
        }
        t = get_next_token();
    }

    append_str(asm_out, &asm_idx, "    exit\n");

    for (int i = 0; i < var_count; i++) {
        append_str(asm_out, &asm_idx, "var_");
        append_str(asm_out, &asm_idx, var_table[i].name);
        append_str(asm_out, &asm_idx, ": data 0\n");
    }

    asm_out[asm_idx] = '\0';

    nano_write_file(argv[2], (const uint8_t*)asm_out, asm_idx);
    nano_print("[nanoc] Generare cod ASM (.s) finalizata cu succes!\n");
    return 0;
}
/*
#include "nano_libc.h"

// Am adăugat TOKEN_PLUS, TOKEN_MINUS și TOKEN_UNKNOWN pentru stabilitate
typedef enum {
    TOKEN_EOF = 0, TOKEN_INT_KW, TOKEN_FLOAT_KW, TOKEN_PRINT_KW, TOKEN_IF_KW, TOKEN_ELSE_KW, TOKEN_WHILE_KW, TOKEN_IDENTIFIER, 
    TOKEN_NUMBER, TOKEN_FLOAT_NUMBER, TOKEN_STRING, TOKEN_ASSIGN, 
    TOKEN_EQ, TOKEN_NE, TOKEN_GT, TOKEN_LT, TOKEN_GE, TOKEN_LE,
    TOKEN_SEMICOLON, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_AND, TOKEN_OR, TOKEN_NOT,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_MOD, TOKEN_UNKNOWN // <-- Adăugate aici!
} TokenType;

typedef struct {
    TokenType type;
    char text[256];
} Token;

typedef struct {
    char name[32];
    int is_float;
} Variable;

static const char* src_ptr = 0;
char asm_out[4096];
int asm_idx = 0;

// Tabel pentru colectarea automată a variabilelor
Variable var_table[128];
int var_count = 0;

void register_variable(const char* name, int is_float) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) return;
    }
    strcpy(var_table[var_count].name, name);
    var_table[var_count].is_float = is_float;
    var_count++;
}

int is_var_float(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) return var_table[i].is_float;
    }
    return 0;
}

// Convertește un șir text de tip float ("3.14") în reprezentarea sa binară IEEE-754 pe 32 de biți
uint32_t float_to_u32(const char* str) {
    float f = 0.0f;
    int i = 0;
    float sign = 1.0f;
    if (str[i] == '-') { sign = -1.0f; i++; }
    while (str[i] >= '0' && str[i] <= '9') {
        f = f * 10.0f + (str[i] - '0');
        i++;
    }
    if (str[i] == '.') {
        i++;
        float div = 10.0f;
        while (str[i] >= '0' && str[i] <= '9') {
            f += (str[i] - '0') / div;
            div *= 10.0f;
            i++;
        }
    }
    f *= sign;
    union { float f_val; uint32_t u_val; } cvt;
    cvt.f_val = f;
    return cvt.u_val;
}

void parse_float_expr();
void parse_float_term();
void parse_float_factor();

void parse_float_factor() {
    Token t = get_next_token();
    if (t.type == TOKEN_NUMBER || t.type == TOKEN_FLOAT_NUMBER) {
        uint32_t u = float_to_u32(t.text);
        register_variable("tmp_float", 1);
        append_str(asm_out, &asm_idx, "    mov rax, ");
        append_int(asm_out, &asm_idx, (int)u);
        append_str(asm_out, &asm_idx, "\n    mov [var_tmp_float], rax\n");
        append_str(asm_out, &asm_idx, "    movss xmm0, [var_tmp_float]\n");
    } else if (t.type == TOKEN_IDENTIFIER) {
        append_str(asm_out, &asm_idx, "    movss xmm0, [var_");
        append_str(asm_out, &asm_idx, t.text);
        append_str(asm_out, &asm_idx, "]\n");
    } else if (t.type == TOKEN_LPAREN) {
        parse_float_expr();
        get_next_token(); // Consumă ')'
    }
}

void parse_float_term() {
    parse_float_factor(); // Rezultatul ajunge în xmm0
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        if (op.type == TOKEN_STAR || op.type == TOKEN_SLASH) {
            register_variable("tmp_f1", 1);
            append_str(asm_out, &asm_idx, "    movss [var_tmp_f1], xmm0\n");
            
            parse_float_factor(); // Al doilea operand ajunge în xmm0
            
            append_str(asm_out, &asm_idx, "    movss xmm1, xmm0\n");
            append_str(asm_out, &asm_idx, "    movss xmm0, [var_tmp_f1]\n");
            
            if (op.type == TOKEN_STAR) {
                append_str(asm_out, &asm_idx, "    mulss xmm0, xmm1\n");
            } else {
                append_str(asm_out, &asm_idx, "    divss xmm0, xmm1\n");
            }
        } else {
            src_ptr = saved_ptr;
            break;
        }
    }
}

void parse_float_expr() {
    parse_float_term(); // Rezultatul ajunge în xmm0
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        if (op.type == TOKEN_PLUS || op.type == TOKEN_MINUS) {
            register_variable("tmp_f2", 1);
            append_str(asm_out, &asm_idx, "    movss [var_tmp_f2], xmm0\n");
            
            parse_float_term(); // Al doilea termen ajunge în xmm0
            
            append_str(asm_out, &asm_idx, "    movss xmm1, xmm0\n");
            append_str(asm_out, &asm_idx, "    movss xmm0, [var_tmp_f2]\n");
            
            if (op.type == TOKEN_PLUS) {
                append_str(asm_out, &asm_idx, "    addss xmm0, xmm1\n");
            } else {
                append_str(asm_out, &asm_idx, "    subss xmm0, xmm1\n");
            }
        } else {
            src_ptr = saved_ptr;
            break;
        }
    }
}

void parse_expr();
void parse_term();
void parse_factor();

void parse_factor() {
    Token t = get_next_token();
    if (t.type == TOKEN_NUMBER) {
        append_str(asm_out, &asm_idx, "    mov rax, ");
        append_str(asm_out, &asm_idx, t.text);
        append_str(asm_out, &asm_idx, "\n");
    } else if (t.type == TOKEN_IDENTIFIER) {
        append_str(asm_out, &asm_idx, "    mov rax, [var_");
        append_str(asm_out, &asm_idx, t.text);
        append_str(asm_out, &asm_idx, "]\n");
    } else if (t.type == TOKEN_LPAREN) {
        parse_expr(); // Evaluează tot ce se află în paranteze
        get_next_token(); // Consumă paranteza de închidere ')'
    }
}

void parse_term() {
    parse_factor(); // Primul factor ajunge în rax
    
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        
        if (op.type == TOKEN_STAR || op.type == TOKEN_SLASH || op.type == TOKEN_MOD) {
            append_str(asm_out, &asm_idx, "    push rax\n"); // Salvăm operandul stâng pe stivă
            
            parse_factor(); // Evaluează operandul drept -> rezultatul ajunge în rax
            
            append_str(asm_out, &asm_idx, "    mov rbx, rax\n"); // Mutăm operandul drept în rbx
            append_str(asm_out, &asm_idx, "    pop rax\n");     // Restaurăm operandul stâng în rax
            
            if (op.type == TOKEN_STAR) {
                append_str(asm_out, &asm_idx, "    imul rbx\n");
            } else if (op.type == TOKEN_SLASH) {
                append_str(asm_out, &asm_idx, "    cqo\n    idiv rbx\n");
            } else if (op.type == TOKEN_MOD) {
                append_str(asm_out, &asm_idx, "    cqo\n    idiv rbx\n    mov rax, rdx\n");
            }
        } else {
            src_ptr = saved_ptr; // Refacem cursorul dacă nu avem operator de prioritate înaltă
            break;
        }
    }
}

void parse_expr() {
    parse_term(); // Primul termen ajunge în rax
    
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        
        if (op.type == TOKEN_PLUS || op.type == TOKEN_MINUS) {
            append_str(asm_out, &asm_idx, "    push rax\n"); // Salvăm termenul stâng pe stivă
            
            parse_term(); // Evaluează termenul drept -> rezultatul ajunge în rax
            
            append_str(asm_out, &asm_idx, "    mov rbx, rax\n"); // Mutăm termenul drept în rbx
            append_str(asm_out, &asm_idx, "    pop rax\n");     // Restaurăm termenul stâng în rax
            
            if (op.type == TOKEN_PLUS) {
                append_str(asm_out, &asm_idx, "    add rax, rbx\n");
            } else if (op.type == TOKEN_MINUS) {
                append_str(asm_out, &asm_idx, "    sub rax, rbx\n");
            }
        } else {
            src_ptr = saved_ptr; // Refacem cursorul dacă nu avem adunare/scădere
            break;
        }
    }
}

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
        case TOKEN_NOT: return "NOT (!)";
        case TOKEN_STAR: return "STAR (*)";
        case TOKEN_SLASH: return "SLASH (/)";
        case TOKEN_MOD: return "MOD (%)";
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
        else if (str_eq(token.text, "float")) token.type = TOKEN_FLOAT_KW;
        else if (str_eq(token.text, "print")) token.type = TOKEN_PRINT_KW;
        else if (str_eq(token.text, "if")) token.type = TOKEN_IF_KW;
        else if (str_eq(token.text, "else")) token.type = TOKEN_ELSE_KW;
		else if (str_eq(token.text, "while")) token.type = TOKEN_WHILE_KW;
        else token.type = TOKEN_IDENTIFIER;
        return token;
    }

    if (is_digit(c)) {
        int i = 0;
        int has_dot = 0;
        while ((is_digit(*src_ptr) || *src_ptr == '.') && i < 63) {
            if (*src_ptr == '.') has_dot = 1;
            token.text[i++] = *src_ptr++;
        }
        token.text[i] = '\0';
        token.type = has_dot ? TOKEN_FLOAT_NUMBER : TOKEN_NUMBER;
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
        case '!':
            if (*src_ptr == '=') { 
                src_ptr++; 
                token.type = TOKEN_NE; 
                str_cpy(token.text, "!="); 
            } else { 
                token.type = TOKEN_NOT; // <--- Simplul '!'
                str_cpy(token.text, "!");
            }
            break;
        case '*': token.type = TOKEN_STAR; break;
        case '/': token.type = TOKEN_SLASH; break;
        case '%': token.type = TOKEN_MOD; break;
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

    

    const char* header = "main:\n";
    for (int i = 0; header[i] != '\0'; i++) asm_out[asm_idx++] = header[i];

    //char var_names[32][32];
    int var_values[32];
    //int var_count = 0;

	int if_counter = 0;       // Numărător pentru a crea etichete unice (if_body_0, if_end_0, etc.)
    int block_stack[32];      // Stivă pentru a ști ce if se închide când dăm de '}'
    int block_depth = 0;
	
	int while_counter = 0;

    Token t = get_next_token();
    
    while (t.type != TOKEN_EOF) {
        if (t.type == TOKEN_INT_KW) {
            Token var_tok = get_next_token();
            register_variable(var_tok.text, 0);
            Token assign_tok = get_next_token();
            if (assign_tok.type == TOKEN_ASSIGN) {
                parse_expr();
                get_next_token(); // Consumă ';'
                append_str(asm_out, &asm_idx, "    mov [var_");
                append_str(asm_out, &asm_idx, var_tok.text);
                append_str(asm_out, &asm_idx, "], rax\n");
            }
        }
        else if (t.type == TOKEN_FLOAT_KW) {
            Token var_tok = get_next_token();
            register_variable(var_tok.text, 1);
            Token assign_tok = get_next_token();
            if (assign_tok.type == TOKEN_ASSIGN) {
                parse_float_expr();
                get_next_token(); // Consumă ';'
                append_str(asm_out, &asm_idx, "    movss [var_");
                append_str(asm_out, &asm_idx, var_tok.text);
                append_str(asm_out, &asm_idx, "], xmm0\n");
            }
        }
        else if (t.type == TOKEN_IDENTIFIER) {
            Token var_tok = t;
            Token assign_tok = get_next_token();
            if (assign_tok.type == TOKEN_ASSIGN) {
                if (is_var_float(var_tok.text)) {
                    parse_float_expr();
                    get_next_token(); // ';'
                    append_str(asm_out, &asm_idx, "    movss [var_");
                    append_str(asm_out, &asm_idx, var_tok.text);
                    append_str(asm_out, &asm_idx, "], xmm0\n");
                } else {
                    parse_expr();
                    get_next_token(); // ';'
                    append_str(asm_out, &asm_idx, "    mov [var_");
                    append_str(asm_out, &asm_idx, var_tok.text);
                    append_str(asm_out, &asm_idx, "], rax\n");
                }
            }
        }
        else if (t.type == TOKEN_PRINT_KW) {
            get_next_token(); // '('
            Token arg_tok = get_next_token();
            get_next_token(); // ')'
            get_next_token(); // ';'
            
            if (arg_tok.type == TOKEN_STRING) {
                append_str(asm_out, &asm_idx, "    print \"");
                append_str(asm_out, &asm_idx, arg_tok.text);
                append_str(asm_out, &asm_idx, "\"\n    newline\n");
            } else if (arg_tok.type == TOKEN_IDENTIFIER) {
                if (is_var_float(arg_tok.text)) {
                    append_str(asm_out, &asm_idx, "    movss xmm0, [var_");
                    append_str(asm_out, &asm_idx, arg_tok.text);
                    append_str(asm_out, &asm_idx, "]\n    print_xmm\n    newline\n");
                } else {
                    append_str(asm_out, &asm_idx, "    mov rax, [var_");
                    append_str(asm_out, &asm_idx, arg_tok.text);
                    append_str(asm_out, &asm_idx, "]\n    print_rax\n    newline\n");
                }
            }
        }
		else if (t.type == TOKEN_IF_KW) {
            get_next_token(); // Trece peste '('
            
            int current_if = if_counter++;
            
            // 1. Verificăm dacă avem o negare (!) la începutul condiției
            Token next_tok_check = get_next_token();
            int is_negated = 0;
            Token var1_tok;
            
            if (next_tok_check.type == TOKEN_NOT) {
                is_negated = 1;
                var1_tok = get_next_token(); // Citim variabila (ex: x din !x)
            } else {
                var1_tok = next_tok_check;
            }
            
            Token op1_tok = get_next_token(); // Poate fi '=', '<', sau chiar ')' pentru if(x)
            
            if (op1_tok.type == TOKEN_ASSIGN) {
                // --- ATRIBUIRE ÎN CONDIȚIE (ex: if (x = 20) sau if (!(x = 20))) ---
                Token val1_tok = get_next_token(); // e.g. '20'
                get_next_token(); // Trece peste ')'
                
                append_str(asm_out, &asm_idx, "    mov rax, ");
                append_str(asm_out, &asm_idx, val1_tok.text);
                append_str(asm_out, &asm_idx, "\n    mov [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "], rax\n");
                
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, 0\n");
                
                // Dacă e normal, fals înseamnă 0 (je). Dacă e negat, fals înseamnă diferit de 0 (jne)
                append_str(asm_out, &asm_idx, "    ");
                append_str(asm_out, &asm_idx, is_negated ? "jne" : "je");
                append_str(asm_out, &asm_idx, " else_body_");
                append_int(asm_out, &asm_idx, current_if);
                append_str(asm_out, &asm_idx, "\n");
                
            } else if (op1_tok.type == TOKEN_RPAREN) {
                // --- VARIABILĂ SIMPLĂ (ex: if (x) sau if (!x)) ---
                // ')' a fost deja consumat de op1_tok
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, 0\n");
                
                append_str(asm_out, &asm_idx, "    ");
                append_str(asm_out, &asm_idx, is_negated ? "jne" : "je");
                append_str(asm_out, &asm_idx, " else_body_");
                append_int(asm_out, &asm_idx, current_if);
                append_str(asm_out, &asm_idx, "\n");
                
            } else {
                // --- COMPARAȚIE CLASICĂ (inclusiv &&, || și ! negare) ---
                Token val1_tok = get_next_token(); 
                
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, ");
                append_str(asm_out, &asm_idx, val1_tok.text);
                append_str(asm_out, &asm_idx, "\n");
                
                Token next_after_cond1 = get_next_token(); // '&&', '||' sau ')'
                
                if (next_after_cond1.type == TOKEN_AND) {
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, is_negated ? get_direct_jump(op1_tok.type) : get_inverse_jump(op1_tok.type));
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
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, is_negated ? get_inverse_jump(op1_tok.type) : get_direct_jump(op1_tok.type));
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
                    // O singură comparație, ex: if (!(x < 5))
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, is_negated ? get_direct_jump(op1_tok.type) : get_inverse_jump(op1_tok.type));
                    append_str(asm_out, &asm_idx, " else_body_");
                    append_int(asm_out, &asm_idx, current_if);
                    append_str(asm_out, &asm_idx, "\n");
                }
            }
            
            // Salt direct la corpul de adevăr
            append_str(asm_out, &asm_idx, "    jmp if_body_");
            append_int(asm_out, &asm_idx, current_if);
            append_str(asm_out, &asm_idx, "\n");
            
            append_str(asm_out, &asm_idx, "if_body_");
            append_int(asm_out, &asm_idx, current_if);
            append_str(asm_out, &asm_idx, ":\n");
            
            Token next_tok = get_next_token();
            if (next_tok.type == TOKEN_LBRACE) {
                block_stack[block_depth++] = current_if;
            } else {
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
            
            int current_while = while_counter++;
            block_stack[block_depth++] = current_while + 2000;
            
            append_str(asm_out, &asm_idx, "while_start_");
            append_int(asm_out, &asm_idx, current_while);
            append_str(asm_out, &asm_idx, ":\n");
            
            // 1. Verificăm dacă avem o negare (!)
            Token next_tok_check = get_next_token();
            int is_negated = 0;
            Token var1_tok;
            
            if (next_tok_check.type == TOKEN_NOT) {
                is_negated = 1;
                var1_tok = get_next_token(); 
            } else {
                var1_tok = next_tok_check;
            }
            
            Token op1_tok = get_next_token(); 
            
            if (op1_tok.type == TOKEN_ASSIGN) {
                // --- ATRIBUIRE ÎN CONDIȚIE WHILE ---
                Token val1_tok = get_next_token(); 
                get_next_token(); // ')'
                
                append_str(asm_out, &asm_idx, "    mov rax, ");
                append_str(asm_out, &asm_idx, val1_tok.text);
                append_str(asm_out, &asm_idx, "\n    mov [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "], rax\n");
                
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, 0\n");
                
                append_str(asm_out, &asm_idx, "    ");
                append_str(asm_out, &asm_idx, is_negated ? "jne" : "je");
                append_str(asm_out, &asm_idx, " while_end_");
                append_int(asm_out, &asm_idx, current_while);
                append_str(asm_out, &asm_idx, "\n");
                
            } else if (op1_tok.type == TOKEN_RPAREN) {
                // --- VARIABILĂ SIMPLĂ (ex: while(x) sau while(!x)) ---
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, 0\n");
                
                append_str(asm_out, &asm_idx, "    ");
                append_str(asm_out, &asm_idx, is_negated ? "jne" : "je");
                append_str(asm_out, &asm_idx, " while_end_");
                append_int(asm_out, &asm_idx, current_while);
                append_str(asm_out, &asm_idx, "\n");
                
            } else {
                // --- COMPARAȚIE CLASICĂ WHILE ---
                Token val1_tok = get_next_token();
                
                append_str(asm_out, &asm_idx, "    mov rdi, [var_");
                append_str(asm_out, &asm_idx, var1_tok.text);
                append_str(asm_out, &asm_idx, "]\n    cmp rdi, ");
                append_str(asm_out, &asm_idx, val1_tok.text);
                append_str(asm_out, &asm_idx, "\n");
                
                Token next_after_cond1 = get_next_token();
                
                if (next_after_cond1.type == TOKEN_AND) {
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, is_negated ? get_direct_jump(op1_tok.type) : get_inverse_jump(op1_tok.type));
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
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, is_negated ? get_inverse_jump(op1_tok.type) : get_direct_jump(op1_tok.type));
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
                    // O singură comparație
                    append_str(asm_out, &asm_idx, "    ");
                    append_str(asm_out, &asm_idx, is_negated ? get_direct_jump(op1_tok.type) : get_inverse_jump(op1_tok.type));
                    append_str(asm_out, &asm_idx, " while_end_");
                    append_int(asm_out, &asm_idx, current_while);
                    append_str(asm_out, &asm_idx, "\n");
                }
            }
            
            Token next_tok = get_next_token();
            if (next_tok.type == TOKEN_LBRACE) {
                // Cazul 1: Bloc cu acolade 
            } else {
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
                
                append_str(asm_out, &asm_idx, "    jmp while_start_");
                append_int(asm_out, &asm_idx, current_while);
                append_str(asm_out, &asm_idx, "\n");
                
                append_str(asm_out, &asm_idx, "while_end_");
                append_int(asm_out, &asm_idx, current_while);
                append_str(asm_out, &asm_idx, ":\n");
                
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
    for (int i = 0; i < var_count; i++) {
        append_str(asm_out, &asm_idx, "var_");
        append_str(asm_out, &asm_idx, var_table[i].name);
        append_str(asm_out, &asm_idx, ": data 0\n");
    }

    asm_out[asm_idx] = '\0';

    nano_write_file(argv[2], (const uint8_t*)asm_out, asm_idx);
    nano_print("[nanoc] Generare cod ASM (.s) finalizata cu succes!\n");
    return 0;
}


*/