#include "mapview.h"
#include <webkit2/webkit2.h>
#include <gtk/gtk.h>

GtkWidget* create_worldmap_tab(void) {
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);

    WebKitWebView *map_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(map_view));

    webkit_web_view_load_uri(map_view, "https://2004.lostcity.rs/worldmap");
    return scrolled;
}
