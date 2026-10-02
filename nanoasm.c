#include "nano_libc.h"

// Header-ul binarului pentru Nano OS
/*
typedef struct {
    uint8_t magic[4];       // 'N', 'A', 'S', '1'
    uint32_t entry_offset;  // Offset-ul de intrare (default 8)
} NanoHeader;
*/
// Structură pentru etichete
typedef struct {
    char name[32];
    int offset;
} Label;

// Structură pentru o instrucțiune parsată
typedef struct {
    char mnemonic[16];
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

// Funcție pentru curățarea liniei de spații și comentarii
// Funcție avansată pentru curățarea și sanitizarea liniei
void clean_line(const char* raw, char* dest) {
    int i = 0, j = 0;

    // 1. Trecem peste BOM-ul UTF-8 (EF BB BF) dacă apare la începutul fișierului
    if ((unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF) {
        i += 3;
    }

    // 2. Trecem peste spații, tab-uri, \r sau alte caractere de control de la început
    while (raw[i] != '\0' && (raw[i] == ' ' || raw[i] == '\t' || raw[i] == '\r' || (unsigned char)raw[i] < 32)) {
        if (raw[i] == '\n') break; // Dacă dăm de newline, am terminat
        i++;
    }

    // 3. Copiem conținutul util până la comentariu (';'), sfârșit de linie sau de șir
    while (raw[i] != '\0' && raw[i] != ';' && raw[i] != '\r' && raw[i] != '\n') {
        // Opțional: putem filtra caracterele non-printable din mijloc, 
        // dar păstrăm spațiile și ghilimelele pentru comanda print.
        dest[j++] = raw[i++];
    }

    // 4. Tăiem spațiile, tab-urile sau caracterele de control rămase la sfârșit
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
        if (j < 15) inst->mnemonic[j++] = line[i];
        i++;
    }
    inst->mnemonic[j] = '\0';

    if (inst->mnemonic[0] == '\0') return 0;

    while (line[i] == ' ' || line[i] == '\t') i++;
    if (line[i] == '\0') return 1;
	
	// --- TRATARE SPECIALĂ PENTRU PRINT ---
    if (strcmp(inst->mnemonic, "print") == 0) {
        // Copiem tot restul liniei direct în op1 (inclusiv ghilimelele și spațiile)
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
		
		// Cazul nou: print [eticheta] (Afișare string dintr-o variabilă/etichetă)
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

            // Căutăm eticheta în tabel
            int target_offset = -1;
            for (int l = 0; l < label_count; l++) {
                if (strcmp(labels[l].name, label_name) == 0) {
                    target_offset = labels[l].offset;
                    break;
                }
            }

            // Dimensiune: mov rax, 1 (7) + lea rdi, [rip+disp32] (7) + int 0x80 (2) = 16 octeți
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

                // Calculăm displacement-ul de la RIP
                int32_t disp32 = target_offset - (disp_pos + 4);
                bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                nano_print("  [ASM] print [label] (RIP-relative) -> opcod generat\n");
            }
            return total_size;
        }
		
        // Parsăm șirul dintre ghilimele și gestionăm escape-ul \n
        char msg[128];
        int m_idx = 0;
        char* p = inst->op1;
        
        // Trecem de prima ghilimea dacă există
        if (*p == '"') p++;

        while (*p != '"' && *p != '\0' && m_idx < 127) {
            if (*p == '\\' && *(p + 1) == 'n') {
                msg[m_idx++] = '\n'; // Newline real (0x0A)
                p += 2;
            } else {
                msg[m_idx++] = *p++;
            }
        }
        msg[m_idx] = '\0';
        int final_str_len = m_idx + 1; // Inclusiv terminatorul '\0'

        // Dimensiunea totală calculată:
        // mov rax, 1 (7) + lea rdi, [rip+disp32] (7) + int 0x80 (2) + jmp skip (2) + lungime șir
        int total_size = 7 + 7 + 2 + 2 + final_str_len;

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

            // 4. jmp peste text (EB rel8)
            bin_buffer[(*bin_idx)++] = 0xEB;
            int jmp_rel_pos = (*bin_idx)++;

            // 5. Scriem șirul efectiv în binar
            int string_offset = *bin_idx;
            for (int m = 0; m <= m_idx; m++) {
                bin_buffer[(*bin_idx)++] = (uint8_t)msg[m];
            }

            // Calculăm displacement-ul de la RIP pentru lea
            int32_t disp32 = string_offset - (disp_pos + 4);
            bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
            bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
            bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
            bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

            // Calculăm saltul scurt peste text
            int jmp_target = *bin_idx;
            int jmp_from = jmp_rel_pos + 1;
            bin_buffer[jmp_rel_pos] = (uint8_t)(jmp_target - jmp_from);

            nano_print("  [ASM] print \"...\" -> opcod generat\n");
        }
        return total_size;
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
            bin_buffer[(*bin_idx)++] = 0x50; // push rax (salvăm rax pe stivă)
            
            bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
            bin_buffer[(*bin_idx)++] = 0x1A; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
            bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;
            
            bin_buffer[(*bin_idx)++] = 0x58; // pop rax (restaurăm rax de pe stivă)
            
            nano_print("  [ASM] newline -> opcod generat (cu protejare RAX)\n");
        }
        return 11; // 1 (push) + 7 (mov) + 2 (int) + 1 (pop) = 11 octeți! (Schimbat de la 9)
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
        
        // 1. mov rax, rdi (Registru la registru specific)
        if (strcmp(inst->op1, "rax") == 0 && strcmp(inst->op2, "rdi") == 0) {
            if (bin_buffer) {
                bin_buffer[(*bin_idx)++] = 0x48;
                bin_buffer[(*bin_idx)++] = 0x89;
                bin_buffer[(*bin_idx)++] = 0xF8;
                nano_print("  [ASM] mov rax, rdi -> opcod generat\n");
            }
            return 3;
        }

        // 2. mov reg, val (Valoare imediată)
        if (strcmp(inst->op1, "rax") == 0 || strcmp(inst->op1, "rdi") == 0 || 
            strcmp(inst->op1, "rsi") == 0 || strcmp(inst->op1, "rdx") == 0) {
            
            // Verificăm să nu fie operandul 2 un pointer
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

        // 3. CAZUL CU PARANTEZE: mov reg, [...] sau mov [...], reg
        if (inst->op2[0] == '[') {
            // Extragem conținutul dintre paranteze
            char inner[32];
            int m_i = 0, m_j = 0;
            while (inst->op2[m_i] != '\0') {
                if (inst->op2[m_i] != '[' && inst->op2[m_i] != ']') {
                    inner[m_j++] = inst->op2[m_i];
                }
                m_i++;
            }
            inner[m_j] = '\0';

            // Verificăm dacă este un REGISTRU (ex: [rdi])
            int src_reg = -1;
            if (strcmp(inner, "rax") == 0) src_reg = 0;
            else if (strcmp(inner, "rdi") == 0) src_reg = 7;
            else if (strcmp(inner, "rsi") == 0) src_reg = 6;
            else if (strcmp(inner, "rdx") == 0) src_reg = 2;

            if (src_reg != -1) {
                // ESTE POINTER PE REGISTRU: mov reg, [reg]
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
                // NU ESTE REGISTRU, DECI ESTE O VARIABILĂ/ETICHETĂ: mov reg, [eticheta]
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
            // Extragem conținutul dintre paranteze pentru operandul 1
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
                // ESTE POINTER PE REGISTRU: mov [reg], reg
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
                // NU ESTE REGISTRU, DECI ESTE O VARIABILĂ: mov [eticheta], reg
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

    // --- JMP, JE & CALL ---
    if (strcmp(inst->mnemonic, "jmp") == 0 || strcmp(inst->mnemonic, "je") == 0 || strcmp(inst->mnemonic, "call") == 0) {
        int is_je = (strcmp(inst->mnemonic, "je") == 0);
        int is_call = (strcmp(inst->mnemonic, "call") == 0);
        
        int target_offset = -1;
        for (int l = 0; l < label_count; l++) {
            if (strcmp(labels[l].name, inst->op1) == 0) {
                target_offset = labels[l].offset;
                break;
            }
        }

        if (bin_buffer) {
            if (is_je) {
                bin_buffer[(*bin_idx)++] = 0x0F;
                bin_buffer[(*bin_idx)++] = 0x84;
            } else if (is_call) {
                bin_buffer[(*bin_idx)++] = 0xE8; // Opcodul pentru CALL rel32
            } else {
                bin_buffer[(*bin_idx)++] = 0xE9; // Opcodul pentru JMP rel32
            }
            
            int rel_pos = *bin_idx;
            *bin_idx += 4;

            int32_t rel32 = target_offset - (rel_pos + 4);
            bin_buffer[rel_pos + 0] = (uint8_t)(rel32 & 0xFF);
            bin_buffer[rel_pos + 1] = (uint8_t)((rel32 >> 8) & 0xFF);
            bin_buffer[rel_pos + 2] = (uint8_t)((rel32 >> 16) & 0xFF);
            bin_buffer[rel_pos + 3] = (uint8_t)((rel32 >> 24) & 0xFF);
            
            nano_print("  [ASM] call / jump / je -> etichetă rezolvată\n");
        }
        return is_je ? 6 : 5;
    }
	
   // --- ADD & SUB (rax, imm32) ---
    if (strcmp(inst->mnemonic, "add") == 0 || strcmp(inst->mnemonic, "sub") == 0) {
        int is_sub = (strcmp(inst->mnemonic, "sub") == 0);
        if (strcmp(inst->op1, "rax") == 0) {
            int val = atoi(inst->op2);
            
            // ModR/M: 
            // add rax -> 0xC0 (extensia /0)
            // sub rax -> 0xE8 (extensia /5)
            uint8_t modrm = is_sub ? 0xE8 : 0xC0;

            if (bin_buffer) {
                bin_buffer[(*bin_idx)++] = 0x48; // REX.W
                bin_buffer[(*bin_idx)++] = 0x81; // Opcode
                bin_buffer[(*bin_idx)++] = modrm; // ModR/M
                
                bin_buffer[(*bin_idx)++] = (uint8_t)(val & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 8) & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 16) & 0xFF);
                bin_buffer[(*bin_idx)++] = (uint8_t)((val >> 24) & 0xFF);
                
                nano_print(is_sub ? "  [ASM] sub rax, val -> opcod generat\n" : "  [ASM] add rax, val -> opcod generat\n");
            }
            return 7; // EXACT 7 OCTEȚI (1 + 1 + 1 + 4)
        }
    }
    

    // --- MUL & DIV (reg) ---
    if (strcmp(inst->mnemonic, "mul") == 0 || strcmp(inst->mnemonic, "div") == 0) {
        int is_div = (strcmp(inst->mnemonic, "div") == 0);
        if (strcmp(inst->op1, "rax") == 0 || strcmp(inst->op1, "rdi") == 0 || 
            strcmp(inst->op1, "rsi") == 0 || strcmp(inst->op1, "rdx") == 0) {
            
            uint8_t modrm = 0xE0; // rax pentru mul (/4)
            if (is_div) modrm = 0xF0; // rax pentru div (/6)

            if (strcmp(inst->op1, "rdi") == 0) modrm += 7;
            else if (strcmp(inst->op1, "rsi") == 0) modrm += 6;
            else if (strcmp(inst->op1, "rdx") == 0) modrm += 2;

            if (bin_buffer) {
                // Curățăm automat RDX (xor rdx, rdx -> 3 octeți: 48 31 D2) 
                // pentru a preveni corupția pe 128-biți a lui RDX:RAX!
                bin_buffer[(*bin_idx)++] = 0x48;
                bin_buffer[(*bin_idx)++] = 0x31;
                bin_buffer[(*bin_idx)++] = 0xD2;

                bin_buffer[(*bin_idx)++] = 0x48;
                bin_buffer[(*bin_idx)++] = 0xF7;
                bin_buffer[(*bin_idx)++] = modrm;
                
                nano_print(is_div ? "  [ASM] xor rdx, rdx + div reg -> opcod generat\n" : "  [ASM] xor rdx, rdx + mul reg -> opcod generat\n");
            }
            return 6; // 3 octeți pentru xor rdx,rdx + 3 octeți pentru mul/div
        }
    }
	
    // --- DIRECTIVĂ DATE (quad / data) ---
    if (strcmp(inst->mnemonic, "quad") == 0 || strcmp(inst->mnemonic, "data") == 0) {
        // Verificăm dacă este un șir de caractere între ghilimele
        if (inst->op1[0] == '"') {
            char str_val[128];
            int s_idx = 0;
            char* p = inst->op1 + 1; // Trecem de prima ghilimea
            while (*p != '"' && *p != '\0' && s_idx < 127) {
                if (*p == '\\' && *(p + 1) == 'n') {
                    str_val[s_idx++] = '\n';
                    p += 2;
                } else {
                    str_val[s_idx++] = *p++;
                }
            }
            str_val[s_idx] = '\0';
            int str_len = s_idx + 1; // Inclusiv '\0'

            if (bin_buffer) {
                for (int b = 0; b < str_len; b++) {
                    bin_buffer[(*bin_idx)++] = (uint8_t)str_val[b];
                }
                nano_print("  [ASM] directivă data string -> octeți generați\n");
            }
            return str_len; // Returnează exact dimensiunea șirului în octeți
        } else {
            // Verificăm dacă este un număr zecimal (conține '.')
            int has_dot = 0;
            for (int k = 0; inst->op1[k] != '\0'; k++) {
                if (inst->op1[k] == '.') { has_dot = 1; break; }
            }

            if (has_dot) {
                float f_val = nano_parse_float(inst->op1); // <--- Folosim parserul nostru sigur
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
                // Număr întreg (quad existent)
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
        // Cazul 1: movss xmmX, [eticheta] (Citire float din variabilă)
        if (inst->op2[0] == '[') {
            char reg_name[16];
            strcpy(reg_name, inst->op1);

            int xmm_code = -1;
            if (reg_name[0] == 'x' && reg_name[1] == 'm' && reg_name[2] == 'm') {
                xmm_code = reg_name[3] - '0';
            }

            if (xmm_code >= 0 && xmm_code <= 7) {
                // Extragem eticheta dintre paranteze
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
                    // Opcod MOVSS load: F3 0F 10 /r (cu R/M = 5 pentru RIP-relative)
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
                return 8; // 3 octeți prefix/opcod + 1 ModR/M + 4 offset
            }
        }
        
        // Cazul 2: movss [eticheta], xmmX (Scriere float în variabilă)
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
                    // Opcod MOVSS store: F3 0F 11 /r
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
            // 1. movq rdi, xmm0 (Opcod SSE2: 66 48 0F 7E C7)
            bin_buffer[(*bin_idx)++] = 0x66;
            bin_buffer[(*bin_idx)++] = 0x48;
            bin_buffer[(*bin_idx)++] = 0x0F;
            bin_buffer[(*bin_idx)++] = 0x7E;
            bin_buffer[(*bin_idx)++] = 0xC7;

            // 2. mov rax, 31 (SYSCALL_PRINT_FLOAT) -> Schimbat de la 17 la 31 (0x1F)
            bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
            bin_buffer[(*bin_idx)++] = 0x1F; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;

            // 3. int 0x80
            bin_buffer[(*bin_idx)++] = 0xCD;
            bin_buffer[(*bin_idx)++] = 0x80;

            nano_print("  [ASM] print_xmm -> opcod generat\n");
        }
        return 14; 
    }
	
	// --- SSE ARITHMETIC FLOATS: addss, subss, mulss, divss ---
    if (strcmp(inst->mnemonic, "addss") == 0 || strcmp(inst->mnemonic, "subss") == 0 ||
        strcmp(inst->mnemonic, "mulss") == 0 || strcmp(inst->mnemonic, "divss") == 0) {
        
        int is_sub = (strcmp(inst->mnemonic, "subss") == 0);
        int is_mul = (strcmp(inst->mnemonic, "mulss") == 0);
        int is_div = (strcmp(inst->mnemonic, "divss") == 0);
        
        uint8_t opcode = 0x58; // addss default
        if (is_sub) opcode = 0x5C;
        else if (is_mul) opcode = 0x59;
        else if (is_div) opcode = 0x5E;

        // Parsăm operandul 1 (registru destinație, ex: xmm0)
        char reg_name[16];
        strcpy(reg_name, inst->op1);
        int xmm_dest = -1;
        if (reg_name[0] == 'x' && reg_name[1] == 'm' && reg_name[2] == 'm') {
            xmm_dest = reg_name[3] - '0';
        }

        if (xmm_dest >= 0 && xmm_dest <= 7) {
            // Cazul A: xmmX, xmmY (Reglat între registre XMM)
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
                    return 4; // 2 octeți prefix + 1 opcod + 1 ModR/M
                }
            }

            // Cazul B: xmmX, [eticheta] (Citire din memorie / variabilă, RIP-relative)
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
                return 8; // 3 octeți prefix/op + 1 ModR/M + 4 offset
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

    nano_print("NanoASM v2.0 (Arhitectura cu Tokeni și Funcție Unică)\n");

    uint8_t src_buffer[1024];
    int bytes_read = nano_read_file(src_file, src_buffer, 1023);
    if (bytes_read <= 0) {
        nano_print("Eroare la citirea fisierului sursa!\n");
        return 0;
    }
    src_buffer[bytes_read] = '\0';

    Label labels[32];
    int label_count = 0;

    // --- PASUL 1 ---
    nano_print("[PASUL 1] Scanare etichete și dimensiuni...\n");
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
                    nano_print("  [DEBUG] Etichetă găsită: '");
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
                    return 0;
                }

                // --- LOG DEBUG PENTRU DIMENSIUNE ȘI LINIE ---
                nano_print("  [DEBUG] Linie: '");
                nano_print(clean);
                nano_print("' | Dimensiune: ");
                char size_buf[16];
                itoa(size, size_buf, 10);
                nano_print(size_buf);
                nano_print(" octeți\n");
                // ---------------------------------------------

                p_bin_idx += size;
            }
        }
    }

    // --- PASUL 2 ---
    nano_print("[PASUL 2] Generare binar...\n");
    uint8_t bin_buffer[1024];
    int bin_idx = 8;

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
                    return 0;
                }
            }
        }
    }

    



int create_res = nano_create_file(out_file, bin_idx);
    nano_print("  [DEBUG] nano_create_file a returnat: ");
    // Dacă ai o funcție de afișat numere sau poți folosi itoa:
    // (sau folosește print_number dacă e disponibil în mediul tău)
    
    if (create_res > 0) {
        nano_print("Creat cu succes!\n");
        
        int write_res = nano_write_file(out_file, bin_buffer, bin_idx);
        nano_print("  [DEBUG] nano_write_file a returnat codul\n");
        
        nano_print("Asamblare v2.0 finalizată!\n");
    } else {
        nano_print("Eroare la crearea fisierului binar!\n");
    }

    return 0;
}
