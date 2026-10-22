#ifndef ELF_H
#define ELF_H

// Structură pentru ELF64 Header (64 octeți)
typedef struct {
    unsigned char e_ident[16]; // Magic number ('\x7F', 'E', 'L', 'F') + altele
    uint16_t      e_type;      // 2 = Executable
    uint16_t      e_machine;   // 0x3E = AMD x86-64
    uint32_t      e_version;   // 1
    uint64_t      e_entry;     // Adresa de start (Entry point) -> ex: 0x400078
    uint64_t      e_phoff;     // Program header offset -> 64
    uint64_t      e_shoff;     // Section header offset -> 0
    uint32_t      e_flags;     // 0
    uint16_t      e_ehsize;    // Dimensiune ELF header -> 64
    uint16_t      e_phentsize; // Dimensiune program header -> 56
    uint16_t      e_phnum;     // Număr program headers -> 1
    uint16_t      e_shentsize; // 0
    uint16_t      e_shnum;     // 0
    uint16_t      e_shstrndx;  // 0
} __attribute__((packed)) Elf64_Ehdr;

// Structură pentru Program Header (56 octeți)
typedef struct {
    uint32_t p_type;   // 1 = PT_LOAD
    uint32_t p_flags;  // 5 = R-X (Read + Execute)
    uint64_t p_offset; // Offset în fișier -> 0
    uint64_t p_vaddr;  // Adresa virtuală în memorie -> 0x400000
    uint64_t p_paddr;  // Adresa fizică -> 0x400000
    uint64_t p_filesz; // Dimensiunea segmentului în fișier
    uint64_t p_memsz;  // Dimensiunea segmentului în memorie
    uint64_t p_align;  // 0x200000
} __attribute__((packed)) Elf64_Phdr;

#endif