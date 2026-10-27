#include "nano_libc.h"

typedef enum {
    TOKEN_EOF = 0, TOKEN_CHAR_KW, TOKEN_INT_KW, TOKEN_FLOAT_KW, TOKEN_PRINT_KW, TOKEN_IF_KW, TOKEN_ELSE_KW, TOKEN_WHILE_KW, TOKEN_FOR_KW, TOKEN_IDENTIFIER, 
    TOKEN_NUMBER, TOKEN_FLOAT_NUMBER, TOKEN_STRING, TOKEN_CHAR_LITERAL, TOKEN_ASSIGN, 
    TOKEN_EQ, TOKEN_NE, TOKEN_GT, TOKEN_LT, TOKEN_GE, TOKEN_LE,
    TOKEN_SEMICOLON, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
	TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_AND, TOKEN_OR, TOKEN_NOT, TOKEN_AMPERSAND,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_MOD, TOKEN_UNKNOWN
} TokenType;

typedef struct {
    TokenType type;
    char text[256];
} Token;

typedef struct {
    char name[32];
    int is_float;
    int is_pointer;
    int is_char;
    int pointer_level;
	int is_array;     // 1 dacă este tablou
    int array_size;   // numărul de elemente
} Variable;

static const char* src_ptr = 0;
char asm_out[4096];
int asm_idx = 0;

Variable var_table[128];
int var_count = 0;

void append_str(char* dest, int* idx, const char* src);
void append_int(char* dest, int* idx, int val);
Token get_next_token();

void parse_float_expr();
void parse_float_term();
void parse_float_factor();

void parse_expr();
void parse_term();
void parse_factor();
void parse_statement();

void register_variable(const char* name, int is_float, int is_pointer, int pointer_level, int is_char) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) return;
    }
    strcpy(var_table[var_count].name, name);
    var_table[var_count].is_float = is_float;
    var_table[var_count].is_pointer = is_pointer;
    var_table[var_count].pointer_level = pointer_level;
    var_table[var_count].is_char = is_char;
    var_count++;
}

void register_array(const char* name, int is_float, int is_char, int size) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) return;
    }
    strcpy(var_table[var_count].name, name);
    var_table[var_count].is_float = is_float;
    var_table[var_count].is_char = is_char;
    var_table[var_count].is_array = 1;
    var_table[var_count].array_size = size;
    var_count++;
}

int is_var_array(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) return var_table[i].is_array;
    }
    return 0;
}

int get_var_pointer_level(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) return var_table[i].pointer_level;
    }
    return 0;
}

int is_var_base_float(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) return var_table[i].is_float;
    }
    return 0;
}

int is_var_base_char(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) return var_table[i].is_char;
    }
    return 0;
}

int is_var_char(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) {
            if (var_table[i].is_pointer) return 0;
            return var_table[i].is_char;
        }
    }
    return 0;
}

int is_deref_float(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) {
            return var_table[i].is_pointer && var_table[i].is_float;
        }
    }
    return 0;
}

int is_var_float(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) {
            if (var_table[i].is_pointer) return 0;
            return var_table[i].is_float;
        }
    }
    return 0;
}

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
        register_variable("tmp_float", 1, 0, 0, 0);
        append_str(asm_out, &asm_idx, "    mov rax, ");
        append_int(asm_out, &asm_idx, (int)u);
        append_str(asm_out, &asm_idx, "\n    mov [var_tmp_float], rax\n");
        append_str(asm_out, &asm_idx, "    movss xmm0, [var_tmp_float]\n");
    } else if (t.type == TOKEN_IDENTIFIER) {
        const char* saved_ptr = src_ptr;
        Token next = get_next_token();
        
        if (next.type == TOKEN_LBRACKET) {
            parse_expr(); // Indexul i -> RAX
            get_next_token(); // Consumă ']'
            
            append_str(asm_out, &asm_idx, "    mov rbx, 8\n");
            append_str(asm_out, &asm_idx, "    imul rbx\n");
            append_str(asm_out, &asm_idx, "    lea rbx, [var_");
            append_str(asm_out, &asm_idx, t.text);
            append_str(asm_out, &asm_idx, "]\n");
            append_str(asm_out, &asm_idx, "    add rax, rbx\n");
            append_str(asm_out, &asm_idx, "    movss xmm0, [rax]\n"); // Citim float-ul
        } else {
            src_ptr = saved_ptr;
            append_str(asm_out, &asm_idx, "    movss xmm0, [var_");
            append_str(asm_out, &asm_idx, t.text);
            append_str(asm_out, &asm_idx, "]\n");
        }
    } else if (t.type == TOKEN_STAR) {
        int star_count = 1;
        Token next = get_next_token();
        while (next.type == TOKEN_STAR) {
            star_count++;
            next = get_next_token();
        }
        Token var_tok = next;
        
        append_str(asm_out, &asm_idx, "    mov rax, [var_");
        append_str(asm_out, &asm_idx, var_tok.text);
        append_str(asm_out, &asm_idx, "]\n");
        
        for (int i = 0; i < star_count - 1; i++) {
            append_str(asm_out, &asm_idx, "    mov rax, [rax]\n");
        }
        append_str(asm_out, &asm_idx, "    movss xmm0, [rax]\n");
    } else if (t.type == TOKEN_LPAREN) {
        parse_float_expr();
        get_next_token();
    }
}

void parse_float_term() {
    parse_float_factor();
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        if (op.type == TOKEN_STAR || op.type == TOKEN_SLASH) {
            register_variable("tmp_f1", 1, 0, 0, 0);
            append_str(asm_out, &asm_idx, "    movss [var_tmp_f1], xmm0\n");
            parse_float_factor();
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
    parse_float_term();
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        if (op.type == TOKEN_PLUS || op.type == TOKEN_MINUS) {
            register_variable("tmp_f2", 1, 0, 0, 0);
            append_str(asm_out, &asm_idx, "    movss [var_tmp_f2], xmm0\n");
            parse_float_term();
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
    if (t.type == TOKEN_NUMBER || t.type == TOKEN_CHAR_LITERAL) {
        append_str(asm_out, &asm_idx, "    mov rax, ");
        append_str(asm_out, &asm_idx, t.text);
        append_str(asm_out, &asm_idx, "\n");
    } else if (t.type == TOKEN_IDENTIFIER) {
        const char* saved_ptr = src_ptr;
        Token next = get_next_token();
        
        if (next.type == TOKEN_LBRACKET) {
            parse_expr(); // Indexul i -> RAX
            get_next_token(); // Consumă ']'
            
            append_str(asm_out, &asm_idx, "    mov rbx, 8\n");
            append_str(asm_out, &asm_idx, "    imul rbx\n");
            append_str(asm_out, &asm_idx, "    lea rbx, [var_");
            append_str(asm_out, &asm_idx, t.text);
            append_str(asm_out, &asm_idx, "]\n");
            append_str(asm_out, &asm_idx, "    add rax, rbx\n");
            append_str(asm_out, &asm_idx, "    mov rax, [rax]\n"); // Citim valoarea
        } else {
            src_ptr = saved_ptr;
            append_str(asm_out, &asm_idx, "    mov rax, [var_");
            append_str(asm_out, &asm_idx, t.text);
            append_str(asm_out, &asm_idx, "]\n");
        }
    } else if (t.type == TOKEN_AMPERSAND) {
        Token var_tok = get_next_token();
        append_str(asm_out, &asm_idx, "    lea rax, [var_");
        append_str(asm_out, &asm_idx, var_tok.text);
        append_str(asm_out, &asm_idx, "]\n");
    } else if (t.type == TOKEN_STAR) {
        int star_count = 1;
        Token next = get_next_token();
        while (next.type == TOKEN_STAR) {
            star_count++;
            next = get_next_token();
        }
        Token var_tok = next;
        
        append_str(asm_out, &asm_idx, "    mov rax, [var_");
        append_str(asm_out, &asm_idx, var_tok.text);
        append_str(asm_out, &asm_idx, "]\n");
        
        for (int i = 0; i < star_count; i++) {
            append_str(asm_out, &asm_idx, "    mov rax, [rax]\n");
        }
    } else if (t.type == TOKEN_LPAREN) {
        parse_expr();
        get_next_token();
    }
}

void parse_term() {
    parse_factor();
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        if (op.type == TOKEN_STAR || op.type == TOKEN_SLASH || op.type == TOKEN_MOD) {
            append_str(asm_out, &asm_idx, "    push rax\n");
            parse_factor();
            append_str(asm_out, &asm_idx, "    mov rbx, rax\n");
            append_str(asm_out, &asm_idx, "    pop rax\n");
            if (op.type == TOKEN_STAR) {
                append_str(asm_out, &asm_idx, "    imul rbx\n");
            } else if (op.type == TOKEN_SLASH) {
                append_str(asm_out, &asm_idx, "    cqo\n    idiv rbx\n");
            } else if (op.type == TOKEN_MOD) {
                append_str(asm_out, &asm_idx, "    cqo\n    idiv rbx\n    mov rax, rdx\n");
            }
        } else {
            src_ptr = saved_ptr;
            break;
        }
    }
}

void parse_expr() {
    parse_term();
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        if (op.type == TOKEN_PLUS || op.type == TOKEN_MINUS) {
            append_str(asm_out, &asm_idx, "    push rax\n");
            parse_term();
            append_str(asm_out, &asm_idx, "    mov rbx, rax\n");
            append_str(asm_out, &asm_idx, "    pop rax\n");
            if (op.type == TOKEN_PLUS) {
                append_str(asm_out, &asm_idx, "    add rax, rbx\n");
            } else if (op.type == TOKEN_MINUS) {
                append_str(asm_out, &asm_idx, "    sub rax, rbx\n");
            }
        } else {
            src_ptr = saved_ptr;
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

const char* get_inverse_jump(TokenType op) {
    if (op == TOKEN_EQ) return "jne"; 
    if (op == TOKEN_NE) return "je";  
    if (op == TOKEN_GT) return "jle"; 
    if (op == TOKEN_LT) return "jge"; 
    if (op == TOKEN_GE) return "jl";  
    if (op == TOKEN_LE) return "jg";  
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

const char* get_inverse_jump_float(TokenType op) {
    if (op == TOKEN_EQ) return "jne"; 
    if (op == TOKEN_NE) return "je";  
    if (op == TOKEN_GT) return "jbe"; 
    if (op == TOKEN_LT) return "jae"; 
    if (op == TOKEN_GE) return "jb";  
    if (op == TOKEN_LE) return "ja";  
    return "jne"; 
}

const char* get_direct_jump_float(TokenType op) {
    if (op == TOKEN_EQ) return "je";
    if (op == TOKEN_NE) return "jne";
    if (op == TOKEN_GT) return "ja"; 
    if (op == TOKEN_LT) return "jb"; 
    if (op == TOKEN_GE) return "jae"; 
    if (op == TOKEN_LE) return "jbe"; 
    return "je";
}

void emit_condition_eval(Token var1_tok, Token op_tok, Token val2_tok, 
                         const char* target_label, int jump_on_true, int is_negated) {
    int is_float = is_var_float(var1_tok.text);

    if (is_float) {
        append_str(asm_out, &asm_idx, "    movss xmm0, [var_");
        append_str(asm_out, &asm_idx, var1_tok.text);
        append_str(asm_out, &asm_idx, "]\n");

        if (val2_tok.type == TOKEN_NUMBER || val2_tok.type == TOKEN_FLOAT_NUMBER) {
            uint32_t u = float_to_u32(val2_tok.text);
            register_variable("tmp_float", 1, 0, 0, 0);
            append_str(asm_out, &asm_idx, "    mov rax, ");
            append_int(asm_out, &asm_idx, (int)u);
            append_str(asm_out, &asm_idx, "\n    mov [var_tmp_float], rax\n");
            append_str(asm_out, &asm_idx, "    movss xmm1, [var_tmp_float]\n");
        } else if (val2_tok.type == TOKEN_IDENTIFIER) {
            append_str(asm_out, &asm_idx, "    movss xmm1, [var_");
            append_str(asm_out, &asm_idx, val2_tok.text);
            append_str(asm_out, &asm_idx, "]\n");
        } else {
            append_str(asm_out, &asm_idx, "    mov rax, 0\n    mov [var_tmp_float], rax\n");
            append_str(asm_out, &asm_idx, "    movss xmm1, [var_tmp_float]\n");
        }
        append_str(asm_out, &asm_idx, "    ucomiss xmm0, xmm1\n");
    } else {
        append_str(asm_out, &asm_idx, "    mov rdi, [var_");
        append_str(asm_out, &asm_idx, var1_tok.text);
        append_str(asm_out, &asm_idx, "]\n");

        if (val2_tok.type == TOKEN_NUMBER || val2_tok.type == TOKEN_FLOAT_NUMBER) {
            append_str(asm_out, &asm_idx, "    cmp rdi, ");
            append_str(asm_out, &asm_idx, val2_tok.text);
            append_str(asm_out, &asm_idx, "\n");
        } else if (val2_tok.type == TOKEN_IDENTIFIER) {
            append_str(asm_out, &asm_idx, "    mov rax, [var_");
            append_str(asm_out, &asm_idx, val2_tok.text);
            append_str(asm_out, &asm_idx, "]\n    cmp rdi, rax\n");
        } else {
            append_str(asm_out, &asm_idx, "    cmp rdi, 0\n");
        }
    }

    const char* jump_instr = NULL;
    if (op_tok.type == TOKEN_ASSIGN || op_tok.type == TOKEN_RPAREN) {
        jump_instr = (is_negated ^ jump_on_true) ? "jne" : "je";
    } else {
        if (is_float) {
            jump_instr = (is_negated ^ jump_on_true) ? get_direct_jump_float(op_tok.type) 
                                                     : get_inverse_jump_float(op_tok.type);
        } else {
            jump_instr = (is_negated ^ jump_on_true) ? get_direct_jump(op_tok.type) 
                                                     : get_inverse_jump(op_tok.type);
        }
    }

    append_str(asm_out, &asm_idx, "    ");
    append_str(asm_out, &asm_idx, jump_instr);
    append_str(asm_out, &asm_idx, " ");
    append_str(asm_out, &asm_idx, target_label);
    append_str(asm_out, &asm_idx, "\n");
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

    if (c == '\'') {
        src_ptr++;
        char char_val = *src_ptr++;
        if (char_val == '\\') {
            char esc = *src_ptr++;
            if (esc == 'n') char_val = '\n';
            else if (esc == 't') char_val = '\t';
            else if (esc == '0') char_val = '\0';
            else char_val = esc;
        }
        if (*src_ptr == '\'') src_ptr++;
        
        itoa((int)(unsigned char)char_val, token.text, 10);
        token.type = TOKEN_CHAR_LITERAL;
        return token;
    }

    if (is_alpha(c)) {
        int i = 0;
        while (is_alnum(*src_ptr) && i < 63) {
            token.text[i++] = *src_ptr++;
        }
        token.text[i] = '\0';
        
        if (str_eq(token.text, "char")) token.type = TOKEN_CHAR_KW;
        else if (str_eq(token.text, "int")) token.type = TOKEN_INT_KW;
        else if (str_eq(token.text, "float")) token.type = TOKEN_FLOAT_KW;
        else if (str_eq(token.text, "print")) token.type = TOKEN_PRINT_KW;
        else if (str_eq(token.text, "if")) token.type = TOKEN_IF_KW;
        else if (str_eq(token.text, "else")) token.type = TOKEN_ELSE_KW;
        else if (str_eq(token.text, "while")) token.type = TOKEN_WHILE_KW;
        else if (str_eq(token.text, "for")) token.type = TOKEN_FOR_KW;
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
            else { token.type = TOKEN_AMPERSAND; str_cpy(token.text, "&"); }
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
		case '[': token.type = TOKEN_LBRACKET; break;
        case ']': token.type = TOKEN_RBRACKET; break;
        default:  token.type = TOKEN_UNKNOWN; break;
    }
    
    return token;
}

void consume_semicolon() {
    const char* saved = src_ptr;
    Token semi = get_next_token();
    if (semi.type != TOKEN_SEMICOLON) {
        src_ptr = saved; 
    }
}

static int if_counter = 0;

void parse_statement_token(Token t) {
    if (t.type == TOKEN_LBRACE) {
        Token inner = get_next_token();
        while (inner.type != TOKEN_RBRACE && inner.type != TOKEN_EOF) {
            parse_statement_token(inner);
            inner = get_next_token();
        }
    }
    else if (t.type == TOKEN_INT_KW || t.type == TOKEN_FLOAT_KW || t.type == TOKEN_CHAR_KW) {
        int is_float = (t.type == TOKEN_FLOAT_KW);
        int is_char  = (t.type == TOKEN_CHAR_KW);
        Token next_tok = get_next_token();
        int is_ptr = 0;
        int ptr_level = 0;
        
        while (next_tok.type == TOKEN_STAR) {
            is_ptr = 1;
            ptr_level++;
            next_tok = get_next_token();
        }
        
        Token var_tok = next_tok;
        Token op_or_semi = get_next_token();

        // Verificăm dacă este o declarare de tablou: type arr[10];
        if (op_or_semi.type == TOKEN_LBRACKET) {
            Token size_tok = get_next_token(); // Dimensiunea (ex: 10)
            int array_sz = atoi(size_tok.text);
            get_next_token(); // Consumă ']'
            consume_semicolon();
            
            register_array(var_tok.text, is_float, is_char, array_sz);
        }
        else {
            register_variable(var_tok.text, is_float, is_ptr, ptr_level, is_char);
            if (op_or_semi.type == TOKEN_ASSIGN) {
                if (is_float && ptr_level == 0) {
                    parse_float_expr();
                    consume_semicolon();
                    append_str(asm_out, &asm_idx, "    movss [var_");
                    append_str(asm_out, &asm_idx, var_tok.text);
                    append_str(asm_out, &asm_idx, "], xmm0\n");
                } else {
                    parse_expr();
                    consume_semicolon();
                    append_str(asm_out, &asm_idx, "    mov [var_");
                    append_str(asm_out, &asm_idx, var_tok.text);
                    append_str(asm_out, &asm_idx, "], rax\n");
                }
            }
        }
    }
    else if (t.type == TOKEN_IDENTIFIER) {
        Token var_tok = t;
        Token next_tok = get_next_token();
        
        if (next_tok.type == TOKEN_LBRACKET) {
            // Atribuire în element de tablou: arr[i] = val;
            parse_expr();     // Calculăm indexul i -> rezultatul în RAX
            get_next_token(); // Consumă ']'
            
            // Calculăm adresa adr = &arr + i * 8
            append_str(asm_out, &asm_idx, "    mov rbx, 8\n");
            append_str(asm_out, &asm_idx, "    imul rbx\n");
            append_str(asm_out, &asm_idx, "    lea rbx, [var_");
            append_str(asm_out, &asm_idx, var_tok.text);
            append_str(asm_out, &asm_idx, "]\n");
            append_str(asm_out, &asm_idx, "    add rax, rbx\n");
            append_str(asm_out, &asm_idx, "    push rax\n"); // Salvăm adresa &arr[i] pe stivă
            
            Token assign_tok = get_next_token(); // Consumă '='
            if (assign_tok.type == TOKEN_ASSIGN) {
                if (is_var_base_float(var_tok.text)) {
                    parse_float_expr();
                    consume_semicolon();
                    append_str(asm_out, &asm_idx, "    pop rbx\n");
                    append_str(asm_out, &asm_idx, "    movss [rbx], xmm0\n");
                } else {
                    parse_expr();
                    consume_semicolon();
                    append_str(asm_out, &asm_idx, "    pop rbx\n");
                    append_str(asm_out, &asm_idx, "    mov [rbx], rax\n");
                }
            }
        }
        else if (next_tok.type == TOKEN_ASSIGN) {
            if (is_var_float(var_tok.text)) {
                parse_float_expr();
                consume_semicolon();
                append_str(asm_out, &asm_idx, "    movss [var_");
                append_str(asm_out, &asm_idx, var_tok.text);
                append_str(asm_out, &asm_idx, "], xmm0\n");
            } else {
                parse_expr();
                consume_semicolon();
                append_str(asm_out, &asm_idx, "    mov [var_");
                append_str(asm_out, &asm_idx, var_tok.text);
                append_str(asm_out, &asm_idx, "], rax\n");
            }
        }
    }
    else if (t.type == TOKEN_STAR) {
        int star_count = 1;
        Token next_tok = get_next_token();
        while (next_tok.type == TOKEN_STAR) {
            star_count++;
            next_tok = get_next_token();
        }
        Token ptr_tok = next_tok;
        Token assign_tok = get_next_token();
        
        if (assign_tok.type == TOKEN_ASSIGN) {
            int is_target_float = is_var_base_float(ptr_tok.text) && (get_var_pointer_level(ptr_tok.text) == star_count);

            if (is_target_float) {
                parse_float_expr();
                consume_semicolon();
                
                append_str(asm_out, &asm_idx, "    mov rax, [var_");
                append_str(asm_out, &asm_idx, ptr_tok.text);
                append_str(asm_out, &asm_idx, "]\n");
                
                for (int i = 0; i < star_count - 1; i++) {
                    append_str(asm_out, &asm_idx, "    mov rax, [rax]\n");
                }
                append_str(asm_out, &asm_idx, "    movss [rax], xmm0\n");
            } else {
                parse_expr();
                consume_semicolon();
                
                append_str(asm_out, &asm_idx, "    push rax\n");
                append_str(asm_out, &asm_idx, "    mov rbx, [var_");
                append_str(asm_out, &asm_idx, ptr_tok.text);
                append_str(asm_out, &asm_idx, "]\n");
                
                for (int i = 0; i < star_count - 1; i++) {
                    append_str(asm_out, &asm_idx, "    mov rbx, [rbx]\n");
                }
                
                append_str(asm_out, &asm_idx, "    pop rax\n");
                append_str(asm_out, &asm_idx, "    mov [rbx], rax\n");
            }
        }
    }
    else if (t.type == TOKEN_PRINT_KW) {
        get_next_token(); // Consumă '('

        const char* saved_ptr = src_ptr;
        Token arg_tok = get_next_token();

        if (arg_tok.type == TOKEN_STRING) {
            get_next_token(); // Consumă ')'
            consume_semicolon();
            append_str(asm_out, &asm_idx, "    print \"");
            append_str(asm_out, &asm_idx, arg_tok.text);
            append_str(asm_out, &asm_idx, "\"\n    newline\n");
        } 
        else {
            int is_float_expression = 0;
            int is_char_expression = 0;

            if (arg_tok.type == TOKEN_CHAR_LITERAL) {
                is_char_expression = 1;
            } else if (arg_tok.type == TOKEN_IDENTIFIER && is_var_char(arg_tok.text)) {
                is_char_expression = 1;
            } else if (arg_tok.type == TOKEN_FLOAT_NUMBER) {
                is_float_expression = 1;
            } else if (arg_tok.type == TOKEN_IDENTIFIER && is_var_float(arg_tok.text)) {
                is_float_expression = 1;
            } else if (arg_tok.type == TOKEN_STAR) {
                int star_count = 1;
                Token next = get_next_token();
                while (next.type == TOKEN_STAR) {
                    star_count++;
                    next = get_next_token();
                }
                Token var_tok = next;
                if (is_var_base_float(var_tok.text) && (get_var_pointer_level(var_tok.text) == star_count)) {
                    is_float_expression = 1;
                }
				else if (is_var_base_char(var_tok.text) && (get_var_pointer_level(var_tok.text) == star_count)) {
                    is_char_expression = 1;
                }
            }

            src_ptr = saved_ptr;

            if (is_char_expression) {
                parse_expr();       // Evaluează expresia (cod ASCII în RAX)
                get_next_token();   // Consumă ')'
                consume_semicolon();
                append_str(asm_out, &asm_idx, "    print_char\n    newline\n");
            } else if (is_float_expression) {
                parse_float_expr();
                get_next_token();   // Consumă ')'
                consume_semicolon();
                append_str(asm_out, &asm_idx, "    print_xmm\n    newline\n");
            } else {
                parse_expr();       // Evaluează expresia de întregi
                get_next_token();   // Consumă ')'
                consume_semicolon();
                append_str(asm_out, &asm_idx, "    print_rax\n    newline\n");
            }
        }
    }
    else if (t.type == TOKEN_IF_KW) {
        get_next_token(); 
        
        int current_if = if_counter++;
        char else_label[32], end_label[32];
        strcpy(else_label, "else_body_"); itoa(current_if, else_label + strlen(else_label), 10);
        strcpy(end_label, "if_end_"); itoa(current_if, end_label + strlen(end_label), 10);

        Token var1_tok = get_next_token();
        Token op_tok = get_next_token();
        Token val2_tok;
        
        if (op_tok.type == TOKEN_RPAREN) {
            val2_tok = (Token){ TOKEN_NUMBER, "0" };
        } else {
            val2_tok = get_next_token();
            get_next_token(); 
        }

        emit_condition_eval(var1_tok, op_tok, val2_tok, else_label, 0, 0);

        Token body_tok = get_next_token();
        parse_statement_token(body_tok);

        append_str(asm_out, &asm_idx, "    jmp ");
        append_str(asm_out, &asm_idx, end_label);
        append_str(asm_out, &asm_idx, "\n");

        append_str(asm_out, &asm_idx, else_label);
        append_str(asm_out, &asm_idx, ":\n");

        const char* saved_ptr = src_ptr;
        Token else_check = get_next_token();
        
        if (else_check.type == TOKEN_ELSE_KW) {
            Token else_body = get_next_token();
            parse_statement_token(else_body);
        } else {
            src_ptr = saved_ptr;
        }

        append_str(asm_out, &asm_idx, end_label);
        append_str(asm_out, &asm_idx, ":\n");
    }
    else if (t.type == TOKEN_WHILE_KW) {
        get_next_token(); 
        
        int current_while = if_counter++; 
        char cond_label[32], end_label[32];
        strcpy(cond_label, "while_cond_"); itoa(current_while, cond_label + strlen(cond_label), 10);
        strcpy(end_label, "while_end_"); itoa(current_while, end_label + strlen(end_label), 10);
        
        append_str(asm_out, &asm_idx, cond_label);
        append_str(asm_out, &asm_idx, ":\n");
        
        Token var1_tok = get_next_token();
        Token op_tok = get_next_token();
        Token val2_tok;
        
        if (op_tok.type == TOKEN_RPAREN) {
            val2_tok = (Token){ TOKEN_NUMBER, "0" };
        } else {
            val2_tok = get_next_token();
            get_next_token(); 
        }
        
        emit_condition_eval(var1_tok, op_tok, val2_tok, end_label, 0, 0);
        
        Token body_tok = get_next_token();
        parse_statement_token(body_tok);
        
        append_str(asm_out, &asm_idx, "    jmp ");
        append_str(asm_out, &asm_idx, cond_label);
        append_str(asm_out, &asm_idx, "\n");
        
        append_str(asm_out, &asm_idx, end_label);
        append_str(asm_out, &asm_idx, ":\n");
    }
    else if (t.type == TOKEN_FOR_KW) {
        get_next_token(); 

        Token init_tok = get_next_token();
        parse_statement_token(init_tok); 

        int current_for = if_counter++; 
        char cond_label[32], end_label[32];
        strcpy(cond_label, "for_cond_"); itoa(current_for, cond_label + strlen(cond_label), 10);
        strcpy(end_label, "for_end_"); itoa(current_for, end_label + strlen(end_label), 10);
        
        append_str(asm_out, &asm_idx, cond_label);
        append_str(asm_out, &asm_idx, ":\n");
        
        Token var1_tok = get_next_token();
        Token op_tok = get_next_token();
        Token val2_tok = get_next_token();
        
        emit_condition_eval(var1_tok, op_tok, val2_tok, end_label, 0, 0);
        consume_semicolon(); 
        
        const char* increment_ptr = src_ptr; 
        
        int paren_count = 1; 
        while(paren_count > 0) {
            Token skip = get_next_token();
            if (skip.type == TOKEN_LPAREN) paren_count++;
            if (skip.type == TOKEN_RPAREN) paren_count--;
            if (skip.type == TOKEN_EOF) break;
        }
        
        Token body_tok = get_next_token();
        parse_statement_token(body_tok);
        
        const char* after_body_ptr = src_ptr; 
        src_ptr = increment_ptr;              

        Token inc_var = get_next_token();
        Token inc_eq = get_next_token();
        if (inc_eq.type == TOKEN_ASSIGN) {
            if (is_var_float(inc_var.text)) {
                parse_float_expr(); 
                append_str(asm_out, &asm_idx, "    movss [var_");
                append_str(asm_out, &asm_idx, inc_var.text);
                append_str(asm_out, &asm_idx, "], xmm0\n");
            } else {
                parse_expr(); 
                append_str(asm_out, &asm_idx, "    mov [var_");
                append_str(asm_out, &asm_idx, inc_var.text);
                append_str(asm_out, &asm_idx, "], rax\n");
            }
        }

        src_ptr = after_body_ptr;

        append_str(asm_out, &asm_idx, "    jmp ");
        append_str(asm_out, &asm_idx, cond_label);
        append_str(asm_out, &asm_idx, "\n");
        
        append_str(asm_out, &asm_idx, end_label);
        append_str(asm_out, &asm_idx, ":\n");
    }
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

    Token t = get_next_token();
    while (t.type != TOKEN_EOF) {
        parse_statement_token(t);
        t = get_next_token();
    }

    append_str(asm_out, &asm_idx, "    exit\n");

    for (int i = 0; i < var_count; i++) {
        append_str(asm_out, &asm_idx, "var_");
        append_str(asm_out, &asm_idx, var_table[i].name);
        append_str(asm_out, &asm_idx, ": data 0\n");
        
        // Dacă este tablou, alocăm restul de (size - 1) sloturi contigue
        if (var_table[i].is_array) {
            for (int k = 1; k < var_table[i].array_size; k++) {
                append_str(asm_out, &asm_idx, "    data 0\n");
            }
        }
    }

    asm_out[asm_idx] = '\0';

    nano_write_file(argv[2], (const uint8_t*)asm_out, asm_idx);
    nano_print("[nanoc] Generare cod ASM (.s) finalizata cu succes!\n");
    return 0;
}