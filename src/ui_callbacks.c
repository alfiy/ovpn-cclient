#include <gtk/gtk.h>
#include <libnm/NetworkManager.h>
#include <arpa/inet.h>
#include <unistd.h> // For close()
#include <sys/time.h> // For struct timeval
#include <sys/socket.h> // For socket functions
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>

#include "../include/ui_callbacks.h"
#include "../include/structs.h"
#include "../include/nm_connection.h"
#include "../include/notify.h"
#include "../include/log_util.h"
#include "../include/core_logic.h"
#include "../include/config.h"
#include "../include/ui_builder.h"

// 判断active_connection是否当前所选
static gboolean is_the_vpn_connection(NMActiveConnection *ac, NMConnection *selected) {
    if (!ac || !selected) return FALSE;
    NMRemoteConnection *conn = nm_active_connection_get_connection(ac);
    if (!conn) return FALSE;
    const char *ac_id = nm_connection_get_id((NMConnection*)conn);
    const char *sel_id = nm_connection_get_id(selected);
    return g_strcmp0(ac_id, sel_id) == 0;
}

// 主动同步当前active_connection（连接/断开/导入/删除后调用）
void sync_active_connection(OVPNClient *client) {
    const GPtrArray *active_array = nm_client_get_active_connections(client->nm_client);
    client->active_connection = NULL;
    for (guint i = 0; i < active_array->len; i++) {
        NMActiveConnection *ac = g_ptr_array_index((GPtrArray *)active_array, i);
        if (is_the_vpn_connection(ac, client->selected_connection)) {
            client->active_connection = ac;
            const char *ac_id = "(null)";
            NMRemoteConnection *conn = nm_active_connection_get_connection(ac);
            if (conn) ac_id = nm_connection_get_id((NMConnection*)conn);
            log_message("DEBUG", "[sync_active_connection] got current active_connection pointer: %p, id: %s", ac, ac_id);
            break;
        }
    }
    if (!client->active_connection)
        log_message("DEBUG", "[sync_active_connection] no active connection matched.");
}

// 日志刷新定时器
gboolean openvpn_log_update_timer(gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    FILE *fp = fopen(get_ovpn_log_path(), "r");
    if (!fp) return TRUE; // 文件不存在
    fseek(fp, client->openvpn_log_offset, SEEK_SET);
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        append_log_to_view(client, line);
    }
    client->openvpn_log_offset = ftell(fp);
    fclose(fp);
    return TRUE;
}

// 删除配置按钮回调
void delete_vpn_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
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
    char command[256];
    snprintf(command, sizeof(command), "nmcli connection delete \"%s\"", connection_name);
    int ret = system(command);
    if (ret == 0) {
        show_notification(client, "VPN connection deleted successfully!", FALSE);
        log_message("INFO", "VPN connection deleted: %s", connection_name);
        scan_existing_connections(client);
    } else {
        show_notification(client, "Failed to delete VPN connection.", TRUE);
    }
    if (client->openvpn_log_timer) {
        g_source_remove(client->openvpn_log_timer); client->openvpn_log_timer = 0;
    }
    client->openvpn_log_offset = 0;
    client->active_connection = NULL;
    client->selected_connection = NULL;
   
    
    refresh_connection_combo_box(client);
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


// 导入文件回调
// void file_chosen_cb(GtkWidget *dialog, gint response_id, gpointer user_data) {
//     OVPNClient *client = (OVPNClient *)user_data;
//     if (!client) {
//         log_message("ERROR", "Invalid client in file_chosen_cb");
//         if (dialog) gtk_widget_destroy(dialog);
//         return;
//     }
//     // 清理log状态
//     if (client->openvpn_log_timer) {
//         g_source_remove(client->openvpn_log_timer); client->openvpn_log_timer = 0;
//     }
//     client->openvpn_log_offset = 0;
//     client->active_connection = NULL;
//     client->selected_connection = NULL;
//     sync_active_connection(client);

//     if (response_id == GTK_RESPONSE_ACCEPT) {
//         char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
//         if (filename) {
//             if (strlen(filename) < sizeof(client->ovpn_file_path)) {
//                 g_strlcpy(client->ovpn_file_path, filename, sizeof(client->ovpn_file_path));
//             } else {
//                 log_message("ERROR", "Filename too long: %s", filename);
//                 show_notification(client, "Filename is too long", TRUE);
//                 g_free(filename);
//                 gtk_widget_destroy(dialog);
//                 return;
//             }
//             if (client->parsed_config) {
//                 safe_free_ovpn_config(client->parsed_config);
//                 client->parsed_config = NULL;
//             }
//             client->parsed_config = parse_ovpn_file(filename);
//             if (client->config_analysis_frame) gtk_widget_show(client->config_analysis_frame);
//             if (client->connection_log_frame) gtk_widget_hide(client->connection_log_frame);

//             if (client->parsed_config) {
//                 char status_text[256];
//                 char *basename = g_path_get_basename(filename);
//                 if (basename) {
//                     g_free(client->last_imported_name);
//                     client->last_imported_name = g_strdup(basename);

//                     snprintf(status_text, sizeof(status_text), "Imported: %s", basename);
//                     if (client->status_label)
//                         gtk_label_set_text(GTK_LABEL(client->status_label), status_text);
//                     g_free(basename);
//                 }
//                 if (validate_certificates(client->parsed_config)) {
//                     // 系统导入vpn配置
//                     char command[1024];
//                     int ret;
//                     snprintf(command, sizeof(command),
//                         "nmcli connection import type openvpn file '%s'", filename);
//                     ret = system(command);
//                     if (ret == 0) {
//                         show_notification(client, "VPN connection imported successfully!", FALSE);
//                         log_message("INFO", "Imported VPN connection with file: %s, status=%d", filename, ret);
//                         // 导入成功后重新scan
//                         scan_existing_connections(client);
//                     } else {
//                         show_notification(client, "Failed to import VPN connection!", TRUE);
//                         log_message("ERROR", "Failed to import VPN connection with file: %s, status=%d", filename, ret);
//                     }
//                     update_config_analysis_view(client);

//                 } else {
//                     show_notification(client, "Failed to validate certificate", TRUE);
//                 }
//             } else {
//                 show_notification(client, "Failed to parse OVPN file", TRUE);
//             }
//             g_free(filename);
//         }
//     }
//     if (dialog) gtk_widget_destroy(dialog);

//     refresh_connection_combo_box(client);
// }
void file_chosen_cb(GtkWidget *dialog, gint response_id, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    if (!client) {
        log_message("ERROR", "Invalid client in file_chosen_cb");
        if (dialog) gtk_widget_destroy(dialog);
        return;
    }
    if (client->openvpn_log_timer) {
        g_source_remove(client->openvpn_log_timer); client->openvpn_log_timer = 0;
    }
    client->openvpn_log_offset = 0;
    client->active_connection = NULL;
    client->selected_connection = NULL;
    sync_active_connection(client);

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

            if (client->config_analysis_frame) gtk_widget_show(client->config_analysis_frame);
            if (client->connection_log_frame) gtk_widget_hide(client->connection_log_frame);

            if (client->parsed_config) {
                char status_text[256];
                char *basename = g_path_get_basename(filename);
                char imported_name[256] = {0};
                if (basename) {
                    g_free(client->last_imported_name);
                    client->last_imported_name = g_strdup(basename);
                    snprintf(status_text, sizeof(status_text), "Imported: %s", basename);
                    if (client->status_label)
                        gtk_label_set_text(GTK_LABEL(client->status_label), status_text);
                    char *dot = strrchr(basename, '.');
                    if (dot) *dot = '\0';
                    strncpy(imported_name, basename, sizeof(imported_name)-1);
                    g_free(basename);
                }
                if (validate_certificates(client->parsed_config)) {
                    // 使用 nmcli 系统导入
                    char command[1024];
                    int ret;
                    snprintf(command, sizeof(command),
                             "nmcli connection import type openvpn file '%s'", filename);
                    ret = system(command);

                    if (ret == 0) {
                        show_notification(client, "VPN connection imported successfully!", FALSE);
                        log_message("INFO", "Imported VPN connection with file: %s, status=%d", filename, ret);
                        scan_existing_connections(client);
                        // 等待确保配置文件写盘
                        g_usleep(200 * 1000); // 200ms

                        // 直接用nmcli修改分流属性
                        FILE *fp = popen("nmcli -t -f NAME,TYPE connection show | grep ':vpn' | tail -n 1 | cut -d':' -f1", "r");
                        char vpn_name[128] = {0};
                        if (fp && fgets(vpn_name, sizeof(vpn_name), fp)) {
                            vpn_name[strcspn(vpn_name, "\r\n")] = 0;
                            pclose(fp);
                            char set_def_cmd[512];
                            snprintf(set_def_cmd, sizeof(set_def_cmd), "nmcli connection modify \"%s\" ipv4.never-default yes", vpn_name);
                            system(set_def_cmd);
                            system("nmcli connection reload");
                            log_message("INFO", "Set never-default by nmcli for connection: %s", vpn_name);
                        }
                        scan_existing_connections(client);

                    } else {
                        show_notification(client, "Failed to import VPN connection!", TRUE);
                        log_message("ERROR", "Failed to import VPN connection with file: %s, status=%d", filename, ret);
                    }
                    update_config_analysis_view(client);
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

    refresh_connection_combo_box(client);
}



// 连接VPN按钮
void connect_vpn_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    const char *connection_name = NULL;
    (void)widget;
    gtk_widget_hide(client->config_analysis_frame);
    gtk_widget_show(client->connection_log_frame);
    GtkTextBuffer *log_buffer = gtk_text_view_get_buffer(client->log_view);
    gtk_text_buffer_set_text(log_buffer, "连接中，请稍候...\n", -1);

    if (!client || !client->connection_combo_box || !client->existing_connections) {
        log_message("ERROR", "Invalid UI elements in connect_vpn_clicked");
        show_notification(client, "UI elements not properly initialized", TRUE);
        return;
    }
    gint active_item = gtk_combo_box_get_active(GTK_COMBO_BOX(client->connection_combo_box));
    if (active_item == -1 || active_item >= (gint)client->existing_connections->len) {
        show_notification(client, "No VPN connection selected.", TRUE);
        return;
    }
    gpointer item_ptr = g_ptr_array_index(client->existing_connections, active_item);
    if (item_ptr) {
        connection_name = (const char *)item_ptr;
        log_message("INFO", "Selected VPN connection: %s", connection_name);
    } else {
        show_notification(client, "Invalid connection selection.", TRUE);
        return;
    }

    // 选择后的连接对象赋值
    client->selected_connection = NULL;
    // 重新根据名字查找selected_connection指针（可用g_ptr_array/或遍历现有NM对象）
    const GPtrArray *connections = nm_client_get_connections(client->nm_client);
    for (guint i = 0; i < connections->len; i++) {
        NMRemoteConnection *rc = g_ptr_array_index((GPtrArray*)connections, i);
        if (0 == g_strcmp0(nm_connection_get_id((NMConnection*)rc), connection_name)) {
            client->selected_connection = (NMConnection*)rc;
            break;
        }
    }

    // 日志刷新相关
    if (client->openvpn_log_timer) {
        g_source_remove(client->openvpn_log_timer); client->openvpn_log_timer = 0;
    }
    client->openvpn_log_offset = 0;

    client->active_connection = NULL;
    sync_active_connection(client);

    // 启动日志定时器
    client->openvpn_log_timer = g_timeout_add(1000, openvpn_log_update_timer, client);

    // 更新UI
    show_notification(client, "连接中...", FALSE);
    if (client->connection_status_label)
        gtk_label_set_text(GTK_LABEL(client->connection_status_label), "VPN 状态: 正在连接...");
    if (client->connect_button)
        gtk_widget_set_sensitive(client->connect_button, FALSE);
    if (client->disconnect_button)
        gtk_widget_set_sensitive(client->disconnect_button, FALSE);

    activate_vpn_connection(client, connection_name);
}

// 连接成功后同步（你的Activate nc后应回调这里）
void vpn_connected_cb(NMClient *client, gpointer user_data) {
    (void) client;
    OVPNClient *data = user_data;
    sync_active_connection(data);
}

// 断开VPN按钮回调
void disconnect_vpn_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    (void)widget;
    if (!client) {
        log_message("ERROR", "Invalid client in disconnect_vpn_clicked");
        return;
    }
    sync_active_connection(client); // 再次同步以确保最新

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
    nm_client_deactivate_connection_async(client->nm_client, client->active_connection, NULL, disconnect_done, client);

    if (client->connection_status_label)
        gtk_label_set_text(GTK_LABEL(client->connection_status_label), "VPN 状态: 正在断开...");
    if (client->disconnect_button)
        gtk_widget_set_sensitive(client->disconnect_button, FALSE);

    if (client->openvpn_log_timer) {
        g_source_remove(client->openvpn_log_timer); client->openvpn_log_timer = 0;
    }
}


// 断开VPN异步回调后逻辑
void disconnect_done(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    (void)source_object;
    OVPNClient *client = (OVPNClient *)user_data;
    GError *error = NULL;
    if (!client || !client->nm_client) {
        log_message("ERROR", "Invalid client in disconnect_done");
        return;
    }
    nm_client_deactivate_connection_finish(client->nm_client, res, &error);
    if (error) {
        log_message("ERROR", "Deactivate VPN failed: %s", error->message);
        show_notification(client, error->message, TRUE);
        g_error_free(error);
        if (client->connect_button) gtk_widget_set_sensitive(client->connect_button, TRUE);
        if (client->disconnect_button) gtk_widget_set_sensitive(client->disconnect_button, FALSE);
        return;
    }
    // 真正断开后同步状态
    sync_active_connection(client);
    client->active_connection = NULL;
    if (client->connection_status_label)
        gtk_label_set_text(GTK_LABEL(client->connection_status_label), "VPN 状态: 已断开");
    if (client->connect_button)
        gtk_widget_set_sensitive(client->connect_button, TRUE);
    if (client->disconnect_button)
        gtk_widget_set_sensitive(client->disconnect_button, FALSE);
    show_notification(client, "VPN disconnected successfully", FALSE);
    vpn_log_cleanup(client);
    log_message("INFO", "VPN disconnected successfully");
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


// 封装：刷新下拉列表函数
void refresh_connection_combo_box(OVPNClient *client) {
   
    if (!client || !client->connection_combo_box || !client->existing_connections) {
        log_message("DEBUG", "refresh_connection_combo_box early return due to NULL");
        return;
    }
    GtkComboBoxText *combo = GTK_COMBO_BOX_TEXT(client->connection_combo_box);
    gtk_combo_box_text_remove_all(combo);

    for (guint i = 0; i < client->existing_connections->len; i++) {
        gtk_combo_box_text_append_text(combo, (char*)g_ptr_array_index(client->existing_connections, i));
    }

    log_message("DEBUG", "Number of existing connections: %u", client->existing_connections->len);

    if (client->existing_connections->len == 0) {
        
        gtk_label_set_text(GTK_LABEL(client->status_label), "没有配置文件，请先导入配置文件。");
        gtk_widget_show(client->status_label);

        // 没有连接项，下拉框无选中，仅允许导入，断开按钮禁用
        gtk_widget_set_sensitive(client->connect_button, FALSE);
        gtk_widget_set_sensitive(client->disconnect_button, FALSE);
        gtk_widget_set_sensitive(client->delete_button, FALSE);

    } else {    
        gtk_widget_hide(client->status_label);

        // 有连接项时（未选中除外），此处连接/删除按钮可用；断开按钮状态由active_connection或其它回调控制
        gtk_widget_set_sensitive(client->connect_button, TRUE);
        // gtk_widget_set_sensitive(client->delete_button, TRUE);
    }

    // === 自动选中新项逻辑 ===
    if (client->existing_connections->len > 0) {
        int active_index = 0;
        if (client->last_imported_name) {
            for (guint i = 0; i < client->existing_connections->len; i++) {
                char *name = (char*)g_ptr_array_index(client->existing_connections, i);
                if (g_strcmp0(name, client->last_imported_name) == 0) {
                    active_index = i;
                    break;
                }
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active_index);
    }
}

// 日志显示区域
void append_log_to_view(OVPNClient *client, const char *log_line) {
    if (!client || !client->log_view || !log_line) return;
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(client->log_view));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_insert(buf, &end, log_line, -1);
}

// 日志读取pipe的回调
gboolean pipe_log_callback(GIOChannel *source, GIOCondition cond, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    gchar *line = NULL;
    gsize len = 0;
    GError *error = NULL;
    GIOStatus status = g_io_channel_read_line(source, &line, &len, NULL, &error);

    if (status == G_IO_STATUS_NORMAL && line) {
        append_log_to_view(client, line);
        g_free(line);
        return TRUE; // 保持watch继续
    }
    if (cond & (G_IO_HUP | G_IO_ERR)) {
        append_log_to_view(client, "\n[日志流关闭]\n");
        return FALSE;
    }
    if (error) {
        g_error_free(error);
    }
    return TRUE;
}

// 启动VPN并记录日志
void start_vpn_and_log(OVPNClient *client, const char *connection_name) {
    gchar *argv[] = {
        "/usr/bin/nmcli", "connection", "up", (gchar*)connection_name, NULL
    };
    GError *error = NULL;
    gint stdout_fd;

    // 启动进程，连接 stdout 管道
    if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
                                  NULL, NULL, &client->vpn_log_pid,
                                  NULL, &stdout_fd, NULL, &error)) {
        show_notification(client, "无法启动日志进程", TRUE);
        log_message("ERROR", "Log pipe spawn failed: %s", error->message);
        g_error_free(error);
        return;
    }
    client->vpn_log_channel = g_io_channel_unix_new(stdout_fd);
    client->vpn_log_watch = g_io_add_watch(client->vpn_log_channel, G_IO_IN | G_IO_HUP | G_IO_ERR,
                                           pipe_log_callback, client);
}

// log释放函数
void vpn_log_cleanup(OVPNClient *client) {
    if (!client) return;
    if (client->vpn_log_watch) {
        g_source_remove(client->vpn_log_watch);
        client->vpn_log_watch = 0;
    }
    if (client->vpn_log_channel) {
        g_io_channel_unref(client->vpn_log_channel);
        client->vpn_log_channel = NULL;
    }
    if (client->vpn_log_pid > 0) {
        kill(client->vpn_log_pid, SIGKILL);
        client->vpn_log_pid = 0;
    }
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

    // 主动断开VPN连接
    // 注意：这里不能同步等待异步断开完成，只能通知用户"正在断开"就直接退出
    if (client->active_connection && client->nm_client) {
        log_message("INFO", "Active VPN detected, disconnecting before quit...");
        nm_client_deactivate_connection_async(client->nm_client, client->active_connection, NULL, NULL, NULL);
        // 你也可以显示通知
        show_notification(client, "Disconnecting VPN before exiting...", FALSE);
    }

    // 日志流释放
    vpn_log_cleanup(client);
    if (client->app) {
        g_application_quit(G_APPLICATION(client->app));
        // 日志流释放
        vpn_log_cleanup(client);
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
    // 改成只show窗口，不用show_all
    gtk_widget_show(client->window);
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