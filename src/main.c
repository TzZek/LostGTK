#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <json-glib/json-glib.h>
#include <glib/gstdio.h>

#define GAME_URL "https://w2-2004.lostcity.rs/rs2.cgi?plugin=0&world=2&lowmem=0"

typedef struct {
    char *user;
    char *pass;
} Account;

typedef enum {
    BAR_TOP      = 0,
    BAR_FLOATING = 1,
    BAR_BOTTOM   = 2,
} BarPos;

static GPtrArray     *accounts;
static WebKitWebView *webview;
static GtkWindow     *main_window;
static GtkLabel      *afk_label;
static GtkPopover    *acct_popover;
static GtkBox        *acct_list;
static GtkWidget     *bar_widget;
static BarPos         bar_pos = BAR_FLOATING;
static guint          afk_seconds = 0;

static void rebuild_account_list(void);
static void apply_bar_pos(void);
static void on_bar_pos_changed(GtkDropDown *dd, GParamSpec *spec, gpointer u);
static gboolean suppress_context_menu(WebKitWebView *view, WebKitContextMenu *menu,
                                      WebKitHitTestResult *hit, gpointer u);

static void free_account(gpointer p) {
    Account *a = p;
    g_free(a->user);
    g_free(a->pass);
    g_free(a);
}

static char *config_path(void) {
    return g_build_filename(g_get_user_config_dir(), "lostgtk", "accounts.json", NULL);
}

static char *settings_path(void) {
    return g_build_filename(g_get_user_config_dir(), "lostgtk", "settings.json", NULL);
}

static void settings_load(void) {
    char *path = settings_path();
    JsonParser *parser = json_parser_new();
    if (json_parser_load_from_file(parser, path, NULL)) {
        JsonNode *root = json_parser_get_root(parser);
        if (root && JSON_NODE_HOLDS_OBJECT(root)) {
            JsonObject *o = json_node_get_object(root);
            if (json_object_has_member(o, "bar_pos")) {
                gint64 v = json_object_get_int_member(o, "bar_pos");
                if (v >= 0 && v <= 2) bar_pos = (BarPos) v;
            }
        }
    }
    g_object_unref(parser);
    g_free(path);
}

static void settings_save(void) {
    JsonBuilder *b = json_builder_new();
    json_builder_begin_object(b);
    json_builder_set_member_name(b, "bar_pos");
    json_builder_add_int_value(b, (gint64) bar_pos);
    json_builder_end_object(b);

    JsonGenerator *gen = json_generator_new();
    json_generator_set_pretty(gen, TRUE);
    json_generator_set_root(gen, json_builder_get_root(b));

    char *path = settings_path();
    char *dir  = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0700);
    json_generator_to_file(gen, path, NULL);

    g_free(dir);
    g_free(path);
    g_object_unref(gen);
    g_object_unref(b);
}

static void accounts_load(void) {
    accounts = g_ptr_array_new_with_free_func(free_account);
    char *path = config_path();
    JsonParser *parser = json_parser_new();
    if (json_parser_load_from_file(parser, path, NULL)) {
        JsonNode *root = json_parser_get_root(parser);
        if (root && JSON_NODE_HOLDS_ARRAY(root)) {
            JsonArray *arr = json_node_get_array(root);
            for (guint i = 0; i < json_array_get_length(arr); i++) {
                JsonObject *o = json_array_get_object_element(arr, i);
                if (!o) continue;
                const char *u = json_object_has_member(o, "user")
                    ? json_object_get_string_member(o, "user") : NULL;
                const char *p = json_object_has_member(o, "pass")
                    ? json_object_get_string_member(o, "pass") : NULL;
                if (!u || !*u) continue;
                Account *a = g_new0(Account, 1);
                a->user = g_strdup(u);
                a->pass = g_strdup(p ? p : "");
                g_ptr_array_add(accounts, a);
            }
        }
    }
    g_object_unref(parser);
    g_free(path);
}

static void accounts_save(void) {
    JsonBuilder *b = json_builder_new();
    json_builder_begin_array(b);
    for (guint i = 0; i < accounts->len; i++) {
        Account *a = g_ptr_array_index(accounts, i);
        json_builder_begin_object(b);
        json_builder_set_member_name(b, "user");
        json_builder_add_string_value(b, a->user);
        json_builder_set_member_name(b, "pass");
        json_builder_add_string_value(b, a->pass);
        json_builder_end_object(b);
    }
    json_builder_end_array(b);

    JsonGenerator *gen = json_generator_new();
    json_generator_set_pretty(gen, TRUE);
    json_generator_set_root(gen, json_builder_get_root(b));

    char *path = config_path();
    char *dir  = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0700);
    json_generator_to_file(gen, path, NULL);
    g_chmod(path, 0600);

    g_free(dir);
    g_free(path);
    g_object_unref(gen);
    g_object_unref(b);
}

static void inject_login(const char *user, const char *pass) {
    char *u = g_strescape(user, "");
    char *p = g_strescape(pass, "");
    char *js = g_strdup_printf(
        "(function(){"
        "var cs=document.querySelectorAll('canvas'),c=null,best=0;"
        "for(var i=0;i<cs.length;i++){var a=cs[i].width*cs[i].height;if(a>best){best=a;c=cs[i];}}"
        "if(!c)return;"
        "var r=c.getBoundingClientRect();"
        "function send(k){"
        "c.dispatchEvent(new KeyboardEvent('keydown',{key:k,code:k,bubbles:true}));"
        "c.dispatchEvent(new KeyboardEvent('keyup',{key:k,code:k,bubbles:true}));}"
        "function click(gx,gy){"
        "var cx=r.left+gx*r.width/789,cy=r.top+gy*r.height/532;"
        "c.dispatchEvent(new MouseEvent('mousedown',{clientX:cx,clientY:cy,button:0,buttons:1,bubbles:true}));"
        "c.dispatchEvent(new MouseEvent('mouseup',{clientX:cx,clientY:cy,button:0,buttons:0,bubbles:true}));}"
        "click(474,306);"
        "setTimeout(function(){"
        "var u=\"%s\",p=\"%s\";"
        "for(var i=0;i<u.length;i++)send(u[i]);"
        "send('Tab');"
        "for(var i=0;i<p.length;i++)send(p[i]);"
        "send('Enter');"
        "},300);"
        "})();",
        u, p);
    webkit_web_view_evaluate_javascript(webview, js, -1, NULL, NULL, NULL, NULL, NULL);
    g_free(js);
    g_free(u);
    g_free(p);
}

static void on_account_clicked(GtkButton *btn, gpointer user_data) {
    Account *a = user_data;
    gtk_popover_popdown(acct_popover);
    inject_login(a->user, a->pass);
    gtk_widget_grab_focus(GTK_WIDGET(webview));
}

static void on_remove_account(GtkButton *btn, gpointer user_data) {
    Account *a = user_data;
    g_ptr_array_remove(accounts, a);
    accounts_save();
    rebuild_account_list();
}

static void on_save_account(GtkButton *btn, gpointer user_data) {
    GtkWindow *dlg = user_data;
    GtkEntry *ue = g_object_get_data(G_OBJECT(dlg), "user_entry");
    GtkEntry *pe = g_object_get_data(G_OBJECT(dlg), "pass_entry");
    const char *u = gtk_editable_get_text(GTK_EDITABLE(ue));
    const char *p = gtk_editable_get_text(GTK_EDITABLE(pe));
    if (u && *u) {
        Account *a = g_new0(Account, 1);
        a->user = g_strdup(u);
        a->pass = g_strdup(p ? p : "");
        g_ptr_array_add(accounts, a);
        accounts_save();
        rebuild_account_list();
    }
    gtk_window_destroy(dlg);
}

static void open_add_dialog(GtkButton *btn, gpointer user_data) {
    gtk_popover_popdown(acct_popover);

    GtkWidget *dlg = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dlg), "Add account");
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dlg), main_window);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 320, -1);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);

    GtkWidget *ue = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ue), "Username");
    GtkWidget *pe = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(pe), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(pe), FALSE);

    GtkWidget *save = gtk_button_new_with_label("Save");
    gtk_widget_add_css_class(save, "suggested-action");

    g_object_set_data(G_OBJECT(dlg), "user_entry", ue);
    g_object_set_data(G_OBJECT(dlg), "pass_entry", pe);
    g_signal_connect(save, "clicked", G_CALLBACK(on_save_account), dlg);
    g_signal_connect_swapped(pe, "activate", G_CALLBACK(gtk_widget_activate), save);

    gtk_box_append(GTK_BOX(box), ue);
    gtk_box_append(GTK_BOX(box), pe);
    gtk_box_append(GTK_BOX(box), save);
    gtk_window_set_child(GTK_WINDOW(dlg), box);
    gtk_window_present(GTK_WINDOW(dlg));
    gtk_widget_grab_focus(ue);
}

static void rebuild_account_list(void) {
    GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(acct_list));
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(acct_list, child);
        child = next;
    }

    if (accounts->len == 0) {
        GtkWidget *l = gtk_label_new("No accounts saved");
        gtk_widget_add_css_class(l, "dim-label");
        gtk_widget_set_margin_top(l, 6);
        gtk_widget_set_margin_bottom(l, 6);
        gtk_box_append(acct_list, l);
    } else {
        for (guint i = 0; i < accounts->len; i++) {
            Account *a = g_ptr_array_index(accounts, i);
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
            GtkWidget *btn = gtk_button_new_with_label(a->user);
            gtk_widget_set_hexpand(btn, TRUE);
            gtk_button_set_has_frame(GTK_BUTTON(btn), FALSE);
            g_signal_connect(btn, "clicked", G_CALLBACK(on_account_clicked), a);

            GtkWidget *del = gtk_button_new_from_icon_name("edit-delete-symbolic");
            gtk_button_set_has_frame(GTK_BUTTON(del), FALSE);
            gtk_widget_set_tooltip_text(del, "Remove");
            g_signal_connect(del, "clicked", G_CALLBACK(on_remove_account), a);

            gtk_box_append(GTK_BOX(row), btn);
            gtk_box_append(GTK_BOX(row), del);
            gtk_box_append(acct_list, row);
        }
    }

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(sep, 4);
    gtk_widget_set_margin_bottom(sep, 4);
    gtk_box_append(acct_list, sep);

    GtkWidget *add = gtk_button_new_with_label("Add account...");
    gtk_button_set_has_frame(GTK_BUTTON(add), FALSE);
    g_signal_connect(add, "clicked", G_CALLBACK(open_add_dialog), NULL);
    gtk_box_append(acct_list, add);

    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(sep2, 4);
    gtk_widget_set_margin_bottom(sep2, 4);
    gtk_box_append(acct_list, sep2);

    GtkWidget *lrow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(lrow, 4);
    gtk_widget_set_margin_end(lrow, 4);
    gtk_widget_set_margin_top(lrow, 2);
    gtk_widget_set_margin_bottom(lrow, 2);

    GtkWidget *llbl = gtk_label_new("Layout");
    gtk_widget_add_css_class(llbl, "dim-label");
    gtk_box_append(GTK_BOX(lrow), llbl);

    static const char * const pos_labels[] = {"Top bar", "Floating", "Bottom", NULL};
    GtkStringList *sl = gtk_string_list_new(pos_labels);
    GtkWidget *dd = gtk_drop_down_new(G_LIST_MODEL(sl), NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), (guint) bar_pos);
    g_signal_connect(dd, "notify::selected", G_CALLBACK(on_bar_pos_changed), NULL);
    gtk_widget_set_hexpand(dd, TRUE);
    gtk_box_append(GTK_BOX(lrow), dd);

    gtk_box_append(acct_list, lrow);
}

static void apply_bar_pos(void) {
    if (!bar_widget) return;
    if (webview) gtk_widget_set_margin_top(GTK_WIDGET(webview), 0);
    gtk_widget_remove_css_class(bar_widget, "lg-bar-slim");
    switch (bar_pos) {
    case BAR_TOP:
        gtk_widget_set_hexpand(bar_widget, TRUE);
        gtk_widget_set_halign(bar_widget, GTK_ALIGN_FILL);
        gtk_widget_set_valign(bar_widget, GTK_ALIGN_START);
        gtk_widget_set_margin_top(bar_widget, 0);
        gtk_widget_set_margin_bottom(bar_widget, 0);
        gtk_widget_set_margin_start(bar_widget, 0);
        gtk_widget_set_margin_end(bar_widget, 0);
        gtk_widget_add_css_class(bar_widget, "lg-bar-slim");
        if (webview) gtk_widget_set_margin_top(GTK_WIDGET(webview), 22);
        break;
    case BAR_FLOATING:
        gtk_widget_set_hexpand(bar_widget, FALSE);
        gtk_widget_set_halign(bar_widget, GTK_ALIGN_END);
        gtk_widget_set_valign(bar_widget, GTK_ALIGN_START);
        gtk_widget_set_margin_top(bar_widget, 4);
        gtk_widget_set_margin_bottom(bar_widget, 0);
        gtk_widget_set_margin_start(bar_widget, 0);
        gtk_widget_set_margin_end(bar_widget, 4);
        break;
    case BAR_BOTTOM:
        gtk_widget_set_hexpand(bar_widget, FALSE);
        gtk_widget_set_halign(bar_widget, GTK_ALIGN_END);
        gtk_widget_set_valign(bar_widget, GTK_ALIGN_END);
        gtk_widget_set_margin_top(bar_widget, 0);
        gtk_widget_set_margin_bottom(bar_widget, 4);
        gtk_widget_set_margin_start(bar_widget, 0);
        gtk_widget_set_margin_end(bar_widget, 4);
        break;
    }
}

static void on_bar_pos_changed(GtkDropDown *dd, GParamSpec *spec, gpointer u) {
    guint sel = gtk_drop_down_get_selected(dd);
    if (sel > 2) return;
    bar_pos = (BarPos) sel;
    apply_bar_pos();
    settings_save();
}

static gboolean suppress_context_menu(WebKitWebView *view, WebKitContextMenu *menu,
                                      WebKitHitTestResult *hit, gpointer u) {
    return TRUE;
}

static gboolean afk_tick(gpointer user_data) {
    afk_seconds++;
    char buf[32];
    g_snprintf(buf, sizeof buf, "%02u:%02u", afk_seconds / 60, afk_seconds % 60);
    gtk_label_set_text(afk_label, buf);
    return G_SOURCE_CONTINUE;
}

static gboolean on_event(GtkEventControllerLegacy *ctrl, GdkEvent *event, gpointer user_data) {
    GdkEventType t = gdk_event_get_event_type(event);
    if (t == GDK_KEY_PRESS || t == GDK_BUTTON_PRESS) afk_seconds = 0;
    return GDK_EVENT_PROPAGATE;
}

static void load_css(void) {
    static const char *css =
        ".lg-bar { background-color: rgba(0,0,0,0.55); border-radius: 6px;"
        " padding: 1px 4px; }"
        ".lg-bar.lg-bar-slim { background-color: rgba(0,0,0,0.85);"
        " border-radius: 0; padding: 1px 6px; }"
        ".lg-bar label { color: #ddd; font-family: monospace; font-size: 11px; }"
        ".lg-bar button { color: #ddd; min-height: 18px; min-width: 18px;"
        " padding: 1px 4px; background: transparent; border: none; box-shadow: none; }"
        ".lg-bar button:hover { background: rgba(255,255,255,0.15); }";
    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_string(p, css);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(p),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(p);
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    main_window = GTK_WINDOW(gtk_application_window_new(app));
    gtk_window_set_title(main_window, "lostgtk");
    gtk_window_set_default_size(main_window, 1024, 768);
    gtk_window_set_decorated(main_window, FALSE);

    load_css();

    GtkWidget *overlay = gtk_overlay_new();
    gtk_window_set_child(main_window, overlay);

    WebKitUserContentManager *ucm = webkit_user_content_manager_new();
    static const char *visibility_script =
        "Object.defineProperty(document,'hidden',"
        "  {get:function(){return false;},configurable:true});"
        "Object.defineProperty(document,'visibilityState',"
        "  {get:function(){return 'visible';},configurable:true});"
        "Object.defineProperty(document,'webkitHidden',"
        "  {get:function(){return false;},configurable:true});"
        "Object.defineProperty(document,'webkitVisibilityState',"
        "  {get:function(){return 'visible';},configurable:true});"
        "document.addEventListener('visibilitychange',"
        "  function(e){e.stopImmediatePropagation();},true);";
    WebKitUserScript *us1 = webkit_user_script_new(
        visibility_script,
        WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
        NULL, NULL);
    webkit_user_content_manager_add_script(ucm, us1);
    webkit_user_script_unref(us1);

    static const char *autofit_script =
        "(function(){"
        "function fit(){"
        " var c=document.getElementById('canvas'); if(!c) return;"
        " var sel=document.getElementById('size');"
        " if(sel && sel.value!=='auto') return;"
        " var ratio=765/503;"
        " var availH=Math.max(100, window.innerHeight-26);"
        " var availW=Math.max(100, window.innerWidth);"
        " var w=Math.min(availH*ratio, availW);"
        " c.style.width=w+'px';"
        " c.style.height=(w/ratio)+'px';"
        " c.style.maxWidth='none';"
        "}"
        "window.addEventListener('resize',fit);"
        "var n=0,iv=setInterval(function(){fit();"
        " if(++n>50) clearInterval(iv);},200);"
        "})();";
    WebKitUserScript *us2 = webkit_user_script_new(
        autofit_script,
        WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
        NULL, NULL);
    webkit_user_content_manager_add_script(ucm, us2);
    webkit_user_script_unref(us2);

    webview = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "user-content-manager", ucm, NULL));
    g_object_unref(ucm);
    g_signal_connect(webview, "context-menu",
        G_CALLBACK(suppress_context_menu), NULL);
    gtk_widget_set_vexpand(GTK_WIDGET(webview), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(webview), TRUE);
    webkit_web_view_load_uri(webview, GAME_URL);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), GTK_WIDGET(webview));

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(bar, "lg-bar");
    bar_widget = bar;

    afk_label = GTK_LABEL(gtk_label_new("00:00"));
    gtk_box_append(GTK_BOX(bar), GTK_WIDGET(afk_label));

    GtkWidget *spacer = gtk_label_new("");
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(bar), spacer);

    GtkWidget *acct_btn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(acct_btn), "avatar-default-symbolic");
    gtk_widget_set_tooltip_text(acct_btn, "Accounts");

    acct_popover = GTK_POPOVER(gtk_popover_new());
    acct_list = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_size_request(GTK_WIDGET(acct_list), 220, -1);
    gtk_popover_set_child(acct_popover, GTK_WIDGET(acct_list));
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(acct_btn), GTK_WIDGET(acct_popover));
    gtk_box_append(GTK_BOX(bar), acct_btn);

    GtkWidget *close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_set_tooltip_text(close_btn, "Close");
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_window_close), main_window);
    gtk_box_append(GTK_BOX(bar), close_btn);

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), bar);
    apply_bar_pos();

    rebuild_account_list();

    GtkEventController *ec = gtk_event_controller_legacy_new();
    gtk_event_controller_set_propagation_phase(ec, GTK_PHASE_CAPTURE);
    g_signal_connect(ec, "event", G_CALLBACK(on_event), NULL);
    gtk_widget_add_controller(GTK_WIDGET(main_window), ec);

    g_timeout_add_seconds(1, afk_tick, NULL);
    gtk_window_present(main_window);
}

int main(int argc, char **argv) {
    settings_load();
    accounts_load();
    GtkApplication *app = gtk_application_new("rs.lostcity.lostgtk", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    g_ptr_array_free(accounts, TRUE);
    return status;
}
