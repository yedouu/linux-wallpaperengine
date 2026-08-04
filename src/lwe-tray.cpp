#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>

static std::string sendCommand (const std::string& json) {
    int fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return "";
    struct sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    strncpy (addr.sun_path, "/tmp/lwe-control.sock", sizeof (addr.sun_path) - 1);
    if (connect (fd, (struct sockaddr*) &addr, sizeof (addr)) < 0) {
        close (fd); return "";
    }
    write (fd, json.c_str (), json.size ());
    close (fd);
    return "";
}

// --- Wallpaper list model ---
enum { COL_PATH, COL_TITLE, COL_TYPE, COL_COUNT };
static GtkListStore* listStore = nullptr;
static GtkWidget* cycleCheck = nullptr;

static void loadWallpaperList () {
    gtk_list_store_clear (listStore);

    // Scan workshop
    const char* home = getenv ("HOME");
    if (!home) return;
    std::string wsRoot = std::string (home) + "/.local/share/Steam/steamapps/workshop/content/431960";
    if (!std::filesystem::is_directory (wsRoot)) return;

    for (const auto& entry : std::filesystem::directory_iterator (wsRoot)) {
        if (!entry.is_directory ()) continue;
        auto projectJson = entry.path () / "project.json";
        if (!std::filesystem::exists (projectJson)) continue;

        std::string type = "scene", title = entry.path ().filename ().string ();
        std::ifstream f (projectJson);
        if (f.good ()) {
            std::string content ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char> ());
            auto extract = [&](const std::string& key) {
                auto pos = content.find ("\"" + key + "\":");
                if (pos == std::string::npos) return std::string ();
                pos = content.find ('"', pos + key.length () + 3);
                if (pos == std::string::npos) return std::string ();
                auto end = content.find ('"', pos + 1);
                if (end == std::string::npos) return std::string ();
                return content.substr (pos + 1, end - pos - 1);
            };
            type = extract ("type");
            if (type.empty ()) type = "scene";
            std::string t = extract ("title");
            if (!t.empty ()) title = t;
        }

        GtkTreeIter iter;
        gtk_list_store_append (listStore, &iter);
        gtk_list_store_set (listStore, &iter,
            COL_PATH, entry.path ().c_str (),
            COL_TITLE, title.c_str (),
            COL_TYPE, type.c_str (), -1);
    }
}

// --- Callbacks ---
static void onNext (GtkWidget*, gpointer)    { sendCommand ("{\"cmd\":\"next\"}"); }
static void onPrev (GtkWidget*, gpointer)    { sendCommand ("{\"cmd\":\"prev\"}"); }
static void onCycleToggled (GtkWidget* w, gpointer) {
    bool active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));
    sendCommand (std::string ("{\"cmd\":\"cycle\",\"enabled\":\"") + (active ? "1" : "0") + "\"}");
}
static void onWallpaperSelected (GtkTreeView* view, GtkTreePath*, GtkTreeViewColumn*, gpointer) {
    GtkTreeSelection* sel = gtk_tree_view_get_selection (view);
    GtkTreeIter iter;
    GtkTreeModel* model;
    if (gtk_tree_selection_get_selected (sel, &model, &iter)) {
        gchar* path = nullptr;
        gtk_tree_model_get (model, &iter, COL_PATH, &path, -1);
        if (path) {
            sendCommand (std::string ("{\"cmd\":\"set\",\"path\":\"") + path + "\"}");
            g_free (path);
        }
    }
}

// --- Popup window ---
static GtkWidget* popup = nullptr;
static void showPopup (GtkWidget* anchor) {
    if (!popup) {
        popup = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title (GTK_WINDOW (popup), "Wallpaper Engine");
        gtk_window_set_decorated (GTK_WINDOW (popup), FALSE);
        gtk_window_set_skip_taskbar_hint (GTK_WINDOW (popup), TRUE);
        gtk_window_set_keep_above (GTK_WINDOW (popup), TRUE);
        gtk_container_set_border_width (GTK_CONTAINER (popup), 8);

        GtkWidget* vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
        gtk_container_add (GTK_CONTAINER (popup), vbox);

        // --- Button bar ---
        GtkWidget* hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

        GtkWidget* prevBtn = gtk_button_new_with_label ("◀ 上一张");
        gtk_box_pack_start (GTK_BOX (hbox), prevBtn, TRUE, TRUE, 0);
        g_signal_connect (prevBtn, "clicked", G_CALLBACK (onPrev), nullptr);

        GtkWidget* nextBtn = gtk_button_new_with_label ("下一张 ▶");
        gtk_box_pack_start (GTK_BOX (hbox), nextBtn, TRUE, TRUE, 0);
        g_signal_connect (nextBtn, "clicked", G_CALLBACK (onNext), nullptr);

        // --- Cycle toggle ---
        cycleCheck = gtk_check_button_new_with_label ("循环播放");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cycleCheck), TRUE);
        gtk_box_pack_start (GTK_BOX (vbox), cycleCheck, FALSE, FALSE, 0);
        g_signal_connect (cycleCheck, "toggled", G_CALLBACK (onCycleToggled), nullptr);

        // --- Separator ---
        GtkWidget* sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start (GTK_BOX (vbox), sep, FALSE, FALSE, 4);

        // --- Wallpaper list ---
        listStore = gtk_list_store_new (COL_COUNT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
        GtkWidget* tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (listStore));
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree), FALSE);

        GtkCellRenderer* renderer = gtk_cell_renderer_text_new ();
        GtkTreeViewColumn* col = gtk_tree_view_column_new_with_attributes ("Title", renderer, "text", COL_TITLE, nullptr);
        gtk_tree_view_append_column (GTK_TREE_VIEW (tree), col);

        GtkWidget* scroll = gtk_scrolled_window_new (nullptr, nullptr);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request (scroll, 300, 250);
        gtk_container_add (GTK_CONTAINER (scroll), tree);
        gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);

        g_signal_connect (tree, "row-activated", G_CALLBACK (onWallpaperSelected), nullptr);

        gtk_widget_show_all (popup);

        // Load wallpapers
        loadWallpaperList ();
    }

    if (gtk_widget_get_visible (popup)) {
        gtk_widget_hide (popup);
    } else {
        // Position near system tray (bottom-right corner)
        GdkScreen* screen = gdk_screen_get_default ();
        int sw = gdk_screen_get_width (screen);
        int sh = gdk_screen_get_height (screen);
        gtk_window_move (GTK_WINDOW (popup), sw - 320, sh - 420);
        gtk_widget_show (popup);
        gtk_window_present (GTK_WINDOW (popup));
    }
}

// --- Main ---
int main (int argc, char** argv) {
    gtk_init (&argc, &argv);

    // AppIndicator tray icon
    AppIndicator* indicator = app_indicator_new (
        "lwe-tray", "applications-multimedia",
        APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status (indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_title (indicator, "Wallpaper Engine");

    // Simple menu
    GtkWidget* menu = gtk_menu_new ();
    GtkWidget* item = gtk_menu_item_new_with_label ("控制面板");
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect (item, "activate", G_CALLBACK (showPopup), nullptr);
    gtk_widget_show (item);

    GtkWidget* quitItem = gtk_menu_item_new_with_label ("退出");
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), quitItem);
    g_signal_connect (quitItem, "activate", G_CALLBACK (gtk_main_quit), nullptr);
    gtk_widget_show (quitItem);

    app_indicator_set_menu (indicator, GTK_MENU (menu));

    gtk_main ();
    return 0;
}
