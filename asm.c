#include "nano_libc.h"


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
        if (inst->op1[0] == '[') {
            char label_name[32];
            int l_i = 0, l_j = 0;
            while (inst->op1[l_i] != '\0') {
                if (inst->op1[l_i] != '[' && inst->op1[l_i] != ']') {
                    label_name[l_j++] = inst->op1[l_i];
                }
                l_i++;
            }
            label_name[l_j] = '\0';

            int target_offset = -1;
            for (int l = 0; l < label_count; l++) {
                if (strcmp(labels[l].name, label_name) == 0) {
                    target_offset = labels[l].offset;
                    break;
                }
            }

            int total_size = 16;
            if (bin_buffer) {
                // 1. mov rax, 1 (SYSCALL_PRINT)
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
                bin_buffer[(*bin_idx)++] = 0x01; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;

                // 2. lea rdi, [rip + disp32]
                bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x8D; bin_buffer[(*bin_idx)++] = 0x3D;
                int disp_pos = *bin_idx;
                *bin_idx += 4;

                // 3. int 0x80
                bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;

                int32_t disp32 = target_offset - (disp_pos + 4);
                bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                nano_print("  [ASM] print [label] (RIP-relative) -> opcod generat\n");
            }
            return total_size;
        }
        
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
        int final_str_len = m_idx + 1;

        int total_size = 7 + 7 + 2 + 2 + final_str_len;
        if (bin_buffer) {
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

            nano_print("  [ASM] print \"...\" -> opcod generat\n");
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
            bin_buffer[(*bin_idx)++] = 0x50; // push rax
            
            bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
            bin_buffer[(*bin_idx)++] = 0x1A; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
            bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;
            
            bin_buffer[(*bin_idx)++] = 0x58; // pop rax
            
            nano_print("  [ASM] newline -> opcod generat (cu protejare RAX)\n");
        }
        return 11;
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
        if (strcmp(inst->op1, "rax") == 0 && strcmp(inst->op2, "rdi") == 0) {
            if (bin_buffer) {
                bin_buffer[(*bin_idx)++] = 0x48;
                bin_buffer[(*bin_idx)++] = 0x89;
                bin_buffer[(*bin_idx)++] = 0xF8;
                nano_print("  [ASM] mov rax, rdi -> opcod generat\n");
            }
            return 3;
        }

        if (strcmp(inst->op1, "rax") == 0 || strcmp(inst->op1, "rdi") == 0 || 
            strcmp(inst->op1, "rsi") == 0 || strcmp(inst->op1, "rdx") == 0) {
            
            if (inst->op2[0] != '[') {
                int val = atoi(inst->op2);
                uint8_t reg_code = 0xC0;
                if (strcmp(inst->op1, "rdi") == 0) reg_code = 0xC7;
                else if (strcmp(inst->op1, "rsi") == 0) reg_code = 0xC6;
                else if (strcmp(inst->op1, "rdx") == 0) reg_code = 0xC2;

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
            else if (strcmp(inner, "rdi") == 0) src_reg = 7;
            else if (strcmp(inner, "rsi") == 0) src_reg = 6;
            else if (strcmp(inner, "rdx") == 0) src_reg = 2;

            if (src_reg != -1) {
                int dest_reg = -1;
                if (strcmp(inst->op1, "rax") == 0) dest_reg = 0;
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
            else if (strcmp(inner, "rdi") == 0) dest_reg = 7;
            else if (strcmp(inner, "rsi") == 0) dest_reg = 6;
            else if (strcmp(inner, "rdx") == 0) dest_reg = 2;

            if (dest_reg != -1) {
                int src_reg = -1;
                if (strcmp(inst->op2, "rax") == 0) src_reg = 0;
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
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x89; bin_buffer[(*bin_idx)++] = 0xC7;
            bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
            bin_buffer[(*bin_idx)++] = 0x10; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
            bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;
            bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0x89; bin_buffer[(*bin_idx)++] = 0xF8;
            nano_print("  [ASM] print_rax -> opcod generat\n");
        }
        return 15;
    }

    // --- EXIT ---
    if (strcmp(inst->mnemonic, "exit") == 0) {
        if (bin_buffer) {
            bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
            bin_buffer[(*bin_idx)++] = 0x14; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
            bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC7;
            bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
            bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;
            nano_print("  [ASM] exit -> opcod generat\n");
        }
        return 16;
    }

    // --- JMP CONDIȚIONALE ȘI NECONDIȚIONALE ---
    if (strcmp(inst->mnemonic, "jmp") == 0 || strcmp(inst->mnemonic, "je") == 0 || 
        strcmp(inst->mnemonic, "jb") == 0 || strcmp(inst->mnemonic, "ja") == 0 ||
        strcmp(inst->mnemonic, "call") == 0) {
        
        int is_je = (strcmp(inst->mnemonic, "je") == 0);
        int is_jb = (strcmp(inst->mnemonic, "jb") == 0);
        int is_ja = (strcmp(inst->mnemonic, "ja") == 0);
        int is_call = (strcmp(inst->mnemonic, "call") == 0);
        
        // Stabilim dimensiunea EXACTĂ a instrucțiunii (esential pentru sincronizarea Pasului 1 cu Pasul 2)
        // JMP rel32 = E9 xx xx xx xx (5 octeți)
        // CALL rel32 = E8 xx xx xx xx (5 octeți)
        // Jcc rel32 = 0F 8x xx xx xx xx (6 octeți)
        int total_size = (is_call || (!is_je && !is_jb && !is_ja)) ? 5 : 6;

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
                // Prevenim crash-ul calculând o distanță de 0
                target_offset = *bin_idx + total_size; 
            }

            if (is_je) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x84;
            } else if (is_jb) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x82;
            } else if (is_ja) {
                bin_buffer[(*bin_idx)++] = 0x0F; bin_buffer[(*bin_idx)++] = 0x87;
            } else if (is_call) {
                bin_buffer[(*bin_idx)++] = 0xE8;
            } else {
                bin_buffer[(*bin_idx)++] = 0xE9; // jmp
            }
            
            // Reținem poziția unde începem să scriem offset-ul de 32 de biți
            int rel_pos = *bin_idx;
            *bin_idx += 4; // Avansăm indexul pentru cei 4 octeți ai displacement-ului
            
            // Calculul displacement-ului (distanța de la sfârșitul instrucțiunii până la etichetă)
            // Sfârșitul instrucțiunii este la rel_pos + 4
            int32_t rel32 = target_offset - (rel_pos + 4);
            
            // Scriem displacement-ul în format Little Endian
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
    
    // --- ADD & SUB ---
    if (strcmp(inst->mnemonic, "add") == 0 || strcmp(inst->mnemonic, "sub") == 0) {
        int is_sub = (strcmp(inst->mnemonic, "sub") == 0);
        if (strcmp(inst->op1, "rax") == 0) {
            int val = atoi(inst->op2);
            uint8_t modrm = is_sub ? 0xE8 : 0xC0;

            if (bin_buffer) {
                bin_buffer[(*bin_idx)++] = 0x48;
                bin_buffer[(*bin_idx)++] = 0x81;
                bin_buffer[(*bin_idx)++] = modrm;
                
                bin_buffer[(*bin_idx)++] = (uint8_t)(val & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 8) & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 16) & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 24) & 0xFF);
                
                nano_print(is_sub ? "  [ASM] sub rax, val -> opcod generat\n" : "  [ASM] add rax, val -> opcod generat\n");
            }
            return 7; // <--- Este crucial să returneze EXACT 7 la Pasul 1 și Pasul 2
        }
    }

    // --- MUL & DIV ---
    if (strcmp(inst->mnemonic, "mul") == 0 || strcmp(inst->mnemonic, "div") == 0) {
        int is_div = (strcmp(inst->mnemonic, "div") == 0);
        if (strcmp(inst->op1, "rax") == 0 || strcmp(inst->op1, "rdi") == 0 || 
            strcmp(inst->op1, "rsi") == 0 || strcmp(inst->op1, "rdx") == 0) {
            
            uint8_t modrm = 0xE0;
            if (is_div) modrm = 0xF0;

            if (strcmp(inst->op1, "rdi") == 0) modrm += 7;
            else if (strcmp(inst->op1, "rsi") == 0) modrm += 6;
            else if (strcmp(inst->op1, "rdx") == 0) modrm += 2;

            if (bin_buffer) {
                bin_buffer[(*bin_idx)++] = 0x48;
                bin_buffer[(*bin_idx)++] = 0x31;
                bin_buffer[(*bin_idx)++] = 0xD2;

                bin_buffer[(*bin_idx)++] = 0x48;
                bin_buffer[(*bin_idx)++] = 0xF7;
                bin_buffer[(*bin_idx)++] = modrm;
                
                nano_print(is_div ? "  [ASM] xor rdx, rdx + div reg -> opcod generat\n" : "  [ASM] xor rdx, rdx + mul reg -> opcod generat\n");
            }
            return 6;
        }
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

    // --- MOVSS (Float Load / Store) ---
    if (strcmp(inst->mnemonic, "movss") == 0) {
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

    return -1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        nano_print("Utilizare: asm <sursa.s> [iesire]\n");
        return 0;
    }

    const char* src_file = argv[1];
    const char* out_file = (argc >= 3) ? argv[2] : "out";

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

    // --- PASUL 1: Scanare etichete și calculare dimensiuni ---
    nano_print("[PASUL 1] Scanare etichete si dimensiuni...\n");
    {
        int p_bin_idx = 8;
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
                    continue;
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
    uint8_t* bin_buffer = (uint8_t*)nano_malloc(8192);
    if (!bin_buffer) {
        nano_print("Eroare: Fara memorie pentru bin_buffer!\n");
        nano_free(src_buffer);
        return 0;
    }
    int bin_idx = 8;

    // Scriem header-ul executabilului Nano OS ('NAS1')
    NanoHeader* hdr = (NanoHeader*)bin_buffer;
    hdr->magic[0] = 'N'; hdr->magic[1] = 'A'; hdr->magic[2] = 'S'; hdr->magic[3] = '1';
    hdr->entry_offset = 8;

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
                if (c_len > 0 && inst.mnemonic[c_len - 1] == ':') continue;

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
