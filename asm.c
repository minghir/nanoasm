#include "nano_libc.h"
#include "elf.h"

// Structură pentru etichete
typedef struct {
    char name[32];
    int offset;
} Label;

// Structură pentru o instrucțiune parsată
typedef struct {
    char mnemonic[32];
    char op1[32];
    char op2[32];
} Instruction;

static int g_generate_elf = 0;

// Algoritmul complet itoa (Integer to ASCII) în cod mașină x86_64 pentru Linux ELF
static const uint8_t itoa_linux[] = {
    0x50, 0x51, 0x52, 0x57, 0x56, 0x41, 0x50, 0x41, 0x51, // push rax, rcx, rdx, rdi, rsi, r8, r9
    0x49, 0x89, 0xE0, 0xB9, 0x00, 0x00, 0x00, 0x00,       // mov r8, rsp; mov ecx, 0
    0x41, 0xB9, 0x0A, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC0, // mov r9d, 10; test rax, rax
    0x75, 0x0B, 0x48, 0xFF, 0xCC, 0xC6, 0x04, 0x24, 0x30, // jnz +11; dec rsp; mov byte ptr [rsp], '0'
    0xFF, 0xC1, 0xEB, 0x18, 0x48, 0x85, 0xC0, 0x74, 0x13, // inc ecx; jmp +24; test rax, rax; jz +19
    0x48, 0x31, 0xD2, 0x49, 0xF7, 0xF1, 0x80, 0xC2, 0x30, // xor rdx, rdx; div r9; add dl, '0'
    0x48, 0xFF, 0xCC, 0x88, 0x14, 0x24, 0xFF, 0xC1, 0xEB, // dec rsp; mov [rsp], dl; inc ecx; jmp -24
    0xE8, 0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00, 0x48, // mov rax, 1 (sys_write)
    0xC7, 0xC7, 0x01, 0x00, 0x00, 0x00, 0x48, 0x89, 0xE6, // mov rdi, 1 (stdout); mov rsi, rsp
    0x48, 0x89, 0xCA, 0x0F, 0x05, 0x4C, 0x89, 0xC4, 0x41, // mov rdx, rcx; syscall; mov rsp, r8
    0x59, 0x41, 0x58, 0x5E, 0x5F, 0x5A, 0x59, 0x58         // pop r9, r8, rsi, rdi, rdx, rcx, rax
};

float nano_parse_float(const char* str) {
    float result = 0.0f;
    float sign = 1.0f;
    int i = 0;

    if (str[0] == '-') {
        sign = -1.0f;
        i++;
    } else if (str[0] == '+') {
        i++;
    }

    // Partea întreagă
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10.0f + (str[i] - '0');
        i++;
    }

    // Partea fracționară
    if (str[i] == '.') {
        i++;
        float fraction = 1.0f;
        while (str[i] >= '0' && str[i] <= '9') {
            fraction /= 10.0f;
            result += (str[i] - '0') * fraction;
            i++;
        }
    }

    return result * sign;
}

// Funcție avansată pentru curățarea și sanitizarea liniei
void clean_line(const char* raw, char* dest) {
    int i = 0, j = 0;

    // 1. Trecem peste BOM-ul UTF-8 (EF BB BF) dacă apare la începutul fișierului
    if ((unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF) {
        i += 3;
    }

    // 2. Trecem peste spații, tab-uri, \r sau alte caractere de control de la început
    while (raw[i] != '\0' && (raw[i] == ' ' || raw[i] == '\t' || raw[i] == '\r' || (unsigned char)raw[i] < 32)) {
        if (raw[i] == '\n') break;
        i++;
    }

    // 3. Copiem conținutul util până la comentariu (';'), sfârșit de linie sau de șir
    while (raw[i] != '\0' && raw[i] != ';' && raw[i] != '\r' && raw[i] != '\n') {
        dest[j++] = raw[i++];
    }

    // 4. Tăiem spațiile sau caracterele de control rămase la sfârșit
    while (j > 0 && (dest[j-1] == ' ' || dest[j-1] == '\t' || dest[j-1] == '\r' || (unsigned char)dest[j-1] < 32)) {
        j--;
    }
    
    dest[j] = '\0';
}

// Tokenizer-ul
int parse_instruction(const char* line, Instruction* inst) {
    inst->mnemonic[0] = '\0';
    inst->op1[0] = '\0';
    inst->op2[0] = '\0';

    int i = 0, j = 0;

    // 1. Mnemonică
    while (line[i] != '\0' && line[i] != ' ' && line[i] != '\t' && line[i] != ',') {
        if (j < 31) inst->mnemonic[j++] = line[i];
        i++;
    }
    inst->mnemonic[j] = '\0';

    if (inst->mnemonic[0] == '\0') return 0;

    while (line[i] == ' ' || line[i] == '\t') i++;
    if (line[i] == '\0') return 1;

    // TRATARE SPECIALĂ PENTRU PRINT
    if (strcmp(inst->mnemonic, "print") == 0) {
        strcpy(inst->op1, &line[i]);
        return 2;
    }

    // 2. Operand 1
    j = 0;
    while (line[i] != '\0' && line[i] != ',' && line[i] != ' ' && line[i] != '\t') {
        if (j < 31) inst->op1[j++] = line[i];
        i++;
    }
    inst->op1[j] = '\0';

    while (line[i] == ' ' || line[i] == '\t' || line[i] == ',') i++;
    if (line[i] == '\0') return 2;

    // 3. Operand 2
    j = 0;
    while (line[i] != '\0') {
        if (j < 31) inst->op2[j++] = line[i];
        i++;
    }
    while (j > 0 && (inst->op2[j-1] == ' ' || inst->op2[j-1] == '\t' || inst->op2[j-1] == '\r')) {
        j--;
    }
    inst->op2[j] = '\0';

    return 3;
}

// Funcție centralizată unică pentru dimensiune și generare binar cu loguri
int process_instruction(Instruction* inst, uint8_t* bin_buffer, int* bin_idx, Label* labels, int label_count) {
    
    // --- ETICHETE ---
    int len = strlen(inst->mnemonic);
    if (len > 0 && inst->mnemonic[len - 1] == ':') {
        return 0;
    }

    // --- PRINT "text" ---
    if (strcmp(inst->mnemonic, "print") == 0) {
        char msg[128];
        int m_idx = 0;
        char* p = inst->op1;
        if (*p == '"') p++;

        while (*p != '"' && *p != '\0' && m_idx < 127) {
            if (*p == '\\' && *(p + 1) == 'n') {
                msg[m_idx++] = '\n';
                p += 2;
            } else {
                msg[m_idx++] = *p++;
            }
        }
        msg[m_idx] = '\0';
        int final_str_len = m_idx;

        int total_size = g_generate_elf ? (32 + final_str_len) : (7 + 7 + 2 + 2 + final_str_len + 1);

        if (bin_buffer) {
            if (g_generate_elf) {
                // 1. mov rax, 1 (sys_write)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x01; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;

                // 2. mov rdi, 1 (stdout)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC7;
                bin_buffer[(*bin_idx)++] = 0x01; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;

                // 3. lea rsi, [rip + disp32]
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x8D; bin_buffer[(*bin_idx)++] = 0x35;
                int disp_pos = *bin_idx;
                *bin_idx += 4;

                // 4. mov rdx, final_str_len
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC2;
                bin_buffer[(*bin_idx)++] = (uint8_t)(final_str_len & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((final_str_len >> 8) & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((final_str_len >> 16) & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((final_str_len >> 24) & 0xFF);

                // 5. syscall
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x05;

                // 6. jmp peste șir
                bin_buffer[(*bin_idx)++] = 0xEB;
                int jmp_rel_pos = (*bin_idx)++;

                int string_offset = *bin_idx;
                for (int m = 0; m < final_str_len; m++) {
                    bin_buffer[(*bin_idx)++] = (uint8_t)msg[m];
                }

                int32_t disp32 = string_offset - (disp_pos + 4);
                bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                int jmp_target = *bin_idx;
                int jmp_from = jmp_rel_pos + 1;
                bin_buffer[jmp_rel_pos] = (uint8_t)(jmp_target - jmp_from);

            } else {
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x01; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;

                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x8D; bin_buffer[(*bin_idx)++] = 0x3D;
                int disp_pos = *bin_idx;
                *bin_idx += 4;

                bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;

                bin_buffer[(*bin_idx)++] = 0xEB;
                int jmp_rel_pos = (*bin_idx)++;

                int string_offset = *bin_idx;
                for (int m = 0; m <= m_idx; m++) {
                    bin_buffer[(*bin_idx)++] = (uint8_t)msg[m];
                }

                int32_t disp32 = string_offset - (disp_pos + 4);
                bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                int jmp_target = *bin_idx;
                int jmp_from = jmp_rel_pos + 1;
                bin_buffer[jmp_rel_pos] = (uint8_t)(jmp_target - jmp_from);
            }
            nano_print("  [ASM] print -> opcod generat cu succes\n");
        }
        return total_size;
    }

    // --- PUSH & POP ---
    if (strcmp(inst->mnemonic, "push") == 0) {
        uint8_t opcode = 0x50; // default rax
        if (strcmp(inst->op1, "rdi") == 0) opcode = 0x57;
        else if (strcmp(inst->op1, "rsi") == 0) opcode = 0x56;
        else if (strcmp(inst->op1, "rdx") == 0) opcode = 0x52;

        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = opcode;
            nano_print("  [ASM] push reg -> opcod generat\n");
        }
        return 1;
    }

    if (strcmp(inst->mnemonic, "pop") == 0) {
        uint8_t opcode = 0x58; // default rax
        if (strcmp(inst->op1, "rdi") == 0) opcode = 0x5F;
        else if (strcmp(inst->op1, "rsi") == 0) opcode = 0x5E;
        else if (strcmp(inst->op1, "rdx") == 0) opcode = 0x5A;

        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = opcode;
            nano_print("  [ASM] pop reg -> opcod generat\n");
        }
        return 1;
    }

    // --- RET ---
    if (strcmp(inst->mnemonic, "ret") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0xC3;
            nano_print("  [ASM] ret -> opcod generat\n");
        }
        return 1;
    }

    // --- INT 0X80 ---
    if (strcmp(inst->mnemonic, "int") == 0 && strcmp(inst->op1, "0x80") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0xCD;
            bin_buffer[(*bin_idx)++] = 0x80;
            nano_print("  [ASM] int 0x80 -> opcod generat\n");
        }
        return 2;
    }

    // --- NEWLINE ---
    if (strcmp(inst->mnemonic, "newline") == 0) {
        if (bin_buffer) {
            if (g_generate_elf) {
                // 1. mov rax, 1 (sys_write)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x01; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                // 2. mov rdi, 1 (stdout)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC7;
                bin_buffer[(*bin_idx)++] = 0x01; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                // 3. lea rsi, [rip + disp32]
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x8D; bin_buffer[(*bin_idx)++] = 0x35;
                int disp_pos = *bin_idx;
                *bin_idx += 4;
                // 4. mov rdx, 1 (lungime 1 octet)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC2;
                bin_buffer[(*bin_idx)++] = 0x01; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                // 5. syscall
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x05;

                // 6. Săritură jmp peste octetul '\n'
                bin_buffer[(*bin_idx)++] = 0xEB;
                int jmp_rel_pos = (*bin_idx)++;
                int string_offset = *bin_idx;
                
                bin_buffer[(*bin_idx)++] = '\n';

                int32_t disp32 = string_offset - (disp_pos + 4);
                bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                int jmp_target = *bin_idx;
                int jmp_from = jmp_rel_pos + 1;
                bin_buffer[jmp_rel_pos] = (uint8_t)(jmp_target - jmp_from);
            } else {
                bin_buffer[(*bin_idx)++] = 0x50;
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x1A; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;
                bin_buffer[(*bin_idx)++] = 0x58;
            }
            nano_print("  [ASM] newline -> opcod generat\n");
        }
        return g_generate_elf ? 33 : 11;
    }

    // --- DEC RDI ---
    if (strcmp(inst->mnemonic, "dec") == 0 && strcmp(inst->op1, "rdi") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0xFF;
            bin_buffer[(*bin_idx)++] = 0xCF;
            nano_print("  [ASM] dec rdi -> opcod generat\n");
        }
        return 3;
    }

    // --- MOV ---
    if (strcmp(inst->mnemonic, "mov") == 0) {
        int src_reg_id = -1;
        if (strcmp(inst->op2, "rax") == 0) src_reg_id = 0;
        else if (strcmp(inst->op2, "rcx") == 0) src_reg_id = 1;
        else if (strcmp(inst->op2, "rdx") == 0) src_reg_id = 2;
        else if (strcmp(inst->op2, "rbx") == 0) src_reg_id = 3;
        else if (strcmp(inst->op2, "rsi") == 0) src_reg_id = 6;
        else if (strcmp(inst->op2, "rdi") == 0) src_reg_id = 7;

        int dst_reg_id = -1;
        if (strcmp(inst->op1, "rax") == 0) dst_reg_id = 0;
        else if (strcmp(inst->op1, "rcx") == 0) dst_reg_id = 1;
        else if (strcmp(inst->op1, "rdx") == 0) dst_reg_id = 2;
        else if (strcmp(inst->op1, "rbx") == 0) dst_reg_id = 3;
        else if (strcmp(inst->op1, "rsi") == 0) dst_reg_id = 6;
        else if (strcmp(inst->op1, "rdi") == 0) dst_reg_id = 7;

        if (dst_reg_id != -1 && src_reg_id != -1) {
            if (bin_buffer) {
                bin_buffer[(*bin_idx)++] = 0x48;
                bin_buffer[(*bin_idx)++] = 0x89;
                bin_buffer[(*bin_idx)++] = (uint8_t)(0xC0 | (src_reg_id << 3) | dst_reg_id);
                nano_print("  [ASM] mov reg, reg -> opcod generat\n");
            }
            return 3;
        }

        if (strcmp(inst->op1, "rax") == 0 || strcmp(inst->op1, "rdi") == 0 || 
            strcmp(inst->op1, "rsi") == 0 || strcmp(inst->op1, "rdx") == 0 ||
            strcmp(inst->op1, "rbx") == 0) {
            
            if (inst->op2[0] != '[') {
                int val = atoi(inst->op2);
                uint8_t reg_code = 0xC0; // rax
                if (strcmp(inst->op1, "rdi") == 0) reg_code = 0xC7;
                else if (strcmp(inst->op1, "rsi") == 0) reg_code = 0xC6;
                else if (strcmp(inst->op1, "rdx") == 0) reg_code = 0xC2;
                else if (strcmp(inst->op1, "rbx") == 0) reg_code = 0xC3;

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0x48;
                    bin_buffer[(*bin_idx)++] = 0xC7;
                    bin_buffer[(*bin_idx)++] = reg_code;
                    bin_buffer[(*bin_idx)++] = (uint8_t)(val & 0xFF);
                    bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 8) & 0xFF);
                    bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 16) & 0xFF);
                    bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 24) & 0xFF);
                    nano_print("  [ASM] mov reg, val -> opcod generat\n");
                }
                return 7;
            }
        }

        if (inst->op2[0] == '[') {
            char inner[32];
            int m_i = 0, m_j = 0;
            while (inst->op2[m_i] != '\0') {
                if (inst->op2[m_i] != '[' && inst->op2[m_i] != ']') {
                    inner[m_j++] = inst->op2[m_i];
                }
                m_i++;
            }
            inner[m_j] = '\0';

            int src_reg = -1;
            if (strcmp(inner, "rax") == 0) src_reg = 0;
            else if (strcmp(inner, "rbx") == 0) src_reg = 3;
            else if (strcmp(inner, "rdi") == 0) src_reg = 7;
            else if (strcmp(inner, "rsi") == 0) src_reg = 6;
            else if (strcmp(inner, "rdx") == 0) src_reg = 2;

            if (src_reg != -1) {
                int dest_reg = -1;
                if (strcmp(inst->op1, "rax") == 0) dest_reg = 0;
                else if (strcmp(inst->op1, "rbx") == 0) dest_reg = 3;
                else if (strcmp(inst->op1, "rdi") == 0) dest_reg = 7;
                else if (strcmp(inst->op1, "rsi") == 0) dest_reg = 6;
                else if (strcmp(inst->op1, "rdx") == 0) dest_reg = 2;

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0x48;
                    bin_buffer[(*bin_idx)++] = 0x8B;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((dest_reg << 3) | src_reg);
                    nano_print("  [ASM] mov reg, [reg] -> opcod generat\n");
                }
                return 3;
            } else {
                int target_offset = -1;
                for (int l = 0; l < label_count; l++) {
                    if (strcmp(labels[l].name, inner) == 0) {
                        target_offset = labels[l].offset;
                        break;
                    }
                }

                int reg_code = 0;
                if (strcmp(inst->op1, "rax") == 0) reg_code = 0;
                else if (strcmp(inst->op1, "rbx") == 0) reg_code = 3;
                else if (strcmp(inst->op1, "rdi") == 0) reg_code = 7;
                else if (strcmp(inst->op1, "rsi") == 0) reg_code = 6;
                else if (strcmp(inst->op1, "rdx") == 0) reg_code = 2;

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0x48;
                    bin_buffer[(*bin_idx)++] = 0x8B;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((reg_code << 3) | 0x05);

                    int disp_pos = *bin_idx;
                    *bin_idx += 4;
                    int32_t disp32 = target_offset - (disp_pos + 4);

                    bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                    bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                    bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                    bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                    nano_print("  [ASM] mov reg, [var] (RIP-relative) -> opcod generat\n");
                }
                return 7;
            }
        }

        if (inst->op1[0] == '[') {
            char inner[32];
            int m_i = 0, m_j = 0;
            while (inst->op1[m_i] != '\0') {
                if (inst->op1[m_i] != '[' && inst->op1[m_i] != ']') {
                    inner[m_j++] = inst->op1[m_i];
                }
                m_i++;
            }
            inner[m_j] = '\0';

            int dest_reg = -1;
            if (strcmp(inner, "rax") == 0) dest_reg = 0;
            else if (strcmp(inner, "rbx") == 0) dest_reg = 3;
            else if (strcmp(inner, "rdi") == 0) dest_reg = 7;
            else if (strcmp(inner, "rsi") == 0) dest_reg = 6;
            else if (strcmp(inner, "rdx") == 0) dest_reg = 2;

            if (dest_reg != -1) {
                int src_reg = -1;
                if (strcmp(inst->op2, "rax") == 0) src_reg = 0;
                else if (strcmp(inst->op2, "rbx") == 0) src_reg = 3;
                else if (strcmp(inst->op2, "rdi") == 0) src_reg = 7;
                else if (strcmp(inst->op2, "rsi") == 0) src_reg = 6;
                else if (strcmp(inst->op2, "rdx") == 0) src_reg = 2;

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0x48;
                    bin_buffer[(*bin_idx)++] = 0x89;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((src_reg << 3) | dest_reg);
                    nano_print("  [ASM] mov [reg], reg -> opcod generat\n");
                }
                return 3;
            } else {
                int target_offset = -1;
                for (int l = 0; l < label_count; l++) {
                    if (strcmp(labels[l].name, inner) == 0) {
                        target_offset = labels[l].offset;
                        break;
                    }
                }

                int src_reg = 0;
                if (strcmp(inst->op2, "rax") == 0) src_reg = 0;
                else if (strcmp(inst->op2, "rbx") == 0) src_reg = 3;
                else if (strcmp(inst->op2, "rdi") == 0) src_reg = 7;
                else if (strcmp(inst->op2, "rsi") == 0) src_reg = 6;
                else if (strcmp(inst->op2, "rdx") == 0) src_reg = 2;

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0x48;
                    bin_buffer[(*bin_idx)++] = 0x89;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((src_reg << 3) | 0x05);

                    int disp_pos = *bin_idx;
                    *bin_idx += 4;
                    int32_t disp32 = target_offset - (disp_pos + 4);

                    bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                    bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                    bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                    bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                    nano_print("  [ASM] mov [var], reg (RIP-relative) -> opcod generat\n");
                }
                return 7;
            }
        }
    }

    // --- CMP ---
    if (strcmp(inst->mnemonic, "cmp") == 0) {
        if (strcmp(inst->op1, "rdi") == 0) {
            int val = atoi(inst->op2);
            if (bin_buffer) {
                bin_buffer[(*bin_idx)++] = 0x48;
                bin_buffer[(*bin_idx)++] = 0x81;
                bin_buffer[(*bin_idx)++] = 0xFF;
                bin_buffer[(*bin_idx)++] = (uint8_t)(val & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 8) & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 16) & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 24) & 0xFF);
                nano_print("  [ASM] cmp rdi, val -> opcod generat\n");
            }
            return 7;
        }
    }

    // --- PRINT_RAX ---
    if (strcmp(inst->mnemonic, "print_rax") == 0) {
        int total_size = g_generate_elf ? sizeof(itoa_linux) : 15;

        if (bin_buffer) {
            if (g_generate_elf) {
                for (size_t b = 0; b < sizeof(itoa_linux); b++) {
                    bin_buffer[(*bin_idx)++] = itoa_linux[b];
                }
            } else {
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x89; bin_buffer[(*bin_idx)++] = 0xC7;
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x10; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x89; bin_buffer[(*bin_idx)++] = 0xF8;
            }
            nano_print("  [ASM] print_rax -> opcod generat\n");
        }
        return total_size;
    }

    // --- EXIT ---
    if (strcmp(inst->mnemonic, "exit") == 0) {
        if (bin_buffer) {
            if (g_generate_elf) {
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x3C; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC7;
                bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x05;
            } else {
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x14; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC7;
                bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;
            }
            nano_print("  [ASM] exit -> opcod generat\n");
        }
        return g_generate_elf ? 16 : 16;
    }

    // --- JMP CONDIȚIONALE ȘI NECONDIȚIONALE ---
    if (strcmp(inst->mnemonic, "jmp") == 0 || strcmp(inst->mnemonic, "je") == 0 ||  
        strcmp(inst->mnemonic, "jne") == 0 || strcmp(inst->mnemonic, "jg") == 0 ||
        strcmp(inst->mnemonic, "jl") == 0 || strcmp(inst->mnemonic, "jge") == 0 ||
        strcmp(inst->mnemonic, "jle") == 0 || strcmp(inst->mnemonic, "jb") == 0 || 
        strcmp(inst->mnemonic, "ja") == 0 || strcmp(inst->mnemonic, "call") == 0) {
        
        int is_je   = (strcmp(inst->mnemonic, "je") == 0);
        int is_jne  = (strcmp(inst->mnemonic, "jne") == 0);
        int is_jg   = (strcmp(inst->mnemonic, "jg") == 0);
        int is_jl   = (strcmp(inst->mnemonic, "jl") == 0);
        int is_jge  = (strcmp(inst->mnemonic, "jge") == 0);
        int is_jle  = (strcmp(inst->mnemonic, "jle") == 0);
        int is_jb   = (strcmp(inst->mnemonic, "jb") == 0);
        int is_ja   = (strcmp(inst->mnemonic, "ja") == 0);
        int is_call = (strcmp(inst->mnemonic, "call") == 0);
        int is_jmp  = (strcmp(inst->mnemonic, "jmp") == 0);
        
        int total_size = is_jmp || is_call ? 5 : 6;

        if (bin_buffer) {
            int target_offset = -1;
            for (int l = 0; l < label_count; l++) {
                if (strcmp(labels[l].name, inst->op1) == 0) {
                    target_offset = labels[l].offset;
                    break;
                }
            }

            if (target_offset == -1) {
                nano_print("  [Eroare ASM] Eticheta nu a fost gasita la Pasul 2: ");
                nano_print(inst->op1);
                nano_print("\n");
                target_offset = *bin_idx + total_size; 
            }

            if (is_je) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x84;
            } else if (is_jne) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x85;
            } else if (is_jg) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x8F;
            } else if (is_jl) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x8C;
            } else if (is_jge) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x8D;
            } else if (is_jle) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x8E;
            } else if (is_jb) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x82;
            } else if (is_ja) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x87;
            } else if (is_call) {
                bin_buffer[(*bin_idx)++] = 0xE8;
            } else if (is_jmp) {
                bin_buffer[(*bin_idx)++] = 0xE9;
            }
            
            int rel_pos = *bin_idx;
            *bin_idx += 4;
            
            int32_t rel32 = target_offset - (rel_pos + 4);
            
            bin_buffer[rel_pos + 0] = (uint8_t)(rel32 & 0xFF);
            bin_buffer[rel_pos + 1] = (uint8_t)((rel32 >> 8) & 0xFF);
            bin_buffer[rel_pos + 2] = (uint8_t)((rel32 >> 16) & 0xFF);
            bin_buffer[rel_pos + 3] = (uint8_t)((rel32 >> 24) & 0xFF);
            
            nano_print("  [ASM] Salt generat. Displacement: ");
            char disp_str[16];
            itoa(rel32, disp_str, 10);
            nano_print(disp_str);
            nano_print("\n");
        }
        
        return total_size;
    }
    
    // --- DIRECTIVĂ DATE (quad / data) ---
    if (strcmp(inst->mnemonic, "quad") == 0 || strcmp(inst->mnemonic, "data") == 0) {
        if (inst->op1[0] == '"') {
            char str_val[128];
            int s_idx = 0;
            char* p = inst->op1 + 1;
            while (*p != '"' && *p != '\0' && s_idx < 127) {
                if (*p == '\\' && *(p + 1) == 'n') {
                    str_val[s_idx++] = '\n';
                    p += 2;
                } else {
                    str_val[s_idx++] = *p++;
                }
            }
            str_val[s_idx] = '\0';
            int str_len = s_idx + 1;

            if (bin_buffer) {
                for (int b = 0; b < str_len; b++) {
                    bin_buffer[(*bin_idx)++] = (uint8_t)str_val[b];
                }
                nano_print("  [ASM] directivă data string -> octeți generați\n");
            }
            return str_len;
        } else {
            int has_dot = 0;
            for (int k = 0; inst->op1[k] != '\0'; k++) {
                if (inst->op1[k] == '.') { has_dot = 1; break; }
            }

            if (has_dot) {
                float f_val = nano_parse_float(inst->op1);
                union { float f; uint32_t u; } cast;
                cast.f = f_val;

                if (bin_buffer) {
                    for (int b = 0; b < 4; b++) {
                        bin_buffer[(*bin_idx)++] = (uint8_t)((cast.u >> (b * 8)) & 0xFF);
                    }
                    nano_print("  [ASM] directivă float (32-bit) -> 4 octeți generați\n");
                }
                return 4;
            } else {
                int64_t val = parse_int64(inst->op1);
                if (bin_buffer) {
                    for (int b = 0; b < 8; b++) {
                        bin_buffer[(*bin_idx)++] = (uint8_t)((val >> (b * 8)) & 0xFF);
                    }
                    nano_print("  [ASM] directivă data număr -> 8 octeți generați\n");
                }
                return 8;
            }
        }
    }

    // --- MOVSS (Float Load / Store / Move) ---
    if (strcmp(inst->mnemonic, "movss") == 0) {
        if (inst->op1[0] == 'x' && inst->op1[1] == 'm' && inst->op1[2] == 'm' &&
            inst->op2[0] == 'x' && inst->op2[1] == 'm' && inst->op2[2] == 'm') {
            
            int xmm_dest = inst->op1[3] - '0';
            int xmm_src  = inst->op2[3] - '0';

            if (xmm_dest >= 0 && xmm_dest <= 7 && xmm_src >= 0 && xmm_src <= 7) {
                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0xF3;
                    bin_buffer[(*bin_idx)++] = 0x0F;
                    bin_buffer[(*bin_idx)++] = 0x10;
                    bin_buffer[(*bin_idx)++] = (uint8_t)(0xC0 | (xmm_dest << 3) | xmm_src);
                    nano_print("  [ASM] movss xmm, xmm -> opcod generat\n");
                }
                return 4;
            }
        }

        if (inst->op2[0] == '[') {
            char reg_name[16];
            strcpy(reg_name, inst->op1);

            int xmm_code = -1;
            if (reg_name[0] == 'x' && reg_name[1] == 'm' && reg_name[2] == 'm') {
                xmm_code = reg_name[3] - '0';
            }

            if (xmm_code >= 0 && xmm_code <= 7) {
                char mem_name[32];
                int m_i = 0, m_j = 0;
                while (inst->op2[m_i] != '\0') {
                    if (inst->op2[m_i] != '[' && inst->op2[m_i] != ']') {
                        mem_name[m_j++] = inst->op2[m_i];
                    }
                    m_i++;
                }
                mem_name[m_j] = '\0';

                int target_offset = -1;
                for (int l = 0; l < label_count; l++) {
                    if (strcmp(labels[l].name, mem_name) == 0) {
                        target_offset = labels[l].offset;
                        break;
                    }
                }

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0xF3;
                    bin_buffer[(*bin_idx)++] = 0x0F;
                    bin_buffer[(*bin_idx)++] = 0x10;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((xmm_code << 3) | 0x05);

                    int disp_pos = *bin_idx;
                    *bin_idx += 4;
                    int32_t disp32 = target_offset - (disp_pos + 4);

                    bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                    bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                    bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                    bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                    nano_print("  [ASM] movss xmm, [var] -> opcod generat\n");
                }
                return 8;
            }
        }
        
        if (inst->op1[0] == '[') {
            char reg_name[16];
            strcpy(reg_name, inst->op2);

            int xmm_code = -1;
            if (reg_name[0] == 'x' && reg_name[1] == 'm' && reg_name[2] == 'm') {
                xmm_code = reg_name[3] - '0';
            }

            if (xmm_code >= 0 && xmm_code <= 7) {
                char mem_name[32];
                int m_i = 0, m_j = 0;
                while (inst->op1[m_i] != '\0') {
                    if (inst->op1[m_i] != '[' && inst->op1[m_i] != ']') {
                        mem_name[m_j++] = inst->op1[m_i];
                    }
                    m_i++;
                }
                mem_name[m_j] = '\0';

                int target_offset = -1;
                for (int l = 0; l < label_count; l++) {
                    if (strcmp(labels[l].name, mem_name) == 0) {
                        target_offset = labels[l].offset;
                        break;
                    }
                }

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0xF3;
                    bin_buffer[(*bin_idx)++] = 0x0F;
                    bin_buffer[(*bin_idx)++] = 0x11;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((xmm_code << 3) | 0x05);

                    int disp_pos = *bin_idx;
                    *bin_idx += 4;
                    int32_t disp32 = target_offset - (disp_pos + 4);

                    bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                    bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                    bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                    bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                    nano_print("  [ASM] movss [var], xmm -> opcod generat\n");
                }
                return 8;
            }
        }
    }

    // --- PRINT_XMM ---
    // --- PRINT_XMM (Cu suport pentru zecimale ftoa) ---
    // --- PRINT_XMM (Cu suport complet ftoa - 2 zecimale) ---
    if (strcmp(inst->mnemonic, "print_xmm") == 0) {
        static const uint8_t ftoa_linux[] = {
            // 1. Salvare registre
            0x50, 0x51, 0x52, 0x57, 0x56, 0x41, 0x50, 0x41, 0x51, // push rax, rcx, rdx, rdi, rsi, r8, r9
            0x0F, 0x28, 0xC8,                                     // movaps xmm1, xmm0 (backup valoare float)

            // 2. Extragere și afișare parte întreagă
            0xF3, 0x48, 0x0F, 0x2C, 0xC0,                         // cvttss2si rax, xmm0
            0xF3, 0x48, 0x0F, 0x2A, 0xD0,                         // cvtsi2ss xmm2, rax (reconversie float pentru scădere)

            // --- itoa_linux pentru partea întreagă ---
            0x49, 0x89, 0xE0, 0xB9, 0x00, 0x00, 0x00, 0x00,       // mov r8, rsp; mov ecx, 0
            0x41, 0xB9, 0x0A, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC0, // mov r9d, 10; test rax, rax
            0x75, 0x0B, 0x48, 0xFF, 0xCC, 0xC6, 0x04, 0x24, 0x30, // jnz +11; dec rsp; mov byte ptr [rsp], '0'
            0xFF, 0xC1, 0xEB, 0x18, 0x48, 0x85, 0xC0, 0x74, 0x13, // inc ecx; jmp +24; test rax, rax; jz +19
            0x48, 0x31, 0xD2, 0x49, 0xF7, 0xF1, 0x80, 0xC2, 0x30, // xor rdx, rdx; div r9; add dl, '0'
            0x48, 0xFF, 0xCC, 0x88, 0x14, 0x24, 0xFF, 0xC1, 0xEB, // dec rsp; mov [rsp], dl; inc ecx; jmp -24
            0xE8, 0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00, 0x48, // mov rax, 1 (sys_write)
            0xC7, 0xC7, 0x01, 0x00, 0x00, 0x00, 0x48, 0x89, 0xE6, // mov rdi, 1 (stdout); mov rsi, rsp
            0x48, 0x89, 0xCA, 0x0F, 0x05, 0x4C, 0x89, 0xC4,       // mov rdx, rcx; syscall; mov rsp, r8

            // 3. Afișare caracter '.'
            0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,             // mov rax, 1
            0x48, 0xC7, 0xC7, 0x01, 0x00, 0x00, 0x00,             // mov rdi, 1
            0x48, 0xFF, 0xCC, 0xC6, 0x04, 0x24, 0x2E,             // dec rsp; mov byte ptr [rsp], '.'
            0x48, 0x89, 0xE6,                                     // mov rsi, rsp
            0x48, 0xC7, 0xC2, 0x01, 0x00, 0x00, 0x00,             // mov rdx, 1
            0x0F, 0x05,                                           // syscall
            0x48, 0x83, 0xC4, 0x01,                               // add rsp, 1

            // 4. Calcul parte fracționară: (xmm1 - xmm2) * 100.0f
            0xF3, 0x0F, 0x5C, 0xCA,                               // subss xmm1, xmm2
            0xB8, 0x00, 0x00, 0xC8, 0x42,                         // mov eax, 0x42c80000 (100.0f IEEE 754)
            0x66, 0x0F, 0x6E, 0xD8,                               // movd xmm3, eax
            0xF3, 0x0F, 0x59, 0xCB,                               // mulss xmm1, xmm3
            0xF3, 0x48, 0x0F, 0x2C, 0xC1,                         // cvttss2si rax, xmm1

            // --- itoa_linux pentru partea fracționară ---
            0x49, 0x89, 0xE0, 0xB9, 0x00, 0x00, 0x00, 0x00,       // mov r8, rsp; mov ecx, 0
            0x41, 0xB9, 0x0A, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC0, // mov r9d, 10; test rax, rax
            0x75, 0x0B, 0x48, 0xFF, 0xCC, 0xC6, 0x04, 0x24, 0x30, // jnz +11; dec rsp; mov byte ptr [rsp], '0'
            0xFF, 0xC1, 0xEB, 0x18, 0x48, 0x85, 0xC0, 0x74, 0x13, // inc ecx; jmp +24; test rax, rax; jz +19
            0x48, 0x31, 0xD2, 0x49, 0xF7, 0xF1, 0x80, 0xC2, 0x30, // xor rdx, rdx; div r9; add dl, '0'
            0x48, 0xFF, 0xCC, 0x88, 0x14, 0x24, 0xFF, 0xC1, 0xEB, // dec rsp; mov [rsp], dl; inc ecx; jmp -24
            0xE8, 0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00, 0x48, // mov rax, 1 (sys_write)
            0xC7, 0xC7, 0x01, 0x00, 0x00, 0x00, 0x48, 0x89, 0xE6, // mov rdi, 1 (stdout); mov rsi, rsp
            0x48, 0x89, 0xCA, 0x0F, 0x05, 0x4C, 0x89, 0xC4,       // mov rdx, rcx; syscall; mov rsp, r8

            // 5. Restaurare registre
            0x41, 0x59, 0x41, 0x58, 0x5E, 0x5F, 0x5A, 0x59, 0x58  // pop r9, r8, rsi, rdi, rdx, rcx, rax
        };

        int total_size = g_generate_elf ? sizeof(ftoa_linux) : 14;

        if (bin_buffer) {
            if (g_generate_elf) {
                for (size_t b = 0; b < sizeof(ftoa_linux); b++) {
                    bin_buffer[(*bin_idx)++] = ftoa_linux[b];
                }
            } else {
                bin_buffer[(*bin_idx)++] = 0x66; bin_buffer[(*bin_idx)++] = 0x48;
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x7E;
                bin_buffer[(*bin_idx)++] = 0xC7;
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x1F; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;
            }
            nano_print("  [ASM] print_xmm (ftoa) -> opcod generat\n");
        }
        return total_size;
    }

    // --- SSE FLOATS: addss, subss, mulss, divss ---
    if (strcmp(inst->mnemonic, "addss") == 0 || strcmp(inst->mnemonic, "subss") == 0 ||
        strcmp(inst->mnemonic, "mulss") == 0 || strcmp(inst->mnemonic, "divss") == 0) {
        
        int is_sub = (strcmp(inst->mnemonic, "subss") == 0);
        int is_mul = (strcmp(inst->mnemonic, "mulss") == 0);
        int is_div = (strcmp(inst->mnemonic, "divss") == 0);
        
        uint8_t opcode = 0x58;
        if (is_sub) opcode = 0x5C;
        else if (is_mul) opcode = 0x59;
        else if (is_div) opcode = 0x5E;

        char reg_name[16];
        strcpy(reg_name, inst->op1);
        int xmm_dest = -1;
        if (reg_name[0] == 'x' && reg_name[1] == 'm' && reg_name[2] == 'm') {
            xmm_dest = reg_name[3] - '0';
        }

        if (xmm_dest >= 0 && xmm_dest <= 7) {
            if (inst->op2[0] == 'x' && inst->op2[1] == 'm' && inst->op2[2] == 'm') {
                int xmm_src = inst->op2[3] - '0';
                if (xmm_src >= 0 && xmm_src <= 7) {
                    if (bin_buffer) {
                        bin_buffer[(*bin_idx)++] = 0xF3;
                        bin_buffer[(*bin_idx)++] = 0x0F;
                        bin_buffer[(*bin_idx)++] = opcode;
                        bin_buffer[(*bin_idx)++] = (uint8_t)(0xC0 | (xmm_dest << 3) | xmm_src);
                        nano_print("  [ASM] float op xmm, xmm -> opcod generat\n");
                    }
                    return 4;
                }
            }

            if (inst->op2[0] == '[') {
                char mem_name[32];
                int m_i = 0, m_j = 0;
                while (inst->op2[m_i] != '\0') {
                    if (inst->op2[m_i] != '[' && inst->op2[m_i] != ']') {
                        mem_name[m_j++] = inst->op2[m_i];
                    }
                    m_i++;
                }
                mem_name[m_j] = '\0';

                int target_offset = -1;
                for (int l = 0; l < label_count; l++) {
                    if (strcmp(labels[l].name, mem_name) == 0) {
                        target_offset = labels[l].offset;
                        break;
                    }
                }

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0xF3;
                    bin_buffer[(*bin_idx)++] = 0x0F;
                    bin_buffer[(*bin_idx)++] = opcode;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((xmm_dest << 3) | 0x05);

                    int disp_pos = *bin_idx;
                    *bin_idx += 4;
                    int32_t disp32 = target_offset - (disp_pos + 4);

                    bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                    bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                    bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                    bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                    nano_print("  [ASM] float op xmm, [var] -> opcod generat\n");
                }
                return 8;
            }
        }
    }

    // --- UCOMISS (Comparare Float) ---
    if (strcmp(inst->mnemonic, "ucomiss") == 0) {
        char reg_name[16];
        strcpy(reg_name, inst->op1);
        int xmm_dest = -1;
        if (reg_name[0] == 'x' && reg_name[1] == 'm' && reg_name[2] == 'm') {
            xmm_dest = reg_name[3] - '0';
        }

        if (xmm_dest >= 0 && xmm_dest <= 7) {
            if (inst->op2[0] == 'x' && inst->op2[1] == 'm' && inst->op2[2] == 'm') {
                int xmm_src = inst->op2[3] - '0';
                if (xmm_src >= 0 && xmm_src <= 7) {
                    if (bin_buffer) {
                        bin_buffer[(*bin_idx)++] = 0x0F;
                        bin_buffer[(*bin_idx)++] = 0x2E;
                        bin_buffer[(*bin_idx)++] = (uint8_t)(0xC0 | (xmm_dest << 3) | xmm_src);
                        nano_print("  [ASM] ucomiss xmm, xmm -> opcod generat\n");
                    }
                    return 3;
                }
            }

            if (inst->op2[0] == '[') {
                char mem_name[32];
                int m_i = 0, m_j = 0;
                while (inst->op2[m_i] != '\0') {
                    if (inst->op2[m_i] != '[' && inst->op2[m_i] != ']') {
                        mem_name[m_j++] = inst->op2[m_i];
                    }
                    m_i++;
                }
                mem_name[m_j] = '\0';

                int target_offset = -1;
                for (int l = 0; l < label_count; l++) {
                    if (strcmp(labels[l].name, mem_name) == 0) {
                        target_offset = labels[l].offset;
                        break;
                    }
                }

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0x0F;
                    bin_buffer[(*bin_idx)++] = 0x2E;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((xmm_dest << 3) | 0x05);

                    int disp_pos = *bin_idx;
                    *bin_idx += 4;
                    int32_t disp32 = target_offset - (disp_pos + 4);

                    bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                    bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                    bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                    bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                    nano_print("  [ASM] ucomiss xmm, [var] -> opcod generat\n");
                }
                return 7;
            }
        }
    }

    // --- Instrucțiuni matematice x86-64 ---
    if (strcmp(inst->mnemonic, "imul") == 0 && strcmp(inst->op1, "rbx") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0xF7;
            bin_buffer[(*bin_idx)++] = 0xEB;
        }
        return 3;
    }

    if (strcmp(inst->mnemonic, "cqo") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0x99;
        }
        return 2;
    }

    if (strcmp(inst->mnemonic, "idiv") == 0 && strcmp(inst->op1, "rbx") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0xF7;
            bin_buffer[(*bin_idx)++] = 0xFB;
        }
        return 3;
    }

    if (strcmp(inst->mnemonic, "add") == 0 && strcmp(inst->op1, "rax") == 0 && strcmp(inst->op2, "rbx") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0x01;
            bin_buffer[(*bin_idx)++] = 0xD8;
        }
        return 3;
    }

    if (strcmp(inst->mnemonic, "sub") == 0 && strcmp(inst->op1, "rax") == 0 && strcmp(inst->op2, "rbx") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0x29;
            bin_buffer[(*bin_idx)++] = 0xD8;
        }
        return 3;
    }

    if (strcmp(inst->mnemonic, "mov") == 0 && strcmp(inst->op1, "rax") == 0 && strcmp(inst->op2, "rdx") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0x89;
            bin_buffer[(*bin_idx)++] = 0xD0;
        }
        return 3;
    }

    return -1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        nano_print("Utilizare: asm [-elf] <sursa.s> [iesire]\n");
        return 0;
    }

    int generate_elf = 0;   
    const char* src_file = 0;
    const char* out_file = 0;

    for (int a = 1; a < argc; a++) {
        if (strcmp(argv[a], "-elf") == 0) {
            generate_elf = 1;
            g_generate_elf = 1;
        } else if (!src_file) {
            src_file = argv[a];
        } else if (!out_file) {
            out_file = argv[a];
        }
    }

    if (!src_file) {
        nano_print("Eroare: Nu ai specificat fisierul sursa!\n");
        return 0;
    }
    if (!out_file) {
        out_file = generate_elf ? "a.out" : "out";
    }

    if (generate_elf) {
        nano_print("NanoASM v2.0 -> Mod Generare: Executabil Linux ELF64\n");
    } else {
        nano_print("NanoASM v2.0 -> Mod Generare: Nativ Nano OS (NAS1)\n");
    }

    uint8_t* src_buffer = (uint8_t*)nano_malloc(8192);
    if (!src_buffer) {
        nano_print("Eroare: Fara memorie pentru src_buffer!\n");
        return 0;
    }
    
    int bytes_read = nano_read_file(src_file, src_buffer, 8191);
    if (bytes_read <= 0) {
        nano_print("Eroare la citirea fisierului sursa!\n");
        nano_free(src_buffer);
        return 0;
    }
    src_buffer[bytes_read] = '\0';

    Label labels[32];
    int label_count = 0;

    int start_offset = generate_elf ? 120 : 8;

    // --- PASUL 1: Scanare etichete și calculare dimensiuni ---
    nano_print("[PASUL 1] Scanare etichete si dimensiuni...\n");
    {
        int p_bin_idx = start_offset;
        int line_start = 0;
        for (int i = 0; i <= bytes_read; i++) {
            if (src_buffer[i] == '\n' || src_buffer[i] == '\0') {
                char raw_line[128];
                int len = i - line_start;
                if (len >= 127) len = 127;
                memcpy(raw_line, &src_buffer[line_start], len);
                raw_line[len] = '\0';
                line_start = i + 1;

                char clean[128];
                clean_line(raw_line, clean);
                if (clean[0] == '\0') continue;

                Instruction inst;
                parse_instruction(clean, &inst);

                int c_len = strlen(inst.mnemonic);
                if (c_len > 0 && inst.mnemonic[c_len - 1] == ':') {
                    inst.mnemonic[c_len - 1] = '\0';
                    strcpy(labels[label_count].name, inst.mnemonic);
                    labels[label_count].offset = p_bin_idx;
                    label_count++;

                    nano_print("  [DEBUG] Eticheta gasita: '");
                    nano_print(inst.mnemonic);
                    nano_print("' la offset-ul ");
                    char buf[16];
                    itoa(p_bin_idx, buf, 10);
                    nano_print(buf);
                    nano_print("\n");

                    if (inst.op1[0] != '\0') {
                        strcpy(inst.mnemonic, inst.op1);
                        strcpy(inst.op1, inst.op2);
                        inst.op2[0] = '\0';
                    } else {
                        continue;
                    }
                }

                int size = process_instruction(&inst, NULL, &p_bin_idx, labels, label_count);
                if (size < 0) {
                    nano_print("Eroare sintaxa (Pas 1): ");
                    nano_print(clean);
                    nano_print("\n");
                    nano_free(src_buffer);
                    return 0;
                }

                nano_print("  [DEBUG] Linie: '");
                nano_print(clean);
                nano_print("' | Dimensiune: ");
                char size_buf[16];
                itoa(size, size_buf, 10);
                nano_print(size_buf);
                nano_print(" octeti\n");

                p_bin_idx += size;
            }
        }
    }

    // --- PASUL 2: Generare binar ---
    nano_print("[PASUL 2] Generare binar...\n");
    uint8_t* bin_buffer = (uint8_t*)nano_malloc(16384);
    if (!bin_buffer) {
        nano_print("Eroare: Fara memorie pentru bin_buffer!\n");
        nano_free(src_buffer);
        return 0;
    }
    
    // Curățăm întreg tamponul la zero pentru a evita câmpuri garbage în antetul ELF
    for (int i = 0; i < 16384; i++) {
        bin_buffer[i] = 0;
    }

    int bin_idx = start_offset;

    if (generate_elf) {
        int entry_offset = 120;
        for (int l = 0; l < label_count; l++) {
            if (strcmp(labels[l].name, "main") == 0 || strcmp(labels[l].name, "_start") == 0) {
                entry_offset = labels[l].offset;
                break;
            }
        }

        Elf64_Ehdr* ehdr = (Elf64_Ehdr*)bin_buffer;
        ehdr->e_ident[0] = 0x7F; ehdr->e_ident[1] = 'E'; ehdr->e_ident[2] = 'L'; ehdr->e_ident[3] = 'F';
        ehdr->e_ident[4] = 2; // 64-bit
        ehdr->e_ident[5] = 1; // Little-endian
        ehdr->e_ident[6] = 1; // ELF Version
        ehdr->e_type = 2;     // Executable
        ehdr->e_machine = 0x3E; // AMD x86-64
        ehdr->e_version = 1;
        ehdr->e_entry = 0x400000 + entry_offset; // Adresă dinamică
        ehdr->e_phoff = 64;   
        ehdr->e_ehsize = sizeof(Elf64_Ehdr);
        ehdr->e_phentsize = sizeof(Elf64_Phdr);
        ehdr->e_phnum = 1;

        Elf64_Phdr* phdr = (Elf64_Phdr*)(bin_buffer + 64);
        phdr->p_type = 1; // PT_LOAD
        phdr->p_flags = 7; // R W X (Read + Write + Execute)
        phdr->p_offset = 0;
        phdr->p_vaddr = 0x400000;
        phdr->p_paddr = 0x400000;
        phdr->p_align = 0x1000;
    } else {
        NanoHeader* hdr = (NanoHeader*)bin_buffer;
        hdr->magic[0] = 'N'; hdr->magic[1] = 'A'; hdr->magic[2] = 'S'; hdr->magic[3] = '1';
        hdr->entry_offset = 8;
    }

    {
        int line_start = 0;
        for (int i = 0; i <= bytes_read; i++) {
            if (src_buffer[i] == '\n' || src_buffer[i] == '\0') {
                char raw_line[128];
                int len = i - line_start;
                if (len >= 127) len = 127;
                memcpy(raw_line, &src_buffer[line_start], len);
                raw_line[len] = '\0';
                line_start = i + 1;

                char clean[128];
                clean_line(raw_line, clean);
                if (clean[0] == '\0') continue;

                Instruction inst;
                parse_instruction(clean, &inst);

                int c_len = strlen(inst.mnemonic);
                if (c_len > 0 && inst.mnemonic[c_len - 1] == ':') {
                    if (inst.op1[0] != '\0') {
                        strcpy(inst.mnemonic, inst.op1);
                        strcpy(inst.op1, inst.op2);
                        inst.op2[0] = '\0';
                    } else {
                        continue;
                    }
                }

                int size = process_instruction(&inst, bin_buffer, &bin_idx, labels, label_count);
                if (size < 0) {
                    nano_print("Eroare sintaxa (Pas 2): ");
                    nano_print(clean);
                    nano_print("\n");
                    nano_free(bin_buffer);
                    nano_free(src_buffer);
                    return 0;
                }
            }
        }
    }

    if (generate_elf) {
        Elf64_Phdr* phdr = (Elf64_Phdr*)(bin_buffer + 64);
        phdr->p_filesz = bin_idx;
        phdr->p_memsz = bin_idx;
    }

    // --- PASUL 3: Salvare fișier pe disc ---
    int create_res = nano_create_file(out_file, bin_idx);
    nano_print("  [DEBUG] nano_create_file a returnat: ");
    char res_buf[16];
    itoa(create_res, res_buf, 10);
    nano_print(res_buf);
    nano_print("\n");

    if (create_res >= 0) {
        nano_print("Creat cu succes!\n");
        
        int write_res = nano_write_file(out_file, bin_buffer, bin_idx);
        nano_print("  [DEBUG] nano_write_file a returnat codul: ");
        itoa(write_res, res_buf, 10);
        nano_print(res_buf);
        nano_print("\n");
        
        nano_print("Asamblare v2.0 finalizata cu succes!\n");
    } else {
        nano_print("Eroare la crearea fisierului binar!\n");
    }

    nano_free(bin_buffer);
    nano_free(src_buffer);

    return 0;
}
/*
#include "nano_libc.h"
#include "elf.h"

// Structură pentru etichete
typedef struct {
    char name[32];
    int offset;
} Label;

// Structură pentru o instrucțiune parsată
typedef struct {
    char mnemonic[32];
    char op1[32];
    char op2[32];
} Instruction;

static int g_generate_elf = 0;


float nano_parse_float(const char* str) {
    float result = 0.0f;
    float sign = 1.0f;
    int i = 0;

    if (str[0] == '-') {
        sign = -1.0f;
        i++;
    } else if (str[0] == '+') {
        i++;
    }

    // Partea întreagă
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10.0f + (str[i] - '0');
        i++;
    }

    // Partea fracționară
    if (str[i] == '.') {
        i++;
        float fraction = 1.0f;
        while (str[i] >= '0' && str[i] <= '9') {
            fraction /= 10.0f;
            result += (str[i] - '0') * fraction;
            i++;
        }
    }

    return result * sign;
}

// Funcție avansată pentru curățarea și sanitizarea liniei
void clean_line(const char* raw, char* dest) {
    int i = 0, j = 0;

    // 1. Trecem peste BOM-ul UTF-8 (EF BB BF) dacă apare la începutul fișierului
    if ((unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF) {
        i += 3;
    }

    // 2. Trecem peste spații, tab-uri, \r sau alte caractere de control de la început
    while (raw[i] != '\0' && (raw[i] == ' ' || raw[i] == '\t' || raw[i] == '\r' || (unsigned char)raw[i] < 32)) {
        if (raw[i] == '\n') break;
        i++;
    }

    // 3. Copiem conținutul util până la comentariu (';'), sfârșit de linie sau de șir
    while (raw[i] != '\0' && raw[i] != ';' && raw[i] != '\r' && raw[i] != '\n') {
        dest[j++] = raw[i++];
    }

    // 4. Tăiem spațiile sau caracterele de control rămase la sfârșit
    while (j > 0 && (dest[j-1] == ' ' || dest[j-1] == '\t' || dest[j-1] == '\r' || (unsigned char)dest[j-1] < 32)) {
        j--;
    }
    
    dest[j] = '\0';
}

// Tokenizer-ul
int parse_instruction(const char* line, Instruction* inst) {
    inst->mnemonic[0] = '\0';
    inst->op1[0] = '\0';
    inst->op2[0] = '\0';

    int i = 0, j = 0;

    // 1. Mnemonică
    while (line[i] != '\0' && line[i] != ' ' && line[i] != '\t' && line[i] != ',') {
        if (j < 31) inst->mnemonic[j++] = line[i];
        i++;
    }
    inst->mnemonic[j] = '\0';

    if (inst->mnemonic[0] == '\0') return 0;

    while (line[i] == ' ' || line[i] == '\t') i++;
    if (line[i] == '\0') return 1;

    // TRATARE SPECIALĂ PENTRU PRINT
    if (strcmp(inst->mnemonic, "print") == 0) {
        strcpy(inst->op1, &line[i]);
        return 2;
    }

    // 2. Operand 1
    j = 0;
    while (line[i] != '\0' && line[i] != ',' && line[i] != ' ' && line[i] != '\t') {
        if (j < 31) inst->op1[j++] = line[i];
        i++;
    }
    inst->op1[j] = '\0';

    while (line[i] == ' ' || line[i] == '\t' || line[i] == ',') i++;
    if (line[i] == '\0') return 2;

    // 3. Operand 2
    j = 0;
    while (line[i] != '\0') {
        if (j < 31) inst->op2[j++] = line[i];
        i++;
    }
    while (j > 0 && (inst->op2[j-1] == ' ' || inst->op2[j-1] == '\t' || inst->op2[j-1] == '\r')) {
        j--;
    }
    inst->op2[j] = '\0';

    return 3;
}

// Funcție centralizată unică pentru dimensiune și generare binar cu loguri
int process_instruction(Instruction* inst, uint8_t* bin_buffer, int* bin_idx, Label* labels, int label_count) {
    
    // --- ETICHETE ---
    int len = strlen(inst->mnemonic);
    if (len > 0 && inst->mnemonic[len - 1] == ':') {
        return 0;
    }

    // --- PRINT "text" SAU print [eticheta] ---
	if (strcmp(inst->mnemonic, "print") == 0) {
        char msg[128];
        int m_idx = 0;
        char* p = inst->op1;
        if (*p == '"') p++;

        while (*p != '"' && *p != '\0' && m_idx < 127) {
            if (*p == '\\' && *(p + 1) == 'n') {
                msg[m_idx++] = '\n';
                p += 2;
            } else {
                msg[m_idx++] = *p++;
            }
        }
        msg[m_idx] = '\0';
        int final_str_len = m_idx; // Fără \0 pentru sys_write pe Linux, sau cu m_idx exact

        // Calculăm dimensiunea totală a instrucțiunii în mod ELF:
        // 1. mov rax, 1 (7 octeți)
        // 2. mov rdi, 1 (7 octeți)
        // 3. lea rsi, [rip + disp32] (7 octeți)
        // 4. mov rdx, len (7 octeți)
        // 5. syscall (2 octeți)
        // 6. jmp rel8 (2 octeți)
        // + textul șirului
        int total_size = g_generate_elf ? (32 + final_str_len) : (7 + 7 + 2 + 2 + final_str_len + 1);

        if (bin_buffer) {
            if (g_generate_elf) {
                // 1. mov rax, 1 (sys_write)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x01; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;

                // 2. mov rdi, 1 (stdout)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC7;
                bin_buffer[(*bin_idx)++] = 0x01; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;

                // 3. lea rsi, [rip + disp32]
                //bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x8D; bin_buffer[(*bin_idx)++] = 0x3D;
				bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x8D; bin_buffer[(*bin_idx)++] = 0x35; // <-- 0x35 este rsi!
                int disp_pos = *bin_idx;
                *bin_idx += 4;

                // 4. mov rdx, final_str_len
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC2;
                bin_buffer[(*bin_idx)++] = (uint8_t)(final_str_len & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((final_str_len >> 8) & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((final_str_len >> 16) & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((final_str_len >> 24) & 0xFF);

                // 5. syscall
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x05;

                // 6. jmp peste șir
                bin_buffer[(*bin_idx)++] = 0xEB;
                int jmp_rel_pos = (*bin_idx)++;

                int string_offset = *bin_idx;
                for (int m = 0; m < final_str_len; m++) {
                    bin_buffer[(*bin_idx)++] = (uint8_t)msg[m];
                }

                int32_t disp32 = string_offset - (disp_pos + 4);
                bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                int jmp_target = *bin_idx;
                int jmp_from = jmp_rel_pos + 1;
                bin_buffer[jmp_rel_pos] = (uint8_t)(jmp_target - jmp_from);

            } else {
                // Logica veche pentru Nano OS (int 0x80)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x01; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;

                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x8D; bin_buffer[(*bin_idx)++] = 0x3D;
                int disp_pos = *bin_idx;
                *bin_idx += 4;

                bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;

                bin_buffer[(*bin_idx)++] = 0xEB;
                int jmp_rel_pos = (*bin_idx)++;

                int string_offset = *bin_idx;
                for (int m = 0; m <= m_idx; m++) {
                    bin_buffer[(*bin_idx)++] = (uint8_t)msg[m];
                }

                int32_t disp32 = string_offset - (disp_pos + 4);
                bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                int jmp_target = *bin_idx;
                int jmp_from = jmp_rel_pos + 1;
                bin_buffer[jmp_rel_pos] = (uint8_t)(jmp_target - jmp_from);
            }
            nano_print("  [ASM] print -> opcod generat cu succes\n");
        }
        return total_size;
    }
    // --- PUSH & POP ---
    if (strcmp(inst->mnemonic, "push") == 0) {
        uint8_t opcode = 0x50; // default rax
        if (strcmp(inst->op1, "rdi") == 0) opcode = 0x57;
        else if (strcmp(inst->op1, "rsi") == 0) opcode = 0x56;
        else if (strcmp(inst->op1, "rdx") == 0) opcode = 0x52;

        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = opcode;
            nano_print("  [ASM] push reg -> opcod generat\n");
        }
        return 1;
    }

    if (strcmp(inst->mnemonic, "pop") == 0) {
        uint8_t opcode = 0x58; // default rax
        if (strcmp(inst->op1, "rdi") == 0) opcode = 0x5F;
        else if (strcmp(inst->op1, "rsi") == 0) opcode = 0x5E;
        else if (strcmp(inst->op1, "rdx") == 0) opcode = 0x5A;

        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = opcode;
            nano_print("  [ASM] pop reg -> opcod generat\n");
        }
        return 1;
    }

    // --- RET ---
    if (strcmp(inst->mnemonic, "ret") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0xC3;
            nano_print("  [ASM] ret -> opcod generat\n");
        }
        return 1;
    }

    // --- INT 0X80 ---
    if (strcmp(inst->mnemonic, "int") == 0 && strcmp(inst->op1, "0x80") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0xCD;
            bin_buffer[(*bin_idx)++] = 0x80;
            nano_print("  [ASM] int 0x80 -> opcod generat\n");
        }
        return 2;
    }

    // --- NEWLINE ---
    if (strcmp(inst->mnemonic, "newline") == 0) {
        if (bin_buffer) {
            if (g_generate_elf) {
                // Pe Linux trimitem un singur octet '\n' folosind sys_write
                // 1. mov rax, 1 (sys_write)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x01; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                // 2. mov rdi, 1 (stdout)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC7;
                bin_buffer[(*bin_idx)++] = 0x01; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                // 3. lea rsi, [rip + disp32] (pointer la caracterul '\n')
                //bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x8D; bin_buffer[(*bin_idx)++] = 0x3D;
				bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x8D; bin_buffer[(*bin_idx)++] = 0x35; // <-- 0x35 este rsi!
                int disp_pos = *bin_idx;
                *bin_idx += 4;
                // 4. mov rdx, 1 (lungime 1 octet)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC2;
                bin_buffer[(*bin_idx)++] = 0x01; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                // 5. syscall
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x05;

                // 6. Săritură jmp peste octetul '\n'
                bin_buffer[(*bin_idx)++] = 0xEB;
                int jmp_rel_pos = (*bin_idx)++;
                int string_offset = *bin_idx;
                
                bin_buffer[(*bin_idx)++] = '\n';

                int32_t disp32 = string_offset - (disp_pos + 4);
                bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                int jmp_target = *bin_idx;
                int jmp_from = jmp_rel_pos + 1;
                bin_buffer[jmp_rel_pos] = (uint8_t)(jmp_target - jmp_from);
            } else {
                // Variantă originală Nano OS
                bin_buffer[(*bin_idx)++] = 0x50; // push rax
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x1A; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;
                bin_buffer[(*bin_idx)++] = 0x58; // pop rax
            }
            nano_print("  [ASM] newline -> opcod generat\n");
        }
        return g_generate_elf ? 33 : 11;
    }

    // --- DEC RDI ---
    if (strcmp(inst->mnemonic, "dec") == 0 && strcmp(inst->op1, "rdi") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0xFF;
            bin_buffer[(*bin_idx)++] = 0xCF;
            nano_print("  [ASM] dec rdi -> opcod generat\n");
        }
        return 3;
    }

    // --- MOV ---
    if (strcmp(inst->mnemonic, "mov") == 0) {
        // ---  Mutare între doi regiștri (ex: mov rax, rdx sau mov rax, rdi) ---
        int src_reg_id = -1;
        if (strcmp(inst->op2, "rax") == 0) src_reg_id = 0;
        else if (strcmp(inst->op2, "rcx") == 0) src_reg_id = 1;
        else if (strcmp(inst->op2, "rdx") == 0) src_reg_id = 2;
        else if (strcmp(inst->op2, "rbx") == 0) src_reg_id = 3;
        else if (strcmp(inst->op2, "rsi") == 0) src_reg_id = 6;
        else if (strcmp(inst->op2, "rdi") == 0) src_reg_id = 7;

        int dst_reg_id = -1;
        if (strcmp(inst->op1, "rax") == 0) dst_reg_id = 0;
        else if (strcmp(inst->op1, "rcx") == 0) dst_reg_id = 1;
        else if (strcmp(inst->op1, "rdx") == 0) dst_reg_id = 2;
        else if (strcmp(inst->op1, "rbx") == 0) dst_reg_id = 3;
        else if (strcmp(inst->op1, "rsi") == 0) dst_reg_id = 6;
        else if (strcmp(inst->op1, "rdi") == 0) dst_reg_id = 7;

        if (dst_reg_id != -1 && src_reg_id != -1) {
            if (bin_buffer) {
                bin_buffer[(*bin_idx)++] = 0x48;
                bin_buffer[(*bin_idx)++] = 0x89;
                bin_buffer[(*bin_idx)++] = (uint8_t)(0xC0 | (src_reg_id << 3) | dst_reg_id);
                nano_print("  [ASM] mov reg, reg -> opcod generat\n");
            }
            return 3;
        }

        // mov rax, rdi
        if (strcmp(inst->op1, "rax") == 0 && strcmp(inst->op2, "rdi") == 0) {
            if (bin_buffer) {
                bin_buffer[(*bin_idx)++] = 0x48;
                bin_buffer[(*bin_idx)++] = 0x89;
                bin_buffer[(*bin_idx)++] = 0xF8;
                nano_print("  [ASM] mov rax, rdi -> opcod generat\n");
            }
            return 3;
        }

        // mov reg, valoare_imediata (ex: mov rbx, 3)
        if (strcmp(inst->op1, "rax") == 0 || strcmp(inst->op1, "rdi") == 0 || 
            strcmp(inst->op1, "rsi") == 0 || strcmp(inst->op1, "rdx") == 0 ||
            strcmp(inst->op1, "rbx") == 0) {
            
            if (inst->op2[0] != '[') {
                int val = atoi(inst->op2);
                uint8_t reg_code = 0xC0; // rax
                if (strcmp(inst->op1, "rdi") == 0) reg_code = 0xC7;
                else if (strcmp(inst->op1, "rsi") == 0) reg_code = 0xC6;
                else if (strcmp(inst->op1, "rdx") == 0) reg_code = 0xC2;
                else if (strcmp(inst->op1, "rbx") == 0) reg_code = 0xC3;

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0x48;
                    bin_buffer[(*bin_idx)++] = 0xC7;
                    bin_buffer[(*bin_idx)++] = reg_code;
                    bin_buffer[(*bin_idx)++] = (uint8_t)(val & 0xFF);
                    bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 8) & 0xFF);
                    bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 16) & 0xFF);
                    bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 24) & 0xFF);
                    nano_print("  [ASM] mov reg, val -> opcod generat\n");
                }
                return 7;
            }
        }

        // mov reg, [sursa]
        if (inst->op2[0] == '[') {
            char inner[32];
            int m_i = 0, m_j = 0;
            while (inst->op2[m_i] != '\0') {
                if (inst->op2[m_i] != '[' && inst->op2[m_i] != ']') {
                    inner[m_j++] = inst->op2[m_i];
                }
                m_i++;
            }
            inner[m_j] = '\0';

            int src_reg = -1;
            if (strcmp(inner, "rax") == 0) src_reg = 0;
            else if (strcmp(inner, "rbx") == 0) src_reg = 3;
            else if (strcmp(inner, "rdi") == 0) src_reg = 7;
            else if (strcmp(inner, "rsi") == 0) src_reg = 6;
            else if (strcmp(inner, "rdx") == 0) src_reg = 2;

            if (src_reg != -1) {
                int dest_reg = -1;
                if (strcmp(inst->op1, "rax") == 0) dest_reg = 0;
                else if (strcmp(inst->op1, "rbx") == 0) dest_reg = 3;
                else if (strcmp(inst->op1, "rdi") == 0) dest_reg = 7;
                else if (strcmp(inst->op1, "rsi") == 0) dest_reg = 6;
                else if (strcmp(inst->op1, "rdx") == 0) dest_reg = 2;

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0x48;
                    bin_buffer[(*bin_idx)++] = 0x8B;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((dest_reg << 3) | src_reg);
                    nano_print("  [ASM] mov reg, [reg] -> opcod generat\n");
                }
                return 3;
            } else {
                int target_offset = -1;
                for (int l = 0; l < label_count; l++) {
                    if (strcmp(labels[l].name, inner) == 0) {
                        target_offset = labels[l].offset;
                        break;
                    }
                }

                int reg_code = 0;
                if (strcmp(inst->op1, "rax") == 0) reg_code = 0;
                else if (strcmp(inst->op1, "rbx") == 0) reg_code = 3;
                else if (strcmp(inst->op1, "rdi") == 0) reg_code = 7;
                else if (strcmp(inst->op1, "rsi") == 0) reg_code = 6;
                else if (strcmp(inst->op1, "rdx") == 0) reg_code = 2;

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0x48;
                    bin_buffer[(*bin_idx)++] = 0x8B;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((reg_code << 3) | 0x05);

                    int disp_pos = *bin_idx;
                    *bin_idx += 4;
                    int32_t disp32 = target_offset - (disp_pos + 4);

                    bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                    bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                    bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                    bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                    nano_print("  [ASM] mov reg, [var] (RIP-relative) -> opcod generat\n");
                }
                return 7;
            }
        }

        // mov [destinatie], reg
        if (inst->op1[0] == '[') {
            char inner[32];
            int m_i = 0, m_j = 0;
            while (inst->op1[m_i] != '\0') {
                if (inst->op1[m_i] != '[' && inst->op1[m_i] != ']') {
                    inner[m_j++] = inst->op1[m_i];
                }
                m_i++;
            }
            inner[m_j] = '\0';

            int dest_reg = -1;
            if (strcmp(inner, "rax") == 0) dest_reg = 0;
            else if (strcmp(inner, "rbx") == 0) dest_reg = 3;
            else if (strcmp(inner, "rdi") == 0) dest_reg = 7;
            else if (strcmp(inner, "rsi") == 0) dest_reg = 6;
            else if (strcmp(inner, "rdx") == 0) dest_reg = 2;

            if (dest_reg != -1) {
                int src_reg = -1;
                if (strcmp(inst->op2, "rax") == 0) src_reg = 0;
                else if (strcmp(inst->op2, "rbx") == 0) src_reg = 3;
                else if (strcmp(inst->op2, "rdi") == 0) src_reg = 7;
                else if (strcmp(inst->op2, "rsi") == 0) src_reg = 6;
                else if (strcmp(inst->op2, "rdx") == 0) src_reg = 2;

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0x48;
                    bin_buffer[(*bin_idx)++] = 0x89;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((src_reg << 3) | dest_reg);
                    nano_print("  [ASM] mov [reg], reg -> opcod generat\n");
                }
                return 3;
            } else {
                int target_offset = -1;
                for (int l = 0; l < label_count; l++) {
                    if (strcmp(labels[l].name, inner) == 0) {
                        target_offset = labels[l].offset;
                        break;
                    }
                }

                int src_reg = 0;
                if (strcmp(inst->op2, "rax") == 0) src_reg = 0;
                else if (strcmp(inst->op2, "rbx") == 0) src_reg = 3;
                else if (strcmp(inst->op2, "rdi") == 0) src_reg = 7;
                else if (strcmp(inst->op2, "rsi") == 0) src_reg = 6;
                else if (strcmp(inst->op2, "rdx") == 0) src_reg = 2;

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0x48;
                    bin_buffer[(*bin_idx)++] = 0x89;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((src_reg << 3) | 0x05);

                    int disp_pos = *bin_idx;
                    *bin_idx += 4;
                    int32_t disp32 = target_offset - (disp_pos + 4);

                    bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                    bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                    bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                    bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                    nano_print("  [ASM] mov [var], reg (RIP-relative) -> opcod generat\n");
                }
                return 7;
            }
        }
    }

    // --- CMP ---
    if (strcmp(inst->mnemonic, "cmp") == 0) {
        if (strcmp(inst->op1, "rdi") == 0) {
            int val = atoi(inst->op2);
            if (bin_buffer) {
                bin_buffer[(*bin_idx)++] = 0x48;
                bin_buffer[(*bin_idx)++] = 0x81;
                bin_buffer[(*bin_idx)++] = 0xFF;
                bin_buffer[(*bin_idx)++] = (uint8_t)(val & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 8) & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 16) & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 24) & 0xFF);
                nano_print("  [ASM] cmp rdi, val -> opcod generat\n");
            }
            return 7;
        }
    }

    // --- PRINT_RAX ---
    if (strcmp(inst->mnemonic, "print_rax") == 0) {
        // Un algoritm complet itoa (Integer to ASCII) compilat în cod mașină nativ x86_64!
        uint8_t itoa_linux[] = {
            0x50, 0x51, 0x52, 0x57, 0x56, 0x41, 0x50, 0x41, 0x51, // push la toți registrii
            0x49, 0x89, 0xE0, 0xB9, 0x00, 0x00, 0x00, 0x00,       // salvăm stiva, setăm contorul la 0
            0x41, 0xB9, 0x0A, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC0, // setăm divizorul la 10 și testăm dacă e 0
            0x75, 0x0B, 0x48, 0xFF, 0xCC, 0xC6, 0x04, 0x24, 0x30, // dacă e 0, punem direct caracterul '0'
            0xFF, 0xC1, 0xEB, 0x18, 0x48, 0x85, 0xC0, 0x74, 0x13, 
            0x48, 0x31, 0xD2, 0x49, 0xF7, 0xF1, 0x80, 0xC2, 0x30, // bucla divizării prin 10
            0x48, 0xFF, 0xCC, 0x88, 0x14, 0x24, 0xFF, 0xC1, 0xEB, 
            0xE8, 0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00, 0x48, // mov rax, 1 (sys_write)
            0xC7, 0xC7, 0x01, 0x00, 0x00, 0x00, 0x48, 0x89, 0xE6, // mov rdi, 1 (stdout)
            0x48, 0x89, 0xCA, 0x0F, 0x05, 0x4C, 0x89, 0xC4, 0x41, // syscall și restaurare stivă
            0x59, 0x41, 0x58, 0x5E, 0x5F, 0x5A, 0x59, 0x58        // pop înapoi la registrii originali
        };

        int total_size = g_generate_elf ? sizeof(itoa_linux) : 15;

        if (bin_buffer) {
            if (g_generate_elf) {
                // Pentru ELF Linux, injectăm algoritmul brut itoa
                for (int b = 0; b < sizeof(itoa_linux); b++) {
                    bin_buffer[(*bin_idx)++] = itoa_linux[b];
                }
            } else {
                // Original pentru Nano OS (int 0x80)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x89; bin_buffer[(*bin_idx)++] = 0xC7;
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x10; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x89; bin_buffer[(*bin_idx)++] = 0xF8;
            }
            nano_print("  [ASM] print_rax -> opcod generat\n");
        }
        return total_size;
    }

    // --- EXIT ---
    if (strcmp(inst->mnemonic, "exit") == 0) {
        if (bin_buffer) {
            if (g_generate_elf) {
                // mov rax, 60 (sys_exit pe Linux)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x3C; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; // 60 = 0x3C
                // mov rdi, 0 (cod de retur 0)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC7;
                bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                // syscall
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x05;
            } else {
                // Exit original pentru Nano OS
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x14; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC7;
                bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
                bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;
            }
            nano_print("  [ASM] exit -> opcod generat\n");
        }
        return g_generate_elf ? 16 : 16;
    }

    // --- JMP CONDIȚIONALE ȘI NECONDIȚIONALE ---
    if (strcmp(inst->mnemonic, "jmp") == 0 || strcmp(inst->mnemonic, "je") == 0 ||  
        strcmp(inst->mnemonic, "jne") == 0 || strcmp(inst->mnemonic, "jg") == 0 ||
        strcmp(inst->mnemonic, "jl") == 0 || strcmp(inst->mnemonic, "jge") == 0 ||
        strcmp(inst->mnemonic, "jle") == 0 || strcmp(inst->mnemonic, "jb") == 0 || 
        strcmp(inst->mnemonic, "ja") == 0 || strcmp(inst->mnemonic, "call") == 0) {
        
        int is_je  = (strcmp(inst->mnemonic, "je") == 0);
        int is_jne = (strcmp(inst->mnemonic, "jne") == 0);
        int is_jg  = (strcmp(inst->mnemonic, "jg") == 0);
        int is_jl  = (strcmp(inst->mnemonic, "jl") == 0);
        int is_jge = (strcmp(inst->mnemonic, "jge") == 0);
        int is_jle = (strcmp(inst->mnemonic, "jle") == 0);
        int is_jb  = (strcmp(inst->mnemonic, "jb") == 0);
        int is_ja  = (strcmp(inst->mnemonic, "ja") == 0);
        int is_call = (strcmp(inst->mnemonic, "call") == 0);
        int is_jmp = (strcmp(inst->mnemonic, "jmp") == 0);
        
        // Dimensiunea EXACTĂ: 
        // JMP / CALL = 5 octeți (E9 / E8 + 4 octeți offset)
        // Jcc (toate cele condiționale) = 6 octeți (0F 8X + 4 octeți offset)
        int total_size = is_jmp || is_call ? 5 : 6;

        if (bin_buffer) {
            int target_offset = -1;
            for (int l = 0; l < label_count; l++) {
                if (strcmp(labels[l].name, inst->op1) == 0) {
                    target_offset = labels[l].offset;
                    break;
                }
            }

            if (target_offset == -1) {
                nano_print("  [Eroare ASM] Eticheta nu a fost gasita la Pasul 2: ");
                nano_print(inst->op1);
                nano_print("\n");
                target_offset = *bin_idx + total_size; 
            }

            // Scriem opcodul corespunzător
            if (is_je) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x84;
            } else if (is_jne) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x85;
            } else if (is_jg) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x8F;
            } else if (is_jl) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x8C;
            } else if (is_jge) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x8D;
            } else if (is_jle) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x8E;
            } else if (is_jb) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x82;
            } else if (is_ja) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x87;
            } else if (is_call) {
                bin_buffer[(*bin_idx)++] = 0xE8;
            } else if (is_jmp) {
                bin_buffer[(*bin_idx)++] = 0xE9;
            }
            
            int rel_pos = *bin_idx;
            *bin_idx += 4;
            
            int32_t rel32 = target_offset - (rel_pos + 4);
            
            bin_buffer[rel_pos + 0] = (uint8_t)(rel32 & 0xFF);
            bin_buffer[rel_pos + 1] = (uint8_t)((rel32 >> 8) & 0xFF);
            bin_buffer[rel_pos + 2] = (uint8_t)((rel32 >> 16) & 0xFF);
            bin_buffer[rel_pos + 3] = (uint8_t)((rel32 >> 24) & 0xFF);
            
            nano_print("  [ASM] Salt generat. Displacement: ");
            char disp_str[16];
            itoa(rel32, disp_str, 10);
            nano_print(disp_str);
            nano_print("\n");
        }
        
        return total_size;
    }
    
    // --- DIRECTIVĂ DATE (quad / data) ---
    if (strcmp(inst->mnemonic, "quad") == 0 || strcmp(inst->mnemonic, "data") == 0) {
        if (inst->op1[0] == '"') {
            char str_val[128];
            int s_idx = 0;
            char* p = inst->op1 + 1;
            while (*p != '"' && *p != '\0' && s_idx < 127) {
                if (*p == '\\' && *(p + 1) == 'n') {
                    str_val[s_idx++] = '\n';
                    p += 2;
                } else {
                    str_val[s_idx++] = *p++;
                }
            }
            str_val[s_idx] = '\0';
            int str_len = s_idx + 1;

            if (bin_buffer) {
                for (int b = 0; b < str_len; b++) {
                    bin_buffer[(*bin_idx)++] = (uint8_t)str_val[b];
                }
                nano_print("  [ASM] directivă data string -> octeți generați\n");
            }
            return str_len;
        } else {
            int has_dot = 0;
            for (int k = 0; inst->op1[k] != '\0'; k++) {
                if (inst->op1[k] == '.') { has_dot = 1; break; }
            }

            if (has_dot) {
                float f_val = nano_parse_float(inst->op1);
                union { float f; uint32_t u; } cast;
                cast.f = f_val;

                if (bin_buffer) {
                    for (int b = 0; b < 4; b++) {
                        bin_buffer[(*bin_idx)++] = (uint8_t)((cast.u >> (b * 8)) & 0xFF);
                    }
                    nano_print("  [ASM] directivă float (32-bit) -> 4 octeți generați\n");
                }
                return 4;
            } else {
                int64_t val = parse_int64(inst->op1);
                if (bin_buffer) {
                    for (int b = 0; b < 8; b++) {
                        bin_buffer[(*bin_idx)++] = (uint8_t)((val >> (b * 8)) & 0xFF);
                    }
                    nano_print("  [ASM] directivă data număr -> 8 octeți generați\n");
                }
                return 8;
            }
        }
    }

   // --- MOVSS (Float Load / Store / Move) ---
    if (strcmp(inst->mnemonic, "movss") == 0) {
        
        // 1. Mutare între regiștri XMM (ex: movss xmm1, xmm0)
        if (inst->op1[0] == 'x' && inst->op1[1] == 'm' && inst->op1[2] == 'm' &&
            inst->op2[0] == 'x' && inst->op2[1] == 'm' && inst->op2[2] == 'm') {
            
            int xmm_dest = inst->op1[3] - '0';
            int xmm_src  = inst->op2[3] - '0';

            if (xmm_dest >= 0 && xmm_dest <= 7 && xmm_src >= 0 && xmm_src <= 7) {
                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0xF3;
                    bin_buffer[(*bin_idx)++] = 0x0F;
                    bin_buffer[(*bin_idx)++] = 0x10;
                    bin_buffer[(*bin_idx)++] = (uint8_t)(0xC0 | (xmm_dest << 3) | xmm_src);
                    nano_print("  [ASM] movss xmm, xmm -> opcod generat\n");
                }
                return 4;
            }
        }

        // 2. Încărcare din memorie în registru XMM (ex: movss xmm0, [var])
        if (inst->op2[0] == '[') {
            char reg_name[16];
            strcpy(reg_name, inst->op1);

            int xmm_code = -1;
            if (reg_name[0] == 'x' && reg_name[1] == 'm' && reg_name[2] == 'm') {
                xmm_code = reg_name[3] - '0';
            }

            if (xmm_code >= 0 && xmm_code <= 7) {
                char mem_name[32];
                int m_i = 0, m_j = 0;
                while (inst->op2[m_i] != '\0') {
                    if (inst->op2[m_i] != '[' && inst->op2[m_i] != ']') {
                        mem_name[m_j++] = inst->op2[m_i];
                    }
                    m_i++;
                }
                mem_name[m_j] = '\0';

                int target_offset = -1;
                for (int l = 0; l < label_count; l++) {
                    if (strcmp(labels[l].name, mem_name) == 0) {
                        target_offset = labels[l].offset;
                        break;
                    }
                }

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0xF3;
                    bin_buffer[(*bin_idx)++] = 0x0F;
                    bin_buffer[(*bin_idx)++] = 0x10;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((xmm_code << 3) | 0x05);

                    int disp_pos = *bin_idx;
                    *bin_idx += 4;
                    int32_t disp32 = target_offset - (disp_pos + 4);

                    bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                    bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                    bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                    bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                    nano_print("  [ASM] movss xmm, [var] -> opcod generat\n");
                }
                return 8;
            }
        }
        
        // 3. Salvare din registru XMM în memorie (ex: movss [var], xmm0)
        if (inst->op1[0] == '[') {
            char reg_name[16];
            strcpy(reg_name, inst->op2);

            int xmm_code = -1;
            if (reg_name[0] == 'x' && reg_name[1] == 'm' && reg_name[2] == 'm') {
                xmm_code = reg_name[3] - '0';
            }

            if (xmm_code >= 0 && xmm_code <= 7) {
                char mem_name[32];
                int m_i = 0, m_j = 0;
                while (inst->op1[m_i] != '\0') {
                    if (inst->op1[m_i] != '[' && inst->op1[m_i] != ']') {
                        mem_name[m_j++] = inst->op1[m_i];
                    }
                    m_i++;
                }
                mem_name[m_j] = '\0';

                int target_offset = -1;
                for (int l = 0; l < label_count; l++) {
                    if (strcmp(labels[l].name, mem_name) == 0) {
                        target_offset = labels[l].offset;
                        break;
                    }
                }

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0xF3;
                    bin_buffer[(*bin_idx)++] = 0x0F;
                    bin_buffer[(*bin_idx)++] = 0x11;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((xmm_code << 3) | 0x05);

                    int disp_pos = *bin_idx;
                    *bin_idx += 4;
                    int32_t disp32 = target_offset - (disp_pos + 4);

                    bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                    bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                    bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                    bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                    nano_print("  [ASM] movss [var], xmm -> opcod generat\n");
                }
                return 8;
            }
        }
    }
    // --- PRINT_XMM ---
    if (strcmp(inst->mnemonic, "print_xmm") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x66;
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0x0F;
            bin_buffer[(*bin_idx)++] = 0x7E;
            bin_buffer[(*bin_idx)++] = 0xC7;

            bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
            bin_buffer[(*bin_idx)++] = 0x1F; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;

            bin_buffer[(*bin_idx)++] = 0xCD;
            bin_buffer[(*bin_idx)++] = 0x80;

            nano_print("  [ASM] print_xmm -> opcod generat\n");
        }
        return 14; 
    }

    // --- SSE FLOATS: addss, subss, mulss, divss ---
    if (strcmp(inst->mnemonic, "addss") == 0 || strcmp(inst->mnemonic, "subss") == 0 ||
        strcmp(inst->mnemonic, "mulss") == 0 || strcmp(inst->mnemonic, "divss") == 0) {
        
        int is_sub = (strcmp(inst->mnemonic, "subss") == 0);
        int is_mul = (strcmp(inst->mnemonic, "mulss") == 0);
        int is_div = (strcmp(inst->mnemonic, "divss") == 0);
        
        uint8_t opcode = 0x58;
        if (is_sub) opcode = 0x5C;
        else if (is_mul) opcode = 0x59;
        else if (is_div) opcode = 0x5E;

        char reg_name[16];
        strcpy(reg_name, inst->op1);
        int xmm_dest = -1;
        if (reg_name[0] == 'x' && reg_name[1] == 'm' && reg_name[2] == 'm') {
            xmm_dest = reg_name[3] - '0';
        }

        if (xmm_dest >= 0 && xmm_dest <= 7) {
            if (inst->op2[0] == 'x' && inst->op2[1] == 'm' && inst->op2[2] == 'm') {
                int xmm_src = inst->op2[3] - '0';
                if (xmm_src >= 0 && xmm_src <= 7) {
                    if (bin_buffer) {
                        bin_buffer[(*bin_idx)++] = 0xF3;
                        bin_buffer[(*bin_idx)++] = 0x0F;
                        bin_buffer[(*bin_idx)++] = opcode;
                        bin_buffer[(*bin_idx)++] = (uint8_t)((xmm_dest << 3) | xmm_src);
                        nano_print("  [ASM] float op xmm, xmm -> opcod generat\n");
                    }
                    return 4;
                }
            }

            if (inst->op2[0] == '[') {
                char mem_name[32];
                int m_i = 0, m_j = 0;
                while (inst->op2[m_i] != '\0') {
                    if (inst->op2[m_i] != '[' && inst->op2[m_i] != ']') {
                        mem_name[m_j++] = inst->op2[m_i];
                    }
                    m_i++;
                }
                mem_name[m_j] = '\0';

                int target_offset = -1;
                for (int l = 0; l < label_count; l++) {
                    if (strcmp(labels[l].name, mem_name) == 0) {
                        target_offset = labels[l].offset;
                        break;
                    }
                }

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0xF3;
                    bin_buffer[(*bin_idx)++] = 0x0F;
                    bin_buffer[(*bin_idx)++] = opcode;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((xmm_dest << 3) | 0x05);

                    int disp_pos = *bin_idx;
                    *bin_idx += 4;
                    int32_t disp32 = target_offset - (disp_pos + 4);

                    bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                    bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                    bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                    bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                    nano_print("  [ASM] float op xmm, [var] -> opcod generat\n");
                }
                return 8;
            }
        }
    }

    // --- UCOMISS (Comparare Float) ---
    if (strcmp(inst->mnemonic, "ucomiss") == 0) {
        char reg_name[16];
        strcpy(reg_name, inst->op1);
        int xmm_dest = -1;
        if (reg_name[0] == 'x' && reg_name[1] == 'm' && reg_name[2] == 'm') {
            xmm_dest = reg_name[3] - '0';
        }

        if (xmm_dest >= 0 && xmm_dest <= 7) {
            if (inst->op2[0] == 'x' && inst->op2[1] == 'm' && inst->op2[2] == 'm') {
                int xmm_src = inst->op2[3] - '0';
                if (xmm_src >= 0 && xmm_src <= 7) {
                    if (bin_buffer) {
                        bin_buffer[(*bin_idx)++] = 0x0F;
                        bin_buffer[(*bin_idx)++] = 0x2E;
                        bin_buffer[(*bin_idx)++] = (uint8_t)(0xC0 | (xmm_dest << 3) | xmm_src);
                        nano_print("  [ASM] ucomiss xmm, xmm -> opcod generat\n");
                    }
                    return 3;
                }
            }

            if (inst->op2[0] == '[') {
                char mem_name[32];
                int m_i = 0, m_j = 0;
                while (inst->op2[m_i] != '\0') {
                    if (inst->op2[m_i] != '[' && inst->op2[m_i] != ']') {
                        mem_name[m_j++] = inst->op2[m_i];
                    }
                    m_i++;
                }
                mem_name[m_j] = '\0';

                int target_offset = -1;
                for (int l = 0; l < label_count; l++) {
                    if (strcmp(labels[l].name, mem_name) == 0) {
                        target_offset = labels[l].offset;
                        break;
                    }
                }

                if (bin_buffer) {
                    bin_buffer[(*bin_idx)++] = 0x0F;
                    bin_buffer[(*bin_idx)++] = 0x2E;
                    bin_buffer[(*bin_idx)++] = (uint8_t)((xmm_dest << 3) | 0x05);

                    int disp_pos = *bin_idx;
                    *bin_idx += 4;
                    int32_t disp32 = target_offset - (disp_pos + 4);

                    bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                    bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                    bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                    bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                    nano_print("  [ASM] ucomiss xmm, [var] -> opcod generat\n");
                }
                return 7;
            }
        }
    }

    // --- NOU: Instrucțiuni matematice x86-64 ---
    if (strcmp(inst->mnemonic, "imul") == 0 && strcmp(inst->op1, "rbx") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0xF7;
            bin_buffer[(*bin_idx)++] = 0xEB;
        }
        return 3;
    }

    if (strcmp(inst->mnemonic, "cqo") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0x99;
        }
        return 2;
    }

    if (strcmp(inst->mnemonic, "idiv") == 0 && strcmp(inst->op1, "rbx") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0xF7;
            bin_buffer[(*bin_idx)++] = 0xFB;
        }
        return 3;
    }

    if (strcmp(inst->mnemonic, "add") == 0 && strcmp(inst->op1, "rax") == 0 && strcmp(inst->op2, "rbx") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0x01;
            bin_buffer[(*bin_idx)++] = 0xD8;
        }
        return 3;
    }

    if (strcmp(inst->mnemonic, "sub") == 0 && strcmp(inst->op1, "rax") == 0 && strcmp(inst->op2, "rbx") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0x29;
            bin_buffer[(*bin_idx)++] = 0xD8;
        }
        return 3;
    }

    if (strcmp(inst->mnemonic, "mov") == 0 && strcmp(inst->op1, "rax") == 0 && strcmp(inst->op2, "rdx") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0x89;
            bin_buffer[(*bin_idx)++] = 0xD0;
        }
        return 3;
    }

    return -1;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        nano_print("Utilizare: asm [-elf] <sursa.s> [iesire]\n");
        return 0;
    }

	int generate_elf = 0;	
    const char* src_file = 0;
    const char* out_file = 0;

    // Parsăm argumentele pentru a detecta flag-ul -elf
    for (int a = 1; a < argc; a++) {
        if (strcmp(argv[a], "-elf") == 0) {
            generate_elf = 1;
			g_generate_elf = 1;
        } else if (!src_file) {
            src_file = argv[a];
        } else if (!out_file) {
            out_file = argv[a];
        }
    }

    if (!src_file) {
        nano_print("Eroare: Nu ai specificat fisierul sursa!\n");
        return 0;
    }
    if (!out_file) {
        out_file = generate_elf ? "a.out" : "out";
    }

    if (generate_elf) {
        nano_print("NanoASM v2.0 -> Mod Generare: Executabil Linux ELF64\n");
    } else {
        nano_print("NanoASM v2.0 -> Mod Generare: Nativ Nano OS (NAS1)\n");
    }
    
    nano_print("NanoASM v2.0 (Arhitectura cu Tokeni si Functie Unica)\n");

    uint8_t* src_buffer = (uint8_t*)nano_malloc(8192);
    if (!src_buffer) {
        nano_print("Eroare: Fara memorie pentru src_buffer!\n");
        return 0;
    }
    
    int bytes_read = nano_read_file(src_file, src_buffer, 8191);
    if (bytes_read <= 0) {
        nano_print("Eroare la citirea fisierului sursa!\n");
        nano_free(src_buffer);
        return 0;
    }
    src_buffer[bytes_read] = '\0';

    Label labels[32];
    int label_count = 0;

    // Setăm offset-ul de start în funcție de format (ELF = 120 octeți antet, NAS1 = 8 octeți)
    int start_offset = generate_elf ? 120 : 8;

    // --- PASUL 1: Scanare etichete și calculare dimensiuni ---
    nano_print("[PASUL 1] Scanare etichete si dimensiuni...\n");
    {
        int p_bin_idx = start_offset;
        int line_start = 0;
        for (int i = 0; i <= bytes_read; i++) {
            if (src_buffer[i] == '\n' || src_buffer[i] == '\0') {
                char raw_line[128];
                int len = i - line_start;
                if (len >= 127) len = 127;
                memcpy(raw_line, &src_buffer[line_start], len);
                raw_line[len] = '\0';
                line_start = i + 1;

                char clean[128];
                clean_line(raw_line, clean);
                if (clean[0] == '\0') continue;

                Instruction inst;
                parse_instruction(clean, &inst);

                int c_len = strlen(inst.mnemonic);
                if (c_len > 0 && inst.mnemonic[c_len - 1] == ':') {
                    inst.mnemonic[c_len - 1] = '\0';
                    strcpy(labels[label_count].name, inst.mnemonic);
                    labels[label_count].offset = p_bin_idx;
                    label_count++;

                    nano_print("  [DEBUG] Eticheta gasita: '");
                    nano_print(inst.mnemonic);
                    nano_print("' la offset-ul ");
                    char buf[16];
                    itoa(p_bin_idx, buf, 10);
                    nano_print(buf);
                    nano_print("\n");

                    // Dacă există instrucțiune pe aceeași linie după etichetă (ex: var_x: data 0)
                    if (inst.op1[0] != '\0') {
                        strcpy(inst.mnemonic, inst.op1);
                        strcpy(inst.op1, inst.op2);
                        inst.op2[0] = '\0';
                    } else {
                        continue;
                    }
                }

                int size = process_instruction(&inst, NULL, &p_bin_idx, labels, label_count);
                if (size < 0) {
                    nano_print("Eroare sintaxa (Pas 1): ");
                    nano_print(clean);
                    nano_print("\n");
                    nano_free(src_buffer);
                    return 0;
                }

                nano_print("  [DEBUG] Linie: '");
                nano_print(clean);
                nano_print("' | Dimensiune: ");
                char size_buf[16];
                itoa(size, size_buf, 10);
                nano_print(size_buf);
                nano_print(" octeti\n");

                p_bin_idx += size;
            }
        }
    }

    // --- PASUL 2: Generare binar ---
    nano_print("[PASUL 2] Generare binar...\n");
    uint8_t* bin_buffer = (uint8_t*)nano_malloc(16384);
    if (!bin_buffer) {
        nano_print("Eroare: Fara memorie pentru bin_buffer!\n");
        nano_free(src_buffer);
        return 0;
    }
    int bin_idx = start_offset;

    // Scriem antetul corespunzător în funcție de flag-ul -elf
    if (generate_elf) {
        Elf64_Ehdr* ehdr = (Elf64_Ehdr*)bin_buffer;
        ehdr->e_ident[0] = 0x7F; ehdr->e_ident[1] = 'E'; ehdr->e_ident[2] = 'L'; ehdr->e_ident[3] = 'F';
        ehdr->e_ident[4] = 2; // 64-bit
        ehdr->e_ident[5] = 1; // Little-endian
        ehdr->e_ident[6] = 1; // ELF Version
        ehdr->e_type = 2;     // Executable
        ehdr->e_machine = 0x3E; // AMD x86-64
        ehdr->e_version = 1;
        ehdr->e_entry = 0x400000 + 120; // Adresa de start în memorie
        ehdr->e_phoff = 64;   // Program header imediat după ELF header
        ehdr->e_ehsize = sizeof(Elf64_Ehdr);
        ehdr->e_phentsize = sizeof(Elf64_Phdr);
        ehdr->e_phnum = 1;

        Elf64_Phdr* phdr = (Elf64_Phdr*)(bin_buffer + 64);
        phdr->p_type = 1; // PT_LOAD
        phdr->p_flags = 7; // R-X (Read + Execute)
        phdr->p_offset = 0;
        phdr->p_vaddr = 0x400000;
        phdr->p_paddr = 0x400000;
        phdr->p_align = 0x200000;
    } else {
        NanoHeader* hdr = (NanoHeader*)bin_buffer;
        hdr->magic[0] = 'N'; hdr->magic[1] = 'A'; hdr->magic[2] = 'S'; hdr->magic[3] = '1';
        hdr->entry_offset = 8;
    }

    {
        int line_start = 0;
        for (int i = 0; i <= bytes_read; i++) {
            if (src_buffer[i] == '\n' || src_buffer[i] == '\0') {
                char raw_line[128];
                int len = i - line_start;
                if (len >= 127) len = 127;
                memcpy(raw_line, &src_buffer[line_start], len);
                raw_line[len] = '\0';
                line_start = i + 1;

                char clean[128];
                clean_line(raw_line, clean);
                if (clean[0] == '\0') continue;

                Instruction inst;
                parse_instruction(clean, &inst);

                int c_len = strlen(inst.mnemonic);
                if (c_len > 0 && inst.mnemonic[c_len - 1] == ':') {
                    if (inst.op1[0] != '\0') {
                        strcpy(inst.mnemonic, inst.op1);
                        strcpy(inst.op1, inst.op2);
                        inst.op2[0] = '\0';
                    } else {
                        continue;
                    }
                }

                int size = process_instruction(&inst, bin_buffer, &bin_idx, labels, label_count);
                if (size < 0) {
                    nano_print("Eroare sintaxa (Pas 2): ");
                    nano_print(clean);
                    nano_print("\n");
                    nano_free(bin_buffer);
                    nano_free(src_buffer);
                    return 0;
                }
            }
        }
    }

    // Dacă suntem în mod ELF, actualizăm dimensiunile în Program Header
    if (generate_elf) {
        Elf64_Phdr* phdr = (Elf64_Phdr*)(bin_buffer + 64);
        phdr->p_filesz = bin_idx;
        phdr->p_memsz = bin_idx;
    }

    // --- PASUL 3: Salvare fișier pe disc ---
    int create_res = nano_create_file(out_file, bin_idx);
    nano_print("  [DEBUG] nano_create_file a returnat: ");
    char res_buf[16];
    itoa(create_res, res_buf, 10);
    nano_print(res_buf);
    nano_print("\n");

    if (create_res >= 0) {
        nano_print("Creat cu succes!\n");
        
        int write_res = nano_write_file(out_file, bin_buffer, bin_idx);
        nano_print("  [DEBUG] nano_write_file a returnat codul: ");
        itoa(write_res, res_buf, 10);
        nano_print(res_buf);
        nano_print("\n");
        
        nano_print("Asamblare v2.0 finalizata cu succes!\n");
    } else {
        nano_print("Eroare la crearea fisierului binar!\n");
    }

    // Eliberare memorie alocată dinamic
    nano_free(bin_buffer);
    nano_free(src_buffer);

    return 0;
}
    */