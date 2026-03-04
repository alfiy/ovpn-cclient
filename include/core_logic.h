#ifndef CORE_LOGIC_H
#define CORE_LOGIC_H 
#include "structs.h"
#include <gtk/gtk.h>


gboolean validate_certificates(OVPNConfig *config);
gboolean test_connection(const char *server, int port, const char *proto);
void scan_nm_connections(OVPNClient *client);
void init_v2ray_manager(OVPNClient *client);
void cleanup_v2ray_manager(OVPNClient *client);
#endif