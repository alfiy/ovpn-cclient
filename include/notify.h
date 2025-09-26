#ifndef NOTIFY_H
#define NOTIFY_H
#include <glib.h>
#include "structs.h"

void show_notification(OVPNClient *client, const char *message, gboolean is_error);

#endif
