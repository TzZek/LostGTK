#include "sidebar.h"
#include "hiscores.h"
#include "mapview.h"
#include "worlds.h"

#include <gtk/gtk.h>

/* Toggle the entire sidebar notebook visible/invisible */
void toggle_sidebar_cb(GtkButton *button, gpointer user_data) {
    GtkWidget *sidebar = GTK_WIDGET(user_data);
    gboolean visible = gtk_widget_get_visible(sidebar);
    gtk_widget_set_visible(sidebar, !visible);
}

/* Build the sidebar notebook with 3 separate tabs */
GtkWidget* create_sidebar_notebook(void) {
    GtkWidget *notebook = gtk_notebook_new();

    // 1) Hiscores
    GtkWidget *hiscores_tab = create_hiscores_tab();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), hiscores_tab, gtk_label_new("Hiscores"));

    // 2) World Map
    GtkWidget *map_tab = create_worldmap_tab();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), map_tab, gtk_label_new("World Map"));

    // 3) Worlds
    GtkWidget *worlds_tab = create_worlds_tab();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), worlds_tab, gtk_label_new("Worlds"));

    return notebook;
}
