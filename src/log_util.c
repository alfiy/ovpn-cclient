#include "../include/log_util.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <glib.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define OVPN_LOG_SUBPATH ".config/ovpn_client.log"
#define OVPN_LOG_DIR ".config"

// 日志文件路径缓存（每进程唯一，全局静态）
static char full_log_path[PATH_MAX] = {0};
static FILE *log_file = NULL;

// 工具函数：返回 $HOME/.config/ovpn_client.log 完整路径
char* get_ovpn_log_path(void) {
    if (full_log_path[0] == '\0') {
        const char *home = g_get_home_dir();
        snprintf(full_log_path, sizeof(full_log_path), "%s/%s", home, OVPN_LOG_SUBPATH);

        // 自动创建 ~/.config 目录（如不存在）
        char config_dir[PATH_MAX];
        snprintf(config_dir, sizeof(config_dir), "%s/%s", home, OVPN_LOG_DIR);
        struct stat st;
        if (stat(config_dir, &st) == -1) {
            mkdir(config_dir, 0700); // 用户权限
        }
    }
    return full_log_path;
}

void log_message(const char *level, const char *format, ...) {
    va_list args;
    time_t now;
    char *time_str;

    // 延迟打开日志文件，先保证路径目录存在
    if (!log_file) {
        log_file = fopen(get_ovpn_log_path(), "a");
        if (!log_file) return;
    }

    // 时间信息
    time(&now);
    time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0';

    // 写log到文件
    fprintf(log_file, "%s - %s - ", time_str, level);
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fprintf(log_file, "\n");
    fflush(log_file);

    // 同步输出到终端
    printf("%s - %s - ", time_str, level);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}
