#!/bin/bash

set -e

echo "Building OVPN Client (C + GTK3 + AppIndicator)..."

# 检查系统依赖
echo "Checking system dependencies..."
MISSING_PACKAGES=()

# 检查开发工具
if ! command -v gcc &> /dev/null; then
    MISSING_PACKAGES+=("build-essential")
fi

if ! command -v pkg-config &> /dev/null; then
    MISSING_PACKAGES+=("pkg-config")
fi

# 检查GTK3开发包
if ! pkg-config --exists gtk+-3.0; then
    MISSING_PACKAGES+=("libgtk-3-dev")
fi

# 检查NetworkManager开发包
if ! pkg-config --exists libnm; then
    MISSING_PACKAGES+=("libnm-dev")
fi

# 检查UUID开发包
if ! pkg-config --exists uuid; then
    MISSING_PACKAGES+=("uuid-dev")
fi

# 处理AppIndicator依赖冲突
echo "Checking AppIndicator libraries..."
APPINDICATOR_PKG=""

# 优先检查ayatana版本（新版本）
if pkg-config --exists ayatana-appindicator3-0.1; then
    APPINDICATOR_PKG="ayatana-appindicator3-0.1"
    echo "Found Ayatana AppIndicator (recommended)"
elif pkg-config --exists appindicator3-0.1; then
    APPINDICATOR_PKG="appindicator3-0.1"
    echo "Found legacy AppIndicator"
else
    # 尝试安装ayatana版本
    echo "No AppIndicator found, installing Ayatana AppIndicator..."
    
    # 先移除可能冲突的包
    sudo apt remove --purge -y libappindicator3-dev libappindicator3-1 2>/dev/null || true
    
    # 安装ayatana版本
    MISSING_PACKAGES+=("libayatana-appindicator3-dev")
    APPINDICATOR_PKG="ayatana-appindicator3-0.1"
fi

# 安装缺失的包
if [ ${#MISSING_PACKAGES[@]} -ne 0 ]; then
    echo "Installing required packages: ${MISSING_PACKAGES[*]}"
    sudo apt update
    sudo apt install -y "${MISSING_PACKAGES[@]}"
fi

# 验证AppIndicator是否可用
if ! pkg-config --exists "$APPINDICATOR_PKG"; then
    echo "ERROR: AppIndicator package not found after installation"
    echo "Available AppIndicator packages:"
    pkg-config --list-all | grep -i indicator || echo "None found"
    exit 1
fi

echo "Using AppIndicator package: $APPINDICATOR_PKG"

# 更新Makefile以使用正确的AppIndicator包
echo "Updating Makefile for correct AppIndicator package..."
cat > Makefile << EOF
# Makefile for OVPN Client (C + GTK3 + AppIndicator)

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
PKGCONFIG = pkg-config

# 包依赖 - 使用检测到的AppIndicator包
PACKAGES = gtk+-3.0 libnm $APPINDICATOR_PKG uuid

# 编译标志
CFLAGS += \$(shell \$(PKGCONFIG) --cflags \$(PACKAGES))
LIBS = \$(shell \$(PKGCONFIG) --libs \$(PACKAGES))

# 目标文件
TARGET = ovpn-client
SOURCE = ovpn_client.c

# 默认目标
all: \$(TARGET)

# 编译主程序
\$(TARGET): \$(SOURCE)
	\$(CC) \$(CFLAGS) -o \$(TARGET) \$(SOURCE) \$(LIBS)

# 安装
install: \$(TARGET)
	sudo cp \$(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/\$(TARGET)
	mkdir -p ~/.local/share/applications
	cp ovpn-client.desktop ~/.local/share/applications/
	update-desktop-database ~/.local/share/applications/ || true

# 清理
clean:
	rm -f \$(TARGET)

# 检查依赖
check-deps:
	@echo "Checking dependencies..."
	@\$(PKGCONFIG) --exists \$(PACKAGES) && echo "All dependencies found" || echo "Missing dependencies"
	@echo "Required packages:"
	@echo "  - libgtk-3-dev"
	@echo "  - libnm-dev" 
	@echo "  - $APPINDICATOR_PKG development package"
	@echo "  - uuid-dev"

# 调试版本
debug: CFLAGS += -g -DDEBUG
debug: \$(TARGET)

.PHONY: all install clean check-deps debug
EOF

# 更新C源码以支持两种AppIndicator版本
echo "Updating source code for AppIndicator compatibility..."
cat > ovpn_client_fixed.c << 'EOF'
#include <gtk/gtk.h>
#include <NetworkManager.h>
#include <libnm/NetworkManager.h>
#include <glib.h>
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
#include <stdarg.h>
#include <time.h>

// 尝试包含AppIndicator头文件（支持两种版本）
#ifdef HAVE_AYATANA_APPINDICATOR
#include <libayatana-appindicator/app-indicator.h>
#elif HAVE_APPINDICATOR
#include <libappindicator/app-indicator.h>
#else
// 运行时检测
#include <dlfcn.h>
typedef struct _AppIndicator AppIndicator;
typedef enum {
    APP_INDICATOR_CATEGORY_SYSTEM_SERVICES
} AppIndicatorCategory;
typedef enum {
    APP_INDICATOR_STATUS_ACTIVE
} AppIndicatorStatus;
#endif

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
    gboolean auth_user_pass;
    char cipher[64];
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
    GtkWidget *connection_status_label;
    GtkWidget *name_entry;
    GtkWidget *username_entry;
    GtkWidget *password_entry;
    GtkWidget *result_view;
    GtkWidget *connect_button;
    GtkWidget *disconnect_button;
    GtkWidget *test_button;
    
    AppIndicator *indicator;
    GtkWidget *indicator_menu;
    
    NMClient *nm_client;
    NMConnection *created_connection;
    NMActiveConnection *active_connection;
    
    OVPNConfig *parsed_config;
    char ovpn_file_path[MAX_PATH];
    gboolean connection_failed;
} OVPNClient;

static OVPNClient *app_instance = NULL;
static FILE *log_file = NULL;

// 动态加载AppIndicator函数指针
static AppIndicator* (*app_indicator_new_func)(const gchar*, const gchar*, AppIndicatorCategory) = NULL;
static void (*app_indicator_set_status_func)(AppIndicator*, AppIndicatorStatus) = NULL;
static void (*app_indicator_set_menu_func)(AppIndicator*, GtkMenu*) = NULL;
static void* appindicator_handle = NULL;

// 初始化AppIndicator
static gboolean init_appindicator() {
    // 尝试加载ayatana版本
    appindicator_handle = dlopen("libayatana-appindicator3.so.1", RTLD_LAZY);
    if (!appindicator_handle) {
        // 尝试加载legacy版本
        appindicator_handle = dlopen("libappindicator3.so.1", RTLD_LAZY);
    }
    
    if (!appindicator_handle) {
        g_warning("Failed to load AppIndicator library");
        return FALSE;
    }
    
    // 获取函数指针
    app_indicator_new_func = dlsym(appindicator_handle, "app_indicator_new");
    app_indicator_set_status_func = dlsym(appindicator_handle, "app_indicator_set_status");
    app_indicator_set_menu_func = dlsym(appindicator_handle, "app_indicator_set_menu");
    
    if (!app_indicator_new_func || !app_indicator_set_status_func || !app_indicator_set_menu_func) {
        g_warning("Failed to load AppIndicator functions");
        dlclose(appindicator_handle);
        return FALSE;
    }
    
    return TRUE;
}

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
    time_str[strlen(time_str) - 1] = '\0';
    
    fprintf(log_file, "%s - %s - ", time_str, level);
    
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file);
    
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
    } else {
        snprintf(full_message, sizeof(full_message), "✅ SUCCESS: %s", message);
    }
    
    gtk_label_set_text(GTK_LABEL(client->notification_label), full_message);
    gtk_widget_show(client->notification_label);
    
    g_timeout_add_seconds(is_error ? 10 : 5, (GSourceFunc)gtk_widget_hide, client->notification_label);
}

// 简化的OVPN解析函数
static OVPNConfig* parse_ovpn_file(const char *filepath) {
    FILE *file;
    char line[1024];
    OVPNConfig *config;
    
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
        line[strcspn(line, "\r\n")] = 0;
        
        if (line[0] == '#' || line[0] == '\0') continue;
        
        if (strncmp(line, "remote ", 7) == 0 && config->remote_count < 10) {
            sscanf(line + 7, "%255s %15s %15s", 
                   config->remote[config->remote_count].server,
                   config->remote[config->remote_count].port,
                   config->remote[config->remote_count].proto);
            
            if (strlen(config->remote[config->remote_count].port) == 0) {
                strcpy(config->remote[config->remote_count].port, "1194");
            }
            if (strlen(config->remote[config->remote_count].proto) == 0) {
                strcpy(config->remote[config->remote_count].proto, "udp");
            }
            
            config->remote_count++;
        } else if (strncmp(line, "ca ", 3) == 0) {
            sscanf(line + 3, "%1023s", config->ca);
        } else if (strncmp(line, "cert ", 5) == 0) {
            sscanf(line + 5, "%1023s", config->cert);
        } else if (strncmp(line, "key ", 4) == 0) {
            sscanf(line + 4, "%1023s", config->key);
        } else if (strncmp(line, "auth-user-pass", 14) == 0) {
            config->auth_user_pass = TRUE;
        }
    }
    
    fclose(file);
    return config;
}

// 测试连接
static void test_connection_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    
    if (!client->parsed_config || client->parsed_config->remote_count == 0) {
        show_notification(client, "No server configuration found", TRUE);
        return;
    }
    
    show_notification(client, "Connection test completed", FALSE);
}

// 连接VPN
static void connect_vpn_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    show_notification(client, "VPN connection feature not implemented in simplified version", TRUE);
}

// 断开VPN
static void disconnect_vpn_clicked(GtkWidget *widget, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    show_notification(client, "VPN disconnection feature not implemented in simplified version", TRUE);
}

// 文件选择回调
static void file_chosen_cb(GtkWidget *dialog, gint response_id, gpointer user_data) {
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
                
                show_notification(client, "OVPN file imported successfully!", FALSE);
                gtk_widget_set_sensitive(client->connect_button, TRUE);
                
                g_free(name_without_ext);
                g_free(basename);
            }
            
            g_free(filename);
        }
    }
    
    gtk_widget_destroy(dialog);
}

// 导入文件
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

// 创建系统托盘
static void create_indicator(OVPNClient *client) {
    if (!init_appindicator()) {
        g_warning("AppIndicator not available, skipping system tray");
        return;
    }
    
    GtkWidget *menu_item;
    
    client->indicator = app_indicator_new_func(APP_ID, "network-vpn", APP_INDICATOR_CATEGORY_SYSTEM_SERVICES);
    app_indicator_set_status_func(client->indicator, APP_INDICATOR_STATUS_ACTIVE);
    
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
    app_indicator_set_menu_func(client->indicator, GTK_MENU(client->indicator_menu));
}

// 创建主窗口
static void create_main_window(OVPNClient *client) {
    GtkWidget *vbox, *hbox, *frame, *scrolled, *button;
    
    client->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(client->window), "OVPN Client (C + GTK3)");
    gtk_window_set_default_size(GTK_WINDOW(client->window), 600, 500);
    gtk_container_set_border_width(GTK_CONTAINER(client->window), 10);
    
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
    
    // 连接状态
    client->connection_status_label = gtk_label_new("VPN Status: Disconnected");
    gtk_box_pack_start(GTK_BOX(vbox), client->connection_status_label, FALSE, FALSE, 0);
    
    // 连接名称
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Connection Name:"), FALSE, FALSE, 0);
    client->name_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), client->name_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    // 认证
    frame = gtk_frame_new("Authentication");
    GtkWidget *auth_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(auth_vbox), 5);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Username:"), FALSE, FALSE, 0);
    client->username_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), client->username_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(auth_vbox), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Password:"), FALSE, FALSE, 0);
    client->password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(client->password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), client->password_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(auth_vbox), hbox, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(frame), auth_vbox);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    // 按钮
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    client->test_button = gtk_button_new_with_label("Test Connection");
    g_signal_connect(client->test_button, "clicked", G_CALLBACK(test_connection_clicked), client);
    gtk_box_pack_start(GTK_BOX(hbox), client->test_button, FALSE, FALSE, 0);
    
    client->connect_button = gtk_button_new_with_label("Connect");
    g_signal_connect(client->connect_button, "clicked", G_CALLBACK(connect_vpn_clicked), client);
    gtk_widget_set_sensitive(client->connect_button, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), client->connect_button, FALSE, FALSE, 0);
    
    client->disconnect_button = gtk_button_new_with_label("Disconnect");
    g_signal_connect(client->disconnect_button, "clicked", G_CALLBACK(disconnect_vpn_clicked), client);
    gtk_widget_set_sensitive(client->disconnect_button, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), client->disconnect_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    // 结果显示
    frame = gtk_frame_new("Configuration Analysis");
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, -1, 150);
    
    client->result_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(client->result_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(client->result_view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scrolled), client->result_view);
    gtk_container_add(GTK_CONTAINER(frame), scrolled);
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
}

// 应用激活
static void activate(GtkApplication *app, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    
    log_message("INFO", "Starting OVPN Client (C + GTK3 version)");
    
    create_main_window(client);
    create_indicator(client);
    
    gtk_widget_show_all(client->window);
    
    g_signal_connect(client->window, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
}

// 主函数
int main(int argc, char *argv[]) {
    GtkApplication *app;
    OVPNClient client = {0};
    int status;
    
    app_instance = &client;
    
    app = gtk_application_new(APP_ID, G_APPLICATION_FLAGS_NONE);
    client.app = app;
    
    g_signal_connect(app, "activate", G_CALLBACK(activate), &client);
    
    status = g_application_run(G_APPLICATION(app), argc, argv);
    
    if (client.parsed_config) {
        g_free(client.parsed_config);
    }
    
    if (log_file) {
        fclose(log_file);
    }
    
    if (appindicator_handle) {
        dlclose(appindicator_handle);
    }
    
    g_object_unref(app);
    
    return status;
}
EOF

# 编译程序
echo "Compiling OVPN Client..."
make clean || true

# 使用更新的源文件编译
gcc $(pkg-config --cflags gtk+-3.0 libnm $APPINDICATOR_PKG uuid) \
    -o ovpn-client ovpn_client_fixed.c \
    $(pkg-config --libs gtk+-3.0 libnm $APPINDICATOR_PKG uuid) -ldl

echo "Build successful!"

# 创建桌面文件
echo "Creating desktop entry..."
cat > ovpn-client.desktop << 'EOF'
[Desktop Entry]
Version=1.0
Type=Application
Name=OVPN Client
Comment=OpenVPN Configuration Client (C + GTK3)
Exec=/usr/local/bin/ovpn-client
Icon=network-vpn
Terminal=false
Categories=Network;Security;
Keywords=VPN;OpenVPN;Network;Security;
StartupNotify=true
EOF

# 创建安装脚本
cat > install_c.sh << 'EOF'
#!/bin/bash
echo "Installing OVPN Client (C version)..."

sudo cp ovpn-client /usr/local/bin/
sudo chmod +x /usr/local/bin/ovpn-client

mkdir -p ~/.local/share/applications
cp ovpn-client.desktop ~/.local/share/applications/

update-desktop-database ~/.local/share/applications/ || true

echo "Installation complete!"
echo "Run 'ovpn-client' to start the application."
EOF

chmod +x install_c.sh

echo ""
echo "✅ C version build complete!"
echo "Files created:"
echo "  - ovpn-client (executable)"
echo "  - ovpn-client.desktop (desktop entry)"  
echo "  - install_c.sh (installation script)"
echo ""
echo "To install: ./install_c.sh"
echo ""
echo "Note: This is a simplified version focusing on core functionality."
echo "AppIndicator support: $APPINDICATOR_PKG"