#ifndef NM_CONNECTION_H
#define NM_CONNECTION_H

#include "../include/parser.h"
#include "../include/structs.h"
#include <libnm/NetworkManager.h>
#include <gtk-3.0/gtk/gtk.h> 


NMConnection* create_nm_vpn_connection(OVPNClient *client, const char *name, OVPNConfig *config);
void activate_vpn_connection(OVPNClient *client);
void vpn_activate_done(GObject *source_obj, GAsyncResult *res, gpointer user_data);
void connection_state_changed_cb(NMActiveConnection *active_connection,
                                        guint state,
                                        guint reason,
                                        gpointer user_data);

void add_connection_done(GObject *source_obj, GAsyncResult *res, gpointer user_data);
void connect_vpn_clicked(GtkWidget *widget, gpointer user_data);

#endif

