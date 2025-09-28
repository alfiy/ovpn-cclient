#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <gtk/gtk.h> 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

    // --- Step 1: 检查 DBUS_SESSION_BUS_ADDRESS ---
    if (getenv("DBUS_SESSION_BUS_ADDRESS") == NULL) {
        log_message("INFO", "DBUS_SESSION_BUS_ADDRESS not set, launching new DBus session...");
        // 注意：使用 system() 启动 dbus-launch 并导出环境变量
        FILE *fp = popen("dbus-launch --sh-syntax", "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line, "DBUS_SESSION_BUS_ADDRESS", 24) == 0 ||
                    strncmp(line, "DBUS_SESSION_BUS_PID", 20) == 0) {
                    // 设置环境变量
                    char *eq = strchr(line, '=');
                    if (eq) {
                        *eq = '\0';
                        char *name = line;
                        char *value = eq + 1;
                        // 去掉末尾换行符
                        value[strcspn(value, "\n")] = 0;
                        setenv(name, value, 1);
                        log_message("INFO", "Exported %s=%s", name, value);
                    }
                }
            }
            pclose(fp);
        } else {
            log_message("WARNING", "Failed to start dbus-launch");
        }
    }

    // --- Step 2: 检查 polkit agent 是否已运行 ---
    if (system("pgrep -x polkit-gnome-authentication-agent-1 > /dev/null") != 0) {
        log_message("INFO", "Polkit agent not running, starting polkit-gnome-authentication-agent-1...");
        if (system("/usr/lib/policykit-1-gnome/polkit-gnome-authentication-agent-1 &") != 0) {
            log_message("WARNING", "Failed to start polkit agent. GUI actions requiring privileges may fail.");
        }
    } else {
        log_message("INFO", "Polkit agent already running.");
    }

    // --- Step 3: 初始化 GTK 应用 ---
    app_instance = &client;
    client.is_running = FALSE;

    app = gtk_application_new(APP_ID, G_APPLICATION_FLAGS_NONE);
    if (!app) {
        log_message("ERROR", "Failed to create GTK application");
        return 1;
    }
    client.app = app;

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