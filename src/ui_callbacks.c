#include <gtk/gtk.h>
#include <libnm/NetworkManager.h>
#include <arpa/inet.h>

#include "../include/ui_callbacks.h"
#include "../include/structs.h"
#include "../include/nm_connection.h"
#include "../include/notify.h"
#include "../include/log_util.h"
#include "../include/core_logic.h"



// 窗口关闭事件处理
gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    (void)event;
    
    log_message("INFO", "Window close requested");
    
    // 如果有系统托盘指示器，隐藏窗口到托盘
    if (client->indicator) {
        log_message("INFO", "Hiding window to system tray");
        gtk_widget_hide(widget);
        return TRUE; // 阻止窗口真正关闭
    } else {
        // 没有系统托盘时，退出应用程序
        log_message("INFO", "No system tray - quitting application");
        g_application_quit(G_APPLICATION(client->app));
        return FALSE;
    }
}

// 导入文件按钮回调
void import_file_clicked(GtkWidget *widget, gpointer user_data) {
    
    OVPNClient *client = (OVPNClient *)user_data;
    GtkWidget *dialog;
    GtkFileFilter *filter;
    
    (void)widget; // 避免未使用参数警告
    
    dialog = gtk_file_chooser_dialog_new("Select .ovpn file",
                                       GTK_WINDOW(client->window),
                                       GTK_FILE_CHOOSER_ACTION_OPEN,
                                       "_Cancel", GTK_RESPONSE_CANCEL,
                                       "_Open", GTK_RESPONSE_ACCEPT,
                                       NULL);
    
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "OpenVPN config");
    gtk_file_filter_add_pattern(filter, "*.ovpn");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    
    g_signal_connect(dialog, "response", G_CALLBACK(file_chosen_cb), client);
    gtk_widget_show_all(dialog);
    log_message("INFO", "OVPN file import and parsing completed.");
}

// 断开VPN的异步回调
void disconnect_done(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    (void)source_object;
    (void)res;
    OVPNClient *client = (OVPNClient *)user_data;
    GError *error = NULL;

    if (error) {
        log_message("ERROR", "Deactivate VPN failed: %s", error->message);
        show_notification(client, error->message, TRUE);
        g_error_free(error);
        return;
    }

    client->active_connection = NULL;
    gtk_label_set_text(GTK_LABEL(client->connection_status_label), "VPN Status: Disconnected");
    gtk_widget_set_sensitive(client->connect_button, TRUE);
    gtk_widget_set_sensitive(client->disconnect_button, FALSE);
    show_notification(client, "VPN disconnected successfully", FALSE);
    log_message("INFO", "VPN disconnected successfully");
}


// 测试连接按钮回调
void test_connection_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    
    (void)widget; // 避免未使用参数警告
    
    if (!client->parsed_config || client->parsed_config->remote_count == 0) {
        show_notification(client, "No server configuration found", TRUE);
        return;
    }
    
    RemoteServer *remote = &client->parsed_config->remote[0];
    int port = atoi(remote->port);
    
    log_message("INFO", "Testing connection to %s:%d (%s)", remote->server, port, remote->proto);
    
    // 简单的socket连接测试
    int sock;
    struct sockaddr_in server_addr;
    
    if (strcmp(remote->proto, "tcp") == 0) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
    } else {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
    }
    
    if (sock < 0) {
        show_notification(client, "Failed to create socket", TRUE);
        return;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, remote->server, &server_addr.sin_addr);
    
    if (strcmp(remote->proto, "tcp") == 0) {
        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
            show_notification(client, "TCP connection successful", FALSE);
        } else {
            show_notification(client, "TCP connection failed", TRUE);
        }
    } else {
        // UDP测试（简单发送数据包）
        char test_data[] = "\x38\x01\x00\x00\x00\x00\x00\x00\x00";
        if (sendto(sock, test_data, sizeof(test_data), 0, 
                  (struct sockaddr*)&server_addr, sizeof(server_addr)) > 0) {
            show_notification(client, "UDP connection test sent", FALSE);
        } else {
            show_notification(client, "UDP connection test failed", TRUE);
        }
    }
    
    close(sock);
}

// 连接VPN按钮回调
void connect_vpn_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    const char *username = NULL;
    const char *password = NULL;
    const char *connection_name = NULL;

    (void)widget; // 避免未使用参数警告
    (void)password;

    // 1. 优先检查：是否从下拉列表中选择了已存在的连接
    gint active_item = gtk_combo_box_get_active(GTK_COMBO_BOX(client->connection_combo_box));
    if (active_item != -1 && active_item < client->existing_connections->len) {
        connection_name = g_ptr_array_index(client->existing_connections, active_item);
        log_message("INFO", "Selected existing connection from dropdown: %s", connection_name);
    } 
    
    // 2. 次要检查：是否通过导入 .ovpn 文件创建了临时连接
    else if (client->created_connection) {
        // 使用创建的临时连接的名称
        connection_name = nm_connection_get_id(client->created_connection);
        log_message("INFO", "Using temporary connection created from .ovpn file: %s", connection_name);
    } 
    
    // 3. 最后检查：文本输入框中是否有连接名称
    else {
        connection_name = gtk_entry_get_text(GTK_ENTRY(client->name_entry));
        if (strlen(connection_name) == 0) {
            show_notification(client, "No VPN connection available. Please create one, select an existing one, or enter a name.", TRUE);
            return;
        }
        log_message("INFO", "Using connection name from entry field: %s", connection_name);
    }

    // 获取用户名和密码
    username = gtk_entry_get_text(GTK_ENTRY(client->username_entry));
    password = gtk_entry_get_text(GTK_ENTRY(client->password_entry));

    // 根据解析出的配置文件检查是否需要用户名/密码
    if (client->parsed_config && client->parsed_config->auth_user_pass) {
        if (strlen(username) == 0) {
            show_notification(client, "Username is required for this VPN connection", TRUE);
            return;
        }
    }

    // UI 状态更新
    show_notification(client, "Connecting to VPN...", FALSE);
    gtk_label_set_text(GTK_LABEL(client->connection_status_label), "VPN Status: Connecting...");
    gtk_widget_set_sensitive(client->connect_button, FALSE);
    gtk_widget_set_sensitive(client->disconnect_button, FALSE);
    
    // 调用 NM 模块中的连接激活函数
    activate_vpn_connection(client, connection_name);
}

// 断开VPN按钮回调
void disconnect_vpn_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    
    (void)widget; // 避免未使用参数警告
    
    if (!client->active_connection) {
        show_notification(client, "No active VPN connection to disconnect", TRUE);
        return;
    }
    
    log_message("INFO", "Disconnecting VPN...");
    nm_client_deactivate_connection_async(client->nm_client, client->active_connection, NULL, disconnect_done, client);
    gtk_label_set_text(GTK_LABEL(client->connection_status_label), "VPN Status: Disconnecting...");
    gtk_widget_set_sensitive(client->disconnect_button, FALSE);
}

// 文件选择对话框回调
void file_chosen_cb(GtkWidget *dialog, gint response_id, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;

    if (response_id == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            strcpy(client->ovpn_file_path, filename);

            if (client->parsed_config) {
                g_free(client->parsed_config);
            }
            client->parsed_config = parse_ovpn_file(filename);

            if (client->parsed_config) {
                char status_text[256];
                char *basename = g_path_get_basename(filename);
                snprintf(status_text, sizeof(status_text), "Imported: %s", basename);
                gtk_label_set_text(GTK_LABEL(client->status_label), status_text);

                char *name_without_ext = g_strndup(basename, strlen(basename) - 5);
                gtk_entry_set_text(GTK_ENTRY(client->name_entry), name_without_ext);

                if (validate_certificates(client->parsed_config)) {
                    const char *connection_name = gtk_entry_get_text(GTK_ENTRY(client->name_entry));
                    if (strlen(connection_name) > 0) {
                        client->created_connection = create_nm_vpn_connection(client, connection_name, client->parsed_config);
                        if (client->created_connection) {
                            nm_client_add_connection_async(client->nm_client, client->created_connection,
                                                         TRUE, NULL, add_connection_done, client);
                            show_notification(client, "VPN connection created successfully!", FALSE);
                        } else {
                            show_notification(client, "Failed to create VPN connection", TRUE);
                        }
                    }
                }

                g_free(name_without_ext);
                g_free(basename);
            }

            g_free(filename);
        }
    }

    gtk_widget_destroy(dialog);
}



// 退出菜单项回调
void quit_menu_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    OVPNClient *client = (OVPNClient *)user_data;
    
    log_message("INFO", "Quit requested from menu");
    
    // 使用 GtkApplication 的 quit 方法安全退出
    g_application_quit(G_APPLICATION(client->app));
}

// 显示窗口菜单项回调
void show_window_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    (void)widget;
    
    gtk_window_present(GTK_WINDOW(client->window));
    gtk_widget_show_all(client->window);
}

