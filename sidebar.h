#ifndef SIDEBAR_H
#define SIDEBAR_H

#include <gtk/gtk.h>

// Builds the right-hand sidebar notebook with multiple tabs
GtkWidget* create_sidebar_notebook(void);

// Toggle callback for showing/hiding the entire notebook
void toggle_sidebar_cb(GtkButton *button, gpointer user_data);

#endif
