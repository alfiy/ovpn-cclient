#include <gtk/gtk.h>
#include <libayatana-appindicator3-0.1/libayatana-appindicator/app-indicator.h>

#include "../include/log_util.h"
#include "../include/structs.h"
#include "../include/ui_callbacks.h"
#include "../include/config.h"



// 创建主窗口
void create_main_window(OVPNClient *client) {
    GtkWidget *main_vbox, *hbox, *frame, *scrolled, *button;
    GtkWidget *auth_vbox;
    
    log_message("INFO", "Creating main window...");
    
    client->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(client->window), "OVPN Client");
    gtk_window_set_default_size(GTK_WINDOW(client->window), 800, 600);
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
    
    // 连接名称输入
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("VPN Connection Name:"), FALSE, FALSE, 0);
    client->name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(client->name_entry), "Enter connection name...");
    gtk_box_pack_start(GTK_BOX(hbox), client->name_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, FALSE, 0);
    
    // 认证框架
    frame = gtk_frame_new("Authentication (if required)");
    auth_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(auth_vbox), 5);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Username:"), FALSE, FALSE, 0);
    client->username_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(client->username_entry), "VPN username (optional)");
    gtk_box_pack_start(GTK_BOX(hbox), client->username_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(auth_vbox), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Password:"), FALSE, FALSE, 0);
    client->password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(client->password_entry), "VPN password (optional)");
    gtk_entry_set_visibility(GTK_ENTRY(client->password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), client->password_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(auth_vbox), hbox, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(frame), auth_vbox);
    gtk_box_pack_start(GTK_BOX(main_vbox), frame, FALSE, FALSE, 0);
    
    // 按钮区域
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    client->test_button = gtk_button_new_with_label("Test Server Connection");
    g_signal_connect(client->test_button, "clicked", G_CALLBACK(test_connection_clicked), client);
    gtk_widget_set_no_show_all(client->test_button, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), client->test_button, FALSE, FALSE, 0);

    client->connect_button = gtk_button_new_with_label("Connect to VPN");
    g_signal_connect(client->connect_button, "clicked", G_CALLBACK(connect_vpn_clicked), client);
    gtk_widget_set_sensitive(client->connect_button, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), client->connect_button, FALSE, FALSE, 0);
    
    client->disconnect_button = gtk_button_new_with_label("Disconnect VPN");
    g_signal_connect(client->disconnect_button, "clicked", G_CALLBACK(disconnect_vpn_clicked), client);
    gtk_widget_set_sensitive(client->disconnect_button, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), client->disconnect_button, FALSE, FALSE, 0);

    // 新增删除按钮
    client->delete_button = gtk_button_new_with_label("Delete VPN Config");
    g_signal_connect(client->delete_button, "clicked", G_CALLBACK(delete_vpn_clicked), client);
    gtk_widget_set_sensitive(client->delete_button, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), client->delete_button, FALSE, FALSE, 0);

    
    gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, FALSE, 0);
    
    // 配置分析区域
    frame = gtk_frame_new("OpenVPN Configuration Analysis");
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, -1, 200);
    
    client->result_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(client->result_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(client->result_view), GTK_WRAP_WORD_CHAR);
    
    // 设置默认文本
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(client->result_view));
    gtk_text_buffer_set_text(buffer, 
        "Import an .ovpn file to see configuration analysis here.\n\n"
        "This application provides:\n"
        "• Real VPN connections through NetworkManager\n"
        "• OpenVPN configuration parsing\n"
        "• Connection testing and monitoring\n"
        "• System tray integration\n\n"
        "Log file: /tmp/ovpn_client.log", -1);
    
    gtk_container_add(GTK_CONTAINER(scrolled), client->result_view);
    gtk_container_add(GTK_CONTAINER(frame), scrolled);
    gtk_box_pack_start(GTK_BOX(main_vbox), frame, TRUE, TRUE, 0);
    
    log_message("INFO", "Main window created successfully");
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