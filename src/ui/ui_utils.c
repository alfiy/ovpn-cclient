#include "ui/ui_utils.h"
#include <stdarg.h>

GtkWindow* ui_get_safe_parent_window(GtkWidget *widget) {
    if (!widget) {
        return NULL;
    }

    GtkWidget *toplevel = gtk_widget_get_toplevel(widget);

    if (GTK_IS_WINDOW(toplevel)) {
        return GTK_WINDOW(toplevel);
    }

    return NULL;
}

void ui_show_error(GtkWidget *anchor,
                   const char *format,
                   ...) {
    GtkWindow *parent = ui_get_safe_parent_window(anchor);

    va_list args;
    va_start(args, format);
    char *message = g_strdup_vprintf(format, args);
    va_end(args);

    GtkWidget *dialog = gtk_message_dialog_new(
        parent,
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_ERROR,
        GTK_BUTTONS_OK,
        "%s",
        message
    );

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    g_free(message);
}
