#ifndef ROUTE_MANAGER_H
#define ROUTE_MANAGER_H

#include <glib.h>
#include <libnm/NetworkManager.h>
#include "structs.h"

// 路由动作类型
typedef enum {
    ROUTE_ACTION_DIRECT,    // 直连（不走VPN）
    ROUTE_ACTION_VPN,       // 走VPN
    ROUTE_ACTION_BLOCK      // 阻断
} RouteAction;

// 路由模式
typedef enum {
    ROUTE_MODE_GLOBAL,      // 全局模式（所有流量走VPN）
    ROUTE_MODE_PAC,         // PAC模式（根据规则分流）
    ROUTE_MODE_DIRECT       // 直连模式（所有流量直连）
} RouteMode;

// 路由规则配置
typedef struct {
    RouteMode mode;
    gboolean enable_geoip;
    gboolean cn_direct;         // 中国IP直连
    gboolean private_direct;    // 私有IP直连
    gboolean lan_direct;        // 局域网直连
    
    GPtrArray *custom_direct_cidrs;  // 自定义直连CIDR列表
    GPtrArray *custom_vpn_cidrs;     // 自定义VPN CIDR列表
    GPtrArray *custom_block_cidrs;   // 自定义阻断CIDR列表
    
    char geoip_db_path[1024];   // GeoIP数据库路径
} RouteConfig;

// 路由管理器 - 使用前向声明的类型
struct RouteManager {
    RouteConfig *config;
    GPtrArray *cn_ip_list;      // 中国IP段列表
    GPtrArray *active_routes;   // 当前激活的路由
    gboolean initialized;
};

// 初始化路由管理器
RouteManager* route_manager_new(void);

// 释放路由管理器
void route_manager_free(RouteManager *manager);

// 加载路由配置
gboolean route_manager_load_config(RouteManager *manager, const char *config_file);

// 保存路由配置
gboolean route_manager_save_config(RouteManager *manager, const char *config_file);

// 加载GeoIP数据（中国IP段）
gboolean route_manager_load_geoip(RouteManager *manager);

// 应用路由规则到NetworkManager连接
gboolean route_manager_apply_rules(RouteManager *manager, NMSettingIPConfig *s_ip4);

// 添加自定义CIDR规则
void route_manager_add_custom_cidr(RouteManager *manager, const char *cidr, RouteAction action);

// 删除自定义CIDR规则
void route_manager_remove_custom_cidr(RouteManager *manager, const char *cidr, RouteAction action);

// 获取路由统计信息
void route_manager_get_stats(RouteManager *manager, int *direct_count, int *vpn_count, int *block_count);

// 导出路由规则到文件
gboolean route_manager_export_rules(RouteManager *manager, const char *output_file);

// 测试IP是否匹配某个CIDR
gboolean route_manager_ip_match_cidr(const char *ip, const char *cidr);

#endif