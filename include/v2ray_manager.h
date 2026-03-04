#ifndef V2RAY_MANAGER_H
#define V2RAY_MANAGER_H

#include <glib.h>
#include "proxy_parser.h"

// V2Ray 状态
typedef enum {
    V2RAY_STATUS_STOPPED,
    V2RAY_STATUS_STARTING,
    V2RAY_STATUS_RUNNING,
    V2RAY_STATUS_STOPPING,
    V2RAY_STATUS_ERROR
} V2RayStatus;

// V2Ray 管理器 - 使用与 structs.h 中一致的不透明类型名
typedef struct V2RayManager_opaque {
    GPid v2ray_pid;
    V2RayStatus status;
    ProxyConfig *current_config;
    char *config_path;
    char *v2ray_binary;
    int local_port;
    gboolean tproxy_enabled;
    GIOChannel *log_channel;
    guint log_watch_id;
    GString *log_buffer;
} V2RayManager;

/**
 * 创建 V2Ray 管理器
 * @return V2RayManager* 管理器实例
 */
V2RayManager* v2ray_manager_new(void);

/**
 * 释放 V2Ray 管理器
 * @param manager 管理器实例
 */
void v2ray_manager_free(V2RayManager *manager);

/**
 * 设置代理配置
 * @param manager 管理器实例
 * @param config 代理配置
 * @return gboolean 成功返回 TRUE
 */
gboolean v2ray_manager_set_config(V2RayManager *manager, ProxyConfig *config);

/**
 * 启动 V2Ray
 * @param manager 管理器实例
 * @param error 错误信息
 * @return gboolean 成功返回 TRUE
 */
gboolean v2ray_manager_start(V2RayManager *manager, GError **error);

/**
 * 停止 V2Ray
 * @param manager 管理器实例
 * @return gboolean 成功返回 TRUE
 */
gboolean v2ray_manager_stop(V2RayManager *manager);

/**
 * 重启 V2Ray
 * @param manager 管理器实例
 * @param error 错误信息
 * @return gboolean 成功返回 TRUE
 */
gboolean v2ray_manager_restart(V2RayManager *manager, GError **error);

/**
 * 获取 V2Ray 状态
 * @param manager 管理器实例
 * @return V2RayStatus 当前状态
 */
V2RayStatus v2ray_manager_get_status(V2RayManager *manager);

/**
 * 获取状态字符串
 * @param status 状态
 * @return 状态字符串
 */
const char* v2ray_manager_get_status_string(V2RayStatus status);

/**
 * 启用透明代理
 * @param manager 管理器实例
 * @param enable 是否启用
 * @param error 错误信息
 * @return gboolean 成功返回 TRUE
 */
gboolean v2ray_manager_enable_tproxy(V2RayManager *manager, gboolean enable, GError **error);

/**
 * 获取日志内容
 * @param manager 管理器实例
 * @return 日志字符串
 */
const char* v2ray_manager_get_log(V2RayManager *manager);

/**
 * 检查 V2Ray 二进制是否存在
 * @return gboolean 存在返回 TRUE
 */
gboolean v2ray_manager_check_binary(void);

/**
 * 获取 V2Ray 版本
 * @return 版本字符串，需要用 g_free 释放
 */
char* v2ray_manager_get_version(void);

#endif // V2RAY_MANAGER_H