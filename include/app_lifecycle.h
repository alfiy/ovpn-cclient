#ifndef APP_LIFECYCLE_H
#define APP_LIFECYCLE_H

#include <gtk/gtk.h>
#include "structs.h"

// 应用程序激活回调，负责初始化UI和NetworkManager客户端
void app_activate(GtkApplication *app, gpointer user_data);

// 应用程序关闭回调，负责清理资源
void app_shutdown(GtkApplication *app, gpointer user_data);

#endif // APP_LIFECYCLE_H