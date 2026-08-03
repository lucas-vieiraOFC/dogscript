#include "dog_runtime.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

int dog_time_now(void) {
    return (int)time(NULL);
}

void dog_time_sleep(int ms) {
#if defined(_WIN32)
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)(ms * 1000));
#endif
}

DogString *dog_time_format(int ts, const char *fmt) {
    time_t t = (time_t)ts;
    struct tm tmv;
    char buf[256];
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    if (!strftime(buf, sizeof(buf), fmt ? fmt : "%Y-%m-%d %H:%M:%S", &tmv)) {
        return dog_string_new("");
    }
    return dog_string_new(buf);
}

int dog_time_parse(const char *s, const char *fmt) {
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
#if defined(_WIN32)
    (void)s;
    (void)fmt;
    return 0;
#else
    if (!strptime(s, fmt, &tmv)) {
        return 0;
    }
    return (int)mktime(&tmv);
#endif
}

static struct tm dog_local_tm(int ts) {
    time_t t = (time_t)ts;
    struct tm tmv;
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    return tmv;
}

int dog_time_year(int ts) { struct tm t = dog_local_tm(ts); return t.tm_year + 1900; }
int dog_time_month(int ts) { struct tm t = dog_local_tm(ts); return t.tm_mon + 1; }
int dog_time_day(int ts) { struct tm t = dog_local_tm(ts); return t.tm_mday; }
int dog_time_hour(int ts) { struct tm t = dog_local_tm(ts); return t.tm_hour; }
int dog_time_minute(int ts) { struct tm t = dog_local_tm(ts); return t.tm_min; }
int dog_time_second(int ts) { struct tm t = dog_local_tm(ts); return t.tm_sec; }
int dog_time_diff(int ts1, int ts2) { return (int)difftime((time_t)ts2, (time_t)ts1); }
int dog_time_add(int ts, int seconds) { return ts + seconds; }
