#include "../include/nm_connection.h"
#include "../include/log_util.h"
#include "../include/notify.h"
#include <uuid/uuid.h>
#include <gtk-3.0/gtk/gtk.h> 

// 创建NetworkManager VPN连接
NMConnection* create_nm_vpn_connection(OVPNClient *client, const char *name, OVPNConfig *config) {
    NMConnection *connection;
    NMSettingConnection *s_con;
    NMSettingVpn *s_vpn;
    NMSettingIPConfig *s_ip4, *s_ip6;
    uuid_t uuid;
    char uuid_str[37];
    char ovpn_dir[MAX_PATH];
    char full_path[MAX_PATH * 2];
    
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
    
    // 设置VPN数据 - 修复远程服务器格式
    if (config->remote_count > 0) {
        // 组合服务器地址和端口
        char remote_with_port[512];
        snprintf(remote_with_port, sizeof(remote_with_port), "%s:%s", 
                config->remote[0].server, config->remote[0].port);
        nm_setting_vpn_add_data_item(s_vpn, "remote", remote_with_port);
    }
    
    // 连接类型
    if (config->auth_user_pass) {
        nm_setting_vpn_add_data_item(s_vpn, "connection-type", "password");
    } else {
        nm_setting_vpn_add_data_item(s_vpn, "connection-type", "tls");
    }
    
    // 设备类型
    if (strncmp(config->dev, "tun", 3) == 0) {
        nm_setting_vpn_add_data_item(s_vpn, "dev", config->dev);
    } else if (strncmp(config->dev, "tap", 3) == 0) {
        nm_setting_vpn_add_data_item(s_vpn, "dev-type", "tap");
        nm_setting_vpn_add_data_item(s_vpn, "dev", config->dev);
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
    
    // 证书密码标志 - 关键添加
    if (config->cert_pass_flags_zero) {
        nm_setting_vpn_add_data_item(s_vpn, "cert-pass-flags", "0");
    }
    
    // TLS认证 - 修复和增强
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
    
    // TLS-Crypt - 关键添加
    if (strlen(config->tls_crypt) > 0) {
        if (config->tls_crypt[0] != '/') {
            snprintf(full_path, sizeof(full_path), "%s/%s", ovpn_dir, config->tls_crypt);
            nm_setting_vpn_add_data_item(s_vpn, "tls-crypt", full_path);
        } else {
            nm_setting_vpn_add_data_item(s_vpn, "tls-crypt", config->tls_crypt);
        }
        log_message("INFO", "Added tls-crypt: %s", config->tls_crypt);
    }
    
    // 远程证书验证 - 关键添加
    if (strlen(config->remote_cert_tls) > 0) {
        nm_setting_vpn_add_data_item(s_vpn, "remote-cert-tls", config->remote_cert_tls);
        log_message("INFO", "Added remote-cert-tls: %s", config->remote_cert_tls);
    }
    
    // TLS版本最小要求 - 关键添加
    if (strlen(config->tls_version_min) > 0) {
        nm_setting_vpn_add_data_item(s_vpn, "tls-version-min", config->tls_version_min);
        log_message("INFO", "Added tls-version-min: %s", config->tls_version_min);
    }
    
    // 加密和认证设置
    if (strlen(config->cipher) > 0) {
        nm_setting_vpn_add_data_item(s_vpn, "cipher", config->cipher);
    }
    
    if (strlen(config->auth) > 0) {
        nm_setting_vpn_add_data_item(s_vpn, "auth", config->auth);
    }
    
    // 压缩设置
    if (config->comp_lzo) {
        nm_setting_vpn_add_data_item(s_vpn, "comp-lzo", "yes");
    }
    
    // 网关重定向
    if (config->redirect_gateway) {
        nm_setting_vpn_add_data_item(s_vpn, "redirect-gateway", "yes");
    }
    
    // 日志记录添加的所有配置项
    log_message("INFO", "VPN connection configuration:");
    log_message("INFO", "  Remote: %s:%s (%s)", config->remote[0].server, config->remote[0].port, config->remote[0].proto);
    log_message("INFO", "  CA: %s", config->ca);
    log_message("INFO", "  Cert: %s", config->cert);
    log_message("INFO", "  Key: %s", config->key);
    log_message("INFO", "  TLS-Crypt: %s", config->tls_crypt);
    log_message("INFO", "  Remote-cert-tls: %s", config->remote_cert_tls);
    log_message("INFO", "  TLS-version-min: %s", config->tls_version_min);
    
    nm_connection_add_setting(connection, NM_SETTING(s_vpn));
    
    // IP设置
    s_ip4 = (NMSettingIPConfig *) nm_setting_ip4_config_new();
    g_object_set(s_ip4, NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_AUTO, NULL);
    nm_connection_add_setting(connection, NM_SETTING(s_ip4));
    
    s_ip6 = (NMSettingIPConfig *) nm_setting_ip6_config_new();
    g_object_set(s_ip6, NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP6_CONFIG_METHOD_AUTO, NULL);
    nm_connection_add_setting(connection, NM_SETTING(s_ip6));
    
    log_message("INFO", "NetworkManager VPN connection created successfully");
    return connection;
}

// 激活VPN连接
void vpn_activate_done(GObject *source_obj, GAsyncResult *res, gpointer user_data)
{
    (void)source_obj;
    OVPNClient *client = user_data;
    NMActiveConnection *active_connection;
    GError *error = NULL;

    // 完成异步操作，获取 NMActiveConnection 对象。
    active_connection = nm_client_activate_connection_finish(client->nm_client, res, &error);
    
    // 处理错误情况
    if (error) {
        log_message("ERROR", "Activate VPN failed: %s", error->message);
        show_notification(client, error->message, TRUE);
        g_error_free(error);

        // 恢复 UI 状态，让用户可以再次尝试
        gtk_label_set_text(GTK_LABEL(client->connection_status_label), "VPN Status: Disconnected");
        gtk_widget_set_sensitive(client->connect_button, TRUE);
        gtk_widget_set_sensitive(client->disconnect_button, FALSE);
        return;
    }

    // 检查返回的对象是否有效
    if (!active_connection) {
        log_message("ERROR", "nm_client_activate_connection_finish returned a NULL object.");
        show_notification(client, "Failed to get active connection object.", TRUE);
        return;
    }

    log_message("INFO", "VPN activation started successfully. Active connection object: %p", active_connection);

    // 将 'state-changed' 信号连接到 NMActiveConnection 对象
    g_signal_connect(active_connection, "state-changed", G_CALLBACK(connection_state_changed_cb), client);

    // 将活动连接对象保存到客户端结构体中
    client->active_connection = active_connection;
    
    // 增加引用计数，确保对象在被使用时不会被过早释放
    g_object_ref(client->active_connection);
}

// 激活vpn链接
void activate_vpn_connection(OVPNClient *client)
{
    g_return_if_fail(client != NULL);
    g_return_if_fail(client->nm_client != NULL);
    g_return_if_fail(client->created_connection != NULL);

    log_message("INFO", "Activating existing VPN connection...");

    nm_client_activate_connection_async(
        client->nm_client,
        client->created_connection,   // 已存在的 connection
        NULL,                         // device（NULL = 自动选择）
        NULL,                         // specific object path（NULL = 默认）
        NULL,                         // cancellable
        vpn_activate_done,            // 回调函数
        client);
}


// VPN连接状态变化回调
void connection_state_changed_cb(NMActiveConnection *active_connection,
                                        guint state,
                                        guint reason,
                                        gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;

    log_message("DEBUG", "Received state change signal for connection: %p, State: %d", active_connection, state);
    log_message("DEBUG", "Client's active connection: %p", client->active_connection);

        // 在这里添加一个关键的检查，确保状态变化是针对我们期望的连接
    if (active_connection != client->active_connection) {
        log_message("WARNING", "State change signal is for a different connection. Skipping UI update.");
        return;
    }
    
    (void)active_connection;
    log_message("INFO","VPN status: %s",state);
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
            gtk_widget_set_sensitive(client->connect_button, FALSE);
            gtk_widget_set_sensitive(client->disconnect_button, FALSE);
            break;
    }
}

// 把连接存到 NM 后的回调
void add_connection_done(GObject *source_obj, GAsyncResult *res, gpointer user_data)

{
    (void)source_obj;
    OVPNClient *client = user_data;
    NMRemoteConnection *remote;
    GError *error = NULL;

    remote = nm_client_add_connection_finish(client->nm_client, res, &error);

    if (error) {

        log_message("ERROR", "Add connection failed: %s", error->message);

        show_notification(client, error->message, TRUE);

        g_error_free(error);

        gtk_widget_set_sensitive(client->connect_button, TRUE);
        return;
    }

    // 1. 持久化成功，设置 created_connection 
    client->created_connection = NM_CONNECTION(remote);

    // 2. 只有在成功保存后，才启用连接按钮
    gtk_widget_set_sensitive(client->connect_button, TRUE);
    show_notification(client, "VPN connection created successfully!", FALSE);
    log_message("INFO", "VPN connection saved to NetworkManager successfully.");
}

