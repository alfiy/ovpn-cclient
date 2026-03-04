#include "../include/route_manager.h"
#include "../include/log_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <libnm/nm-setting-ip4-config.h>

// 私有IP段
static const char *PRIVATE_IP_RANGES[] = {
    "10.0.0.0/8",
    "172.16.0.0/12",
    "192.168.0.0/16",
    "127.0.0.0/8",
    "169.254.0.0/16",
    NULL
};

// 中国IP段（示例，完整列表需要从GeoIP数据库加载）
static const char *CN_IP_RANGES[] = {
    "1.0.1.0/24",
    "1.0.2.0/23",
    "1.0.8.0/21",
    "1.0.32.0/19",
    "1.1.0.0/24",
    "1.1.2.0/23",
    "1.1.4.0/22",
    "1.1.8.0/24",
    "1.2.0.0/23",
    "1.2.2.0/24",
    "1.2.4.0/22",
    "1.3.0.0/16",
    "1.4.0.0/15",
    "1.8.0.0/16",
    "1.10.0.0/23",
    "1.12.0.0/22",
    "1.24.0.0/13",
    "1.45.0.0/16",
    "1.48.0.0/15",
    "1.50.0.0/16",
    "1.51.0.0/17",
    "1.51.128.0/18",
    "1.51.192.0/19",
    "1.51.224.0/20",
    "1.51.240.0/21",
    "1.52.0.0/14",
    "1.56.0.0/13",
    "1.68.0.0/14",
    "1.80.0.0/13",
    "1.88.0.0/14",
    "1.92.0.0/15",
    "1.94.0.0/16",
    // 更多IP段应从文件加载...
    NULL
};

// 创建路由管理器
RouteManager* route_manager_new(void) {
    RouteManager *manager = g_malloc0(sizeof(RouteManager));
    
    manager->config = g_malloc0(sizeof(RouteConfig));
    manager->config->mode = ROUTE_MODE_PAC;
    manager->config->enable_geoip = TRUE;
    manager->config->cn_direct = TRUE;
    manager->config->private_direct = TRUE;
    manager->config->lan_direct = TRUE;
    
    manager->config->custom_direct_cidrs = g_ptr_array_new_with_free_func(g_free);
    manager->config->custom_vpn_cidrs = g_ptr_array_new_with_free_func(g_free);
    manager->config->custom_block_cidrs = g_ptr_array_new_with_free_func(g_free);
    
    manager->cn_ip_list = g_ptr_array_new_with_free_func(g_free);
    manager->active_routes = g_ptr_array_new_with_free_func(g_free);
    
    manager->initialized = FALSE;
    
    log_message("INFO", "Route manager created");
    return manager;
}

// 释放路由管理器
void route_manager_free(RouteManager *manager) {
    if (!manager) return;
    
    if (manager->config) {
        if (manager->config->custom_direct_cidrs) {
            g_ptr_array_free(manager->config->custom_direct_cidrs, TRUE);
        }
        if (manager->config->custom_vpn_cidrs) {
            g_ptr_array_free(manager->config->custom_vpn_cidrs, TRUE);
        }
        if (manager->config->custom_block_cidrs) {
            g_ptr_array_free(manager->config->custom_block_cidrs, TRUE);
        }
        g_free(manager->config);
    }
    
    if (manager->cn_ip_list) {
        g_ptr_array_free(manager->cn_ip_list, TRUE);
    }
    
    if (manager->active_routes) {
        g_ptr_array_free(manager->active_routes, TRUE);
    }
    
    g_free(manager);
    log_message("INFO", "Route manager freed");
}

// 解析CIDR格式（例如：192.168.1.0/24）
static gboolean parse_cidr(const char *cidr, uint32_t *network, uint32_t *netmask) {
    char ip_str[64];
    int prefix_len;
    
    if (sscanf(cidr, "%63[^/]/%d", ip_str, &prefix_len) != 2) {
        return FALSE;
    }
    
    if (prefix_len < 0 || prefix_len > 32) {
        return FALSE;
    }
    
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) != 1) {
        return FALSE;
    }
    
    *network = ntohl(addr.s_addr);
    *netmask = (prefix_len == 0) ? 0 : (~0U << (32 - prefix_len));
    
    return TRUE;
}

// 测试IP是否匹配CIDR
gboolean route_manager_ip_match_cidr(const char *ip, const char *cidr) {
    uint32_t network, netmask, ip_addr;
    struct in_addr addr;
    
    if (!parse_cidr(cidr, &network, &netmask)) {
        return FALSE;
    }
    
    if (inet_pton(AF_INET, ip, &addr) != 1) {
        return FALSE;
    }
    
    ip_addr = ntohl(addr.s_addr);
    
    return (ip_addr & netmask) == (network & netmask);
}

// 加载GeoIP数据
gboolean route_manager_load_geoip(RouteManager *manager) {
    if (!manager) return FALSE;
    
    log_message("INFO", "Loading GeoIP data...");
    
    // 清空现有列表
    g_ptr_array_set_size(manager->cn_ip_list, 0);
    
    // 加载内置的中国IP段
    for (int i = 0; CN_IP_RANGES[i] != NULL; i++) {
        g_ptr_array_add(manager->cn_ip_list, g_strdup(CN_IP_RANGES[i]));
    }
    
    // 如果指定了GeoIP数据库文件，从文件加载
    if (strlen(manager->config->geoip_db_path) > 0) {
        FILE *fp = fopen(manager->config->geoip_db_path, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                // 移除换行符
                line[strcspn(line, "\r\n")] = 0;
                
                // 跳过空行和注释
                if (line[0] == '\0' || line[0] == '#') continue;
                
                // 验证CIDR格式
                uint32_t network, netmask;
                if (parse_cidr(line, &network, &netmask)) {
                    g_ptr_array_add(manager->cn_ip_list, g_strdup(line));
                }
            }
            fclose(fp);
            log_message("INFO", "Loaded GeoIP data from file: %s", manager->config->geoip_db_path);
        } else {
            log_message("WARNING", "Failed to open GeoIP database: %s", manager->config->geoip_db_path);
        }
    }
    
    log_message("INFO", "Loaded %d Chinese IP ranges", manager->cn_ip_list->len);
    manager->initialized = TRUE;
    return TRUE;
}

// 应用路由规则到NetworkManager连接
gboolean route_manager_apply_rules(RouteManager *manager, NMSettingIPConfig *s_ip4) {
    if (!manager || !s_ip4) return FALSE;
    
    log_message("INFO", "Applying routing rules (mode: %d)", manager->config->mode);
    
    GError *error = NULL;
    int route_count = 0;
    
    // 全局模式：所有流量走VPN（不添加额外路由）
    if (manager->config->mode == ROUTE_MODE_GLOBAL) {
        log_message("INFO", "Global mode: all traffic through VPN");
        return TRUE;
    }
    
    // 直连模式：所有流量直连（设置never-default）
    if (manager->config->mode == ROUTE_MODE_DIRECT) {
        log_message("INFO", "Direct mode: all traffic direct");
        g_object_set(s_ip4, "never-default", TRUE, NULL);
        return TRUE;
    }
    
    // PAC模式：根据规则分流
    if (manager->config->mode == ROUTE_MODE_PAC) {
        // 设置never-default，防止VPN成为默认网关
        g_object_set(s_ip4, "never-default", TRUE, NULL);
        
        // 1. 添加私有IP段直连路由
        if (manager->config->private_direct) {
            for (int i = 0; PRIVATE_IP_RANGES[i] != NULL; i++) {
                NMIPRoute *route = nm_ip_route_new(AF_INET, PRIVATE_IP_RANGES[i], 
                                                   0, NULL, 100, &error);
                if (route) {
                    nm_setting_ip_config_add_route(NM_SETTING_IP_CONFIG(s_ip4), route);
                    nm_ip_route_unref(route);
                    route_count++;
                } else if (error) {
                    log_message("ERROR", "Failed to add private route: %s", error->message);
                    g_error_free(error);
                    error = NULL;
                }
            }
            log_message("INFO", "Added %d private IP routes", route_count);
        }
        
        // 2. 添加中国IP段直连路由
        if (manager->config->cn_direct && manager->config->enable_geoip) {
            if (!manager->initialized) {
                route_manager_load_geoip(manager);
            }
            
            int cn_route_count = 0;
            for (guint i = 0; i < manager->cn_ip_list->len; i++) {
                const char *cidr = g_ptr_array_index(manager->cn_ip_list, i);
                NMIPRoute *route = nm_ip_route_new(AF_INET, cidr, 0, NULL, 100, &error);
                if (route) {
                    nm_setting_ip_config_add_route(NM_SETTING_IP_CONFIG(s_ip4), route);
                    nm_ip_route_unref(route);
                    cn_route_count++;
                } else if (error) {
                    log_message("ERROR", "Failed to add CN route %s: %s", cidr, error->message);
                    g_error_free(error);
                    error = NULL;
                }
            }
            log_message("INFO", "Added %d Chinese IP routes", cn_route_count);
            route_count += cn_route_count;
        }
        
        // 3. 添加自定义直连路由
        for (guint i = 0; i < manager->config->custom_direct_cidrs->len; i++) {
            const char *cidr = g_ptr_array_index(manager->config->custom_direct_cidrs, i);
            NMIPRoute *route = nm_ip_route_new(AF_INET, cidr, 0, NULL, 100, &error);
            if (route) {
                nm_setting_ip_config_add_route(NM_SETTING_IP_CONFIG(s_ip4), route);
                nm_ip_route_unref(route);
                route_count++;
            } else if (error) {
                log_message("ERROR", "Failed to add custom direct route %s: %s", cidr, error->message);
                g_error_free(error);
                error = NULL;
            }
        }
        
        log_message("INFO", "Total routes applied: %d", route_count);
    }
    
    return TRUE;
}

// 添加自定义CIDR规则
void route_manager_add_custom_cidr(RouteManager *manager, const char *cidr, RouteAction action) {
    if (!manager || !cidr) return;
    
    // 验证CIDR格式
    uint32_t network, netmask;
    if (!parse_cidr(cidr, &network, &netmask)) {
        log_message("ERROR", "Invalid CIDR format: %s", cidr);
        return;
    }
    
    GPtrArray *target_array = NULL;
    const char *action_name = "";
    
    switch (action) {
        case ROUTE_ACTION_DIRECT:
            target_array = manager->config->custom_direct_cidrs;
            action_name = "direct";
            break;
        case ROUTE_ACTION_VPN:
            target_array = manager->config->custom_vpn_cidrs;
            action_name = "vpn";
            break;
        case ROUTE_ACTION_BLOCK:
            target_array = manager->config->custom_block_cidrs;
            action_name = "block";
            break;
    }
    
    if (target_array) {
        g_ptr_array_add(target_array, g_strdup(cidr));
        log_message("INFO", "Added custom %s rule: %s", action_name, cidr);
    }
}

// 删除自定义CIDR规则
void route_manager_remove_custom_cidr(RouteManager *manager, const char *cidr, RouteAction action) {
    if (!manager || !cidr) return;
    
    GPtrArray *target_array = NULL;
    
    switch (action) {
        case ROUTE_ACTION_DIRECT:
            target_array = manager->config->custom_direct_cidrs;
            break;
        case ROUTE_ACTION_VPN:
            target_array = manager->config->custom_vpn_cidrs;
            break;
        case ROUTE_ACTION_BLOCK:
            target_array = manager->config->custom_block_cidrs;
            break;
    }
    
    if (target_array) {
        for (guint i = 0; i < target_array->len; i++) {
            const char *item = g_ptr_array_index(target_array, i);
            if (strcmp(item, cidr) == 0) {
                g_ptr_array_remove_index(target_array, i);
                log_message("INFO", "Removed custom rule: %s", cidr);
                return;
            }
        }
    }
}

// 获取路由统计信息
void route_manager_get_stats(RouteManager *manager, int *direct_count, int *vpn_count, int *block_count) {
    if (!manager) return;
    
    if (direct_count) {
        *direct_count = manager->config->custom_direct_cidrs->len;
        if (manager->config->cn_direct) {
            *direct_count += manager->cn_ip_list->len;
        }
        if (manager->config->private_direct) {
            int i = 0;
            while (PRIVATE_IP_RANGES[i] != NULL) {
                (*direct_count)++;
                i++;
            }
        }
    }
    
    if (vpn_count) {
        *vpn_count = manager->config->custom_vpn_cidrs->len;
    }
    
    if (block_count) {
        *block_count = manager->config->custom_block_cidrs->len;
    }
}

// 加载路由配置
gboolean route_manager_load_config(RouteManager *manager, const char *config_file) {
    if (!manager || !config_file) return FALSE;
    
    GKeyFile *keyfile = g_key_file_new();
    GError *error = NULL;
    
    if (!g_key_file_load_from_file(keyfile, config_file, G_KEY_FILE_NONE, &error)) {
        log_message("WARNING", "Failed to load route config: %s", error ? error->message : "unknown error");
        if (error) g_error_free(error);
        g_key_file_free(keyfile);
        return FALSE;
    }
    
    // 读取配置
    manager->config->mode = g_key_file_get_integer(keyfile, "General", "mode", NULL);
    manager->config->enable_geoip = g_key_file_get_boolean(keyfile, "General", "enable_geoip", NULL);
    manager->config->cn_direct = g_key_file_get_boolean(keyfile, "General", "cn_direct", NULL);
    manager->config->private_direct = g_key_file_get_boolean(keyfile, "General", "private_direct", NULL);
    
    char *geoip_path = g_key_file_get_string(keyfile, "General", "geoip_db_path", NULL);
    if (geoip_path) {
        strncpy(manager->config->geoip_db_path, geoip_path, sizeof(manager->config->geoip_db_path) - 1);
        g_free(geoip_path);
    }
    
    g_key_file_free(keyfile);
    log_message("INFO", "Route configuration loaded from: %s", config_file);
    return TRUE;
}

// 保存路由配置
gboolean route_manager_save_config(RouteManager *manager, const char *config_file) {
    if (!manager || !config_file) return FALSE;
    
    GKeyFile *keyfile = g_key_file_new();
    
    g_key_file_set_integer(keyfile, "General", "mode", manager->config->mode);
    g_key_file_set_boolean(keyfile, "General", "enable_geoip", manager->config->enable_geoip);
    g_key_file_set_boolean(keyfile, "General", "cn_direct", manager->config->cn_direct);
    g_key_file_set_boolean(keyfile, "General", "private_direct", manager->config->private_direct);
    g_key_file_set_string(keyfile, "General", "geoip_db_path", manager->config->geoip_db_path);
    
    GError *error = NULL;
    gboolean success = g_key_file_save_to_file(keyfile, config_file, &error);
    
    if (!success) {
        log_message("ERROR", "Failed to save route config: %s", error ? error->message : "unknown error");
        if (error) g_error_free(error);
    } else {
        log_message("INFO", "Route configuration saved to: %s", config_file);
    }
    
    g_key_file_free(keyfile);
    return success;
}

// 导出路由规则到文件
gboolean route_manager_export_rules(RouteManager *manager, const char *output_file) {
    if (!manager || !output_file) return FALSE;
    
    FILE *fp = fopen(output_file, "w");
    if (!fp) {
        log_message("ERROR", "Failed to open output file: %s", output_file);
        return FALSE;
    }
    
    fprintf(fp, "# Route Rules Export\n");
    fprintf(fp, "# Generated by ovpn-client\n\n");
    
    fprintf(fp, "# Direct Routes (CN IP)\n");
    for (guint i = 0; i < manager->cn_ip_list->len; i++) {
        const char *cidr = g_ptr_array_index(manager->cn_ip_list, i);
        fprintf(fp, "direct,%s\n", cidr);
    }
    
    fprintf(fp, "\n# Custom Direct Routes\n");
    for (guint i = 0; i < manager->config->custom_direct_cidrs->len; i++) {
        const char *cidr = g_ptr_array_index(manager->config->custom_direct_cidrs, i);
        fprintf(fp, "direct,%s\n", cidr);
    }
    
    fprintf(fp, "\n# Custom VPN Routes\n");
    for (guint i = 0; i < manager->config->custom_vpn_cidrs->len; i++) {
        const char *cidr = g_ptr_array_index(manager->config->custom_vpn_cidrs, i);
        fprintf(fp, "vpn,%s\n", cidr);
    }
    
    fclose(fp);
    log_message("INFO", "Route rules exported to: %s", output_file);
    return TRUE;
}