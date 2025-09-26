#include "../include/log_util.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>

#define LOG_FILE "/tmp/ovpn_client.log"

static FILE *log_file = NULL;

void log_message(const char *level, const char *format, ...) {
    va_list args;
    time_t now;
    char *time_str;

    if (!log_file) {
        log_file = fopen(LOG_FILE, "a");
        if (!log_file) return;
    }
    time(&now);
    time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0';

    fprintf(log_file, "%s - %s - ", time_str, level);
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fprintf(log_file, "\n");
    fflush(log_file);

    printf("%s - %s - ", time_str, level);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}
