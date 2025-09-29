#ifndef UI_BUILDER_H
#define UI_BUILDER_H
#include "structs.h"
#include "ui_callbacks.h"

//创建主窗口、按钮、标签、输入框、文本视图等所有 GTK 控件

void create_main_window(OVPNClient *client);
gboolean create_indicator(OVPNClient *client);
void update_config_analysis_view(OVPNClient *client);

#endif