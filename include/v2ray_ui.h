#ifndef V2RAY_UI_H
#define V2RAY_UI_H

#include <gtk/gtk.h>
#include "v2ray_manager.h"

/**
 * 创建 V2Ray 配置对话框
 * @param parent 父窗口
 * @param manager V2Ray 管理器
 */
void v2ray_ui_show_config_dialog(GtkWindow *parent, V2RayManager *manager);

/**
 * 创建 V2Ray 状态指示器
 * @param manager V2Ray 管理器
 * @return GtkWidget* 状态指示器控件
 */
GtkWidget* v2ray_ui_create_status_indicator(V2RayManager *manager);

/**
 * 更新状态指示器
 * @param indicator 状态指示器控件
 * @param manager V2Ray 管理器
 */
void v2ray_ui_update_status_indicator(GtkWidget *indicator, V2RayManager *manager);

#endif // V2RAY_UI_H