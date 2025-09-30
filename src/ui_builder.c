#include <gtk/gtk.h>
#include <libayatana-appindicator3-0.1/libayatana-appindicator/app-indicator.h>

#include "../include/log_util.h"
#include "../include/structs.h"
#include "../include/ui_callbacks.h"
#include "../include/config.h"



// 创建主窗口
void create_main_window(OVPNClient *client) {
    GtkWidget *main_vbox, *hbox, *scrolled, *button;
    
    log_message("INFO", "Creating main window...");
    
    client->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(client->window), "OVPN Client");
    gtk_window_set_default_size(GTK_WINDOW(client->window), 400, 600);
    gtk_container_set_border_width(GTK_CONTAINER(client->window), 10);
    
    // 设置窗口关闭事件处理
    g_signal_connect(client->window, "delete-event", G_CALLBACK(on_window_delete_event), client);
    
    // 主布局
    main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(client->window), main_vbox);
    
    // 导入按钮
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    button = gtk_button_new_with_label("导入配置文件");
    g_signal_connect(button, "clicked", G_CALLBACK(import_file_clicked), client);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, FALSE, 0);

    // 创建一个新的 GtkComboBoxText 来显示已有的连接
    client->connection_combo_box = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text(client->connection_combo_box, "Select an existing VPN connection");
    gtk_box_pack_start(GTK_BOX(main_vbox), client->connection_combo_box, FALSE, FALSE, 5);
    // 连接 "changed" 信号到新的回调函数
    g_signal_connect(client->connection_combo_box, "changed", G_CALLBACK(on_connection_selected), client);


    // 状态标签
    client->status_label = gtk_label_new("No file imported yet.");
    gtk_widget_set_halign(client->status_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_vbox), client->status_label, FALSE, FALSE, 0);
    
    // 通知标签
    client->notification_label = gtk_label_new("");
    gtk_label_set_line_wrap(GTK_LABEL(client->notification_label), TRUE);
    gtk_widget_set_no_show_all(client->notification_label, TRUE);
    gtk_box_pack_start(GTK_BOX(main_vbox), client->notification_label, FALSE, FALSE, 0);
    
    // VPN连接状态
    client->connection_status_label = gtk_label_new("VPN状态: 未连接");
    gtk_widget_set_halign(client->connection_status_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_vbox), client->connection_status_label, FALSE, FALSE, 0);
    
    
    // 按钮区域
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    client->test_button = gtk_button_new_with_label("Test Server Connection");
    g_signal_connect(client->test_button, "clicked", G_CALLBACK(test_connection_clicked), client);
    gtk_widget_set_no_show_all(client->test_button, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), client->test_button, FALSE, FALSE, 0);

    client->connect_button = gtk_button_new_with_label("连接VPN");
    g_signal_connect(client->connect_button, "clicked", G_CALLBACK(connect_vpn_clicked), client);
    gtk_widget_set_sensitive(client->connect_button, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), client->connect_button, FALSE, FALSE, 0);
    
    client->disconnect_button = gtk_button_new_with_label("断开VPN");
    g_signal_connect(client->disconnect_button, "clicked", G_CALLBACK(disconnect_vpn_clicked), client);
    gtk_widget_set_sensitive(client->disconnect_button, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), client->disconnect_button, FALSE, FALSE, 0);

    // 新增删除按钮
    client->delete_button = gtk_button_new_with_label("删除配置文件");
    g_signal_connect(client->delete_button, "clicked", G_CALLBACK(delete_vpn_clicked), client);
    gtk_widget_set_sensitive(client->delete_button, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), client->delete_button, FALSE, FALSE, 0);

    
    gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, FALSE, 0);
    


    // 配置分析区域
    client->config_analysis_frame = gtk_frame_new("配置文件解析");
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, -1, 200);

    client->result_view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(client->result_view, FALSE);
    gtk_text_view_set_wrap_mode(client->result_view, GTK_WRAP_WORD_CHAR);

    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(client->result_view));
    gtk_container_add(GTK_CONTAINER(client->config_analysis_frame), scrolled);
    gtk_box_pack_start(GTK_BOX(main_vbox), client->config_analysis_frame, TRUE, TRUE, 0);


    // 连接日志区域
    client->connection_log_frame = gtk_frame_new("连接日志");
    GtkWidget *log_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(log_scrolled),
                                GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(log_scrolled, -1, 200);

    client->log_view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(client->log_view, FALSE);
    gtk_text_view_set_wrap_mode(client->log_view, GTK_WRAP_WORD_CHAR);

    gtk_container_add(GTK_CONTAINER(log_scrolled), GTK_WIDGET(client->log_view));
    gtk_container_add(GTK_CONTAINER(client->connection_log_frame), log_scrolled);
    gtk_box_pack_start(GTK_BOX(main_vbox), client->connection_log_frame, TRUE, TRUE, 0);

    gtk_widget_show_all(main_vbox);

    log_message("INFO", "Main window created successfully");
    gtk_widget_hide(client->config_analysis_frame);
    gtk_widget_hide(client->connection_log_frame);
    log_message("INFO", "All frame hidden, window will be shown");

    
}


// 配置分析区域内容更新
void update_config_analysis_view(OVPNClient *client) {
    if (!client || !client->parsed_config || !client->result_view) return;

    OVPNConfig *conf = client->parsed_config;
    char analysis[2048] = {0};
    int offset = 0;

    offset += snprintf(analysis + offset, sizeof(analysis) - offset, 
        "【配置解析成功】\n\n");

    // remote/server部分
    for (int i = 0; i < conf->remote_count; i++) {
        offset += snprintf(analysis + offset, sizeof(analysis) - offset,
            "远程服务器 %d：\n"
            "  服务器地址: %s\n"
            "  端口: %s\n"
            "  协议: %s\n", 
            i+1, conf->remote[i].server, conf->remote[i].port, conf->remote[i].proto);
    }

    // 加密/证书部分
    offset += snprintf(analysis + offset, sizeof(analysis) - offset,
        "\n加密参数:\n"
        "  Cipher: %s\n"
        "  TLS最低版本: %s\n",
        conf->cipher[0] ? conf->cipher : "(未指定)",
        conf->tls_version_min[0] ? conf->tls_version_min : "(未指定)");

    offset += snprintf(analysis + offset, sizeof(analysis) - offset,
        "\n证书相关:\n"
        "  CA证书: %s\n"
        "  用户证书: %s\n"
        "  私钥文件: %s\n",
        conf->ca[0] ? conf->ca : "(未指定)",
        conf->cert[0] ? conf->cert : "(未指定)",
        conf->key[0] ? conf->key : "(未指定)");

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(client->result_view));
    gtk_text_buffer_set_text(buffer, analysis, -1);
}


// 创建系统托盘指示器 - 添加错误处理
gboolean create_indicator(OVPNClient *client) {
    GtkWidget *menu_item;
    
    log_message("INFO", "Creating system tray indicator...");
    
    client->indicator = app_indicator_new(APP_ID,
                                        "network-vpn", /* 初始名随便 */
                                        APP_INDICATOR_CATEGORY_SYSTEM_SERVICES);
    
    if (!client->indicator) {
        log_message("WARNING", "Failed to create system tray indicator - continuing without it");
        return FALSE;
    }
    
    app_indicator_set_status(client->indicator, APP_INDICATOR_STATUS_ACTIVE);
    
    // 设置系统托盘图标
    app_indicator_set_icon_full(client->indicator, "/usr/share/pixmaps/icons8-openvpn-48.png", "OVPN Client Tray Icon");

    // 可选：设置关注状态图标
    app_indicator_set_attention_icon_full(client->indicator, "/usr/share/pixmaps/icons8-openvpn-48.png", "OVPN Client Attention");
    
    // 创建菜单
    client->indicator_menu = gtk_menu_new();
    
    menu_item = gtk_menu_item_new_with_label("Show Window");
    g_signal_connect(menu_item, "activate", G_CALLBACK(show_window_clicked), client);
    gtk_menu_shell_append(GTK_MENU_SHELL(client->indicator_menu), menu_item);
    
    menu_item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(client->indicator_menu), menu_item);
    
    menu_item = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(menu_item, "activate", G_CALLBACK(quit_menu_clicked), client);
    gtk_menu_shell_append(GTK_MENU_SHELL(client->indicator_menu), menu_item);
    
    gtk_widget_show_all(client->indicator_menu);
    app_indicator_set_menu(client->indicator, GTK_MENU(client->indicator_menu));
    
    log_message("INFO", "System tray indicator created successfully");
    return TRUE;
}