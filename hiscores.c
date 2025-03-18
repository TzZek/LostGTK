#include "hiscores.h"
#include "mapview.h"
#include <webkit2/webkit2.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <gtk/gtk.h>

/* Helper: Convert hiscores category type to a readable name. */
static const char *get_category_name(int type) {
    switch (type) {
        case 0:  return "Overall";
        case 1:  return "Attack";
        case 2:  return "Defence";
        case 3:  return "Strength";
        case 4:  return "Hitpoints";
        case 5:  return "Ranged";
        case 6:  return "Prayer";
        case 7:  return "Magic";
        case 8:  return "Cooking";
        case 9:  return "Woodcutting";
        case 10: return "Fletching";
        case 11: return "Fishing";
        case 12: return "Firemaking";
        case 13: return "Crafting";
        case 14: return "Smithing";
        case 15: return "Mining";
        case 16: return "Herblore";
        case 17: return "Agility";
        case 18: return "Thieving";
        case 21: return "Runecrafting";
        default: return "Unknown";
    }
}

/* A global pointer for the hiscores text view (where results are displayed). */
static GtkWidget *result_view = NULL;

/* Callback: libsoup finished our hiscores HTTP request. */
static void on_request_finished(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    SoupSession *session = SOUP_SESSION(source_object);
    GError *error = NULL;

    GBytes *bytes = soup_session_send_and_read_finish(session, res, &error);
    if (error) {
        g_print("HTTP error: %s\n", error->message);
        g_error_free(error);
        g_object_unref(session);
        return;
    }

    gsize size;
    const gchar *data = g_bytes_get_data(bytes, &size);

    JsonParser *parser = json_parser_new();
    json_parser_load_from_data(parser, data, size, &error);
    if (error) {
        g_print("JSON parse error: %s\n", error->message);
        g_error_free(error);
        g_object_unref(parser);
        g_object_unref(session);
        g_bytes_unref(bytes);
        return;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_ARRAY(root)) {
        g_print("Unexpected JSON format.\n");
    } else {
        JsonArray *array = json_node_get_array(root);
        guint n = json_array_get_length(array);
        GString *result_string = g_string_new("");

        for (guint i = 0; i < n; i++) {
            JsonObject *obj = json_array_get_object_element(array, i);
            gint type = json_object_get_int_member(obj, "type");
            gint level = json_object_get_int_member(obj, "level");
            gint value = json_object_get_int_member(obj, "value"); // xp * 10
            gint xp = value / 10;  // Divide by 10
            gint rank = json_object_get_int_member(obj, "rank");

            g_string_append_printf(
                result_string,
                "%s, Lvl %d, Xp: %d, Rank: %d\n",
                get_category_name(type), level, xp, rank
            );
        }

        /* Show the results in the text view. */
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(result_view));
        gtk_text_buffer_set_text(buffer, result_string->str, -1);
        g_string_free(result_string, TRUE);
    }

    g_object_unref(parser);
    g_object_unref(session);
    g_bytes_unref(bytes);
}

/* Callback: user clicked "Search" in the Hiscores tab. */
static void on_search_clicked(GtkButton *button, gpointer user_data) {
    GtkEntry *entry = GTK_ENTRY(user_data);
    const gchar *username = gtk_entry_get_text(entry);
    if (!username || username[0] == '\0') {
        g_print("Please enter a username.\n");
        return;
    }

    gchar *url = g_strdup_printf("https://2004.lostcity.rs/api/hiscores/player/%s", username);

    SoupSession *session = soup_session_new();
    SoupMessage *msg = soup_message_new("GET", url);

    /* libsoup 3 call to run asynchronously. */
    soup_session_send_and_read_async(session,
                                     msg,
                                     G_PRIORITY_DEFAULT,
                                     NULL,
                                     on_request_finished,
                                     NULL);

    g_free(url);
}

/* This function builds a notebook with 2 tabs:
   1) Hiscores (lookup UI + text view),
   2) World Map (from create_worldmap_tab()). */
GtkWidget* create_sidebar_notebook(void) {
    GtkWidget *notebook = gtk_notebook_new();

    /* == Hiscores Tab == */
    GtkWidget *hiscores_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    GtkWidget *lookup_label = gtk_label_new("Hiscores Lookup");
    gtk_box_pack_start(GTK_BOX(hiscores_box), lookup_label, FALSE, FALSE, 5);

    GtkWidget *username_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(username_entry), "Enter username");
    gtk_box_pack_start(GTK_BOX(hiscores_box), username_entry, FALSE, FALSE, 5);

    GtkWidget *search_btn = gtk_button_new_with_label("Search");
    g_signal_connect(search_btn, "clicked", G_CALLBACK(on_search_clicked), username_entry);
    gtk_box_pack_start(GTK_BOX(hiscores_box), search_btn, FALSE, FALSE, 5);

    /* A text view to display the results. */
    result_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(result_view), FALSE);

    GtkWidget *result_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(result_scrolled), result_view);

    gtk_box_pack_start(GTK_BOX(hiscores_box), result_scrolled, TRUE, TRUE, 5);

    /* Add the Hiscores box as the first tab */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             hiscores_box,
                             gtk_label_new("Hiscores"));

    /* == World Map Tab == */
    GtkWidget *worldmap_tab = create_worldmap_tab();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             worldmap_tab,
                             gtk_label_new("World Map"));

    return notebook;
}

/* Callback: toggles the entire sidebar's visibility. */
void toggle_sidebar_cb(GtkButton *button, gpointer user_data) {
    GtkWidget *sidebar = GTK_WIDGET(user_data);
    gboolean visible = gtk_widget_get_visible(sidebar);
    gtk_widget_set_visible(sidebar, !visible);
}
