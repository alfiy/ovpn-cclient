#include <gtk/gtk.h> 
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
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include "../include/log_util.h"
#include "../include/message.h"
#include "../include/structs.h"
#include "../include/notify.h"
#include "../include/parser.h"
#include "../include/nm_connection.h"
#include "../include/ui_builder.h"
#include "../include/ui_callbacks.h"
#include "../include/core_logic.h"
#include "../include/config.h"


static OVPNClient *app_instance = NULL;
static FILE *log_file = NULL;


// 应用程序激活回调
static void activate(GtkApplication *app, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    
    log_message("INFO", "Activating application...");
    
    // 防止重复激活
    if (client->window) {
        log_message("INFO", "Application already activated, showing window");
        gtk_window_present(GTK_WINDOW(client->window));
        return;
    }
    
    // 初始化NetworkManager客户端
    client->nm_client = nm_client_new(NULL, NULL);
    if (!client->nm_client) {
        log_message("ERROR", "Failed to initialize NetworkManager client");
        return;
    }
    
    log_message("INFO", "NetworkManager client initialized");
    
    // 创建主窗口
    create_main_window(client);
    
    // 设置应用程序窗口
    gtk_application_add_window(app, GTK_WINDOW(client->window));
    
    // 创建系统托盘指示器（可选，失败不影响主程序）
    create_indicator(client);
    
    // 显示窗口
    log_message("INFO", "Showing main window...");
    gtk_widget_show_all(client->window);
    
    // 确保窗口获得焦点
    gtk_window_present(GTK_WINDOW(client->window));
    
    client->is_running = TRUE;
    
    log_message("INFO", "Application activation completed");
}

// 应用程序关闭处理
static void on_app_shutdown(GtkApplication *app, gpointer user_data) {
    OVPNClient *client = (OVPNClient *)user_data;
    (void)app;
    
    log_message("INFO", "Application shutting down...");
    
    client->is_running = FALSE;
    
    // 清理资源
    if (client->parsed_config) {
        g_free(client->parsed_config);
        client->parsed_config = NULL;
    }
    
    if (client->vpn_connections) {
        g_ptr_array_free(client->vpn_connections, TRUE);
        client->vpn_connections = NULL;
    }
    
    if (client->nm_client) {
        g_object_unref(client->nm_client);
        client->nm_client = NULL;
    }
    
    log_message("INFO", "Application cleanup completed");
}

// 主函数
int main(int argc, char *argv[]) {
    GtkApplication *app;
    OVPNClient client = {0};
    int status;
    
    log_message("INFO", "Starting OVPN Client application...");
    
    app_instance = &client;
    client.is_running = FALSE;
    
    // 初始化GTK应用程序
    app = gtk_application_new(APP_ID, G_APPLICATION_FLAGS_NONE);
    if (!app) {
        log_message("ERROR", "Failed to create GTK application");
        return 1;
    }
    
    client.app = app;
    
    // 连接信号
    g_signal_connect(app, "activate", G_CALLBACK(activate), &client);
    g_signal_connect(app, "shutdown", G_CALLBACK(on_app_shutdown), &client);
    
    log_message("INFO", "Running GTK application...");
    status = g_application_run(G_APPLICATION(app), argc, argv);
    
    log_message("INFO", "Application exiting with status: %d", status);
    
    // 最终清理
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    
    g_object_unref(app);
    
    return status;
}
