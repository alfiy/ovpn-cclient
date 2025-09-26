#ifndef CORE_LOGIC_H
#define CORE_LOGIC_H 
#include "structs.h"
#include <gtk/gtk.h>


gboolean validate_certificates(OVPNConfig *config);
gboolean test_connection(const char *server, int port, const char *proto);

#endif