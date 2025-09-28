#include <gtk/gtk.h>
#include <libnm/NetworkManager.h>
#include <arpa/inet.h>
#include <unistd.h> // For close()
#include <sys/time.h> // For struct timeval
#include <sys/socket.h> // For socket functions

#include "../include/ui_callbacks.h"
#include "../include/structs.h"
#include "../include/nm_connection.h"
#include "../include/notify.h"
#include "../include/log_util.h"
#include "../include/core_logic.h"
#include "../include/config.h"

// 删除配置按钮回调
void delete_vpn_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    if (!client || !client->connection_combo_box || !client->existing_connections) {
        show_notification(client, "Internal error: No connection selected.", TRUE);
        return;
    }
    gint active_item = gtk_combo_box_get_active(GTK_COMBO_BOX(client->connection_combo_box));
    if (active_item < 0 || active_item >= (gint)client->existing_connections->len) {
        show_notification(client, "No connection selected to delete.", TRUE);
        return;
    }
    const char *connection_name = (const char *)g_ptr_array_index(client->existing_connections, active_item);
    if (!connection_name || strlen(connection_name) == 0) {
        show_notification(client, "Invalid connection name.", TRUE);
        return;
    }
    // 删除连接
    char command[256];
    snprintf(command, sizeof(command), "nmcli connection delete \"%s\"", connection_name);
    int ret = system(command);
    if (ret == 0) {
        show_notification(client, "VPN connection deleted successfully!", FALSE);
        log_message("INFO", "VPN connection deleted: %s", connection_name);
        // 刷新下拉框
        scanned_connections_cb(NULL, NULL, client);
    } else {
        show_notification(client, "Failed to delete VPN connection.", TRUE);
    }
}


// 安全释放OVPN配置的函数
static void safe_free_ovpn_config(OVPNConfig *config) {
    if (!config) {
        return;
    }
    
    // 根据您的OVPNConfig结构体定义来释放内存
    // 这里使用通用的free，您可能需要根据实际情况调整
    if (config) {
        // 如果OVPNConfig是用malloc分配的，使用free
        // 如果是用g_malloc分配的，使用g_free
        // 暂时使用g_free，您可以根据实际情况调整
        g_free(config);
    }
}

// 当下拉列表选中项改变时触发的回调
void on_connection_selected(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    if (!client || !widget) {
        log_message("ERROR", "Invalid parameters in on_connection_selected");
        return;
    }
    gint active_item = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    gboolean valid = (active_item != -1 && client->existing_connections &&
                      active_item >= 0 && active_item < (gint)client->existing_connections->len);
    gtk_widget_set_sensitive(client->connect_button, valid);
    if (client->delete_button) {
        gtk_widget_set_sensitive(client->delete_button, valid);
    }
}

// 窗口关闭事件处理
gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    (void)event;
    
    // 添加空指针检查
    if (!client) {
        log_message("ERROR", "Invalid client in on_window_delete_event");
        return FALSE;
    }
    
    log_message("INFO", "Window close requested");
    if (client->indicator) {
        log_message("INFO", "Hiding window to system tray");
        gtk_widget_hide(widget);
        return TRUE;
    } else {
        log_message("INFO", "No system tray - quitting application");
        if (client->app) {
            g_application_quit(G_APPLICATION(client->app));
        }
        return FALSE;
    }
}

// 导入文件按钮回调
void import_file_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    GtkWidget *dialog;
    GtkFileFilter *filter;
    (void)widget;
    
    // 添加空指针检查
    if (!client || !client->window) {
        log_message("ERROR", "Invalid client or window in import_file_clicked");
        return;
    }
    
    dialog = gtk_file_chooser_dialog_new("Select .ovpn file",
                                         GTK_WINDOW(client->window),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Open", GTK_RESPONSE_ACCEPT,
                                         NULL);
    
    if (!dialog) {
        log_message("ERROR", "Failed to create file chooser dialog");
        return;
    }
    
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "OpenVPN config");
    gtk_file_filter_add_pattern(filter, "*.ovpn");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    g_signal_connect(dialog, "response", G_CALLBACK(file_chosen_cb), client);
    gtk_widget_show_all(dialog);
}

// 断开VPN的异步回调
void disconnect_done(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    (void)source_object;
    OVPNClient *client = (OVPNClient *)user_data;
    GError *error = NULL;

    // 添加空指针检查
    if (!client || !client->nm_client) {
        log_message("ERROR", "Invalid client in disconnect_done");
        return;
    }

    nm_client_deactivate_connection_finish(client->nm_client, res, &error);

    if (error) {
        log_message("ERROR", "Deactivate VPN failed: %s", error->message);
        show_notification(client, error->message, TRUE);
        g_error_free(error);
        
        if (client->connect_button) {
            gtk_widget_set_sensitive(client->connect_button, TRUE);
        }
        if (client->disconnect_button) {
            gtk_widget_set_sensitive(client->disconnect_button, FALSE);
        }
        return;
    }

    client->active_connection = NULL;
    if (client->connection_status_label) {
        gtk_label_set_text(GTK_LABEL(client->connection_status_label), "VPN Status: Disconnected");
    }
    if (client->connect_button) {
        gtk_widget_set_sensitive(client->connect_button, TRUE);
    }
    if (client->disconnect_button) {
        gtk_widget_set_sensitive(client->disconnect_button, FALSE);
    }
    show_notification(client, "VPN disconnected successfully", FALSE);
    log_message("INFO", "VPN disconnected successfully");
}

// 测试连接按钮回调 - 修复timeval问题
void test_connection_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    (void)widget;
    
    // 添加空指针检查
    if (!client) {
        log_message("ERROR", "Invalid client in test_connection_clicked");
        return;
    }
    
    if (!client->parsed_config || client->parsed_config->remote_count == 0) {
        show_notification(client, "No server configuration found", TRUE);
        return;
    }
    
    RemoteServer *remote = &client->parsed_config->remote[0];
    int port = atoi(remote->port);
    log_message("INFO", "Testing connection to %s:%d (%s)", remote->server, port, remote->proto);
    
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
    
    // 设置超时 - 修复struct timeval问题
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_message("WARNING", "Failed to set receive timeout");
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_message("WARNING", "Failed to set send timeout");
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // 改进IP地址解析
    if (inet_pton(AF_INET, remote->server, &server_addr.sin_addr) != 1) {
        show_notification(client, "Invalid server IP address", TRUE);
        close(sock);
        return;
    }
    
    if (strcmp(remote->proto, "tcp") == 0) {
        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
            show_notification(client, "TCP connection successful", FALSE);
        } else {
            show_notification(client, "TCP connection failed", TRUE);
        }
    } else {
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

// 连接VPN按钮回调 - 主要修复区域
void connect_vpn_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    const char *username = NULL;
    const char *password = NULL;
    const char *connection_name = NULL;
    (void)widget;
    (void)password;

    // 添加全面的空指针检查
    if (!client) {
        log_message("ERROR", "Invalid client in connect_vpn_clicked");
        return;
    }
    
    if (!client->connection_combo_box || !client->name_entry) {
        log_message("ERROR", "Invalid UI elements in connect_vpn_clicked");
        show_notification(client, "UI elements not properly initialized", TRUE);
        return;
    }

    gint active_item = gtk_combo_box_get_active(GTK_COMBO_BOX(client->connection_combo_box));
    
    // 1. 从下拉列表中获取连接名称 - 修复边界检查
    if (active_item != -1 && client->existing_connections && 
        active_item >= 0 && active_item < (gint)client->existing_connections->len) {
        // 安全地从列表中获取连接名称
        gpointer item_ptr = g_ptr_array_index(client->existing_connections, active_item);
        if (item_ptr) {
            connection_name = (const char *)item_ptr;
            log_message("INFO", "Selected existing connection from dropdown: %s", connection_name);
        } else {
            log_message("WARNING", "Null connection name at index %d", active_item);
        }
    }
    
    // 2. 如果下拉列表没有有效选择，使用文本输入框中的名称
    if (!connection_name || strlen(connection_name) == 0) {
        connection_name = gtk_entry_get_text(GTK_ENTRY(client->name_entry));
        if (!connection_name || strlen(connection_name) == 0) {
            show_notification(client, "No VPN connection available. Please create one, select an existing one, or enter a name.", TRUE);
            return;
        }
        log_message("INFO", "Using connection name from entry field: %s", connection_name);
    }

    // 验证用户名（如果需要）
    if (client->username_entry) {
        username = gtk_entry_get_text(GTK_ENTRY(client->username_entry));
        if (client->parsed_config && client->parsed_config->auth_user_pass) {
            if (!username || strlen(username) == 0) {
                show_notification(client, "Username is required for this VPN connection", TRUE);
                return;
            }
        }
    }

    // 更新UI状态
    show_notification(client, "Connecting to VPN...", FALSE);
    if (client->connection_status_label) {
        gtk_label_set_text(GTK_LABEL(client->connection_status_label), "VPN Status: Connecting...");
    }
    if (client->connect_button) {
        gtk_widget_set_sensitive(client->connect_button, FALSE);
    }
    if (client->disconnect_button) {
        gtk_widget_set_sensitive(client->disconnect_button, FALSE);
    }
    
    // 调用 NM 模块中的连接激活函数
    activate_vpn_connection(client, connection_name);
}

// 断开VPN按钮回调
void disconnect_vpn_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    (void)widget;
    
    // 添加空指针检查
    if (!client) {
        log_message("ERROR", "Invalid client in disconnect_vpn_clicked");
        return;
    }
    
    if (!client->active_connection) {
        show_notification(client, "No active VPN connection to disconnect", TRUE);
        return;
    }
    
    if (!client->nm_client) {
        log_message("ERROR", "NetworkManager client not available");
        show_notification(client, "NetworkManager client not available", TRUE);
        return;
    }
    
    log_message("INFO", "Disconnecting VPN...");
    nm_client_deactivate_connection_async(client->nm_client, client->active_connection, 
                                          NULL, disconnect_done, client);
    
    if (client->connection_status_label) {
        gtk_label_set_text(GTK_LABEL(client->connection_status_label), "VPN Status: Disconnecting...");
    }
    if (client->disconnect_button) {
        gtk_widget_set_sensitive(client->disconnect_button, FALSE);
    }
    
    // 注意：不要在这里设置 active_connection = NULL，等异步回调完成
}

// 封装：刷新下拉列表函数
void refresh_connection_combo_box(OVPNClient *client) {
    if (!client || !client->connection_combo_box || !client->existing_connections) return;
    GtkComboBoxText *combo = GTK_COMBO_BOX_TEXT(client->connection_combo_box);
    gtk_combo_box_text_remove_all(combo);
    for (guint i = 0; i < client->existing_connections->len; i++) {
        gtk_combo_box_text_append_text(combo, (char*)g_ptr_array_index(client->existing_connections, i));
    }
}

// 查找VPN连接是否同名，并返回UUID（用于后续覆盖配置）
static gboolean find_nmcli_vpn_uuid(const char *connection_name, char *found_uuid, size_t uuid_buf_size)
{
    FILE *fp = popen("nmcli -t -f NAME,UUID,TYPE connection show", "r");
    if (!fp) return FALSE;
    char line[512];
    gboolean found = FALSE;
    while (fgets(line, sizeof(line), fp)) {
        char *name = strtok(line, ":");
        char *uuid = strtok(NULL, ":");
        char *type = strtok(NULL, "\n");
        if (type && strcmp(type, "vpn") == 0 && name && strcmp(name, connection_name) == 0) {
            if (uuid && found_uuid)
                strncpy(found_uuid, uuid, uuid_buf_size - 1);
            found = TRUE;
            break;
        }
    }
    pclose(fp);
    return found;
}

static void delete_nmcli_vpn_by_name(const char *connection_name)
{
    char command[256];
    snprintf(command, sizeof(command), "nmcli connection delete \"%s\"", connection_name);
    system(command);
}

void file_chosen_cb(GtkWidget *dialog, gint response_id, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    if (!client) {
        log_message("ERROR", "Invalid client in file_chosen_cb");
        if (dialog) gtk_widget_destroy(dialog);
        return;
    }

    if (response_id == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            if (strlen(filename) < sizeof(client->ovpn_file_path)) {
                g_strlcpy(client->ovpn_file_path, filename, sizeof(client->ovpn_file_path));
            } else {
                log_message("ERROR", "Filename too long: %s", filename);
                show_notification(client, "Filename is too long", TRUE);
                g_free(filename);
                gtk_widget_destroy(dialog);
                return;
            }

            if (client->parsed_config) {
                safe_free_ovpn_config(client->parsed_config);
                client->parsed_config = NULL;
            }
            client->parsed_config = parse_ovpn_file(filename);

            if (client->parsed_config) {
                char status_text[256];
                char *basename = g_path_get_basename(filename);

                if (basename) {
                    snprintf(status_text, sizeof(status_text), "Imported: %s", basename);
                    if (client->status_label) {
                        gtk_label_set_text(GTK_LABEL(client->status_label), status_text);
                    }
                    size_t basename_len = strlen(basename);
                    if (basename_len > 5) { // .ovpn扩展名
                        char *name_without_ext = g_strndup(basename, basename_len - 5);
                        if (name_without_ext && client->name_entry) {
                            gtk_entry_set_text(GTK_ENTRY(client->name_entry), name_without_ext);
                        }
                        g_free(name_without_ext);
                    }
                    g_free(basename);
                }

                if (validate_certificates(client->parsed_config)) {
                    const char *connection_name = NULL;
                    if (client->name_entry) {
                        connection_name = gtk_entry_get_text(GTK_ENTRY(client->name_entry));
                    }
                    if (connection_name && strlen(connection_name) > 0) {
                        char existing_uuid[128] = {0};
                        gboolean exists = find_nmcli_vpn_uuid(connection_name, existing_uuid, sizeof(existing_uuid));
                        char command[1024];
                        int ret;

                        if (exists) {
                            // 旧版本 nmcli 不支持 --update，需先删除再导入
                            delete_nmcli_vpn_by_name(connection_name); // 删除已有连接
                            snprintf(command, sizeof(command),
                                "nmcli connection import type openvpn file '%s'", filename);
                            ret = system(command);
                            if (ret == 0)
                                show_notification(client, "VPN configuration updated successfully!", FALSE);
                            else
                                show_notification(client, "Failed to update VPN configuration!", TRUE);
                            log_message("INFO", "Overwrote existing VPN connection: %s (UUID=%s), status=%d",
                                        connection_name, existing_uuid, ret);
                        } else {
                            // 新建VPN
                            snprintf(command, sizeof(command),
                                "nmcli connection import type openvpn file '%s'", filename);
                            ret = system(command);
                            if (ret == 0)
                                show_notification(client, "VPN connection imported successfully!", FALSE);
                            else
                                show_notification(client, "Failed to import VPN connection!", TRUE);
                            log_message("INFO", "Imported new VPN connection: %s, status=%d",
                                        connection_name, ret);
                        }

                        // 刷新下拉框
                        scanned_connections_cb(NULL, NULL, client);
                    }
                } else {
                    show_notification(client, "Failed to validate certificate", TRUE);
                }
            } else {
                show_notification(client, "Failed to parse OVPN file", TRUE);
            }
            g_free(filename);
        }
    }

    if (dialog) gtk_widget_destroy(dialog);
}


// 退出菜单项回调
void quit_menu_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    OVPNClient *client = (OVPNClient *)user_data;
    
    if (!client) {
        log_message("ERROR", "Invalid client in quit_menu_clicked");
        return;
    }
    
    log_message("INFO", "Quit requested from menu");
    if (client->app) {
        g_application_quit(G_APPLICATION(client->app));
    }
}

// 显示窗口菜单项回调
void show_window_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    (void)widget;
    
    if (!client || !client->window) {
        log_message("ERROR", "Invalid client or window in show_window_clicked");
        return;
    }
    
    gtk_window_present(GTK_WINDOW(client->window));
    gtk_widget_show_all(client->window);
}

// 新增：初始化 existing_connections 数组的辅助函数
void initialize_existing_connections(OVPNClient *client) {
    if (!client) {
        return;
    }
    
    if (!client->existing_connections) {
        client->existing_connections = g_ptr_array_new_with_free_func(g_free);
        log_message("INFO", "Initialized existing_connections array");
    }
}

// 新增：安全清理函数
void cleanup_client_resources(OVPNClient *client) {
    if (!client) {
        return;
    }
    
    if (client->existing_connections) {
        g_ptr_array_free(client->existing_connections, TRUE);
        client->existing_connections = NULL;
    }
    
    if (client->parsed_config) {
        safe_free_ovpn_config(client->parsed_config);
        client->parsed_config = NULL;
    }
    
    log_message("INFO", "Client resources cleaned up");
}