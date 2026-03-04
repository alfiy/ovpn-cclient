#ifndef ROUTE_UI_H
#define ROUTE_UI_H

#include <gtk/gtk.h>
#include "structs.h"
#include "route_manager.h"

// 路由配置对话框
typedef struct {
    GtkWidget *dialog;
    GtkWidget *notebook;
    
    // 基本设置页面
    GtkWidget *mode_combo;
    GtkWidget *enable_geoip_check;
    GtkWidget *cn_direct_check;
    GtkWidget *private_direct_check;
    GtkWidget *lan_direct_check;
    GtkWidget *geoip_path_entry;
    GtkWidget *geoip_browse_button;
    
    // 自定义规则页面
    GtkWidget *custom_rules_view;
    GtkWidget *custom_rules_store;
    GtkWidget *add_rule_button;
    GtkWidget *remove_rule_button;
    GtkWidget *import_rules_button;
    GtkWidget *export_rules_button;
    
    // 统计信息页面
    GtkWidget *stats_label;
    GtkWidget *direct_count_label;
    GtkWidget *vpn_count_label;
    GtkWidget *block_count_label;
    
    RouteManager *route_manager;
    OVPNClient *client;
} RouteConfigDialog;

// 创建路由配置对话框
RouteConfigDialog* route_config_dialog_new(OVPNClient *client, RouteManager *manager);

// 显示路由配置对话框
void route_config_dialog_show(RouteConfigDialog *dialog);

// 释放路由配置对话框
void route_config_dialog_free(RouteConfigDialog *dialog);

// 更新统计信息
void route_config_dialog_update_stats(RouteConfigDialog *dialog);

// 添加路由配置按钮到主窗口
void add_route_config_button(OVPNClient *client, GtkWidget *container, RouteManager *manager);

// 显示路由配置对话框的便捷函数（用于按钮回调）
void route_ui_show_config_dialog(OVPNClient *client);

#endif