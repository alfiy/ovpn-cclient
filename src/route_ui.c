#include "../include/route_ui.h"
#include "../include/log_util.h"
#include <stdio.h>
#include <string.h>

// 模式选项
static const char *MODE_OPTIONS[] = {
    "全局模式 (所有流量走VPN)",
    "PAC模式 (智能分流)",
    "直连模式 (所有流量直连)",
    NULL
};

// ==================== 辅助函数：显示错误对话框 ====================
static void show_error_message(GtkWindow *parent, const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(
        parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_ERROR,
        GTK_BUTTONS_OK,
        "%s", message
    );
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// 添加自定义规则对话框回调
static void on_add_rule_clicked(GtkButton *button, gpointer user_data) {
    RouteConfigDialog *dialog = (RouteConfigDialog *)user_data;
    
    GtkWidget *add_dialog = gtk_dialog_new_with_buttons(
        "添加自定义规则",
        GTK_WINDOW(dialog->dialog),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "取消", GTK_RESPONSE_CANCEL,
        "添加", GTK_RESPONSE_OK,
        NULL
    );
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(add_dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    
    // CIDR输入
    GtkWidget *cidr_label = gtk_label_new("CIDR (例如: 192.168.1.0/24):");
    GtkWidget *cidr_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(cidr_entry), "192.168.1.0/24");
    
    // 动作选择
    GtkWidget *action_label = gtk_label_new("动作:");
    GtkWidget *action_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(action_combo), "直连");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(action_combo), "走VPN");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(action_combo), "阻断");
    gtk_combo_box_set_active(GTK_COMBO_BOX(action_combo), 0);
    
    gtk_grid_attach(GTK_GRID(grid), cidr_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), cidr_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), action_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), action_combo, 1, 1, 1, 1);
    
    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(add_dialog);
    
    gint response = gtk_dialog_run(GTK_DIALOG(add_dialog));
    
    if (response == GTK_RESPONSE_OK) {
        const char *cidr = gtk_entry_get_text(GTK_ENTRY(cidr_entry));
        int action_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(action_combo));
        
        if (strlen(cidr) > 0) {
            RouteAction action = (RouteAction)action_idx;
            route_manager_add_custom_cidr(dialog->route_manager, cidr, action);
            
            // 添加到列表视图
            GtkTreeIter iter;
            gtk_list_store_append(GTK_LIST_STORE(dialog->custom_rules_store), &iter);
            gtk_list_store_set(GTK_LIST_STORE(dialog->custom_rules_store), &iter,
                             0, cidr,
                             1, MODE_OPTIONS[action_idx],
                             -1);
            
            route_config_dialog_update_stats(dialog);
            log_message("INFO", "Added custom rule: %s -> %d", cidr, action_idx);
        }
    }
    
    gtk_widget_destroy(add_dialog);
}

// 删除自定义规则回调
static void on_remove_rule_clicked(GtkButton *button, gpointer user_data) {
    RouteConfigDialog *dialog = (RouteConfigDialog *)user_data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->custom_rules_view));
    GtkTreeIter iter;
    GtkTreeModel *model;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        char *cidr;
        char *action_str;
        gtk_tree_model_get(model, &iter, 0, &cidr, 1, &action_str, -1);
        
        // 确定动作类型
        RouteAction action = ROUTE_ACTION_DIRECT;
        if (strcmp(action_str, "走VPN") == 0) {
            action = ROUTE_ACTION_VPN;
        } else if (strcmp(action_str, "阻断") == 0) {
            action = ROUTE_ACTION_BLOCK;
        }
        
        route_manager_remove_custom_cidr(dialog->route_manager, cidr, action);
        gtk_list_store_remove(GTK_LIST_STORE(dialog->custom_rules_store), &iter);
        
        route_config_dialog_update_stats(dialog);
        log_message("INFO", "Removed custom rule: %s", cidr);
        
        g_free(cidr);
        g_free(action_str);
    }
}

// 导出规则回调
static void on_export_rules_clicked(GtkButton *button, gpointer user_data) {
    RouteConfigDialog *dialog = (RouteConfigDialog *)user_data;
    
    GtkWidget *file_chooser = gtk_file_chooser_dialog_new(
        "导出路由规则",
        GTK_WINDOW(dialog->dialog),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "取消", GTK_RESPONSE_CANCEL,
        "保存", GTK_RESPONSE_ACCEPT,
        NULL
    );
    
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(file_chooser), "route_rules.txt");
    
    if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
        route_manager_export_rules(dialog->route_manager, filename);
        g_free(filename);
    }
    
    gtk_widget_destroy(file_chooser);
}

// 浏览GeoIP文件回调
static void on_geoip_browse_clicked(GtkButton *button, gpointer user_data) {
    RouteConfigDialog *dialog = (RouteConfigDialog *)user_data;
    
    GtkWidget *file_chooser = gtk_file_chooser_dialog_new(
        "选择GeoIP数据库文件",
        GTK_WINDOW(dialog->dialog),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "取消", GTK_RESPONSE_CANCEL,
        "打开", GTK_RESPONSE_ACCEPT,
        NULL
    );
    
    if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
        gtk_entry_set_text(GTK_ENTRY(dialog->geoip_path_entry), filename);
        g_free(filename);
    }
    
    gtk_widget_destroy(file_chooser);
}

// 更新统计信息
void route_config_dialog_update_stats(RouteConfigDialog *dialog) {
    if (!dialog || !dialog->route_manager) return;
    
    int direct_count = 0, vpn_count = 0, block_count = 0;
    route_manager_get_stats(dialog->route_manager, &direct_count, &vpn_count, &block_count);
    
    char stats_text[512];
    snprintf(stats_text, sizeof(stats_text),
             "路由规则统计:\n\n"
             "直连规则: %d 条\n"
             "VPN规则: %d 条\n"
             "阻断规则: %d 条\n\n"
             "总计: %d 条",
             direct_count, vpn_count, block_count,
             direct_count + vpn_count + block_count);
    
    gtk_label_set_text(GTK_LABEL(dialog->stats_label), stats_text);
}

// 创建基本设置页面
static GtkWidget* create_basic_settings_page(RouteConfigDialog *dialog) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 15);
    
    int row = 0;
    
    // 路由模式
    GtkWidget *mode_label = gtk_label_new("路由模式:");
    gtk_widget_set_halign(mode_label, GTK_ALIGN_START);
    dialog->mode_combo = gtk_combo_box_text_new();
    for (int i = 0; MODE_OPTIONS[i] != NULL; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dialog->mode_combo), MODE_OPTIONS[i]);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->mode_combo), 
                             dialog->route_manager->config->mode);
    
    gtk_grid_attach(GTK_GRID(grid), mode_label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), dialog->mode_combo, 1, row, 2, 1);
    row++;
    
    // 分隔线
    GtkWidget *separator1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_grid_attach(GTK_GRID(grid), separator1, 0, row, 3, 1);
    row++;
    
    // GeoIP设置
    dialog->enable_geoip_check = gtk_check_button_new_with_label("启用GeoIP分流");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->enable_geoip_check),
                                 dialog->route_manager->config->enable_geoip);
    gtk_grid_attach(GTK_GRID(grid), dialog->enable_geoip_check, 0, row, 3, 1);
    row++;
    
    dialog->cn_direct_check = gtk_check_button_new_with_label("中国IP直连");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cn_direct_check),
                                 dialog->route_manager->config->cn_direct);
    gtk_grid_attach(GTK_GRID(grid), dialog->cn_direct_check, 0, row, 3, 1);
    row++;
    
    dialog->private_direct_check = gtk_check_button_new_with_label("私有IP直连");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->private_direct_check),
                                 dialog->route_manager->config->private_direct);
    gtk_grid_attach(GTK_GRID(grid), dialog->private_direct_check, 0, row, 3, 1);
    row++;
    
    dialog->lan_direct_check = gtk_check_button_new_with_label("局域网直连");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->lan_direct_check),
                                 dialog->route_manager->config->lan_direct);
    gtk_grid_attach(GTK_GRID(grid), dialog->lan_direct_check, 0, row, 3, 1);
    row++;
    
    // 分隔线
    GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_grid_attach(GTK_GRID(grid), separator2, 0, row, 3, 1);
    row++;
    
    // GeoIP数据库路径
    GtkWidget *geoip_label = gtk_label_new("GeoIP数据库:");
    gtk_widget_set_halign(geoip_label, GTK_ALIGN_START);
    dialog->geoip_path_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(dialog->geoip_path_entry),
                      dialog->route_manager->config->geoip_db_path);
    gtk_entry_set_placeholder_text(GTK_ENTRY(dialog->geoip_path_entry),
                                   "/path/to/cn_ip.txt");
    
    dialog->geoip_browse_button = gtk_button_new_with_label("浏览...");
    g_signal_connect(dialog->geoip_browse_button, "clicked",
                    G_CALLBACK(on_geoip_browse_clicked), dialog);
    
    gtk_grid_attach(GTK_GRID(grid), geoip_label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), dialog->geoip_path_entry, 1, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), dialog->geoip_browse_button, 2, row, 1, 1);
    row++;
    
    // 说明文本（使用 GtkBox 包裹 GtkLabel，避免类型错误）
    GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_top(info_box, 10);
    
    GtkWidget *info_label = gtk_label_new(
        "提示:\n"
        "• 全局模式: 所有流量通过VPN\n"
        "• PAC模式: 根据规则智能分流\n"
        "• 直连模式: 所有流量不走VPN\n\n"
        "GeoIP数据库格式: 每行一个CIDR (例如: 1.0.1.0/24)"
    );
    gtk_label_set_xalign(GTK_LABEL(info_label), 0);
    gtk_container_add(GTK_CONTAINER(info_box), info_label);
    
    gtk_grid_attach(GTK_GRID(grid), info_box, 0, row, 3, 1);
    
    return grid;
}

// 创建自定义规则页面
static GtkWidget* create_custom_rules_page(RouteConfigDialog *dialog) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
    
    // 创建列表视图
    dialog->custom_rules_store = GTK_WIDGET(gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING));
    dialog->custom_rules_view = gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(dialog->custom_rules_store)
    );
    
    // 添加列
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        "CIDR", renderer, "text", 0, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(dialog->custom_rules_view), column);
    
    column = gtk_tree_view_column_new_with_attributes(
        "动作", renderer, "text", 1, NULL
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(dialog->custom_rules_view), column);
    
    // 滚动窗口
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), dialog->custom_rules_view);
    gtk_widget_set_size_request(scrolled, -1, 300);
    
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
    
    // 按钮区域
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    
    dialog->add_rule_button = gtk_button_new_with_label("添加规则");
    dialog->remove_rule_button = gtk_button_new_with_label("删除规则");
    dialog->export_rules_button = gtk_button_new_with_label("导出规则");
    
    g_signal_connect(dialog->add_rule_button, "clicked",
                    G_CALLBACK(on_add_rule_clicked), dialog);
    g_signal_connect(dialog->remove_rule_button, "clicked",
                    G_CALLBACK(on_remove_rule_clicked), dialog);
    g_signal_connect(dialog->export_rules_button, "clicked",
                    G_CALLBACK(on_export_rules_clicked), dialog);
    
    gtk_box_pack_start(GTK_BOX(button_box), dialog->add_rule_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), dialog->remove_rule_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), dialog->export_rules_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 0);
    
    return vbox;
}

// 创建统计信息页面
static GtkWidget* create_stats_page(RouteConfigDialog *dialog) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
    
    dialog->stats_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(dialog->stats_label), 0);
    gtk_box_pack_start(GTK_BOX(vbox), dialog->stats_label, FALSE, FALSE, 0);
    
    route_config_dialog_update_stats(dialog);
    
    return vbox;
}

// 创建路由配置对话框（添加空指针检查）
RouteConfigDialog* route_config_dialog_new(OVPNClient *client, RouteManager *manager) {
    // ✅ 严格的参数验证
    if (!client) {
        log_message("ERROR", "route_config_dialog_new: client is NULL");
        return NULL;
    }
    
    if (!manager) {
        log_message("ERROR", "route_config_dialog_new: route_manager is NULL");
        return NULL;
    }
    
    if (!GTK_IS_WINDOW(client->window)) {
        log_message("ERROR", "route_config_dialog_new: client->window is not a valid GtkWindow");
        return NULL;
    }
    
    RouteConfigDialog *dialog = g_malloc0(sizeof(RouteConfigDialog));
    dialog->client = client;
    dialog->route_manager = manager;
    
    dialog->dialog = gtk_dialog_new_with_buttons(
        "路由配置",
        GTK_WINDOW(client->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "取消", GTK_RESPONSE_CANCEL,
        "保存", GTK_RESPONSE_OK,
        NULL
    );
    
    gtk_window_set_default_size(GTK_WINDOW(dialog->dialog), 600, 500);
    
    // 创建笔记本
    dialog->notebook = gtk_notebook_new();
    
    // 添加页面
    GtkWidget *basic_page = create_basic_settings_page(dialog);
    GtkWidget *rules_page = create_custom_rules_page(dialog);
    GtkWidget *stats_page = create_stats_page(dialog);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(dialog->notebook), basic_page,
                            gtk_label_new("基本设置"));
    gtk_notebook_append_page(GTK_NOTEBOOK(dialog->notebook), rules_page,
                            gtk_label_new("自定义规则"));
    gtk_notebook_append_page(GTK_NOTEBOOK(dialog->notebook), stats_page,
                            gtk_label_new("统计信息"));
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog->dialog));
    gtk_container_add(GTK_CONTAINER(content), dialog->notebook);
    
    return dialog;
}

// 显示路由配置对话框
void route_config_dialog_show(RouteConfigDialog *dialog) {
    if (!dialog) {
        log_message("ERROR", "route_config_dialog_show: dialog is NULL");
        return;
    }
    
    gtk_widget_show_all(dialog->dialog);
    
    gint response = gtk_dialog_run(GTK_DIALOG(dialog->dialog));
    
    if (response == GTK_RESPONSE_OK) {
        // 保存配置
        dialog->route_manager->config->mode = gtk_combo_box_get_active(
            GTK_COMBO_BOX(dialog->mode_combo)
        );
        dialog->route_manager->config->enable_geoip = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(dialog->enable_geoip_check)
        );
        dialog->route_manager->config->cn_direct = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(dialog->cn_direct_check)
        );
        dialog->route_manager->config->private_direct = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(dialog->private_direct_check)
        );
        dialog->route_manager->config->lan_direct = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(dialog->lan_direct_check)
        );
        
        const char *geoip_path = gtk_entry_get_text(GTK_ENTRY(dialog->geoip_path_entry));
        strncpy(dialog->route_manager->config->geoip_db_path, geoip_path,
               sizeof(dialog->route_manager->config->geoip_db_path) - 1);
        
        // 保存到配置文件
        const char *config_dir = g_get_user_config_dir();
        char config_path[1024];
        snprintf(config_path, sizeof(config_path), "%s/ovpn-client/route_config.ini", config_dir);
        
        // 确保目录存在
        char *dir = g_path_get_dirname(config_path);
        g_mkdir_with_parents(dir, 0755);
        g_free(dir);
        
        route_manager_save_config(dialog->route_manager, config_path);
        
        log_message("INFO", "Route configuration saved");
    }
    
    gtk_widget_hide(dialog->dialog);
}

// 释放路由配置对话框
void route_config_dialog_free(RouteConfigDialog *dialog) {
    if (!dialog) return;
    
    if (dialog->dialog) {
        gtk_widget_destroy(dialog->dialog);
    }
    
    g_free(dialog);
}

// 路由配置按钮回调（添加空指针检查）
static void on_route_config_clicked(GtkButton *button, gpointer user_data) {
    RouteManager *manager = (RouteManager *)user_data;
    OVPNClient *client = g_object_get_data(G_OBJECT(button), "client");
    
    // ✅ 严格的参数验证
    if (!client) {
        log_message("ERROR", "on_route_config_clicked: client is NULL");
        return;
    }
    
    if (!manager) {
        log_message("ERROR", "on_route_config_clicked: route_manager is NULL");
        if (GTK_IS_WINDOW(client->window)) {
            show_error_message(GTK_WINDOW(client->window), 
                             "路由管理器未初始化\n请确保应用程序已完全启动");
        }
        return;
    }
    
    RouteConfigDialog *dialog = route_config_dialog_new(client, manager);
    if (!dialog) {
        log_message("ERROR", "Failed to create route config dialog");
        if (GTK_IS_WINDOW(client->window)) {
            show_error_message(GTK_WINDOW(client->window), 
                             "无法创建路由配置对话框");
        }
        return;
    }
    
    route_config_dialog_show(dialog);
    route_config_dialog_free(dialog);
}

// 添加路由配置按钮到主窗口（添加空指针检查）
void add_route_config_button(OVPNClient *client, GtkWidget *container, RouteManager *manager) {
    if (!client || !container || !manager) {
        log_message("ERROR", "add_route_config_button: invalid parameters");
        return;
    }
    
    if (!GTK_IS_CONTAINER(container)) {
        log_message("ERROR", "add_route_config_button: container is not a GtkContainer");
        return;
    }
    
    GtkWidget *button = gtk_button_new_with_label("路由配置");
    g_object_set_data(G_OBJECT(button), "client", client);
    g_signal_connect(button, "clicked", G_CALLBACK(on_route_config_clicked), manager);
    gtk_container_add(GTK_CONTAINER(container), button);
}

// ✅ 修复：显示路由配置对话框的便捷函数（添加完整的错误处理）
void route_ui_show_config_dialog(OVPNClient *client) {
    // 严格的参数验证
    if (!client) {
        log_message("ERROR", "route_ui_show_config_dialog: client is NULL");
        return;
    }
    
    if (!client->route_manager) {
        log_message("ERROR", "route_ui_show_config_dialog: client->route_manager is NULL");
        
        // 显示友好的错误提示（确保 window 是有效的 GtkWindow）
        if (client->window && GTK_IS_WINDOW(client->window)) {
            show_error_message(GTK_WINDOW(client->window),
                             "路由管理器尚未初始化\n"
                             "请等待应用程序完全启动后再尝试打开路由配置");
        }
        return;
    }
    
    // 验证 window 是否为有效的 GtkWindow
    if (!client->window || !GTK_IS_WINDOW(client->window)) {
        log_message("ERROR", "route_ui_show_config_dialog: client->window is not a valid GtkWindow");
        return;
    }
    
    // 创建并显示对话框
    RouteConfigDialog *dialog = route_config_dialog_new(client, client->route_manager);
    if (!dialog) {
        log_message("ERROR", "Failed to create route config dialog");
        show_error_message(GTK_WINDOW(client->window), 
                         "无法创建路由配置对话框\n请查看日志获取详细信息");
        return;
    }
    
    route_config_dialog_show(dialog);
    route_config_dialog_free(dialog);
}