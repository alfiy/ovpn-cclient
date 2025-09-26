#include <gtk/gtk.h> 

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
#include "../include/app_lifecycle.h"

// 全局变量
static OVPNClient *app_instance = NULL;
static FILE *log_file = NULL;


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
    
    // 连接信号到新的生命周期管理函数
    g_signal_connect(app, "activate", G_CALLBACK(app_activate), &client);
    g_signal_connect(app, "shutdown", G_CALLBACK(app_shutdown), &client);
    
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
