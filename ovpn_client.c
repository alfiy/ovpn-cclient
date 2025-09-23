#include <gtk-3.0/gtk/gtk.h>
#include <libayatana-appindicator3-0.1/libayatana-appindicator/app-indicator.h>
#include <libnm/NetworkManager.h>
#include <glib-2.0/glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <uuid/uuid.h>
#include <regex.h>

#define APP_ID "com.example.ovpn-client"
#define LOG_FILE "/tmp/ovpn_importer.log"
#define MAX_CONNECTIONS 50
#define MAX_PATH 1024
#define MAX_TEXT 2048

typedef struct {
    char server[256];
    char port[16];
    char proto[16];
} RemoteServer;

typedef struct {
    RemoteServer remote[10];
    int remote_count;
    char proto[16];
    char port[16];
    char dev[16];
    char ca[MAX_PATH];
    char cert[MAX_PATH];
    char key[MAX_PATH];
    char tls_auth[MAX_PATH];
    char tls_crypt[MAX_PATH];
    char tls_crypt_v2[MAX_PATH];
    gboolean auth_user_pass;
    char cipher[64];
    char auth[64];
    gboolean comp_lzo;
    gboolean redirect_gateway;
    char key_direction[8];
    char raw_config[8192];
} OVPNConfig;

typedef struct {
    GtkApplication *app;
    GtkWidget *window;
    GtkWidget *status_label;
    GtkWidget *notification_label;
    GtkWidget *vpn_list_box;
    GtkWidget *vpn_count_label;
    GtkWidget *connection_status_label;
    GtkWidget *name_entry;
    GtkWidget *username_entry;
    GtkWidget *password_entry;
    GtkWidget *result_view;
    GtkWidget *connect_button;
    GtkWidget *disconnect_button;
    GtkWidget *delete_button;
    GtkWidget *test_button;
    
    AppIndicator *indicator;
    GtkWidget *indicator_menu;
    
    NMClient *nm_client;
    NMConnection *created_connection;
    NMActiveConnection *active_connection;
    NMConnection *selected_connection;
    
    OVPNConfig *parsed_config;
    char ovpn_file_path[MAX_PATH];
    gboolean connection_failed;
    
    GPtrArray *vpn_connections;
} OVPNClient;

static OVPNClient *app_instance = NULL;
static FILE *log_file = NULL;

// 日志函数
static void log_message(const char *level, const char *format, ...) {
    va_list args;
    time_t now;
    char *time_str;
    
    if (!log_file) {
        log_file = fopen(LOG_FILE, "a");
        if (!log_file) return;
    }
    
    time(&now);
    time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // 移除换行符
    
    fprintf(log_file, "%s - %s - ", time_str, level);
    
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file);
    
    // 同时输出到控制台
    printf("%s - %s - ", time_str, level);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

// 显示通知
static void show_notification(OVPNClient *client, const char *message, gboolean is_error) {
    char full_message[MAX_TEXT];
    
    if (is_error) {
        snprintf(full_message, sizeof(full_message), "❌ ERROR: %s", message);
        gtk_widget_set_name(client->notification_label, "error");
    } else {
        snprintf(full_message, sizeof(full_message), "✅ SUCCESS: %s", message);
        gtk_widget_set_name(client->notification_label, "success");
    }
    
    gtk_label_set_text(GTK_LABEL(client->notification_label), full_message);
    gtk_widget_show(client->notification_label);
    
    // 5秒后自动隐藏
    g_timeout_add_seconds(is_error ? 10 : 5, (GSourceFunc)gtk_widget_hide, client->notification_label);
}

// 解析OVPN文件
static OVPNConfig* parse_ovpn_file(const char *filepath) {
    FILE *file;
    char line[1024];
    OVPNConfig *config;
    regex_t regex;
    regmatch_t matches[4];
    
    log_message("INFO", "Parsing OVPN file: %s", filepath);
    
    file = fopen(filepath, "r");
    if (!file) {
        log_message("ERROR", "Failed to open file: %s", filepath);
        return NULL;
    }
    
    config = g_malloc0(sizeof(OVPNConfig));
    strcpy(config->proto, "udp");
    strcpy(config->port, "1194");
    strcpy(config->dev, "tun");
    
    while (fgets(line, sizeof(line), file)) {
        // 移除换行符
        line[strcspn(line, "\r\n")] = 0;
        
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\0') continue;
        
        // 解析remote指令
        if (regcomp(&regex, "^remote[[:space:]]+([^[:space:]]+)([[:space:]]+([0-9]+))?([[:space:]]+([a-zA-Z]+))?", REG_EXTENDED) == 0) {
            if (regexec(&regex, line, 4, matches, 0) == 0 && config->remote_count < 10) {
                // 提取服务器地址
                int len = matches[1].rm_eo - matches[1].rm_so;
                strncpy(config->remote[config->remote_count].server, line + matches[1].rm_so, len);
                config->remote[config->remote_count].server[len] = '\0';
                
                // 提取端口（如果存在）
                if (matches[2].rm_so != -1) {
                    len = matches[3].rm_eo - matches[3].rm_so;
                    strncpy(config->remote[config->remote_count].port, line + matches[3].rm_so, len);
                    config->remote[config->remote_count].port[len] = '\0';
                } else {
                    strcpy(config->remote[config->remote_count].port, "1194");
                }
                
                // 提取协议（如果存在）
                if (matches[3].rm_so != -1) {
                    len = matches[4].rm_eo - matches[4].rm_so;
                    strncpy(config->remote[config->remote_count].proto, line + matches[4].rm_so, len);
                    config->remote[config->remote_count].proto[len] = '\0';
                } else {
                    strcpy(config->remote[config->remote_count].proto, "udp");
                }
                
                config->remote_count++;
                log_message("DEBUG", "Found remote server: %s:%s (%s)", 
                          config->remote[config->remote_count-1].server,
                          config->remote[config->remote_count-1].port,
                          config->remote[config->remote_count-1].proto);
            }
            regfree(&regex);
        }
        
        // 解析其他指令
        if (strncmp(line, "proto ", 6) == 0) {
            sscanf(line + 6, "%15s", config->proto);
        } else if (strncmp(line, "port ", 5) == 0) {
            sscanf(line + 5, "%15s", config->port);
        } else if (strncmp(line, "dev ", 4) == 0) {
            sscanf(line + 4, "%15s", config->dev);
        } else if (strncmp(line, "ca ", 3) == 0) {
            sscanf(line + 3, "%1023s", config->ca);
        } else if (strncmp(line, "cert ", 5) == 0) {
            sscanf(line + 5, "%1023s", config->cert);
        } else if (strncmp(line, "key ", 4) == 0) {
            sscanf(line + 4, "%1023s", config->key);
        } else if (strncmp(line, "tls-auth ", 9) == 0) {
            char direction[8] = "";
            sscanf(line + 9, "%1023s %7s", config->tls_auth, direction);
            if (strlen(direction) > 0) {
                strcpy(config->key_direction, direction);
            }
        } else if (strncmp(line, "tls-crypt ", 10) == 0) {
            sscanf(line + 10, "%1023s", config->tls_crypt);
        } else if (strncmp(line, "tls-crypt-v2 ", 13) == 0) {
            sscanf(line + 13, "%1023s", config->tls_crypt_v2);
        } else if (strncmp(line, "auth-user-pass", 14) == 0) {
            config->auth_user_pass = TRUE;
        } else if (strncmp(line, "cipher ", 7) == 0) {
            sscanf(line + 7, "%63s", config->cipher);
        } else if (strncmp(line, "auth ", 5) == 0) {
            sscanf(line + 5, "%63s", config->auth);
        } else if (strncmp(line, "comp-lzo", 8) == 0) {
            config->comp_lzo = TRUE;
        } else if (strncmp(line, "redirect-gateway", 16) == 0) {
            config->redirect_gateway = TRUE;
        }
        
        // 保存原始配置
        if (strlen(config->raw_config) + strlen(line) + 2 < sizeof(config->raw_config)) {
            strcat(config->raw_config, line);
            strcat(config->raw_config, "\n");
        }
    }
    
    fclose(file);
    return config;
}

// 验证证书文件
static gboolean validate_certificates(OVPNConfig *config) {
    gboolean valid = TRUE;
    
    if (strlen(config->ca) == 0) {
        log_message("ERROR", "CA certificate is required");
        valid = FALSE;
    } else if (access(config->ca, R_OK) != 0) {
        log_message("ERROR", "CA certificate file not found: %s", config->ca);
        valid = FALSE;
    }
    
    if (!config->auth_user_pass) {
        if (strlen(config->cert) > 0 && access(config->cert, R_OK) != 0) {
            log_message("ERROR", "Client certificate file not found: %s", config->cert);
            valid = FALSE;
        }
        if (strlen(config->key) > 0 && access(config->key, R_OK) != 0) {
            log_message("ERROR", "Private key file not found: %s", config->key);
            valid = FALSE;
        }
    }
    
    if (strlen(config->tls_auth) > 0 && access(config->tls_auth, R_OK) != 0) {
        log_message("ERROR", "TLS auth key file not found: %s", config->tls_auth);
        valid = FALSE;
    }
    
    return valid;
}

// 创建NetworkManager VPN连接
static NMConnection* create_nm_vpn_connection(OVPNClient *client, const char *name, OVPNConfig *config) {
    NMConnection *connection;
    NMSettingConnection *s_con;
    NMSettingVpn *s_vpn;
    NMSettingIPConfig *s_ip4, *s_ip6;
    uuid_t uuid;
    char uuid_str[37];
    char ovpn_dir[MAX_PATH];
    char full_path[MAX_PATH];
    
    log_message("INFO", "Creating NetworkManager VPN connection: %s", name);
    
    connection = nm_simple_connection_new();
    
    // 基本连接设置
    s_con = (NMSettingConnection *) nm_setting_connection_new();
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str);
    
    g_object_set(s_con,
                 NM_SETTING_CONNECTION_ID, name,
                 NM_SETTING_CONNECTION_TYPE, "vpn",
                 NM_SETTING_CONNECTION_UUID, uuid_str,
                 NULL);
    nm_connection_add_setting(connection, NM_SETTING(s_con));
    
    // VPN设置
    s_vpn = (NMSettingVpn *) nm_setting_vpn_new();
    g_object_set(s_vpn, NM_SETTING_VPN_SERVICE_TYPE, "org.freedesktop.NetworkManager.openvpn", NULL);
    
    // 获取OVPN文件目录
    strcpy(ovpn_dir, client->ovpn_file_path);
    char *last_slash = strrchr(ovpn_dir, '/');
    if (last_slash) *last_slash = '\0';
    
    // 设置VPN数据
    if (config->remote_count > 0) {
        nm_setting_vpn_add_data_item(s_vpn, "remote", config->remote[0].server);
        nm_setting_vpn_add_data_item(s_vpn, "port", config->remote[0].port);
        nm_setting_vpn_add_data_item(s_vpn, "proto-tcp", 
                                   strcmp(config->remote[0].proto, "tcp") == 0 ? "yes" : "no");
    }
    
    // 连接类型
    if (config->auth_user_pass) {
        nm_setting_vpn_add_data_item(s_vpn, "connection-type", "password");
    } else {
        nm_setting_vpn_add_data_item(s_vpn, "connection-type", "tls");
    }
    
    // 设备类型
    if (strncmp(config->dev, "tun", 3) == 0) {
        nm_setting_vpn_add_data_item(s_vpn, "dev-type", "tun");
    } else if (strncmp(config->dev, "tap", 3) == 0) {
        nm_setting_vpn_add_data_item(s_vpn, "dev-type", "tap");
    }
    
    // 证书文件路径
    if (strlen(config->ca) > 0) {
        if (config->ca[0] != '/') {
            snprintf(full_path, sizeof(full_path), "%s/%s", ovpn_dir, config->ca);
            nm_setting_vpn_add_data_item(s_vpn, "ca", full_path);
        } else {
            nm_setting_vpn_add_data_item(s_vpn, "ca", config->ca);
        }
    }
    
    if (strlen(config->cert) > 0) {
        if (config->cert[0] != '/') {
            snprintf(full_path, sizeof(full_path), "%s/%s", ovpn_dir, config->cert);
            nm_setting_vpn_add_data_item(s_vpn, "cert", full_path);
        } else {
            nm_setting_vpn_add_data_item(s_vpn, "cert", config->cert);
        }
    }
    
    if (strlen(config->key) > 0) {
        if (config->key[0] != '/') {
            snprintf(full_path, sizeof(full_path), "%s/%s", ovpn_dir, config->key);
            nm_setting_vpn_add_data_item(s_vpn, "key", full_path);
        } else {
            nm_setting_vpn_add_data_item(s_vpn, "key", config->key);
        }
    }
    
    // TLS认证
    if (strlen(config->tls_auth) > 0) {
        if (config->tls_auth[0] != '/') {
            snprintf(full_path, sizeof(full_path), "%s/%s", ovpn_dir, config->tls_auth);
            nm_setting_vpn_add_data_item(s_vpn, "tls-auth", full_path);
        } else {
            nm_setting_vpn_add_data_item(s_vpn, "tls-auth", config->tls_auth);
        }
        
        if (strlen(config->key_direction) > 0) {
            nm_setting_vpn_add_data_item(s_vpn, "key-direction", config->key_direction);
        }
    }
    
    // 其他设置
    if (strlen(config->cipher) > 0) {
        nm_setting_vpn_add_data_item(s_vpn, "cipher", config->cipher);
    }
    
    if (strlen(config->auth) > 0) {
        nm_setting_vpn_add_data_item(s_vpn, "auth", config->auth);
    }
    
    if (config->comp_lzo) {
        nm_setting_vpn_add_data_item(s_vpn, "comp-lzo", "yes");
    }
    
    if (config->redirect_gateway) {
        nm_setting_vpn_add_data_item(s_vpn, "redirect-gateway", "yes");
    }
    
    // 超时设置
    nm_setting_vpn_add_data_item(s_vpn, "connect-timeout", "120");
    nm_setting_vpn_add_data_item(s_vpn, "ping", "10");
    nm_setting_vpn_add_data_item(s_vpn, "ping-restart", "60");
    
    nm_connection_add_setting(connection, NM_SETTING(s_vpn));
    
    // IP设置
    s_ip4 = (NMSettingIPConfig *) nm_setting_ip4_config_new();
    g_object_set(s_ip4, NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_AUTO, NULL);
    nm_connection_add_setting(connection, NM_SETTING(s_ip4));
    
    s_ip6 = (NMSettingIPConfig *) nm_setting_ip6_config_new();
    g_object_set(s_ip6, NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP6_CONFIG_METHOD_AUTO, NULL);
    nm_connection_add_setting(connection, NM_SETTING(s_ip6));
    
    return connection;
}

// 连接状态变化回调
static void connection_state_changed_cb(NMActiveConnection *active_connection,
                                      guint state,
                                      guint reason,
                                      gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    
    switch (state) {
        case NM_ACTIVE_CONNECTION_STATE_ACTIVATED:
            gtk_label_set_text(GTK_LABEL(client->connection_status_label), "VPN Status: Connected");
            gtk_widget_set_sensitive(client->connect_button, FALSE);
            gtk_widget_set_sensitive(client->disconnect_button, TRUE);
            show_notification(client, "VPN connection established!", FALSE);
            gtk_widget_hide(client->test_button);
            client->connection_failed = FALSE;
            break;
            
        case NM_ACTIVE_CONNECTION_STATE_DEACTIVATED:
            gtk_label_set_text(GTK_LABEL(client->connection_status_label), "VPN Status: Disconnected");
            gtk_widget_set_sensitive(client->connect_button, TRUE);
            gtk_widget_set_sensitive(client->disconnect_button, FALSE);
            client->active_connection = NULL;
            
            if (reason == NM_ACTIVE_CONNECTION_STATE_REASON_CONNECT_TIMEOUT) {
                show_notification(client, "VPN connection timeout - check server connectivity", TRUE);
                gtk_widget_show(client->test_button);
                client->connection_failed = TRUE;
            } else if (reason == NM_ACTIVE_CONNECTION_STATE_REASON_LOGIN_FAILED) {
                show_notification(client, "VPN login failed - check credentials", TRUE);
                gtk_widget_show(client->test_button);
                client->connection_failed = TRUE;
            }
            break;
            
        case NM_ACTIVE_CONNECTION_STATE_ACTIVATING:
            gtk_label_set_text(GTK_LABEL(client->connection_status_label), "VPN Status: Connecting...");
            break;
    }
}

// 激活VPN连接
static void activate_vpn_connection(OVPNClient *client) {
    if (!client->created_connection) return;
    
    log_message("DEBUG", "Activating VPN connection...");
    
    nm_client_activate_connection_async(client->nm_client,
                                      client->created_connection,
                                      NULL, NULL, NULL,
                                      NULL, NULL);
}

// 测试连接按钮回调
static void test_connection_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    
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
static void connect_vpn_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    const char *username, *password;
    
    if (!client->created_connection) {
        show_notification(client, "No VPN connection available. Please create one first.", TRUE);
        return;
    }
    
    username = gtk_entry_get_text(GTK_ENTRY(client->username_entry));
    password = gtk_entry_get_text(GTK_ENTRY(client->password_entry));
    
    if (client->parsed_config && client->parsed_config->auth_user_pass) {
        if (strlen(username) == 0) {
            show_notification(client, "Username is required for this VPN connection", TRUE);
            return;
        }
    }
    
    show_notification(client, "Connecting to VPN...", FALSE);
    gtk_label_set_text(GTK_LABEL(client->connection_status_label), "VPN Status: Connecting...");
    
    activate_vpn_connection(client);
}

// 断开VPN按钮回调
static void disconnect_vpn_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    
    if (!client->active_connection) {
        show_notification(client, "No active VPN connection to disconnect", TRUE);
        return;
    }
    
    log_message("INFO", "Disconnecting VPN...");
    nm_client_deactivate_connection_async(client->nm_client, client->active_connection,
                                        NULL, NULL, NULL);
}

// 文件选择对话框回调
static void file_chosen_cb(GtkWidget *dialog, gint response_id, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    
    if (response_id == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            strcpy(client->ovpn_file_path, filename);
            
            // 解析OVPN文件
            if (client->parsed_config) {
                g_free(client->parsed_config);
            }
            client->parsed_config = parse_ovpn_file(filename);
            
            if (client->parsed_config) {
                char status_text[256];
                char *basename = g_path_get_basename(filename);
                snprintf(status_text, sizeof(status_text), "Imported: %s", basename);
                gtk_label_set_text(GTK_LABEL(client->status_label), status_text);
                
                // 自动填充连接名称
                char *name_without_ext = g_strndup(basename, strlen(basename) - 5); // 移除.ovpn
                gtk_entry_set_text(GTK_ENTRY(client->name_entry), name_without_ext);
                
                // 验证并创建VPN连接
                if (validate_certificates(client->parsed_config)) {
                    const char *connection_name = gtk_entry_get_text(GTK_ENTRY(client->name_entry));
                    if (strlen(connection_name) > 0) {
                        client->created_connection = create_nm_vpn_connection(client, connection_name, client->parsed_config);
                        if (client->created_connection) {
                            // 添加连接到NetworkManager
                            nm_client_add_connection_async(client->nm_client, client->created_connection,
                                                         TRUE, NULL, NULL, NULL);
                            show_notification(client, "VPN connection created successfully!", FALSE);
                            gtk_widget_set_sensitive(client->connect_button, TRUE);
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

// 导入文件按钮回调
static void import_file_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    GtkWidget *dialog;
    GtkFileFilter *filter;
    
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
}

// 创建系统托盘指示器
static void create_indicator(OVPNClient *client) {
    GtkWidget *menu_item;
    
    client->indicator = app_indicator_new(APP_ID,
                                        "network-vpn",
                                        APP_INDICATOR_CATEGORY_SYSTEM_SERVICES);
    
    app_indicator_set_status(client->indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_attention_icon(client->indicator, "network-vpn-acquiring");
    
    // 创建菜单
    client->indicator_menu = gtk_menu_new();
    
    menu_item = gtk_menu_item_new_with_label("Show Window");
    g_signal_connect_swapped(menu_item, "activate", G_CALLBACK(gtk_window_present), client->window);
    gtk_menu_shell_append(GTK_MENU_SHELL(client->indicator_menu), menu_item);
    
    menu_item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(client->indicator_menu), menu_item);
    
    menu_item = gtk_menu_item_new_with_label("Quit");
    g_signal_connect_swapped(menu_item, "activate", G_CALLBACK(gtk_main_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(client->indicator_menu), menu_item);
    
    gtk_widget_show_all(client->indicator_menu);
    app_indicator_set_menu(client->indicator, GTK_MENU(client->indicator_menu));
}

// 创建主窗口
static void create_main_window(OVPNClient *client) {
    GtkWidget *vbox, *hbox, *frame, *scrolled, *button;
    
    client->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(client->window), "OVPN Client with NetworkManager");
    gtk_window_set_default_size(GTK_WINDOW(client->window), 800, 600);
    gtk_container_set_border_width(GTK_CONTAINER(client->window), 10);
    
    // 主布局
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(client->window), vbox);
    
    // 导入按钮
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    button = gtk_button_new_with_label("Import .ovpn File");
    g_signal_connect(button, "clicked", G_CALLBACK(import_file_clicked), client);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    // 状态标签
    client->status_label = gtk_label_new("No file imported yet.");
    gtk_box_pack_start(GTK_BOX(vbox), client->status_label, FALSE, FALSE, 0);
    
    // 通知标签
    client->notification_label = gtk_label_new("");
    gtk_label_set_line_wrap(GTK_LABEL(client->notification_label), TRUE);
    gtk_widget_set_no_show_all(client->notification_label, TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), client->notification_label, FALSE, FALSE, 0);
    
    // VPN连接状态
    client->connection_status_label = gtk_label_new("VPN Status: Disconnected");
    gtk_box_pack_start(GTK_BOX(vbox), client->connection_status_label, FALSE, FALSE, 0);
    
    // 连接名称输入
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("VPN Connection Name:"), FALSE, FALSE, 0);
    client->name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(client->name_entry), "Enter connection name...");
    gtk_box_pack_start(GTK_BOX(hbox), client->name_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    // 认证框架
    frame = gtk_frame_new("Authentication (if required)");
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Username:"), FALSE, FALSE, 0);
    client->username_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(client->username_entry), "VPN username (optional)");
    gtk_box_pack_start(GTK_BOX(hbox), client->username_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Password:"), FALSE, FALSE, 0);
    client->password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(client->password_entry), "VPN password (optional)");
    gtk_entry_set_visibility(GTK_ENTRY(client->password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), client->password_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(frame), vbox);
    gtk_box_pack_start(GTK_BOX(gtk_widget_get_parent(client->connection_status_label)), frame, FALSE, FALSE, 0);
    
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
    
    gtk_box_pack_start(GTK_BOX(gtk_widget_get_parent(client->connection_status_label)), hbox, FALSE, FALSE, 0);
    
    // 配置分析区域
    frame = gtk_frame_new("OpenVPN Configuration Analysis");
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, -1, 200);
    
    client->result_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(client->result_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(client->result_view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scrolled), client->result_view);
    gtk_container_add(GTK_CONTAINER(frame), scrolled);
    gtk_box_pack_start(GTK_BOX(gtk_widget_get_parent(client->connection_status_label)), frame, TRUE, TRUE, 0);
}

// 应用程序激活回调
static void activate(GtkApplication *app, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    
    // 初始化NetworkManager客户端
    client->nm_client = nm_client_new(NULL, NULL);
    if (!client->nm_client) {
        log_message("ERROR", "Failed to initialize NetworkManager client");
        return;
    }
    
    log_message("INFO", "NetworkManager client initialized");
    
    // 创建主窗口
    create_main_window(client);
    
    // 创建系统托盘指示器
    create_indicator(client);
    
    gtk_widget_show_all(client->window);
    
    // 窗口关闭时隐藏到系统托盘
    g_signal_connect(client->window, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
}

// 主函数
int main(int argc, char *argv[]) {
    GtkApplication *app;
    OVPNClient client = {0};
    int status;
    
    app_instance = &client;
    
    // 初始化GTK应用程序
    app = gtk_application_new(APP_ID, G_APPLICATION_FLAGS_NONE);
    client.app = app;
    
    g_signal_connect(app, "activate", G_CALLBACK(activate), &client);
    
    status = g_application_run(G_APPLICATION(app), argc, argv);
    
    // 清理
    if (client.parsed_config) {
        g_free(client.parsed_config);
    }
    
    if (client.vpn_connections) {
        g_ptr_array_free(client.vpn_connections, TRUE);
    }
    
    if (log_file) {
        fclose(log_file);
    }
    
    g_object_unref(app);
    
    return status;
}