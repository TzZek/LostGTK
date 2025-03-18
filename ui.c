#include "ui.h"
#include "hiscores.h"  // for create_sidebar_notebook(), toggle_sidebar_cb
#include "mapview.h"

#include <webkit2/webkit2.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <time.h>
#include <stdio.h>

// We'll reference the game view here
static WebKitWebView *g_game_view = NULL;

/* -------------------------------------------------------------
   DISCORD BUTTON
   ------------------------------------------------------------- */
static void on_discord_clicked(GtkButton *button, gpointer user_data) {
    GtkWindow *window = GTK_WINDOW(user_data);
    GError *error = NULL;
    if (!gtk_show_uri_on_window(window, "https://discord.lostcity.rs/", GDK_CURRENT_TIME, &error)) {
        g_print("Failed to open Discord link: %s\n", error ? error->message : "unknown");
        if (error) g_error_free(error);
    }
}

/* -------------------------------------------------------------
   SNAPSHOT THE ENTIRE WEBVIEW (TIMESTAMPED FILENAME)
   ------------------------------------------------------------- */
static void on_snapshot_finished(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    WebKitWebView *web_view = WEBKIT_WEB_VIEW(source_object);
    GError *error = NULL;

    cairo_surface_t *surface = webkit_web_view_get_snapshot_finish(web_view, res, &error);
    if (!surface) {
        g_print("Snapshot error: %s\n", (error ? error->message : "unknown"));
        g_clear_error(&error);
        return;
    }

    // Create a timestamp-based filename
    char filename[64];
    time_t now = time(NULL);
    struct tm *localt = localtime(&now);
    strftime(filename, sizeof(filename), "screenshot_%Y-%m-%d_%H-%M-%S.png", localt);

    cairo_status_t status = cairo_surface_write_to_png(surface, filename);
    cairo_surface_destroy(surface);

    if (status == CAIRO_STATUS_SUCCESS) {
        g_print("Screenshot saved to %s\n", filename);
    } else {
        g_print("Failed to write PNG: %s\n", cairo_status_to_string(status));
    }
}

static void on_screenshot_clicked(GtkButton *button, gpointer user_data) {
    WebKitWebView *web_view = WEBKIT_WEB_VIEW(user_data);
    if (!web_view) return;

    // Capture the visible region
    webkit_web_view_get_snapshot(
        web_view,
        WEBKIT_SNAPSHOT_REGION_VISIBLE,
        WEBKIT_SNAPSHOT_OPTIONS_NONE,
        NULL,
        on_snapshot_finished,
        NULL
    );
}

/* -------------------------------------------------------------
   CREATE MAIN WINDOW
   ------------------------------------------------------------- */
GtkWidget* create_main_window(void) {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);

    // Header bar
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "2004scape Client");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    // 1) Toggle Sidebar
    GtkWidget *toggle_btn = gtk_button_new_with_label("Toggle Sidebar");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), toggle_btn);

    // 2) Discord
    GtkWidget *discord_btn = gtk_button_new();
    // If you have "discord.png" in the project folder
    GtkWidget *discord_img = gtk_image_new_from_file("discord.png");
    gtk_button_set_image(GTK_BUTTON(discord_btn), discord_img);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), discord_btn);

    // 3) Screenshot
    GtkWidget *screenshot_btn = gtk_button_new_with_label("Screenshot");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), screenshot_btn);

    // Paned layout
    GtkWidget *hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add(GTK_CONTAINER(window), hpaned);

    // Left side: game client
    g_game_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_widget_set_size_request(GTK_WIDGET(g_game_view), 600, -1);
    webkit_web_view_load_uri(g_game_view, "https://2004.lostcity.rs/client?world=1&detail=high&method=0");
    gtk_paned_pack1(GTK_PANED(hpaned), GTK_WIDGET(g_game_view), TRUE, TRUE);

    // Right side: notebook
    GtkWidget *sidebar_notebook = create_sidebar_notebook();
    gtk_widget_set_size_request(sidebar_notebook, 300, -1);
    gtk_paned_pack2(GTK_PANED(hpaned), sidebar_notebook, FALSE, TRUE);

    // Signals
    g_signal_connect(toggle_btn, "clicked", G_CALLBACK(toggle_sidebar_cb), sidebar_notebook);
    g_signal_connect(discord_btn, "clicked", G_CALLBACK(on_discord_clicked), window);
    g_signal_connect(screenshot_btn, "clicked", G_CALLBACK(on_screenshot_clicked), g_game_view);

    // Exit
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    return window;
}
