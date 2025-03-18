#ifndef HISCORES_H
#define HISCORES_H

#include <gtk/gtk.h>

// Builds the right-hand sidebar with tabs for Hiscores & Map
GtkWidget* create_sidebar_notebook(void);

// Toggle callback for showing/hiding the entire sidebar
void toggle_sidebar_cb(GtkButton *button, gpointer user_data);

#endif
