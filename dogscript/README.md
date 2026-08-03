# DogScript

DogScript is a statically-typed compiled language with a raw-C toolchain.

## Build

```sh
make
```

Build outputs:
- `dog` (compiler CLI)
- `libdogruntime.a` (native runtime)

## CLI

```sh
dog compile hello.dog hello
dog compile hello.dog hello --asm
dog run hello.dog
dog check hello.dog
dog version
dog help
```

## Native Pipeline

DogScript no longer emits C as an intermediate artifact.

Pipeline:
1. `lexer` -> tokens
2. `parser` -> AST
3. `analyzer` -> semantic checks
4. `irgen` -> linear IR (`src/ir.h`, `src/irgen.c`)
5. `asmemit` -> x86-64 assembly (`src/asmemit.c`)
6. `as` + `ld` -> native executable

Temporary `.s` and `.o` files are created in `/tmp/` and removed after linking (unless `--asm` is used, which writes `<output>.asm` for inspection).

## Runtime

Runtime lives in `src/runtime/` and is linked as `libdogruntime.a`.
It provides stdlib native functions for:
- requests, os, sys, math, json, time, gui
- string, list, io

## Notes

- Raw C only (C11, libc)
- No intermediate `.dog.c` generation
- CLI progress logs print to stderr
