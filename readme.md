# NanoASM & NanoC

A lightweight, zero-dependency **x86-64 C compiler and two-pass assembler** written entirely from scratch in C. Translates C source code directly into x86-64 assembly and emits standalone **Linux ELF64** executables or custom **Nano OS (`NAS1`)** binaries—no GCC, Clang, GAS, or external linkers required in the pipeline.

## Toolchain Components

* **`cc` (`cc.c`)** — From-scratch C compiler generating x86-64 assembly (`.s`) with full stack-frame management and register-based calling conventions.
* **`asm` (`asm.c`)** — Two-pass x86-64 machine-code assembler (`NanoASM v2.0`) capable of emitting native Linux `ELF64` executables (`-elf`) as well as flat `NAS1` OS binaries.

## Supported C Features

* **Types & Pointers:** `int`, `float` (hardware SSE `xmm` instructions), `char`, `void`, multi-level pointers (`*`, `&`), and arrays.
* **Structs:** `struct` definitions (including self-referential pointers for linked data structures), chained `.` and `->` member access, and arrays of structs (`struct Node arr[10]`).
* **Control Flow & ABI:** `if`/`else`, `while`, `for`, `break`, `continue`, local/global variables, built-in `print()`, and System V-style register parameter passing (`rdi`, `rsi`, `rdx`, `rcx` / `xmm0`–`xmm3`).

## Quick Start

```bash
# Build the compiler and assembler
make

# 1. Compile C -> x86-64 Assembly
./cc program.c program.s

# 2. Assemble -> Standalone Linux ELF64 executable
./asm -elf program.s program
./program

# (Optional) Assemble -> Native Nano OS (NAS1) binary
./asm program.s program.bin
```