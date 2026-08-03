#include "dog_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

static const char *DOGSCRIPT_VERSION = "DogScript 0.2.0-runtime";

void dog_sys_set_args(int argc, char **argv) {
    dog_argc = argc;
    dog_argv = argv;
}

void dog_sys_exit(int code) {
    exit(code);
}

DogList *dog_sys_argv(void) {
    long i;
    DogList *l = dog_list_new();
    if (!l) return NULL;
    for (i = 0; i < dog_argc; i++) {
        dog_list_push(l, dog_string_new(dog_argv[i]));
    }
    return l;
}

DogString *dog_sys_platform(void) {
#if defined(_WIN32)
    return dog_string_new("windows");
#elif defined(__APPLE__)
    return dog_string_new("macos");
#else
    return dog_string_new("linux");
#endif
}

DogString *dog_sys_version(void) {
    return dog_string_new(DOGSCRIPT_VERSION);
}

int dog_sys_memused(void) {
#if defined(__linux__)
    struct sysinfo info;
    if (sysinfo(&info) != 0) return 0;
    return (int)((info.totalram - info.freeram) / 1024);
#else
    return 0;
#endif
}

int dog_sys_memtotal(void) {
#if defined(__linux__)
    struct sysinfo info;
    if (sysinfo(&info) != 0) return 0;
    return (int)(info.totalram / 1024);
#else
    return 0;
#endif
}

int dog_sys_cpucount(void) {
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (int)info.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
#endif
}

DogString *dog_sys_stdin_read(void) {
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) {
        return dog_string_new("");
    }
    buf[strcspn(buf, "\n")] = '\0';
    return dog_string_new(buf);
}

void dog_sys_stderr_write(const char *msg) {
    fprintf(stderr, "%s", msg ? msg : "");
}
