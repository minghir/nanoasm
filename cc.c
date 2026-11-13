#include "nano_libc.h"

typedef enum {
    TOKEN_EOF = 0, TOKEN_CHAR_KW, TOKEN_INT_KW, TOKEN_FLOAT_KW, TOKEN_VOID_KW, TOKEN_PRINT_KW,
    TOKEN_IF_KW, TOKEN_ELSE_KW, TOKEN_WHILE_KW, TOKEN_FOR_KW, TOKEN_BREAK_KW, TOKEN_CONTINUE_KW,
    TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_FLOAT_NUMBER, TOKEN_STRING, TOKEN_CHAR_LITERAL, TOKEN_ASSIGN, TOKEN_RETURN_KW, 
    TOKEN_EQ, TOKEN_NE, TOKEN_GT, TOKEN_LT, TOKEN_GE, TOKEN_LE,
    TOKEN_SEMICOLON, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_AND, TOKEN_OR, TOKEN_NOT, TOKEN_AMPERSAND, TOKEN_COMMA,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_MOD, TOKEN_INC, TOKEN_DEC, TOKEN_UNKNOWN
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
    int is_array;
    int array_size;
    int is_local;
    int stack_offset;
    int has_init;
    int init_val;
} Variable;

char current_function[64] = "";

static const char* src_ptr = 0;
char asm_out[32768];
int asm_idx = 0;

static int if_counter = 0;

typedef struct {
    char break_label[32];
    char continue_label[32];
} LoopContext;

LoopContext loop_stack[32];
int loop_depth = 0;

Variable global_var_table[128];
int global_var_count = 0;

Variable local_var_table[128];
int local_var_count = 0;
int current_stack_offset = 0;

typedef struct {
    char name[32];
    char value[256];
} StringLiteral;

StringLiteral string_table[128];
int string_count = 0;

typedef struct {
    char name[64];
    int return_is_void;
    int return_is_float;
    int return_is_ptr;
    int return_is_char;
    int param_count;
    int param_is_float[8];
} FuncInfo;

FuncInfo func_table[64];
int func_count = 0;

FuncInfo* find_func(const char* name);
void emit_function_call(const char* func_name);

void append_str(char* dest, int* idx, const char* src);
void append_int(char* dest, int* idx, int val);

Token get_next_token();
const char* get_direct_jump(TokenType op);
const char* get_direct_jump_float(TokenType op);



void parse_float_expr();
void parse_float_term();
void parse_float_factor();

void parse_expr();
void parse_term();
void parse_factor();
void parse_statement();
void consume_semicolon();

Variable* find_var(const char* name) {
    for (int i = 0; i < local_var_count; i++) {
        if (strcmp(local_var_table[i].name, name) == 0) return &local_var_table[i];
    }
    for (int i = 0; i < global_var_count; i++) {
        if (strcmp(global_var_table[i].name, name) == 0) return &global_var_table[i];
    }
    return 0;
}

void append_var_ref(char* dest, int* idx, const char* name) {
    Variable* v = find_var(name);
    if (v && v->is_local) {
        append_str(dest, idx, "[rbp-");
        append_int(dest, idx, v->stack_offset);
        append_str(dest, idx, "]");
    } else {
        append_str(dest, idx, "[var_");
        append_str(dest, idx, name);
        append_str(dest, idx, "]");
    }
}

void register_global_variable_init(const char* name, int is_float, int is_pointer, int pointer_level, int is_char, int has_init, int init_val) {
    for (int i = 0; i < global_var_count; i++) {
        if (strcmp(global_var_table[i].name, name) == 0) return;
    }
    strcpy(global_var_table[global_var_count].name, name);
    global_var_table[global_var_count].is_float = is_float;
    global_var_table[global_var_count].is_pointer = is_pointer;
    global_var_table[global_var_count].pointer_level = pointer_level;
    global_var_table[global_var_count].is_char = is_char;
    global_var_table[global_var_count].is_array = 0;
    global_var_table[global_var_count].array_size = 0;
    global_var_table[global_var_count].is_local = 0;
    global_var_table[global_var_count].stack_offset = 0;
    global_var_table[global_var_count].has_init = has_init;
    global_var_table[global_var_count].init_val = init_val;
    global_var_count++;
}

void register_global_variable(const char* name, int is_float, int is_pointer, int pointer_level, int is_char) {
    register_global_variable_init(name, is_float, is_pointer, pointer_level, is_char, 0, 0);
}

void register_global_array(const char* name, int is_float, int is_char, int size) {
    for (int i = 0; i < global_var_count; i++) {
        if (strcmp(global_var_table[i].name, name) == 0) return;
    }
    strcpy(global_var_table[global_var_count].name, name);
    global_var_table[global_var_count].is_float = is_float;
    global_var_table[global_var_count].is_pointer = 0;
    global_var_table[global_var_count].pointer_level = 0;
    global_var_table[global_var_count].is_char = is_char;
    global_var_table[global_var_count].is_array = 1;
    global_var_table[global_var_count].array_size = size;
    global_var_table[global_var_count].is_local = 0;
    global_var_table[global_var_count].stack_offset = 0;
    global_var_table[global_var_count].has_init = 0;
    global_var_table[global_var_count].init_val = 0;
    global_var_count++;
}

void register_variable(const char* name, int is_float, int is_pointer, int pointer_level, int is_char) {
    for (int i = 0; i < local_var_count; i++) {
        if (strcmp(local_var_table[i].name, name) == 0) return;
    }
    current_stack_offset += 8;
    strcpy(local_var_table[local_var_count].name, name);
    local_var_table[local_var_count].is_float = is_float;
    local_var_table[local_var_count].is_pointer = is_pointer;
    local_var_table[local_var_count].pointer_level = pointer_level;
    local_var_table[local_var_count].is_char = is_char;
    local_var_table[local_var_count].is_array = 0;
    local_var_table[local_var_count].array_size = 0;
    local_var_table[local_var_count].is_local = 1;
    local_var_table[local_var_count].stack_offset = current_stack_offset;
    local_var_count++;
}

void register_array(const char* name, int is_float, int is_char, int size) {
    for (int i = 0; i < local_var_count; i++) {
        if (strcmp(local_var_table[i].name, name) == 0) return;
    }
    current_stack_offset += size * 8;
    strcpy(local_var_table[local_var_count].name, name);
    local_var_table[local_var_count].is_float = is_float;
    local_var_table[local_var_count].is_pointer = 0;
    local_var_table[local_var_count].pointer_level = 0;
    local_var_table[local_var_count].is_char = is_char;
    local_var_table[local_var_count].is_array = 1;
    local_var_table[local_var_count].array_size = size;
    local_var_table[local_var_count].is_local = 1;
    local_var_table[local_var_count].stack_offset = current_stack_offset;
    local_var_count++;
}

int is_var_array(const char* name) {
    Variable* v = find_var(name);
    return v ? v->is_array : 0;
}

int get_var_pointer_level(const char* name) {
    Variable* v = find_var(name);
    return v ? v->pointer_level : 0;
}

int is_var_base_float(const char* name) {
    Variable* v = find_var(name);
    return v ? v->is_float : 0;
}

int is_var_base_char(const char* name) {
    Variable* v = find_var(name);
    return v ? v->is_char : 0;
}

int is_var_char(const char* name) {
    Variable* v = find_var(name);
    if (!v || v->is_pointer || v->is_array) return 0;
    return v->is_char;
}

int is_var_str_ptr(const char* name) {
    Variable* v = find_var(name);
    return v ? (v->is_char && v->is_pointer && v->pointer_level == 1) : 0;
}

int is_deref_float(const char* name) {
    Variable* v = find_var(name);
    return v ? (v->is_pointer && v->is_float) : 0;
}

int is_var_float(const char* name) {
    Variable* v = find_var(name);
    if (!v || v->is_pointer) return 0;
    return v->is_float;
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
        register_global_variable("tmp_float", 1, 0, 0, 0);
        append_str(asm_out, &asm_idx, "    mov rax, ");
        append_int(asm_out, &asm_idx, (int)u);
        append_str(asm_out, &asm_idx, "\n    mov [var_tmp_float], rax\n");
        append_str(asm_out, &asm_idx, "    movss xmm0, [var_tmp_float]\n");
    } 
    else if (t.type == TOKEN_MINUS) {
        parse_float_factor();
        register_global_variable("tmp_float", 1, 0, 0, 0);
        append_str(asm_out, &asm_idx, "    movss xmm1, xmm0\n");
        append_str(asm_out, &asm_idx, "    mov rax, 0\n    mov [var_tmp_float], rax\n");
        append_str(asm_out, &asm_idx, "    movss xmm0, [var_tmp_float]\n");
        append_str(asm_out, &asm_idx, "    subss xmm0, xmm1\n");
    }
    else if (t.type == TOKEN_INC || t.type == TOKEN_DEC) {
        Token var_tok = get_next_token();
        consume_semicolon();
        
        append_str(asm_out, &asm_idx, "    mov rax, ");
        append_var_ref(asm_out, &asm_idx, var_tok.text);
        append_str(asm_out, &asm_idx, "\n");
        
        if (t.type == TOKEN_INC) append_str(asm_out, &asm_idx, "    inc rax\n");
        else append_str(asm_out, &asm_idx, "    dec rax\n");
        
        append_str(asm_out, &asm_idx, "    mov ");
        append_var_ref(asm_out, &asm_idx, var_tok.text);
        append_str(asm_out, &asm_idx, ", rax\n");
    }
    else if (t.type == TOKEN_IDENTIFIER) {
        const char* saved_ptr = src_ptr;
        Token next = get_next_token();
        
        if (next.type == TOKEN_LPAREN) {
            emit_function_call(t.text);
        }
        else if (next.type == TOKEN_LBRACKET) {
            parse_expr();
            get_next_token();
            
            append_str(asm_out, &asm_idx, "    mov rbx, 8\n");
            append_str(asm_out, &asm_idx, "    imul rbx\n");
            if (get_var_pointer_level(t.text) > 0) {
				append_str(asm_out, &asm_idx, "    mov rbx, ");
			} else {
				append_str(asm_out, &asm_idx, "    lea rbx, ");
			}
            append_var_ref(asm_out, &asm_idx, t.text);
            append_str(asm_out, &asm_idx, "\n");
            append_str(asm_out, &asm_idx, "    add rax, rbx\n");
            append_str(asm_out, &asm_idx, "    movss xmm0, [rax]\n");
        } else {
            src_ptr = saved_ptr;
            append_str(asm_out, &asm_idx, "    movss xmm0, ");
            append_var_ref(asm_out, &asm_idx, t.text);
            append_str(asm_out, &asm_idx, "\n");
        }
    } else if (t.type == TOKEN_STAR) {
        int star_count = 1;
        Token next = get_next_token();
        while (next.type == TOKEN_STAR) {
            star_count++;
            next = get_next_token();
        }
        Token var_tok = next;
        
        append_str(asm_out, &asm_idx, "    mov rax, ");
        append_var_ref(asm_out, &asm_idx, var_tok.text);
        append_str(asm_out, &asm_idx, "\n");
        
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
            append_str(asm_out, &asm_idx, "    movq rax, xmm0\n");
            append_str(asm_out, &asm_idx, "    push rax\n");
            parse_float_factor();
            append_str(asm_out, &asm_idx, "    movss xmm1, xmm0\n");
            append_str(asm_out, &asm_idx, "    pop rax\n");
            append_str(asm_out, &asm_idx, "    movq xmm0, rax\n");
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
            append_str(asm_out, &asm_idx, "    movq rax, xmm0\n");
            append_str(asm_out, &asm_idx, "    push rax\n");
            parse_float_term();
            append_str(asm_out, &asm_idx, "    movss xmm1, xmm0\n");
            append_str(asm_out, &asm_idx, "    pop rax\n");
            append_str(asm_out, &asm_idx, "    movq xmm0, rax\n");
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
    if (t.type == TOKEN_STRING) {
        char str_name[32];
        strcpy(str_name, "str_lit_");
        itoa(string_count, str_name + 8, 10);
        
        strcpy(string_table[string_count].name, str_name);
        strcpy(string_table[string_count].value, t.text);
        string_count++;
        
        append_str(asm_out, &asm_idx, "    lea rax, [");
        append_str(asm_out, &asm_idx, str_name);
        append_str(asm_out, &asm_idx, "]\n");
    }
    else if (t.type == TOKEN_NUMBER || t.type == TOKEN_CHAR_LITERAL) {
        append_str(asm_out, &asm_idx, "    mov rax, ");
        append_str(asm_out, &asm_idx, t.text);
        append_str(asm_out, &asm_idx, "\n");
    } else if (t.type == TOKEN_MINUS) {
        parse_factor();
        append_str(asm_out, &asm_idx, "    mov rbx, rax\n");
        append_str(asm_out, &asm_idx, "    mov rax, 0\n");
        append_str(asm_out, &asm_idx, "    sub rax, rbx\n");
    }
    else if (t.type == TOKEN_IDENTIFIER) {
        const char* saved_ptr = src_ptr;
        Token next = get_next_token();

        if (next.type == TOKEN_LPAREN) {
            emit_function_call(t.text);
        }
        else if (next.type == TOKEN_LBRACKET) {
            parse_expr();
            get_next_token();
            
            if (is_var_str_ptr(t.text) || is_var_base_char(t.text)) {
                if (is_var_str_ptr(t.text)) {
                    append_str(asm_out, &asm_idx, "    mov rbx, ");
                    append_var_ref(asm_out, &asm_idx, t.text);
                    append_str(asm_out, &asm_idx, "\n");
                } else {
                    append_str(asm_out, &asm_idx, "    lea rbx, ");
                    append_var_ref(asm_out, &asm_idx, t.text);
                    append_str(asm_out, &asm_idx, "\n");
                }
                append_str(asm_out, &asm_idx, "    add rbx, rax\n");
                append_str(asm_out, &asm_idx, "    movzx rax, [rbx]\n");
            } else {
                append_str(asm_out, &asm_idx, "    mov rbx, 8\n");
                append_str(asm_out, &asm_idx, "    imul rbx\n");
                if (get_var_pointer_level(t.text) > 0) {
                    append_str(asm_out, &asm_idx, "    mov rbx, ");
                } else {
                    append_str(asm_out, &asm_idx, "    lea rbx, ");
                }
                append_var_ref(asm_out, &asm_idx, t.text);
                append_str(asm_out, &asm_idx, "\n");
                append_str(asm_out, &asm_idx, "    add rax, rbx\n");
                append_str(asm_out, &asm_idx, "    mov rax, [rax]\n");
            }
        } else {
            src_ptr = saved_ptr;
            if (is_var_array(t.text)) {
                append_str(asm_out, &asm_idx, "    lea rax, ");
            } else {
                append_str(asm_out, &asm_idx, "    mov rax, ");
            }
            append_var_ref(asm_out, &asm_idx, t.text);
            append_str(asm_out, &asm_idx, "\n");
        }
    } else if (t.type == TOKEN_AMPERSAND) {
        Token var_tok = get_next_token();
        append_str(asm_out, &asm_idx, "    lea rax, ");
        append_var_ref(asm_out, &asm_idx, var_tok.text);
        append_str(asm_out, &asm_idx, "\n");
    } else if (t.type == TOKEN_STAR) {
        int star_count = 1;
        Token next = get_next_token();
        while (next.type == TOKEN_STAR) {
            star_count++;
            next = get_next_token();
        }
        Token var_tok = next;
        
        append_str(asm_out, &asm_idx, "    mov rax, ");
        append_var_ref(asm_out, &asm_idx, var_tok.text);
        append_str(asm_out, &asm_idx, "\n");
        
        int is_char_deref = is_var_base_char(var_tok.text) && (get_var_pointer_level(var_tok.text) == star_count);
        for (int i = 0; i < star_count; i++) {
            if (i == star_count - 1 && is_char_deref) {
                append_str(asm_out, &asm_idx, "    movzx rax, [rax]\n");
            } else {
                append_str(asm_out, &asm_idx, "    mov rax, [rax]\n");
            }
        }
    } else if (t.type == TOKEN_LPAREN) {
        parse_expr();
        get_next_token();
    } else if (t.type == TOKEN_NOT) {
        parse_factor();
        int lbl = if_counter++;
        char true_lbl[32], end_lbl[32];
        strcpy(true_lbl, "not_true_"); itoa(lbl, true_lbl + strlen(true_lbl), 10);
        strcpy(end_lbl, "not_end_"); itoa(lbl, end_lbl + strlen(end_lbl), 10);

        append_str(asm_out, &asm_idx, "    cmp rax, 0\n");
        append_str(asm_out, &asm_idx, "    je ");
        append_str(asm_out, &asm_idx, true_lbl);
        append_str(asm_out, &asm_idx, "\n    mov rax, 0\n    jmp ");
        append_str(asm_out, &asm_idx, end_lbl);
        append_str(asm_out, &asm_idx, "\n");
        append_str(asm_out, &asm_idx, true_lbl);
        append_str(asm_out, &asm_idx, ":\n    mov rax, 1\n");
        append_str(asm_out, &asm_idx, end_lbl);
        append_str(asm_out, &asm_idx, ":\n");
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

/*
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
*/

int is_float_ahead() {
    const char* saved = src_ptr;
    Token t = get_next_token();
    while (t.type == TOKEN_LPAREN || t.type == TOKEN_MINUS) {
        t = get_next_token();
    }
    int res = 0;
    if (t.type == TOKEN_FLOAT_NUMBER) {
        res = 1;
    } else if (t.type == TOKEN_IDENTIFIER) {
        Token next = get_next_token();
        if (next.type == TOKEN_LBRACKET) {
            if (is_var_base_float(t.text)) res = 1;
        } else if (next.type == TOKEN_LPAREN) {
            FuncInfo* fn = find_func(t.text);
            if (fn && fn->return_is_float) res = 1;
        } else {
            if (is_var_float(t.text)) res = 1;
        }
    } else if (t.type == TOKEN_STAR) {
        int stars = 1;
        Token next = get_next_token();
        while (next.type == TOKEN_STAR) {
            stars++;
            next = get_next_token();
        }
        if (next.type == TOKEN_IDENTIFIER && is_var_base_float(next.text) && get_var_pointer_level(next.text) == stars) {
            res = 1;
        }
    }
    src_ptr = saved;
    return res;
}

void parse_additive() {
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

void parse_relational() {
    if (is_float_ahead()) {
        parse_float_expr();
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        if (op.type == TOKEN_LT || op.type == TOKEN_GT || op.type == TOKEN_LE || op.type == TOKEN_GE ||
            op.type == TOKEN_EQ || op.type == TOKEN_NE) {
            append_str(asm_out, &asm_idx, "    movq rax, xmm0\n");
            append_str(asm_out, &asm_idx, "    push rax\n");
            parse_float_expr();
            append_str(asm_out, &asm_idx, "    movss xmm1, xmm0\n");
            append_str(asm_out, &asm_idx, "    pop rax\n");
            append_str(asm_out, &asm_idx, "    movq xmm0, rax\n");
            append_str(asm_out, &asm_idx, "    ucomiss xmm0, xmm1\n");

            int lbl = if_counter++;
            char true_lbl[32], end_lbl[32];
            strcpy(true_lbl, "fcmp_true_"); itoa(lbl, true_lbl + strlen(true_lbl), 10);
            strcpy(end_lbl, "fcmp_end_"); itoa(lbl, end_lbl + strlen(end_lbl), 10);

            append_str(asm_out, &asm_idx, "    ");
            append_str(asm_out, &asm_idx, get_direct_jump_float(op.type));
            append_str(asm_out, &asm_idx, " ");
            append_str(asm_out, &asm_idx, true_lbl);
            append_str(asm_out, &asm_idx, "\n    mov rax, 0\n    jmp ");
            append_str(asm_out, &asm_idx, end_lbl);
            append_str(asm_out, &asm_idx, "\n");
            append_str(asm_out, &asm_idx, true_lbl);
            append_str(asm_out, &asm_idx, ":\n    mov rax, 1\n");
            append_str(asm_out, &asm_idx, end_lbl);
            append_str(asm_out, &asm_idx, ":\n");
        } else {
            src_ptr = saved_ptr;
        }
        return;
    }

    parse_additive();
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        if (op.type == TOKEN_LT || op.type == TOKEN_GT || op.type == TOKEN_LE || op.type == TOKEN_GE) {
            append_str(asm_out, &asm_idx, "    push rax\n");
            parse_additive();
            append_str(asm_out, &asm_idx, "    mov rbx, rax\n");
            append_str(asm_out, &asm_idx, "    pop rax\n");
            append_str(asm_out, &asm_idx, "    cmp rax, rbx\n");

            int lbl = if_counter++;
            char true_lbl[32], end_lbl[32];
            strcpy(true_lbl, "cmp_true_"); itoa(lbl, true_lbl + strlen(true_lbl), 10);
            strcpy(end_lbl, "cmp_end_"); itoa(lbl, end_lbl + strlen(end_lbl), 10);

            append_str(asm_out, &asm_idx, "    ");
            append_str(asm_out, &asm_idx, get_direct_jump(op.type));
            append_str(asm_out, &asm_idx, " ");
            append_str(asm_out, &asm_idx, true_lbl);
            append_str(asm_out, &asm_idx, "\n    mov rax, 0\n    jmp ");
            append_str(asm_out, &asm_idx, end_lbl);
            append_str(asm_out, &asm_idx, "\n");
            append_str(asm_out, &asm_idx, true_lbl);
            append_str(asm_out, &asm_idx, ":\n    mov rax, 1\n");
            append_str(asm_out, &asm_idx, end_lbl);
            append_str(asm_out, &asm_idx, ":\n");
        } else {
            src_ptr = saved_ptr;
            break;
        }
    }
}

void parse_equality() {
    parse_relational();
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        if (op.type == TOKEN_EQ || op.type == TOKEN_NE) {
            append_str(asm_out, &asm_idx, "    push rax\n");
            parse_relational();
            append_str(asm_out, &asm_idx, "    mov rbx, rax\n");
            append_str(asm_out, &asm_idx, "    pop rax\n");
            append_str(asm_out, &asm_idx, "    cmp rax, rbx\n");

            int lbl = if_counter++;
            char true_lbl[32], end_lbl[32];
            strcpy(true_lbl, "eq_true_"); itoa(lbl, true_lbl + strlen(true_lbl), 10);
            strcpy(end_lbl, "eq_end_"); itoa(lbl, end_lbl + strlen(end_lbl), 10);

            append_str(asm_out, &asm_idx, "    ");
            append_str(asm_out, &asm_idx, get_direct_jump(op.type));
            append_str(asm_out, &asm_idx, " ");
            append_str(asm_out, &asm_idx, true_lbl);
            append_str(asm_out, &asm_idx, "\n    mov rax, 0\n    jmp ");
            append_str(asm_out, &asm_idx, end_lbl);
            append_str(asm_out, &asm_idx, "\n");
            append_str(asm_out, &asm_idx, true_lbl);
            append_str(asm_out, &asm_idx, ":\n    mov rax, 1\n");
            append_str(asm_out, &asm_idx, end_lbl);
            append_str(asm_out, &asm_idx, ":\n");
        } else {
            src_ptr = saved_ptr;
            break;
        }
    }
}

void parse_logical_and() {
    parse_equality();
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        if (op.type == TOKEN_AND) {
            int lbl = if_counter++;
            char false_lbl[32], end_lbl[32];
            strcpy(false_lbl, "and_false_"); itoa(lbl, false_lbl + strlen(false_lbl), 10);
            strcpy(end_lbl, "and_end_"); itoa(lbl, end_lbl + strlen(end_lbl), 10);

            append_str(asm_out, &asm_idx, "    cmp rax, 0\n");
            append_str(asm_out, &asm_idx, "    je ");
            append_str(asm_out, &asm_idx, false_lbl);
            append_str(asm_out, &asm_idx, "\n");

            parse_equality();

            append_str(asm_out, &asm_idx, "    cmp rax, 0\n");
            append_str(asm_out, &asm_idx, "    je ");
            append_str(asm_out, &asm_idx, false_lbl);
            append_str(asm_out, &asm_idx, "\n    mov rax, 1\n    jmp ");
            append_str(asm_out, &asm_idx, end_lbl);
            append_str(asm_out, &asm_idx, "\n");
            append_str(asm_out, &asm_idx, false_lbl);
            append_str(asm_out, &asm_idx, ":\n    mov rax, 0\n");
            append_str(asm_out, &asm_idx, end_lbl);
            append_str(asm_out, &asm_idx, ":\n");
        } else {
            src_ptr = saved_ptr;
            break;
        }
    }
}

void parse_logical_or() {
    parse_logical_and();
    while (1) {
        const char* saved_ptr = src_ptr;
        Token op = get_next_token();
        if (op.type == TOKEN_OR) {
            int lbl = if_counter++;
            char true_lbl[32], end_lbl[32];
            strcpy(true_lbl, "or_true_"); itoa(lbl, true_lbl + strlen(true_lbl), 10);
            strcpy(end_lbl, "or_end_"); itoa(lbl, end_lbl + strlen(end_lbl), 10);

            append_str(asm_out, &asm_idx, "    cmp rax, 0\n");
            append_str(asm_out, &asm_idx, "    jne ");
            append_str(asm_out, &asm_idx, true_lbl);
            append_str(asm_out, &asm_idx, "\n");

            parse_logical_and();

            append_str(asm_out, &asm_idx, "    cmp rax, 0\n");
            append_str(asm_out, &asm_idx, "    jne ");
            append_str(asm_out, &asm_idx, true_lbl);
            append_str(asm_out, &asm_idx, "\n    mov rax, 0\n    jmp ");
            append_str(asm_out, &asm_idx, end_lbl);
            append_str(asm_out, &asm_idx, "\n");
            append_str(asm_out, &asm_idx, true_lbl);
            append_str(asm_out, &asm_idx, ":\n    mov rax, 1\n");
            append_str(asm_out, &asm_idx, end_lbl);
            append_str(asm_out, &asm_idx, ":\n");
        } else {
            src_ptr = saved_ptr;
            break;
        }
    }
}

void parse_expr() {
    parse_logical_or();
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

/*
const char* get_inverse_jump(TokenType op) {
    if (op == TOKEN_EQ) return "jne"; 
    if (op == TOKEN_NE) return "je";  
    if (op == TOKEN_GT) return "jle"; 
    if (op == TOKEN_LT) return "jge"; 
    if (op == TOKEN_GE) return "jl";  
    if (op == TOKEN_LE) return "jg";  
    return "jne"; 
}
*/

const char* get_direct_jump(TokenType op) {
    if (op == TOKEN_EQ) return "je";
    if (op == TOKEN_NE) return "jne";
    if (op == TOKEN_GT) return "jg"; 
    if (op == TOKEN_LT) return "jl"; 
    if (op == TOKEN_GE) return "jge"; 
    if (op == TOKEN_LE) return "jle"; 
    return "je";
}

/*
const char* get_inverse_jump_float(TokenType op) {
    if (op == TOKEN_EQ) return "jne"; 
    if (op == TOKEN_NE) return "je";  
    if (op == TOKEN_GT) return "jbe"; 
    if (op == TOKEN_LT) return "jae"; 
    if (op == TOKEN_GE) return "jb";  
    if (op == TOKEN_LE) return "ja";  
    return "jne"; 
}
*/

const char* get_direct_jump_float(TokenType op) {
    if (op == TOKEN_EQ) return "je";
    if (op == TOKEN_NE) return "jne";
    if (op == TOKEN_GT) return "ja"; 
    if (op == TOKEN_LT) return "jb"; 
    if (op == TOKEN_GE) return "jae"; 
    if (op == TOKEN_LE) return "jbe"; 
    return "je";
}

/*
void emit_condition_eval(Token var1_tok, Token op_tok, Token val2_tok, 
                         const char* target_label, int jump_on_true, int is_negated) {
    int is_float = is_var_float(var1_tok.text);

    if (is_float) {
        append_str(asm_out, &asm_idx, "    movss xmm0, ");
        append_var_ref(asm_out, &asm_idx, var1_tok.text);
        append_str(asm_out, &asm_idx, "\n");

        if (val2_tok.type == TOKEN_NUMBER || val2_tok.type == TOKEN_FLOAT_NUMBER) {
            uint32_t u = float_to_u32(val2_tok.text);
            register_global_variable("tmp_float", 1, 0, 0, 0);
            append_str(asm_out, &asm_idx, "    mov rax, ");
            append_int(asm_out, &asm_idx, (int)u);
            append_str(asm_out, &asm_idx, "\n    mov [var_tmp_float], rax\n");
            append_str(asm_out, &asm_idx, "    movss xmm1, [var_tmp_float]\n");
        } else if (val2_tok.type == TOKEN_IDENTIFIER) {
            append_str(asm_out, &asm_idx, "    movss xmm1, ");
            append_var_ref(asm_out, &asm_idx, val2_tok.text);
            append_str(asm_out, &asm_idx, "\n");
        } else {
            register_global_variable("tmp_float", 1, 0, 0, 0);
            append_str(asm_out, &asm_idx, "    mov rax, 0\n    mov [var_tmp_float], rax\n");
            append_str(asm_out, &asm_idx, "    movss xmm1, [var_tmp_float]\n");
        }
        append_str(asm_out, &asm_idx, "    ucomiss xmm0, xmm1\n");
    } else {
        append_str(asm_out, &asm_idx, "    mov rdi, ");
        append_var_ref(asm_out, &asm_idx, var1_tok.text);
        append_str(asm_out, &asm_idx, "\n");

        if (val2_tok.type == TOKEN_NUMBER || val2_tok.type == TOKEN_FLOAT_NUMBER) {
            append_str(asm_out, &asm_idx, "    cmp rdi, ");
            append_str(asm_out, &asm_idx, val2_tok.text);
            append_str(asm_out, &asm_idx, "\n");
        } else if (val2_tok.type == TOKEN_IDENTIFIER) {
            append_str(asm_out, &asm_idx, "    mov rax, ");
            append_var_ref(asm_out, &asm_idx, val2_tok.text);
            append_str(asm_out, &asm_idx, "\n    cmp rdi, rax\n");
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
*/


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
        else if (str_eq(token.text, "void")) token.type = TOKEN_VOID_KW;
        else if (str_eq(token.text, "print")) token.type = TOKEN_PRINT_KW;
        else if (str_eq(token.text, "if")) token.type = TOKEN_IF_KW;
        else if (str_eq(token.text, "else")) token.type = TOKEN_ELSE_KW;
        else if (str_eq(token.text, "while")) token.type = TOKEN_WHILE_KW;
        else if (str_eq(token.text, "for")) token.type = TOKEN_FOR_KW;
		else if (str_eq(token.text, "break")) token.type = TOKEN_BREAK_KW;
        else if (str_eq(token.text, "continue")) token.type = TOKEN_CONTINUE_KW;
        else if (str_eq(token.text, "return")) token.type = TOKEN_RETURN_KW;
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
        case '+': 
            if (*src_ptr == '+') { src_ptr++; token.type = TOKEN_INC; str_cpy(token.text, "++"); }
            else { token.type = TOKEN_PLUS; }
            break;
        case '-': 
            if (*src_ptr == '-') { src_ptr++; token.type = TOKEN_DEC; str_cpy(token.text, "--"); }
            else { token.type = TOKEN_MINUS; }
            break;
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
        case ',': token.type = TOKEN_COMMA; break;
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

void parse_statement_token(Token t) {
    if (t.type == TOKEN_LBRACE) {
        Token inner = get_next_token();
        while (inner.type != TOKEN_RBRACE && inner.type != TOKEN_EOF) {
            parse_statement_token(inner);
            inner = get_next_token();
        }
    }
    else if (t.type == TOKEN_INT_KW || t.type == TOKEN_FLOAT_KW || t.type == TOKEN_CHAR_KW || t.type == TOKEN_VOID_KW) {
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

        if (op_or_semi.type == TOKEN_LBRACKET) {
            Token size_tok = get_next_token();
            int array_sz = atoi(size_tok.text);
            get_next_token();
            consume_semicolon();
            
            register_array(var_tok.text, is_float, is_char, array_sz);
        }
        else {
            register_variable(var_tok.text, is_float, is_ptr, ptr_level, is_char);
            if (op_or_semi.type == TOKEN_ASSIGN) {
                if (is_float && ptr_level == 0) {
                    parse_float_expr();
                    consume_semicolon();
                    append_str(asm_out, &asm_idx, "    movss ");
                    append_var_ref(asm_out, &asm_idx, var_tok.text);
                    append_str(asm_out, &asm_idx, ", xmm0\n");
                } else {
                    parse_expr();
                    consume_semicolon();
                    append_str(asm_out, &asm_idx, "    mov ");
                    append_var_ref(asm_out, &asm_idx, var_tok.text);
                    append_str(asm_out, &asm_idx, ", rax\n");
                }
            }
        }
    }
    else if (t.type == TOKEN_IDENTIFIER) {
        Token var_tok = t;
        Token next_tok = get_next_token();
        
        if (next_tok.type == TOKEN_LBRACKET) {
            parse_expr();
            get_next_token();
            
            if (is_var_str_ptr(var_tok.text) || is_var_base_char(var_tok.text)) {
                if (is_var_str_ptr(var_tok.text)) {
                    append_str(asm_out, &asm_idx, "    mov rbx, ");
                    append_var_ref(asm_out, &asm_idx, var_tok.text);
                    append_str(asm_out, &asm_idx, "\n");
                } else {
                    append_str(asm_out, &asm_idx, "    lea rbx, ");
                    append_var_ref(asm_out, &asm_idx, var_tok.text);
                    append_str(asm_out, &asm_idx, "\n");
                }
                append_str(asm_out, &asm_idx, "    add rbx, rax\n");
                append_str(asm_out, &asm_idx, "    push rbx\n");
                
                Token assign_tok = get_next_token();
                if (assign_tok.type == TOKEN_ASSIGN) {
                    parse_expr();
                    consume_semicolon();
                    append_str(asm_out, &asm_idx, "    pop rbx\n");
                    append_str(asm_out, &asm_idx, "    mov [rbx], al\n");
                }
            } else {
                append_str(asm_out, &asm_idx, "    mov rbx, 8\n");
                append_str(asm_out, &asm_idx, "    imul rbx\n");
                //append_str(asm_out, &asm_idx, "    lea rbx, ");
				if (get_var_pointer_level(t.text) > 0) {
                    append_str(asm_out, &asm_idx, "    mov rbx, ");
                } else {
                    append_str(asm_out, &asm_idx, "    lea rbx, ");
                }
                append_var_ref(asm_out, &asm_idx, var_tok.text);
                append_str(asm_out, &asm_idx, "\n");
                append_str(asm_out, &asm_idx, "    add rax, rbx\n");
                append_str(asm_out, &asm_idx, "    push rax\n");
                
                Token assign_tok = get_next_token();
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
        }
        else if (next_tok.type == TOKEN_ASSIGN) {
            if (is_var_float(var_tok.text)) {
                parse_float_expr();
                consume_semicolon();
                append_str(asm_out, &asm_idx, "    movss ");
                append_var_ref(asm_out, &asm_idx, var_tok.text);
                append_str(asm_out, &asm_idx, ", xmm0\n");
            } else {
                parse_expr();
                consume_semicolon();
                append_str(asm_out, &asm_idx, "    mov ");
                append_var_ref(asm_out, &asm_idx, var_tok.text);
                append_str(asm_out, &asm_idx, ", rax\n");
            }
        }
        else if (next_tok.type == TOKEN_INC || next_tok.type == TOKEN_DEC) {
            consume_semicolon();
            append_str(asm_out, &asm_idx, "    mov rax, ");
            append_var_ref(asm_out, &asm_idx, var_tok.text);
            append_str(asm_out, &asm_idx, "\n");
            
            if (next_tok.type == TOKEN_INC) append_str(asm_out, &asm_idx, "    inc rax\n");
            else append_str(asm_out, &asm_idx, "    dec rax\n");
            
            append_str(asm_out, &asm_idx, "    mov ");
            append_var_ref(asm_out, &asm_idx, var_tok.text);
            append_str(asm_out, &asm_idx, ", rax\n");
        }
        else if (next_tok.type == TOKEN_LPAREN) {
            emit_function_call(var_tok.text);
            consume_semicolon();
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
                
                append_str(asm_out, &asm_idx, "    mov rax, ");
                append_var_ref(asm_out, &asm_idx, ptr_tok.text);
                append_str(asm_out, &asm_idx, "\n");
                
                for (int i = 0; i < star_count - 1; i++) {
                    append_str(asm_out, &asm_idx, "    mov rax, [rax]\n");
                }
                append_str(asm_out, &asm_idx, "    movss [rax], xmm0\n");
            } else {
                parse_expr();
                consume_semicolon();
                
                append_str(asm_out, &asm_idx, "    push rax\n");
                append_str(asm_out, &asm_idx, "    mov rbx, ");
                append_var_ref(asm_out, &asm_idx, ptr_tok.text);
                append_str(asm_out, &asm_idx, "\n");
                
                for (int i = 0; i < star_count - 1; i++) {
                    append_str(asm_out, &asm_idx, "    mov rbx, [rbx]\n");
                }
                
                append_str(asm_out, &asm_idx, "    pop rax\n");
                append_str(asm_out, &asm_idx, "    mov [rbx], rax\n");
            }
        }
    }
    else if (t.type == TOKEN_PRINT_KW) {
        get_next_token();

        const char* saved_ptr = src_ptr;
        Token arg_tok = get_next_token();

        if (arg_tok.type == TOKEN_STRING) {
            get_next_token();
            consume_semicolon();
            append_str(asm_out, &asm_idx, "    print \"");
            append_str(asm_out, &asm_idx, arg_tok.text);
            append_str(asm_out, &asm_idx, "\"\n    newline\n");
        } 
        else {
            int is_float_expression = 0;
            int is_char_expression = 0;
            int is_str_ptr_expression = 0;

            const char* check_ptr = src_ptr;
            Token peek_next = get_next_token();
            src_ptr = check_ptr;

            if (arg_tok.type == TOKEN_CHAR_LITERAL) {
                is_char_expression = 1;
            } 
            else if (arg_tok.type == TOKEN_IDENTIFIER && peek_next.type == TOKEN_LBRACKET) {
                if (is_var_str_ptr(arg_tok.text) || is_var_base_char(arg_tok.text)) {
                    is_char_expression = 1;
                } else if (is_var_base_float(arg_tok.text)) {
                    is_float_expression = 1;
                }
            } 
            else if (arg_tok.type == TOKEN_IDENTIFIER && peek_next.type == TOKEN_LPAREN) {
                FuncInfo* fn = find_func(arg_tok.text);
                if (fn && fn->return_is_float) is_float_expression = 1;
                else if (fn && fn->return_is_char && fn->return_is_ptr) is_str_ptr_expression = 1;
            }
			else if (arg_tok.type == TOKEN_IDENTIFIER && 
                    (is_var_str_ptr(arg_tok.text) || (is_var_array(arg_tok.text) && is_var_base_char(arg_tok.text)))) {
                is_str_ptr_expression = 1;
            }	
            //else if (arg_tok.type == TOKEN_IDENTIFIER && is_var_str_ptr(arg_tok.text)) {
              //  is_str_ptr_expression = 1;
            //} 
            else if (arg_tok.type == TOKEN_IDENTIFIER && is_var_char(arg_tok.text)) {
                is_char_expression = 1;
            }
					
            else if (arg_tok.type == TOKEN_FLOAT_NUMBER) {
                is_float_expression = 1;
            } 
            else if (arg_tok.type == TOKEN_IDENTIFIER && is_var_float(arg_tok.text)) {
                is_float_expression = 1;
            }

            src_ptr = saved_ptr;

            if (is_str_ptr_expression) {
                parse_expr();
                get_next_token();
                consume_semicolon();
                append_str(asm_out, &asm_idx, "    print_str\n    newline\n");
            } else if (is_char_expression) {
                parse_expr();
                get_next_token();
                consume_semicolon();
                append_str(asm_out, &asm_idx, "    print_char\n    newline\n");
            } else if (is_float_expression) {
                parse_float_expr();
                get_next_token();
                consume_semicolon();
                append_str(asm_out, &asm_idx, "    print_xmm\n    newline\n");
            } else {
                parse_expr();
                get_next_token();
                consume_semicolon();
                append_str(asm_out, &asm_idx, "    print_rax\n    newline\n");
            }
        }
    }
    else if (t.type == TOKEN_IF_KW) {
        get_next_token(); // '('
        
        int current_if = if_counter++;
        char else_label[32], end_label[32];
        strcpy(else_label, "else_body_"); itoa(current_if, else_label + strlen(else_label), 10);
        strcpy(end_label, "if_end_"); itoa(current_if, end_label + strlen(end_label), 10);

        parse_expr();
        get_next_token(); // ')'

        append_str(asm_out, &asm_idx, "    cmp rax, 0\n");
        append_str(asm_out, &asm_idx, "    je ");
        append_str(asm_out, &asm_idx, else_label);
        append_str(asm_out, &asm_idx, "\n");

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
        get_next_token(); // '('
        
        int current_while = if_counter++; 
        char cond_label[32], end_label[32];
        strcpy(cond_label, "while_cond_"); itoa(current_while, cond_label + strlen(cond_label), 10);
        strcpy(end_label, "while_end_"); itoa(current_while, end_label + strlen(end_label), 10);
        
        append_str(asm_out, &asm_idx, cond_label);
        append_str(asm_out, &asm_idx, ":\n");
        
        parse_expr();
        get_next_token(); // ')'

        append_str(asm_out, &asm_idx, "    cmp rax, 0\n");
        append_str(asm_out, &asm_idx, "    je ");
        append_str(asm_out, &asm_idx, end_label);
        append_str(asm_out, &asm_idx, "\n");

        strcpy(loop_stack[loop_depth].break_label, end_label);
        strcpy(loop_stack[loop_depth].continue_label, cond_label);
        loop_depth++;
        
        Token body_tok = get_next_token();
        parse_statement_token(body_tok);

        loop_depth--;
        
        append_str(asm_out, &asm_idx, "    jmp ");
        append_str(asm_out, &asm_idx, cond_label);
        append_str(asm_out, &asm_idx, "\n");
        
        append_str(asm_out, &asm_idx, end_label);
        append_str(asm_out, &asm_idx, ":\n");
    }
    else if (t.type == TOKEN_FOR_KW) {
        get_next_token(); // '('

        Token init_tok = get_next_token();
        if (init_tok.type != TOKEN_SEMICOLON) {
            parse_statement_token(init_tok);
        }

        int current_for = if_counter++; 
        char cond_label[32], step_label[32], end_label[32];
        strcpy(cond_label, "for_cond_"); itoa(current_for, cond_label + strlen(cond_label), 10);
        strcpy(step_label, "for_step_"); itoa(current_for, step_label + strlen(step_label), 10);
        strcpy(end_label, "for_end_"); itoa(current_for, end_label + strlen(end_label), 10);
        
        append_str(asm_out, &asm_idx, cond_label);
        append_str(asm_out, &asm_idx, ":\n");
        
        const char* cond_check_ptr = src_ptr;
        Token cond_peek = get_next_token();
        if (cond_peek.type != TOKEN_SEMICOLON) {
            src_ptr = cond_check_ptr;
            parse_expr();
            consume_semicolon();
            append_str(asm_out, &asm_idx, "    cmp rax, 0\n");
            append_str(asm_out, &asm_idx, "    je ");
            append_str(asm_out, &asm_idx, end_label);
            append_str(asm_out, &asm_idx, "\n");
        }
        
        const char* increment_ptr = src_ptr; 
        
        int paren_count = 1; 
        while (paren_count > 0) {
            Token skip = get_next_token();
            if (skip.type == TOKEN_LPAREN) paren_count++;
            if (skip.type == TOKEN_RPAREN) paren_count--;
            if (skip.type == TOKEN_EOF) break;
        }

        strcpy(loop_stack[loop_depth].break_label, end_label);
        strcpy(loop_stack[loop_depth].continue_label, step_label);
        loop_depth++;
        
        Token body_tok = get_next_token();
        parse_statement_token(body_tok);

        loop_depth--;
        
        const char* after_body_ptr = src_ptr; 
        src_ptr = increment_ptr;              

        append_str(asm_out, &asm_idx, step_label);
        append_str(asm_out, &asm_idx, ":\n");

        Token tok1 = get_next_token();
        if (tok1.type != TOKEN_RPAREN) {
            Token tok2 = get_next_token();

            if (tok1.type == TOKEN_INC || tok1.type == TOKEN_DEC) {
                append_str(asm_out, &asm_idx, "    mov rax, ");
                append_var_ref(asm_out, &asm_idx, tok2.text);
                append_str(asm_out, &asm_idx, "\n");
                
                if (tok1.type == TOKEN_INC) append_str(asm_out, &asm_idx, "    inc rax\n");
                else append_str(asm_out, &asm_idx, "    dec rax\n");
                
                append_str(asm_out, &asm_idx, "    mov ");
                append_var_ref(asm_out, &asm_idx, tok2.text);
                append_str(asm_out, &asm_idx, ", rax\n");
            }
            else if (tok2.type == TOKEN_INC || tok2.type == TOKEN_DEC) {
                append_str(asm_out, &asm_idx, "    mov rax, ");
                append_var_ref(asm_out, &asm_idx, tok1.text);
                append_str(asm_out, &asm_idx, "\n");
                
                if (tok2.type == TOKEN_INC) append_str(asm_out, &asm_idx, "    inc rax\n");
                else append_str(asm_out, &asm_idx, "    dec rax\n");
                
                append_str(asm_out, &asm_idx, "    mov ");
                append_var_ref(asm_out, &asm_idx, tok1.text);
                append_str(asm_out, &asm_idx, ", rax\n");
            }
            else if (tok2.type == TOKEN_ASSIGN) {
                if (is_var_float(tok1.text)) {
                    parse_float_expr(); 
                    append_str(asm_out, &asm_idx, "    movss ");
                    append_var_ref(asm_out, &asm_idx, tok1.text);
                    append_str(asm_out, &asm_idx, ", xmm0\n");
                } else {
                    parse_expr(); 
                    append_str(asm_out, &asm_idx, "    mov ");
                    append_var_ref(asm_out, &asm_idx, tok1.text);
                    append_str(asm_out, &asm_idx, ", rax\n");
                }
            }
        }

        src_ptr = after_body_ptr;

        append_str(asm_out, &asm_idx, "    jmp ");
        append_str(asm_out, &asm_idx, cond_label);
        append_str(asm_out, &asm_idx, "\n");
        
        append_str(asm_out, &asm_idx, end_label);
        append_str(asm_out, &asm_idx, ":\n");
    }
    else if (t.type == TOKEN_BREAK_KW) {
        consume_semicolon();
        if (loop_depth > 0) {
            append_str(asm_out, &asm_idx, "    jmp ");
            append_str(asm_out, &asm_idx, loop_stack[loop_depth - 1].break_label);
            append_str(asm_out, &asm_idx, "\n");
        }
    }
    else if (t.type == TOKEN_CONTINUE_KW) {
        consume_semicolon();
        if (loop_depth > 0) {
            append_str(asm_out, &asm_idx, "    jmp ");
            append_str(asm_out, &asm_idx, loop_stack[loop_depth - 1].continue_label);
            append_str(asm_out, &asm_idx, "\n");
        }
    }
    else if (t.type == TOKEN_RETURN_KW) {
        const char* check_ptr = src_ptr;
        Token next_t = get_next_token();

        if (next_t.type != TOKEN_SEMICOLON) {
            src_ptr = check_ptr;
            FuncInfo* cur_fn = find_func(current_function);
            if (cur_fn && cur_fn->return_is_float) parse_float_expr();
            else parse_expr();
            consume_semicolon();
        }

        if (str_eq(current_function, "main")) {
            append_str(asm_out, &asm_idx, "    exit\n");
        } else {
            append_str(asm_out, &asm_idx, "    mov rsp, rbp\n");
            append_str(asm_out, &asm_idx, "    pop rbp\n");
            append_str(asm_out, &asm_idx, "    ret\n");
        }
    }
}

FuncInfo* find_func(const char* name) {
    for (int i = 0; i < func_count; i++) {
        if (str_eq(func_table[i].name, name)) return &func_table[i];
    }
    return 0;
}

void emit_function_call(const char* func_name) {
    FuncInfo* fn = find_func(func_name);
    int arg_count = 0;
    int arg_is_float[8] = {0};

    const char* check_ptr = src_ptr;
    Token empty_chk = get_next_token();
    if (empty_chk.type != TOKEN_RPAREN) {
        src_ptr = check_ptr;
        while (1) {
            int is_flt = 0;
            if (fn && arg_count < fn->param_count) {
                is_flt = fn->param_is_float[arg_count];
            } else {
                const char* peek_p = src_ptr;
                Token peek_t = get_next_token();
                src_ptr = peek_p;
                if (peek_t.type == TOKEN_FLOAT_NUMBER || 
                   (peek_t.type == TOKEN_IDENTIFIER && is_var_float(peek_t.text))) {
                    is_flt = 1;
                }
            }

            arg_is_float[arg_count] = is_flt;

            if (is_flt) {
                parse_float_expr();
                append_str(asm_out, &asm_idx, "    movq rax, xmm0\n");
                append_str(asm_out, &asm_idx, "    push rax\n");
            } else {
                parse_expr();
                append_str(asm_out, &asm_idx, "    push rax\n");
            }
            arg_count++;

            Token sep = get_next_token();
            if (sep.type == TOKEN_RPAREN) break;
            if (sep.type != TOKEN_COMMA) break;
        }
    }

    int target_reg_idx[8];
    int int_cnt = 0, xmm_cnt = 0;
    for (int i = 0; i < arg_count; i++) {
        if (arg_is_float[i]) target_reg_idx[i] = xmm_cnt++;
        else target_reg_idx[i] = int_cnt++;
    }

    const char* int_regs[] = {"rdi", "rsi", "rdx", "rcx"};
    const char* xmm_regs[] = {"xmm0", "xmm1", "xmm2", "xmm3"};

    for (int i = arg_count - 1; i >= 0; i--) {
        int r = target_reg_idx[i];
        if (arg_is_float[i]) {
            append_str(asm_out, &asm_idx, "    pop rax\n");
            if (r < 4) {
                append_str(asm_out, &asm_idx, "    movq ");
                append_str(asm_out, &asm_idx, xmm_regs[r]);
                append_str(asm_out, &asm_idx, ", rax\n");
            }
        } else {
            if (r < 4) {
                append_str(asm_out, &asm_idx, "    pop ");
                append_str(asm_out, &asm_idx, int_regs[r]);
                append_str(asm_out, &asm_idx, "\n");
            } else {
                append_str(asm_out, &asm_idx, "    pop rax\n");
            }
        }
    }

    append_str(asm_out, &asm_idx, "    call ");
    append_str(asm_out, &asm_idx, func_name);
    append_str(asm_out, &asm_idx, "\n");
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

    append_str(asm_out, &asm_idx, "    jmp main\n");

    Token t = get_next_token();
    while (t.type != TOKEN_EOF) {
        if (t.type == TOKEN_INT_KW || t.type == TOKEN_FLOAT_KW || t.type == TOKEN_CHAR_KW || t.type == TOKEN_VOID_KW) {
            int ret_is_void  = (t.type == TOKEN_VOID_KW);
            int ret_is_float = (t.type == TOKEN_FLOAT_KW);
            int ret_is_char  = (t.type == TOKEN_CHAR_KW);
            int ret_is_ptr   = 0;
			int ptr_level    = 0;

            Token name_tok = get_next_token();
            while (name_tok.type == TOKEN_STAR) {
                ret_is_ptr = 1;
                ret_is_void = 0;
				ptr_level++;
                name_tok = get_next_token();
            }

            Token next_tok = get_next_token();

            if (next_tok.type == TOKEN_LPAREN) {
                strcpy(current_function, name_tok.text);

                // Resetăm cadrul local de stivă pentru funcția curentă
                local_var_count = 0;
                current_stack_offset = 0;

                FuncInfo* fn = find_func(name_tok.text);
                if (!fn) {
                    fn = &func_table[func_count++];
                    strcpy(fn->name, name_tok.text);
                }
                fn->return_is_void  = ret_is_void;
                fn->return_is_float = ret_is_float;
                fn->return_is_char  = ret_is_char;
                fn->return_is_ptr   = ret_is_ptr;

                char temp_params[8][32];
                int param_is_float[8] = {0};
                int param_count = 0;

                Token p = get_next_token();
                while (p.type != TOKEN_RPAREN && p.type != TOKEN_EOF) {
                    if (p.type == TOKEN_VOID_KW) {
                        const char* peek_p = src_ptr;
                        Token after_void = get_next_token();
                        if (after_void.type == TOKEN_STAR) {
                            int p_lvl = 1;
                            Token p_next = get_next_token();
                            while (p_next.type == TOKEN_STAR) {
                                p_lvl++;
                                p_next = get_next_token();
                            }
                            strcpy(temp_params[param_count], p_next.text);
                            param_is_float[param_count] = 0;
                            fn->param_is_float[param_count] = 0;
                            register_variable(p_next.text, 0, 1, p_lvl, 0);
                            param_count++;
                        } else {
                            src_ptr = peek_p;
                        }
                    }
                    else if (p.type == TOKEN_INT_KW || p.type == TOKEN_CHAR_KW || p.type == TOKEN_FLOAT_KW) {
                        int p_float = (p.type == TOKEN_FLOAT_KW);
                        int p_char  = (p.type == TOKEN_CHAR_KW);
                        int p_ptr   = 0;
                        int p_lvl   = 0;

                        Token p_next = get_next_token();
                        while (p_next.type == TOKEN_STAR) {
                            p_ptr = 1;
                            p_lvl++;
                            p_next = get_next_token();
                        }

                        strcpy(temp_params[param_count], p_next.text);
                        param_is_float[param_count] = p_float;
                        fn->param_is_float[param_count] = p_float;

                        register_variable(p_next.text, p_float, p_ptr, p_lvl, p_char);
                        param_count++;
                    }
                    p = get_next_token();
                }
                fn->param_count = param_count;

                Token brace = get_next_token();
                if (brace.type == TOKEN_SEMICOLON) {
                    t = get_next_token();
                    continue;
                } else if (brace.type == TOKEN_LBRACE) {
                    append_str(asm_out, &asm_idx, name_tok.text);
                    append_str(asm_out, &asm_idx, ":\n");

                    // Prolog Stack Frame
                    append_str(asm_out, &asm_idx, "    push rbp\n");
                    append_str(asm_out, &asm_idx, "    mov rbp, rsp\n");
                    append_str(asm_out, &asm_idx, "    sub rsp, 256\n");

                    const char* int_regs[] = {"rdi", "rsi", "rdx", "rcx"};
                    const char* xmm_regs[] = {"xmm0", "xmm1", "xmm2", "xmm3"};
                    int int_idx = 0, xmm_idx = 0;

                    for (int i = 0; i < param_count; i++) {
                        if (param_is_float[i]) {
                            if (xmm_idx < 4) {
                                append_str(asm_out, &asm_idx, "    movss ");
                                append_var_ref(asm_out, &asm_idx, temp_params[i]);
                                append_str(asm_out, &asm_idx, ", ");
                                append_str(asm_out, &asm_idx, xmm_regs[xmm_idx++]);
                                append_str(asm_out, &asm_idx, "\n");
                            }
                        } else {
                            if (int_idx < 4) {
                                append_str(asm_out, &asm_idx, "    mov ");
                                append_var_ref(asm_out, &asm_idx, temp_params[i]);
                                append_str(asm_out, &asm_idx, ", ");
                                append_str(asm_out, &asm_idx, int_regs[int_idx++]);
                                append_str(asm_out, &asm_idx, "\n");
                            }
                        }
                    }

                    Token inner = get_next_token();
                    while (inner.type != TOKEN_RBRACE && inner.type != TOKEN_EOF) {
                        parse_statement_token(inner);
                        inner = get_next_token();
                    }

                    if (str_eq(name_tok.text, "main")) {
                        append_str(asm_out, &asm_idx, "    exit\n");
                    } else {
                        append_str(asm_out, &asm_idx, "    mov rsp, rbp\n");
                        append_str(asm_out, &asm_idx, "    pop rbp\n");
                        append_str(asm_out, &asm_idx, "    ret\n");
                    }
                }
            }
			else if (next_tok.type == TOKEN_LBRACKET) {
                Token size_tok = get_next_token();
                int array_sz = atoi(size_tok.text);
                get_next_token(); // Consumă ']'
                consume_semicolon();
                register_global_array(name_tok.text, ret_is_float, ret_is_char, array_sz);
            }else if (next_tok.type == TOKEN_ASSIGN || next_tok.type == TOKEN_SEMICOLON) {
                int has_init = 0;
                int init_val = 0;
                if (next_tok.type == TOKEN_ASSIGN) {
                    has_init = 1;
                    int sign = 1;
                    Token val_tok = get_next_token();
                    if (val_tok.type == TOKEN_MINUS) {
                        sign = -1;
                        val_tok = get_next_token();
                    }
                    if (ret_is_float && ptr_level == 0) {
                        uint32_t u = float_to_u32(val_tok.text);
                        if (sign == -1) u ^= 0x80000000U;
                        init_val = (int)u;
                    } else {
                        init_val = sign * atoi(val_tok.text);
                    }
                    consume_semicolon();
                }
                register_global_variable_init(name_tok.text, ret_is_float, ret_is_ptr, ptr_level, ret_is_char, has_init, init_val);
            }
        }
        
        t = get_next_token();
    }

    // Secțiunea .data conține doar variabilele globale (ex. tmp_float) și literalii string
    for (int i = 0; i < global_var_count; i++) {
        append_str(asm_out, &asm_idx, "var_");
        append_str(asm_out, &asm_idx, global_var_table[i].name);
        append_str(asm_out, &asm_idx, ": data ");
        append_int(asm_out, &asm_idx, global_var_table[i].has_init ? global_var_table[i].init_val : 0);
        append_str(asm_out, &asm_idx, "\n");
        
        if (global_var_table[i].is_array) {
            for (int k = 1; k < global_var_table[i].array_size; k++) {
                append_str(asm_out, &asm_idx, "    data 0\n");
            }
        }
    }
    
    for (int i = 0; i < string_count; i++) {
        append_str(asm_out, &asm_idx, string_table[i].name);
        append_str(asm_out, &asm_idx, ": data \"");
        append_str(asm_out, &asm_idx, string_table[i].value);
        append_str(asm_out, &asm_idx, "\"\n    data 0\n"); 
    }

    asm_out[asm_idx] = '\0';

    nano_write_file(argv[2], (const uint8_t*)asm_out, asm_idx);
    nano_print("[nanoc] Generare cod ASM (.s) finalizata cu succes!\n");
    return 0;
}