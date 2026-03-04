#ifndef UI_UTILS_H
#define UI_UTILS_H

#include <gtk/gtk.h>

GtkWindow* ui_get_safe_parent_window(GtkWidget *widget);

void ui_show_error(GtkWidget *anchor,
                   const char *format,
                   ...) G_GNUC_PRINTF(2, 3);

#endif
