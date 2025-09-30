#ifndef STRUCTS_H
#define STRUCTS_H

#include <glib.h>
#include <gtk/gtk.h>

#define MAX_REMOTES 10
#define MAX_PATH 1024

// 前向声明 GTK 类型
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkApplication GtkApplication;
// 前向声明 AppIndicator 和 NetworkManager 类型
typedef struct _AppIndicator AppIndicator;
typedef struct _NMClient NMClient;
typedef struct _NMConnection NMConnection;
typedef struct _NMActiveConnection NMActiveConnection;


typedef struct {
    char server[128];
    char port[16];
    char proto[16];
    char cipher[32];
    char tls_version[16];
    int remote_count;
} RemoteServer;

typedef struct {
    RemoteServer remote[MAX_REMOTES]; // 允许最多 MAX_REMOTES 个remote
    int remote_count;

    char proto[16];
    char port[16];
    char dev[16];
    char remote_cert_tls[64];
    char tls_version_min[16];
    gboolean cert_pass_flags_zero;

    // 文件路径
    char ca[1024];
    char cert[1024];
    char key[1024];
    char tls_auth[1024];
    char tls_crypt[1024];
    char tls_crypt_v2[1024];

    // inline block 内容
    char *ca_inline;
    char *cert_inline;
    char *key_inline;
    char *tls_crypt_inline;
    char *tls_crypt_v2_inline;

    char key_direction[8];
    gboolean auth_user_pass;
    char cipher[64];
    char auth[64];
    gboolean comp_lzo;
    gboolean redirect_gateway;

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
    gboolean is_running;
    GtkWidget *connection_combo_box; // 用于显示已存在的连接文件
    GPtrArray *existing_connections; // 存储已扫描的连接文件名

    GtkWidget *config_analysis_frame; // 配置文件解析
    GtkWidget *connection_log_frame; // 连接日志
    GtkTextView *result_view; // 配置文件解析内容
    GtkTextView *log_view; // 连接日志内容
    GtkWidget *scrolled;

    GPid vpn_log_pid;
    GIOChannel *vpn_log_channel;
    guint vpn_log_watch;
    guint openvpn_log_timer;
    gsize openvpn_log_offset;
    char *last_imported_name;
} OVPNClient;

#endif
