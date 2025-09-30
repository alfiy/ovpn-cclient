#ifndef UI_CALLBACKS_H
#define UI_CALLBACKS_H

#include <gtk/gtk.h>
#include "structs.h"
#include "nm_connection.h"

gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data);
void import_file_clicked(GtkWidget *widget, gpointer user_data);
void file_chosen_cb(GtkWidget *dialog, gint response_id, gpointer user_data);
void test_connection_clicked(GtkWidget *widget, gpointer user_data);
void disconnect_done(GObject *source_object, GAsyncResult *res, gpointer user_data);
void quit_menu_clicked(GtkWidget *widget, gpointer user_data);
void disconnect_vpn_clicked(GtkWidget *widget, gpointer user_data);
void show_window_clicked(GtkWidget *widget, gpointer user_data);
void connect_vpn_clicked(GtkWidget *widget, gpointer user_data);
void on_connection_selected(GtkWidget *widget, gpointer user_data);

void refresh_connection_combo_box(OVPNClient *client);
void delete_vpn_clicked(GtkWidget *widget, gpointer user_data);
void append_log_to_view(OVPNClient *client, const char *log_line);
gboolean pipe_log_callback(GIOChannel *source, GIOCondition cond, gpointer user_data);
void start_vpn_and_log(OVPNClient *client, const char *connection_name);
void vpn_log_cleanup(OVPNClient *client);

#endif