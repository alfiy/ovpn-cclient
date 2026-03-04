#include "v2ray_ui.h"
#include "proxy_parser.h"
#include <string.h>
#include "ui/ui_utils.h"


typedef struct {
    GtkWidget *dialog;
    GtkWidget *url_entry;
    GtkWidget *status_label;
    GtkWidget *start_button;
    GtkWidget *stop_button;
    GtkWidget *tproxy_switch;
    GtkWidget *log_view;
    GtkTextBuffer *log_buffer;
    V2RayManager *manager;
    guint update_timer;
} V2RayDialog;

// 更新日志显示
static gboolean update_log_view(gpointer user_data) {
    V2RayDialog *dialog = (V2RayDialog*)user_data;
    
    const char *log = v2ray_manager_get_log(dialog->manager);
    gtk_text_buffer_set_text(dialog->log_buffer, log, -1);
    
    // 滚动到底部
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(dialog->log_buffer, &end);
    GtkTextMark *mark = gtk_text_buffer_create_mark(dialog->log_buffer, NULL, &end, FALSE);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(dialog->log_view), mark, 0.0, TRUE, 0.0, 1.0);
    
    return TRUE;
}

// 更新状态
static void update_status(V2RayDialog *dialog) {
    if (!dialog || !dialog->manager) {
        return;
    }
    
    V2RayStatus status = v2ray_manager_get_status(dialog->manager);
    const char *status_str = v2ray_manager_get_status_string(status);
    
    char markup[256];
    const char *color = "gray";
    
    switch (status) {
        case V2RAY_STATUS_RUNNING:
            color = "green";
            gtk_widget_set_sensitive(dialog->start_button, FALSE);
            gtk_widget_set_sensitive(dialog->stop_button, TRUE);
            gtk_widget_set_sensitive(dialog->url_entry, FALSE);
            gtk_widget_set_sensitive(dialog->tproxy_switch, TRUE);
            break;
        case V2RAY_STATUS_STOPPED:
            color = "gray";
            gtk_widget_set_sensitive(dialog->start_button, TRUE);
            gtk_widget_set_sensitive(dialog->stop_button, FALSE);
            gtk_widget_set_sensitive(dialog->url_entry, TRUE);  // ✅ 允许编辑
            gtk_widget_set_sensitive(dialog->tproxy_switch, FALSE);
            break;
        case V2RAY_STATUS_ERROR:
            color = "red";
            gtk_widget_set_sensitive(dialog->start_button, TRUE);
            gtk_widget_set_sensitive(dialog->stop_button, FALSE);
            gtk_widget_set_sensitive(dialog->url_entry, TRUE);  // ✅ 允许编辑
            gtk_widget_set_sensitive(dialog->tproxy_switch, FALSE);
            break;
        default:
            color = "orange";
            gtk_widget_set_sensitive(dialog->start_button, TRUE);
            gtk_widget_set_sensitive(dialog->stop_button, FALSE);
            gtk_widget_set_sensitive(dialog->url_entry, TRUE);  // ✅ 默认允许编辑
            gtk_widget_set_sensitive(dialog->tproxy_switch, FALSE);
            break;
    }
    
    snprintf(markup, sizeof(markup), "<span foreground='%s' weight='bold'>状态: %s</span>", 
             color, status_str);
    gtk_label_set_markup(GTK_LABEL(dialog->status_label), markup);
}

// 启动按钮回调
static void on_start_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    V2RayDialog *dialog = (V2RayDialog*)user_data;
    
    const char *url = gtk_entry_get_text(GTK_ENTRY(dialog->url_entry));
    if (!url || strlen(url) == 0) {

        ui_show_error(dialog->dialog, "请输入代理节点链接");

        return;
    }
    
    // 解析代理链接
    ProxyConfig *config = proxy_parser_parse(url);
    if (!config) {
        ui_show_error(dialog->dialog, "无法解析代理链接，请检查格式是否正确");
        return;
    }
    
    // 设置配置
    if (!v2ray_manager_set_config(dialog->manager, config)) {

        ui_show_error(dialog->dialog, "无法设置代理配置");
        return;
    }
    
    // 启动 V2Ray
    GError *error = NULL;
    if (!v2ray_manager_start(dialog->manager, &error)) {

        ui_show_error(dialog->dialog, "启动 V2Ray 失败请检查代理链接是否正确");
        return;
    }
    
    update_status(dialog);
    
    // 启动日志更新定时器
    if (dialog->update_timer == 0) {
        dialog->update_timer = g_timeout_add(1000, update_log_view, dialog);
    }
}

// 停止按钮回调
static void on_stop_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    V2RayDialog *dialog = (V2RayDialog*)user_data;
    
    v2ray_manager_stop(dialog->manager);
    update_status(dialog);
    
    // 停止日志更新定时器
    if (dialog->update_timer > 0) {
        g_source_remove(dialog->update_timer);
        dialog->update_timer = 0;
    }
}

// 透明代理开关回调
static void on_tproxy_toggled(GtkSwitch *widget, gboolean state, gpointer user_data) {
    V2RayDialog *dialog = (V2RayDialog*)user_data;

    GError *error = NULL;
    if (!v2ray_manager_enable_tproxy(dialog->manager, state, &error)) {

        ui_show_error(
            dialog->dialog,
            "配置透明代理失败: %s\n\n"
            "请确保:\n"
            "1. 已安装 pkexec\n"
            "2. 脚本有执行权限\n"
            "3. 输入了正确的 sudo 密码",
            error ? error->message : "未知错误"
        );

        if (error) {
            g_error_free(error);
        }

        /* 恢复开关状态，避免递归触发 */
        g_signal_handlers_block_by_func(
            widget,
            G_CALLBACK(on_tproxy_toggled),
            user_data
        );

        gtk_switch_set_active(widget, !state);

        g_signal_handlers_unblock_by_func(
            widget,
            G_CALLBACK(on_tproxy_toggled),
            user_data
        );
    }
}


// 对话框关闭回调
static void on_dialog_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    V2RayDialog *dialog = (V2RayDialog*)user_data;
    
    if (dialog->update_timer > 0) {
        g_source_remove(dialog->update_timer);
    }
    
    g_free(dialog);
}

// 显示配置对话框
void v2ray_ui_show_config_dialog(GtkWindow *parent, V2RayManager *manager) {
    V2RayDialog *dialog_data = g_new0(V2RayDialog, 1);
    dialog_data->manager = manager;
    dialog_data->update_timer = 0;  // ✅ 初始化定时器为 0
    
    // 创建对话框
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "V2Ray 代理配置",
        parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "关闭", GTK_RESPONSE_CLOSE,
        NULL
    );
    gtk_window_set_default_size(GTK_WINDOW(dialog), 700, 500);
    dialog_data->dialog = dialog;
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);
    
    // 创建笔记本（标签页）
    GtkWidget *notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(content), notebook, TRUE, TRUE, 0);
    
    // === 标签页1: 节点配置 ===
    GtkWidget *config_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(config_box), 10);
    
    // 状态显示
    dialog_data->status_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(dialog_data->status_label), 
                        "<span foreground='gray' weight='bold'>状态: 已停止</span>");
    gtk_box_pack_start(GTK_BOX(config_box), dialog_data->status_label, FALSE, FALSE, 0);
    
    // 节点输入
    GtkWidget *url_frame = gtk_frame_new("代理节点");
    gtk_box_pack_start(GTK_BOX(config_box), url_frame, FALSE, FALSE, 0);
    
    GtkWidget *url_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(url_box), 10);
    gtk_container_add(GTK_CONTAINER(url_frame), url_box);
    
    GtkWidget *url_label = gtk_label_new("请输入代理链接 (支持 ss://, vmess://, vless://, trojan://):");
    gtk_label_set_xalign(GTK_LABEL(url_label), 0);
    gtk_box_pack_start(GTK_BOX(url_box), url_label, FALSE, FALSE, 0);
    
    dialog_data->url_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(dialog_data->url_entry), "ss://YWVzLTI1Ni1nY206...");
    gtk_box_pack_start(GTK_BOX(url_box), dialog_data->url_entry, FALSE, FALSE, 0);
    
    // ✅ 确保输入框初始状态为可编辑
    gtk_widget_set_sensitive(dialog_data->url_entry, TRUE);
    gtk_editable_set_editable(GTK_EDITABLE(dialog_data->url_entry), TRUE);
    
    GtkWidget *hint_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(hint_label), 
                        "<small><i>提示: 粘贴你的节点链接,点击启动即可</i></small>");
    gtk_label_set_xalign(GTK_LABEL(hint_label), 0);
    gtk_box_pack_start(GTK_BOX(url_box), hint_label, FALSE, FALSE, 0);
    
    // 控制按钮
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(config_box), button_box, FALSE, FALSE, 0);
    
    dialog_data->start_button = gtk_button_new_with_label("启动 V2Ray");
    gtk_widget_set_size_request(dialog_data->start_button, 120, -1);
    g_signal_connect(dialog_data->start_button, "clicked", G_CALLBACK(on_start_clicked), dialog_data);
    gtk_box_pack_start(GTK_BOX(button_box), dialog_data->start_button, FALSE, FALSE, 0);
    
    dialog_data->stop_button = gtk_button_new_with_label("停止 V2Ray");
    gtk_widget_set_size_request(dialog_data->stop_button, 120, -1);
    gtk_widget_set_sensitive(dialog_data->stop_button, FALSE);
    g_signal_connect(dialog_data->stop_button, "clicked", G_CALLBACK(on_stop_clicked), dialog_data);
    gtk_box_pack_start(GTK_BOX(button_box), dialog_data->stop_button, FALSE, FALSE, 0);
    
    // 透明代理开关
    GtkWidget *tproxy_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(config_box), tproxy_box, FALSE, FALSE, 0);
    
    GtkWidget *tproxy_label = gtk_label_new("启用透明代理 (全局分流):");
    gtk_box_pack_start(GTK_BOX(tproxy_box), tproxy_label, FALSE, FALSE, 0);
    
    dialog_data->tproxy_switch = gtk_switch_new();
    gtk_widget_set_sensitive(dialog_data->tproxy_switch, FALSE);  // ✅ 初始状态禁用
    g_signal_connect(dialog_data->tproxy_switch, "state-set", G_CALLBACK(on_tproxy_toggled), dialog_data);
    gtk_box_pack_start(GTK_BOX(tproxy_box), dialog_data->tproxy_switch, FALSE, FALSE, 0);
    
    GtkWidget *tproxy_hint = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(tproxy_hint), 
                        "<small><i>需要 root 权限,启用后所有流量将自动分流</i></small>");
    gtk_label_set_xalign(GTK_LABEL(tproxy_hint), 0);
    gtk_box_pack_start(GTK_BOX(config_box), tproxy_hint, FALSE, FALSE, 0);
    
    // 路由规则说明
    GtkWidget *rules_frame = gtk_frame_new("路由规则");
    gtk_box_pack_start(GTK_BOX(config_box), rules_frame, FALSE, FALSE, 0);
    
    GtkWidget *rules_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(rules_box), 10);
    
    GtkWidget *rules_label = gtk_label_new(
        "优先级 1: 内网 IP (10.8.0.0/24) → OpenVPN 隧道\n"
        "优先级 2: 中国大陆 IP/域名 → 直连\n"
        "优先级 3: 国外 IP/域名 → V2Ray 代理\n"
        "优先级 4: 广告/恶意域名 → 阻断"
    );
    gtk_label_set_xalign(GTK_LABEL(rules_label), 0);
    gtk_container_add(GTK_CONTAINER(rules_box), rules_label);
    gtk_container_add(GTK_CONTAINER(rules_frame), rules_box);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), config_box, gtk_label_new("节点配置"));
    
    // === 标签页2: 运行日志 ===
    GtkWidget *log_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(log_box), 10);
    
    GtkWidget *log_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(log_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(log_box), log_scroll, TRUE, TRUE, 0);
    
    dialog_data->log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(dialog_data->log_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(dialog_data->log_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(dialog_data->log_view), GTK_WRAP_WORD_CHAR);
    dialog_data->log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(dialog_data->log_view));
    gtk_container_add(GTK_CONTAINER(log_scroll), dialog_data->log_view);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), log_box, gtk_label_new("运行日志"));
    
    // === 标签页3: 帮助信息 ===
    GtkWidget *help_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(help_box), 10);
    
    GtkWidget *help_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(help_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(help_box), help_scroll, TRUE, TRUE, 0);
    
    GtkWidget *help_label = gtk_label_new(
        "V2Ray 代理配置帮助\n\n"
        "1. 获取节点链接\n"
        "   - 从你的代理服务商获取 ss://, vmess://, vless:// 或 trojan:// 链接\n"
        "   - 支持订阅链接中的单个节点\n\n"
        "2. 启动代理\n"
        "   - 粘贴节点链接到输入框\n"
        "   - 点击 \"启动 V2Ray\" 按钮\n"
        "   - 等待状态变为 \"运行中\"\n\n"
        "3. 启用透明代理\n"
        "   - 打开 \"启用透明代理\" 开关\n"
        "   - 输入 sudo 密码（需要 root 权限）\n"
        "   - 所有流量将自动分流,无需配置浏览器\n\n"
        "4. 流量分流规则\n"
        "   - 访问 OpenVPN 内网 (10.8.0.0/24) → 走 OpenVPN 隧道\n"
        "   - 访问中国大陆网站 → 直连,不走代理\n"
        "   - 访问国外网站 → 走 V2Ray 代理\n"
        "   - 广告和恶意网站 → 自动阻断\n\n"
        "5. 故障排查\n"
        "   - 如果无法启动,检查节点链接格式是否正确\n"
        "   - 如果透明代理失败,确保安装了 pkexec 和 iptables\n"
        "   - 查看 \"运行日志\" 标签页获取详细错误信息\n"
        "   - 确保 V2Ray 二进制文件已安装到 data/v2ray/ 目录\n\n"
        "6. 注意事项\n"
        "   - 透明代理需要 root 权限,会修改 iptables 规则\n"
        "   - 停止 V2Ray 时会自动清理 iptables 规则\n"
        "   - 建议先测试节点是否可用,再启用透明代理\n"
        "   - 同时使用 OpenVPN 和 V2Ray 时,内网流量优先走 OpenVPN\n"
    );
    gtk_label_set_xalign(GTK_LABEL(help_label), 0);
    gtk_label_set_yalign(GTK_LABEL(help_label), 0);
    gtk_label_set_line_wrap(GTK_LABEL(help_label), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(help_label), PANGO_WRAP_WORD_CHAR);
    
    // ✅ 使用 viewport 包裹 label,避免 GTK 警告
    GtkWidget *viewport = gtk_viewport_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(viewport), help_label);
    gtk_container_add(GTK_CONTAINER(help_scroll), viewport);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), help_box, gtk_label_new("使用帮助"));
    
    // 连接信号
    g_signal_connect(dialog, "destroy", G_CALLBACK(on_dialog_destroy), dialog_data);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    
    // ✅ 在显示对话框之前先显示所有控件
    gtk_widget_show_all(dialog);
    
    // ✅ 最后更新状态（此时所有控件已创建并可见）
    update_status(dialog_data);
}

// 创建状态指示器
GtkWidget* v2ray_ui_create_status_indicator(V2RayManager *manager) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    
    GtkWidget *icon = gtk_image_new_from_icon_name("network-idle", GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);
    
    GtkWidget *label = gtk_label_new("V2Ray: 未运行");
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    
    // 存储管理器指针
    g_object_set_data(G_OBJECT(box), "manager", manager);
    g_object_set_data(G_OBJECT(box), "icon", icon);
    g_object_set_data(G_OBJECT(box), "label", label);
    
    return box;
}

// 更新状态指示器
void v2ray_ui_update_status_indicator(GtkWidget *indicator, V2RayManager *manager) {
    if (!indicator || !manager) {
        return;
    }
    
    GtkWidget *icon = g_object_get_data(G_OBJECT(indicator), "icon");
    GtkWidget *label = g_object_get_data(G_OBJECT(indicator), "label");
    
    if (!icon || !label) {
        return;
    }
    
    V2RayStatus status = v2ray_manager_get_status(manager);
    const char *status_str = v2ray_manager_get_status_string(status);
    
    char text[128];
    snprintf(text, sizeof(text), "V2Ray: %s", status_str);
    
    const char *icon_name = "network-idle";
    switch (status) {
        case V2RAY_STATUS_RUNNING:
            icon_name = "network-transmit-receive";
            break;
        case V2RAY_STATUS_ERROR:
            icon_name = "network-error";
            break;
        default:
            icon_name = "network-idle";
            break;
    }
    
    gtk_image_set_from_icon_name(GTK_IMAGE(icon), icon_name, GTK_ICON_SIZE_BUTTON);
    gtk_label_set_text(GTK_LABEL(label), text);
}