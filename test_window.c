// 创建一个最小测试版本 - test_window.c
#include <gtk/gtk.h>

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *label;
    
    printf("Creating test window...\n");
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Test Window");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
    
    label = gtk_label_new("If you can see this, GTK is working!");
    gtk_container_add(GTK_CONTAINER(window), label);
    
    gtk_application_add_window(app, GTK_WINDOW(window));
    gtk_widget_show_all(window);
    
    printf("Test window should be visible now\n");
}

int main(int argc, char *argv[]) {
    GtkApplication *app;
    int status;
    
    app = gtk_application_new("com.test.window", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}
