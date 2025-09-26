#include <gtk/gtk.h>
#include <libnm/NetworkManager.h>
#include "../include/log_util.h"
#include "../include/structs.h"
#include "../include/ui_builder.h"
#include "../include/app_lifecycle.h"


// 应用程序激活回调
void app_activate(GtkApplication *app, gpointer user_data) {
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
void app_shutdown(GtkApplication *app, gpointer user_data) {
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