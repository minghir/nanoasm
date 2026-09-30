#include "nano_libc.h"

// Header-ul binarului pentru Nano OS
typedef struct {
    uint8_t magic[4];       // 'N', 'A', 'S', '1'
    uint32_t entry_offset;  // Offset-ul de intrare (default 8)
} NanoHeader;

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

// Funcție pentru curățarea liniei de spații și comentarii
void clean_line(const char* raw, char* dest) {
    int i = 0, j = 0;
    while (raw[i] == ' ' || raw[i] == '\t' || raw[i] == '\r') i++;
    
    while (raw[i] != '\0' && raw[i] != ';' && raw[i] != '\r' && raw[i] != '\n') {
        dest[j++] = raw[i++];
    }
    while (j > 0 && (dest[j-1] == ' ' || dest[j-1] == '\t')) {
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
            bin_buffer[(*bin_idx)++] = 0x48; bin_buffer[(*bin_idx)++] = 0xC7; bin_buffer[(*bin_idx)++] = 0xC0;
            bin_buffer[(*bin_idx)++] = 0x1A; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00; bin_buffer[(*bin_idx)++] = 0x00;
            bin_buffer[(*bin_idx)++] = 0xCD; bin_buffer[(*bin_idx)++] = 0x80;
            nano_print("  [ASM] newline -> opcod generat\n");
        }
        return 9;
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
        // mov reg, imm32
        if (strcmp(inst->op1, "rax") == 0 || strcmp(inst->op1, "rdi") == 0 || 
            strcmp(inst->op1, "rsi") == 0 || strcmp(inst->op1, "rdx") == 0) {
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

    // --- JMP & JE ---
    if (strcmp(inst->mnemonic, "jmp") == 0 || strcmp(inst->mnemonic, "je") == 0) {
        int is_je = (strcmp(inst->mnemonic, "je") == 0);
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
            } else {
                bin_buffer[(*bin_idx)++] = 0xE9;
            }
            int rel_pos = *bin_idx;
            *bin_idx += 4;

            int32_t rel32 = target_offset - (rel_pos + 4);
            bin_buffer[rel_pos + 0] = (uint8_t)(rel32 & 0xFF);
            bin_buffer[rel_pos + 1] = (uint8_t)((rel32 >> 8) & 0xFF);
            bin_buffer[rel_pos + 2] = (uint8_t)((rel32 >> 16) & 0xFF);
            bin_buffer[rel_pos + 3] = (uint8_t)((rel32 >> 24) & 0xFF);
            nano_print("  [ASM] jump / je -> etichetă rezolvată\n");
        }
        return is_je ? 6 : 5;
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

    if (nano_create_file(out_file, bin_idx) == 0) {
        nano_write_file(out_file, bin_buffer, bin_idx);
        nano_print("Asamblare v2.0 finalizată cu succes pe Linux!\n");
    } else {
        nano_print("Eroare la scrierea fisierului binar!\n");
    }

    return 0;
}