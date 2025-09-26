#include <glib.h>
#include <gtk-3.0/gtk/gtk.h>
#include "../include/message.h"

// 添加专用的回调函数来隐藏通知（在文件的适当位置添加）
gboolean hide_notification_timeout(gpointer data) {
    GtkWidget *label = GTK_WIDGET(data);
    if (label && GTK_IS_WIDGET(label)) {
        gtk_widget_hide(label);
    }
    return FALSE; // 返回 FALSE 表示不重复执行此超时回调
}
