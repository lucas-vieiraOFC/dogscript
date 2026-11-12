# DogScript

DogScript is an experimental compiled language with a toolchain written in C11 (libc only).
It compiles `.dog` source files directly to x86-64 assembly, then assembles and links them with `as` and `ld`.

> **Status: early prototype.** The pipeline works end to end for simple programs, but the type system,
> classes and standard library are incomplete. See [Known limitations](#known-limitations).

## Build

```sh
make
```

Build outputs:
- `dog` (compiler CLI)
- `libdogruntime.a` (native runtime)

Tested on x86-64 Linux with gcc and GNU binutils.

## CLI

```sh
dog compile hello.dog hello
dog compile hello.dog hello --asm
dog run hello.dog
dog check hello.dog
dog version
dog help
```

> Currently `dog` must be run from the repository root, because the linker looks for
> `libdogruntime.a` in the current directory (`-L.`).

## Pipeline

1. `lexer` -> tokens
2. `parser` -> AST
3. `analyzer` -> semantic checks (currently: unknown identifiers)
4. `irgen` -> linear IR (`src/ir.h`, `src/irgen.c`)
5. `asmemit` -> x86-64 assembly (`src/asmemit.c`), simple stack-based code, no optimization
6. `as` + `ld` -> native executable

Temporary `.s` and `.o` files are created in `/tmp/` and removed after linking
(unless `--asm` is used, which writes `<output>.asm` for inspection).

## What works

- Functions, recursion, `if` / `else`, `while` loops, integer arithmetic and comparisons
- String output with `print`
- Runtime modules written in C and exposed through `@extern` declarations:
  string, list, io, os, sys, math, json, time (requests and gui are partial)
- All programs in `examples/` compile and run

## Known limitations

- **Type checking is not enforced yet.** Mismatched types (e.g. assigning a `str` to an `int`)
  are not rejected at compile time.
- **Printing integers is not supported yet.** `print(int)` and `str(int)` crash at runtime.
- **Classes are parsed but not implemented.** Class declarations are accepted by the parser,
  but objects cannot be instantiated and methods cannot be called.
- No compiler test suite yet; `examples/` only exercises the runtime modules.
- Linker path is hardcoded to the current directory.
