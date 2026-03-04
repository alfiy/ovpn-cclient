#include "../include/v2ray_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <gio/gio.h>

#define V2RAY_BINARY_PATH "data/v2ray/v2ray"
#define V2RAY_CONFIG_DIR ".config/ovpn-client/v2ray"
#define V2RAY_CONFIG_FILE "config.json"
#define V2RAY_LOG_FILE "/tmp/v2ray.log"
#define TPROXY_SCRIPT_PATH "scripts/setup_tproxy.sh"
#define DEFAULT_TPROXY_PORT 12345

// 日志回调
static gboolean log_watch_cb(GIOChannel *channel, GIOCondition condition, gpointer user_data) {
    V2RayManager *manager = (V2RayManager*)user_data;
    
    if (condition & G_IO_IN) {
        gchar *line = NULL;
        gsize length;
        GError *error = NULL;
        
        while (g_io_channel_read_line(channel, &line, &length, NULL, &error) == G_IO_STATUS_NORMAL) {
            if (line) {
                g_string_append(manager->log_buffer, line);
                g_free(line);
            }
        }
        
        if (error) {
            g_error_free(error);
        }
    }
    
    if (condition & (G_IO_HUP | G_IO_ERR)) {
        return FALSE;
    }
    
    return TRUE;
}

// 创建管理器
V2RayManager* v2ray_manager_new(void) {
    V2RayManager *manager = g_new0(V2RayManager, 1);
    
    manager->status = V2RAY_STATUS_STOPPED;
    manager->v2ray_pid = 0;
    manager->local_port = DEFAULT_TPROXY_PORT;
    manager->tproxy_enabled = FALSE;
    manager->log_buffer = g_string_new("");
    
    // 设置配置路径
    const char *home = g_get_home_dir();
    char config_dir[1024];
    snprintf(config_dir, sizeof(config_dir), "%s/%s", home, V2RAY_CONFIG_DIR);
    g_mkdir_with_parents(config_dir, 0755);
    
    manager->config_path = g_strdup_printf("%s/%s", config_dir, V2RAY_CONFIG_FILE);
    
    // 设置 V2Ray 二进制路径
    manager->v2ray_binary = g_strdup(V2RAY_BINARY_PATH);
    
    return manager;
}

// 释放管理器
void v2ray_manager_free(V2RayManager *manager) {
    if (!manager) return;
    
    v2ray_manager_stop(manager);
    
    if (manager->current_config) {
        proxy_parser_free(manager->current_config);
    }
    
    g_free(manager->config_path);
    g_free(manager->v2ray_binary);
    g_string_free(manager->log_buffer, TRUE);
    g_free(manager);
}

// 设置配置
gboolean v2ray_manager_set_config(V2RayManager *manager, ProxyConfig *config) {
    if (!manager || !config) return FALSE;
    
    // 释放旧配置
    if (manager->current_config) {
        proxy_parser_free(manager->current_config);
    }
    
    manager->current_config = config;
    
    // 生成 V2Ray 配置文件
    char *json_config = proxy_parser_generate_v2ray_config(config, manager->local_port);
    if (!json_config) {
        g_warning("Failed to generate V2Ray config");
        return FALSE;
    }
    
    // 写入配置文件
    GError *error = NULL;
    if (!g_file_set_contents(manager->config_path, json_config, -1, &error)) {
        g_warning("Failed to write V2Ray config: %s", error->message);
        g_error_free(error);
        g_free(json_config);
        return FALSE;
    }
    
    g_free(json_config);
    return TRUE;
}

// 启动 V2Ray
gboolean v2ray_manager_start(V2RayManager *manager, GError **error) {
    if (!manager) return FALSE;
    
    if (manager->status == V2RAY_STATUS_RUNNING) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "V2Ray is already running");
        return FALSE;
    }
    
    // 检查二进制文件
    if (!g_file_test(manager->v2ray_binary, G_FILE_TEST_EXISTS)) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NOENT, 
                   "V2Ray binary not found at %s", manager->v2ray_binary);
        return FALSE;
    }
    
    // 检查配置文件
    if (!g_file_test(manager->config_path, G_FILE_TEST_EXISTS)) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NOENT, 
                   "V2Ray config not found. Please set a proxy first.");
        return FALSE;
    }
    
    manager->status = V2RAY_STATUS_STARTING;
    
    // 启动 V2Ray 进程
    char *argv[] = {
        manager->v2ray_binary,
        "run",
        "-c",
        manager->config_path,
        NULL
    };
    
    GSpawnFlags flags = G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH;
    gint stdout_fd, stderr_fd;
    
    if (!g_spawn_async_with_pipes(NULL, argv, NULL, flags, NULL, NULL,
                                   &manager->v2ray_pid, NULL, &stdout_fd, &stderr_fd, error)) {
        manager->status = V2RAY_STATUS_ERROR;
        return FALSE;
    }
    
    // 监控日志输出
    manager->log_channel = g_io_channel_unix_new(stderr_fd);
    g_io_channel_set_encoding(manager->log_channel, NULL, NULL);
    g_io_channel_set_flags(manager->log_channel, G_IO_FLAG_NONBLOCK, NULL);
    manager->log_watch_id = g_io_add_watch(manager->log_channel, 
                                           G_IO_IN | G_IO_HUP | G_IO_ERR,
                                           log_watch_cb, manager);
    
    manager->status = V2RAY_STATUS_RUNNING;
    g_message("V2Ray started with PID %d", manager->v2ray_pid);
    
    // 如果启用了透明代理，配置 iptables
    if (manager->tproxy_enabled) {
        v2ray_manager_enable_tproxy(manager, TRUE, NULL);
    }
    
    return TRUE;
}

// 停止 V2Ray
gboolean v2ray_manager_stop(V2RayManager *manager) {
    if (!manager) return FALSE;
    
    if (manager->status != V2RAY_STATUS_RUNNING) {
        return TRUE;
    }
    
    manager->status = V2RAY_STATUS_STOPPING;
    
    // 停止日志监控
    if (manager->log_watch_id > 0) {
        g_source_remove(manager->log_watch_id);
        manager->log_watch_id = 0;
    }
    
    if (manager->log_channel) {
        g_io_channel_shutdown(manager->log_channel, FALSE, NULL);
        g_io_channel_unref(manager->log_channel);
        manager->log_channel = NULL;
    }
    
    // 关闭透明代理
    if (manager->tproxy_enabled) {
        v2ray_manager_enable_tproxy(manager, FALSE, NULL);
    }
    
    // 终止进程
    if (manager->v2ray_pid > 0) {
        kill(manager->v2ray_pid, SIGTERM);
        
        // 等待进程退出
        int status;
        for (int i = 0; i < 50; i++) {
            if (waitpid(manager->v2ray_pid, &status, WNOHANG) != 0) {
                break;
            }
            usleep(100000); // 100ms
        }
        
        // 强制终止
        if (kill(manager->v2ray_pid, 0) == 0) {
            kill(manager->v2ray_pid, SIGKILL);
            waitpid(manager->v2ray_pid, &status, 0);
        }
        
        g_spawn_close_pid(manager->v2ray_pid);
        manager->v2ray_pid = 0;
    }
    
    manager->status = V2RAY_STATUS_STOPPED;
    g_message("V2Ray stopped");
    
    return TRUE;
}

// 重启 V2Ray
gboolean v2ray_manager_restart(V2RayManager *manager, GError **error) {
    if (!manager) return FALSE;
    
    v2ray_manager_stop(manager);
    usleep(500000); // 等待 500ms
    return v2ray_manager_start(manager, error);
}

// 获取状态
V2RayStatus v2ray_manager_get_status(V2RayManager *manager) {
    if (!manager) return V2RAY_STATUS_ERROR;
    return manager->status;
}

// 获取状态字符串
const char* v2ray_manager_get_status_string(V2RayStatus status) {
    switch (status) {
        case V2RAY_STATUS_STOPPED: return "已停止";
        case V2RAY_STATUS_STARTING: return "启动中";
        case V2RAY_STATUS_RUNNING: return "运行中";
        case V2RAY_STATUS_STOPPING: return "停止中";
        case V2RAY_STATUS_ERROR: return "错误";
        default: return "未知";
    }
}

// 启用透明代理
gboolean v2ray_manager_enable_tproxy(V2RayManager *manager, gboolean enable, GError **error) {
    if (!manager) return FALSE;
    
    // 检查脚本是否存在
    if (!g_file_test(TPROXY_SCRIPT_PATH, G_FILE_TEST_EXISTS)) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                   "TProxy setup script not found at %s", TPROXY_SCRIPT_PATH);
        return FALSE;
    }
    
    // 执行脚本
    char *argv[] = {
        "pkexec",
        TPROXY_SCRIPT_PATH,
        enable ? "start" : "stop",
        g_strdup_printf("%d", manager->local_port),
        NULL
    };
    
    gint exit_status;
    if (!g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                      NULL, NULL, &exit_status, error)) {
        return FALSE;
    }
    
    g_free(argv[3]);
    
    if (exit_status != 0) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   "TProxy setup failed with exit code %d", exit_status);
        return FALSE;
    }
    
    manager->tproxy_enabled = enable;
    g_message("TProxy %s", enable ? "enabled" : "disabled");
    
    return TRUE;
}

// 获取日志
const char* v2ray_manager_get_log(V2RayManager *manager) {
    if (!manager) return "";
    return manager->log_buffer->str;
}

// 检查二进制
gboolean v2ray_manager_check_binary(void) {
    return g_file_test(V2RAY_BINARY_PATH, G_FILE_TEST_EXISTS);
}

// 获取版本
char* v2ray_manager_get_version(void) {
    if (!v2ray_manager_check_binary()) {
        return g_strdup("未安装");
    }
    
    char *argv[] = { V2RAY_BINARY_PATH, "version", NULL };
    char *output = NULL;
    GError *error = NULL;
    
    if (g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                     &output, NULL, NULL, &error)) {
        // 解析版本号
        char *line = strtok(output, "\n");
        if (line) {
            char *version = g_strdup(line);
            g_free(output);
            return version;
        }
        g_free(output);
    }
    
    if (error) {
        g_error_free(error);
    }
    
    return g_strdup("未知版本");
}