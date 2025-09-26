#include <glib.h>
#include <gtk-3.0/gtk/gtk.h>

#include "../include/notify.h"
#include "../include/log_util.h"
#include "../include/structs.h"
#include "../include/message.h"

#define MAX_TEXT 2048

// show_notification 通知函数
void show_notification(OVPNClient *client, const char *message, gboolean is_error) {
    char full_message[MAX_TEXT];
    
    if (!client || !client->notification_label) {
        log_message("ERROR", "Invalid client or notification label in show_notification");
        return;
    }
    
    if (is_error) {
        snprintf(full_message, sizeof(full_message), "❌ ERROR: %s", message);
        gtk_widget_set_name(client->notification_label, "error");
    } else {
        snprintf(full_message, sizeof(full_message), "✅ SUCCESS: %s", message);
        gtk_widget_set_name(client->notification_label, "success");
    }
    
    gtk_label_set_text(GTK_LABEL(client->notification_label), full_message);
    gtk_widget_show(client->notification_label);
    
    // 使用专用的回调函数，避免函数类型转换警告
    g_timeout_add_seconds(is_error ? 10 : 5, hide_notification_timeout, client->notification_label);
}
