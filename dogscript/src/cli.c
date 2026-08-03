#include "cli.h"

#include "analyzer.h"
#include "asmemit.h"
#include "ir.h"
#include "irgen.h"
#include "parser.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DOG_VERSION "DogScript 0.3.0"

static void print_help(void) {
    puts("DogScript compiler");
    puts("Usage:");
    puts("  dog compile <input.dog> <output> [--asm]");
    puts("  dog run <input.dog>");
    puts("  dog check <input.dog>");
    puts("  dog version");
    puts("  dog help");
}

static int has_dog_ext(const char *path) {
    size_t n = strlen(path);
    return n >= 4 && strcmp(path + n - 4, ".dog") == 0;
}

static int parse_and_analyze(const char *input_path, Program *program) {
    char *source = ds_read_file(input_path, NULL);
    Parser parser;
    if (!source) {
        fprintf(stderr, "dog error: failed to read %s\n", input_path);
        return -1;
    }
    parser_init(&parser, source);
    *program = parser_parse(&parser);
    token_free(&parser.current);
    if (parser.had_previous) {
        token_free(&parser.previous);
    }
    free(source);
    if (analyze_program(program) != 0) {
        parser_free_program(program);
        return -1;
    }
    return 0;
}

static void tmp_base(char *out, size_t out_sz) {
    snprintf(out, out_sz, "/tmp/dog_%d_%ld", (int)getpid(), (long)time(NULL));
}

static int file_copy(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    FILE *out = fopen(dst, "wb");
    char buf[8192];
    size_t n;
    if (!in || !out) {
        if (in) fclose(in);
        if (out) fclose(out);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

static int assemble_and_link(const char *input_path, const char *outname, const char *asm_src) {
    char tmp_o[1024];
    char err_log[512];
    char *base = ds_path_basename_no_ext(input_path);
    char cmd[4096];
    int rc;
    snprintf(tmp_o, sizeof(tmp_o), "%s.o", asm_src);
    snprintf(err_log, sizeof(err_log), "/tmp/dog_%s_err.log", base ? base : "build");
    free(base);

    snprintf(cmd, sizeof(cmd), "as -o \"%s\" \"%s\" 2>\"%s\"", tmp_o, asm_src, err_log);
    rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "dog error: failed to assemble %s\n", input_path);
        fprintf(stderr, "  -> assembler exited with code %d\n", WEXITSTATUS(rc));
        fprintf(stderr, "  -> see %s for details\n", err_log);
        remove(tmp_o);
        remove(asm_src);
        return -1;
    }

#ifdef __APPLE__
    snprintf(cmd, sizeof(cmd),
             "ld -o \"%s\" \"%s\" -L. -ldogruntime -lSystem -syslibroot $(xcrun --show-sdk-path) -e _start -arch x86_64 2>>\"%s\"",
             outname, tmp_o, err_log);
#else
    snprintf(cmd, sizeof(cmd),
             "ld -o \"%s\" \"%s\" -L. -ldogruntime -lm "
             "--as-needed -lssl -lcrypto --no-as-needed -lc "
             "-dynamic-linker /lib64/ld-linux-x86-64.so.2 2>>\"%s\"",
             outname, tmp_o, err_log);
#endif
    rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "dog error: failed to link %s\n", input_path);
        fprintf(stderr, "  -> linker exited with code %d\n", WEXITSTATUS(rc));
        fprintf(stderr, "  -> see %s for details\n", err_log);
        remove(outname);
        remove(tmp_o);
        remove(asm_src);
        return -1;
    }

    remove(tmp_o);
    remove(asm_src);
    return 0;
}

static int compile_file(const char *input_path, const char *outname, int emit_asm) {
    Program program;
    IRProgram ir;
    char base[512];
    char tmp_s[600];
    char out_asm[600];
    struct stat st;

    if (!has_dog_ext(input_path)) {
        fprintf(stderr, "dog error: input must end with .dog\n");
        return -1;
    }

    fprintf(stderr, "Compiling %s...\n", input_path);
    fprintf(stderr, "  [1/3] Parsing...       ");
    if (parse_and_analyze(input_path, &program) != 0) {
        fprintf(stderr, "FAIL\n");
        return -1;
    }
    fprintf(stderr, "OK\n");

    fprintf(stderr, "  [2/3] Generating IR... ");
    if (irgen_program(&program, &ir) != 0) {
        parser_free_program(&program);
        fprintf(stderr, "FAIL\n");
        return -1;
    }
    fprintf(stderr, "OK\n");

    tmp_base(base, sizeof(base));
    snprintf(tmp_s, sizeof(tmp_s), "%s.s", base);

    fprintf(stderr, "  [3/3] Assembling...    ");
    if (asmemit_x64(&ir, tmp_s) != 0) {
        parser_free_program(&program);
        ir_free(&ir);
        fprintf(stderr, "FAIL\n");
        return -1;
    }

    if (emit_asm) {
        snprintf(out_asm, sizeof(out_asm), "%s.asm", outname);
        if (file_copy(tmp_s, out_asm) != 0) {
            fprintf(stderr, "FAIL\n");
            fprintf(stderr, "dog error: failed to write %s\n", out_asm);
            parser_free_program(&program);
            ir_free(&ir);
            remove(tmp_s);
            return -1;
        }
    }

    if (assemble_and_link(input_path, outname, tmp_s) != 0) {
        parser_free_program(&program);
        ir_free(&ir);
        fprintf(stderr, "FAIL\n");
        return -1;
    }
    fprintf(stderr, "OK\n");

    if (stat(outname, &st) == 0) {
        if (outname[0] == '/') {
            fprintf(stderr, "Output: %s (%ld bytes)\n", outname, (long)st.st_size);
        } else {
            fprintf(stderr, "Output: ./%s (%ld bytes)\n", outname, (long)st.st_size);
        }
    } else {
        if (outname[0] == '/') {
            fprintf(stderr, "Output: %s\n", outname);
        } else {
            fprintf(stderr, "Output: ./%s\n", outname);
        }
    }
    if (emit_asm) {
        fprintf(stderr, "Assembly: ./%s.asm\n", outname);
    }

    parser_free_program(&program);
    ir_free(&ir);
    return 0;
}

static int run_file(const char *input_path) {
    char tmp_out[512];
    int rc;
    snprintf(tmp_out, sizeof(tmp_out), "/tmp/dog_run_%d_%ld", (int)getpid(), (long)time(NULL));
    if (compile_file(input_path, tmp_out, 0) != 0) {
        return -1;
    }
    rc = system(tmp_out);
    remove(tmp_out);
    return rc == 0 ? 0 : -1;
}

int cli_run(int argc, char **argv) {
    if (argc < 2) {
        print_help();
        return 1;
    }
    if (strcmp(argv[1], "help") == 0) {
        print_help();
        return 0;
    }
    if (strcmp(argv[1], "version") == 0) {
        puts(DOG_VERSION);
        return 0;
    }
    if (strcmp(argv[1], "check") == 0) {
        Program p;
        if (argc != 3) {
            print_help();
            return 1;
        }
        if (parse_and_analyze(argv[2], &p) != 0) {
            return 1;
        }
        parser_free_program(&p);
        return 0;
    }
    if (strcmp(argv[1], "run") == 0) {
        if (argc != 3) {
            print_help();
            return 1;
        }
        return run_file(argv[2]) == 0 ? 0 : 1;
    }
    if (strcmp(argv[1], "compile") == 0) {
        int emit_asm = 0;
        if (argc < 4 || argc > 5) {
            print_help();
            return 1;
        }
        if (argc == 5) {
            if (strcmp(argv[4], "--asm") != 0) {
                print_help();
                return 1;
            }
            emit_asm = 1;
        }
        return compile_file(argv[2], argv[3], emit_asm) == 0 ? 0 : 1;
    }
    print_help();
    return 1;
}
