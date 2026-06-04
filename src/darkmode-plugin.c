/*
 * Dark Mode Switcher - Xfce panel plugin
 *
 * Switches between user-selected light/dark GTK/Xfce themes and publishes a
 * color-scheme preference for newer applications that follow GNOME/portal
 * settings (Chrome/Chromium, Firefox, GTK4/libadwaita apps, etc.).
 */

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#ifndef DARKMODE_STANDALONE
#include <libxfce4panel/libxfce4panel.h>
#endif

#define APP_NAME "darkmode-switcher"
#define CONFIG_GROUP "DarkModeSwitcher"

typedef struct
{
#ifndef DARKMODE_STANDALONE
    XfcePanelPlugin *plugin;
#endif
    GtkWidget *button;
    GtkWidget *image;
    GtkWidget *label;
    gboolean dark;
    gboolean set_xfwm;
    gboolean set_modern_apps;
    gchar *light_theme;
    gchar *dark_theme;
    gchar *last_error;
} DarkModePlugin;

static void darkmode_apply(DarkModePlugin *dm, gboolean dark);
static void darkmode_show_configure(DarkModePlugin *dm, GtkWindow *parent);
static void darkmode_save_config(DarkModePlugin *dm);
static gchar *detect_current_gtk_theme(void);

static void
set_error(DarkModePlugin *dm, const gchar *format, ...)
{
    va_list args;
    g_free(dm->last_error);
    va_start(args, format);
    dm->last_error = g_strdup_vprintf(format, args);
    va_end(args);
}

static gboolean
program_exists(const gchar *name)
{
    gchar *path = g_find_program_in_path(name);
    gboolean ok = path != NULL;
    g_free(path);
    return ok;
}

static gboolean
spawn_argv(char **argv, gboolean required, gchar **error_out)
{
    gchar *stderr_text = NULL;
    gint status = 0;
    GError *error = NULL;

    if (!g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                      NULL, &stderr_text, &status, &error))
    {
        if (required && error_out)
            *error_out = g_strdup(error ? error->message : "failed to spawn command");
        g_clear_error(&error);
        g_free(stderr_text);
        return FALSE;
    }

    if (!g_spawn_check_wait_status(status, &error))
    {
        if (required && error_out)
        {
            if (stderr_text && *stderr_text)
                *error_out = g_strdup(stderr_text);
            else
                *error_out = g_strdup(error ? error->message : "command failed");
        }
        g_clear_error(&error);
        g_free(stderr_text);
        return FALSE;
    }

    g_free(stderr_text);
    return TRUE;
}

static gboolean
xfconf_set_string(const gchar *channel, const gchar *property, const gchar *value, gchar **error_out)
{
    if (!program_exists("xfconf-query"))
    {
        if (error_out)
            *error_out = g_strdup("xfconf-query was not found");
        return FALSE;
    }

    char *argv1[] = { "xfconf-query", "-c", (char *)channel, "-p", (char *)property, "-s", (char *)value, NULL };
    if (spawn_argv(argv1, FALSE, NULL))
        return TRUE;

    char *argv2[] = { "xfconf-query", "-c", (char *)channel, "-p", (char *)property,
                      "-n", "-t", "string", "-s", (char *)value, NULL };
    return spawn_argv(argv2, TRUE, error_out);
}

static gboolean
xfconf_set_bool(const gchar *channel, const gchar *property, gboolean value, gchar **error_out)
{
    if (!program_exists("xfconf-query"))
    {
        if (error_out)
            *error_out = g_strdup("xfconf-query was not found");
        return FALSE;
    }

    const gchar *value_text = value ? "true" : "false";
    char *argv1[] = { "xfconf-query", "-c", (char *)channel, "-p", (char *)property, "-s", (char *)value_text, NULL };
    if (spawn_argv(argv1, FALSE, NULL))
        return TRUE;

    char *argv2[] = { "xfconf-query", "-c", (char *)channel, "-p", (char *)property,
                      "-n", "-t", "bool", "-s", (char *)value_text, NULL };
    return spawn_argv(argv2, TRUE, error_out);
}

static void
gsettings_set_literal(const gchar *schema, const gchar *key, const gchar *value_literal)
{
    if (!program_exists("gsettings"))
        return;

    char *argv[] = { "gsettings", "set", (char *)schema, (char *)key, (char *)value_literal, NULL };
    spawn_argv(argv, FALSE, NULL);
}

static void
gsettings_set_string(const gchar *schema, const gchar *key, const gchar *value)
{
    if (!program_exists("gsettings"))
        return;

    GString *quoted = g_string_new("'");
    for (const gchar *p = value ? value : ""; *p; p++)
    {
        if (*p == '\\' || *p == '\'')
            g_string_append_c(quoted, '\\');
        g_string_append_c(quoted, *p);
    }
    g_string_append_c(quoted, '\'');
    char *argv[] = { "gsettings", "set", (char *)schema, (char *)key, quoted->str, NULL };
    spawn_argv(argv, FALSE, NULL);
    g_string_free(quoted, TRUE);
}

static void
reload_xfce_panel(void)
{
#ifndef DARKMODE_STANDALONE
    if (program_exists("xfce4-panel"))
        g_spawn_command_line_async("xfce4-panel -r", NULL);
#endif
}

static void
ensure_xfsettingsd(void)
{
#ifndef DARKMODE_STANDALONE
    if (program_exists("pgrep") && program_exists("xfsettingsd"))
    {
        char *argv[] = { "pgrep", "-x", "xfsettingsd", NULL };
        if (!spawn_argv(argv, FALSE, NULL))
            g_spawn_command_line_async("xfsettingsd --replace", NULL);
    }
#endif
}


static gchar *
find_existing_file(const gchar *first, const gchar *second, const gchar *fallback)
{
    if (first && g_file_test(first, G_FILE_TEST_EXISTS))
        return g_strdup(first);
    if (second && g_file_test(second, G_FILE_TEST_EXISTS))
        return g_strdup(second);
    return g_strdup(fallback ? fallback : first);
}

static void
set_ini_key_preserve(const gchar *path, const gchar *group, const gchar *key, const gchar *value)
{
    gchar *contents = NULL;
    gsize len = 0;
    GString *out = g_string_new(NULL);
    gboolean in_group = FALSE;
    gboolean saw_group = FALSE;
    gboolean wrote_key = FALSE;
    gchar *group_header = g_strdup_printf("[%s]", group);
    gchar *key_prefix = g_strdup_printf("%s=", key);

    g_file_get_contents(path, &contents, &len, NULL);
    if (contents && *contents)
    {
        gchar **lines = g_strsplit(contents, "\n", -1);
        for (guint i = 0; lines[i] != NULL; i++)
        {
            const gchar *line = lines[i];
            gboolean is_last_empty = lines[i + 1] == NULL && *line == '\0';
            if (is_last_empty)
                break;

            if (line[0] == '[')
            {
                if (in_group && !wrote_key)
                {
                    g_string_append_printf(out, "%s=%s\n", key, value);
                    wrote_key = TRUE;
                }
                in_group = g_strcmp0(line, group_header) == 0;
                if (in_group)
                    saw_group = TRUE;
                g_string_append_printf(out, "%s\n", line);
                continue;
            }

            if (in_group && g_str_has_prefix(line, key_prefix))
            {
                g_string_append_printf(out, "%s=%s\n", key, value);
                wrote_key = TRUE;
            }
            else
            {
                g_string_append_printf(out, "%s\n", line);
            }
        }
        g_strfreev(lines);
    }

    if (!saw_group)
        g_string_append_printf(out, "\n[%s]\n", group);
    if (!wrote_key)
        g_string_append_printf(out, "%s=%s\n", key, value);

    gchar *dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0700);
    g_file_set_contents(path, out->str, -1, NULL);
    g_free(dir);
    g_free(contents);
    g_free(group_header);
    g_free(key_prefix);
    g_string_free(out, TRUE);
}

static void
update_qtct_settings_file(const gchar *tool_name, gboolean dark)
{
    gchar *dir = g_build_filename(g_get_user_config_dir(), tool_name, NULL);
    gchar *path = g_build_filename(dir, dark ? "dummy" : "dummy", NULL);
    g_free(path);
    path = g_strdup_printf("%s/%s.conf", dir, tool_name);

    gchar *light_default = g_strdup_printf("/usr/share/%s/colors/airy.conf", tool_name);
    gchar *light_alt = g_strdup_printf("/usr/share/%s/colors/simple.conf", tool_name);
    gchar *dark_default = g_strdup_printf("/usr/share/%s/colors/darker.conf", tool_name);
    gchar *dark_alt = g_strdup_printf("/usr/share/%s/colors/waves.conf", tool_name);
    gchar *scheme = dark
        ? find_existing_file(dark_default, dark_alt, dark_default)
        : find_existing_file(light_default, light_alt, light_default);

    set_ini_key_preserve(path, "Appearance", "color_scheme_path", scheme);
    set_ini_key_preserve(path, "Appearance", "custom_palette", "false");
    set_ini_key_preserve(path, "Appearance", "style", "Fusion");

    g_free(scheme);
    g_free(dark_alt);
    g_free(dark_default);
    g_free(light_alt);
    g_free(light_default);
    g_free(path);
    g_free(dir);
}

static void
update_qt_settings(gboolean dark)
{
    if (g_file_test("/usr/share/qt5ct/colors", G_FILE_TEST_IS_DIR) ||
        g_file_test(g_build_filename(g_get_user_config_dir(), "qt5ct", NULL), G_FILE_TEST_IS_DIR))
        update_qtct_settings_file("qt5ct", dark);

    if (g_file_test("/usr/share/qt6ct/colors", G_FILE_TEST_IS_DIR) ||
        g_file_test(g_build_filename(g_get_user_config_dir(), "qt6ct", NULL), G_FILE_TEST_IS_DIR))
        update_qtct_settings_file("qt6ct", dark);
}

static void
update_gtk_settings_file(const gchar *version, const gchar *theme, gboolean dark)
{
    gchar *dir = g_build_filename(g_get_user_config_dir(), version, NULL);
    gchar *path = g_build_filename(dir, "settings.ini", NULL);
    GKeyFile *kf = g_key_file_new();
    GError *error = NULL;

    g_mkdir_with_parents(dir, 0700);
    if (g_file_test(path, G_FILE_TEST_EXISTS))
        g_key_file_load_from_file(kf, path, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_string(kf, "Settings", "gtk-theme-name", theme);
    g_key_file_set_boolean(kf, "Settings", "gtk-application-prefer-dark-theme", dark);

    gchar *data = g_key_file_to_data(kf, NULL, NULL);
    if (data)
        g_file_set_contents(path, data, -1, &error);
    g_clear_error(&error);
    g_free(data);
    g_key_file_unref(kf);
    g_free(path);
    g_free(dir);
}

static gboolean
theme_has_subdir(const gchar *theme, const gchar *subdir)
{
    const gchar * const *data_dirs = g_get_system_data_dirs();
    gchar *p = NULL;
    gboolean ok = FALSE;

    p = g_build_filename(g_get_home_dir(), ".themes", theme, subdir, NULL);
    ok = g_file_test(p, G_FILE_TEST_IS_DIR);
    g_free(p);
    if (ok) return TRUE;

    p = g_build_filename(g_get_user_data_dir(), "themes", theme, subdir, NULL);
    ok = g_file_test(p, G_FILE_TEST_IS_DIR);
    g_free(p);
    if (ok) return TRUE;

    for (guint i = 0; data_dirs && data_dirs[i]; i++)
    {
        p = g_build_filename(data_dirs[i], "themes", theme, subdir, NULL);
        ok = g_file_test(p, G_FILE_TEST_IS_DIR);
        g_free(p);
        if (ok) return TRUE;
    }
    return FALSE;
}

static gboolean
theme_is_gtk_theme(const gchar *theme)
{
    return theme_has_subdir(theme, "gtk-3.0") || theme_has_subdir(theme, "gtk-4.0") || theme_has_subdir(theme, "gtk-2.0");
}

static void
collect_themes_from_dir(GHashTable *set, const gchar *base)
{
    GDir *dir = g_dir_open(base, 0, NULL);
    if (!dir) return;

    const gchar *name;
    while ((name = g_dir_read_name(dir)) != NULL)
    {
        if (name[0] == '.') continue;
        gchar *theme_dir = g_build_filename(base, name, NULL);
        if (g_file_test(theme_dir, G_FILE_TEST_IS_DIR) && theme_is_gtk_theme(name))
            g_hash_table_add(set, g_strdup(name));
        g_free(theme_dir);
    }
    g_dir_close(dir);
}

static gint
string_ptr_compare(gconstpointer a, gconstpointer b)
{
    const gchar *sa = *(const gchar * const *)a;
    const gchar *sb = *(const gchar * const *)b;
    return g_utf8_collate(sa, sb);
}

static GPtrArray *
list_gtk_themes(void)
{
    GHashTable *set = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    GPtrArray *themes = g_ptr_array_new_with_free_func(g_free);
    const gchar * const *data_dirs = g_get_system_data_dirs();

    gchar *home_themes = g_build_filename(g_get_home_dir(), ".themes", NULL);
    gchar *user_themes = g_build_filename(g_get_user_data_dir(), "themes", NULL);
    collect_themes_from_dir(set, home_themes);
    collect_themes_from_dir(set, user_themes);
    for (guint i = 0; data_dirs && data_dirs[i]; i++)
    {
        gchar *sys = g_build_filename(data_dirs[i], "themes", NULL);
        collect_themes_from_dir(set, sys);
        g_free(sys);
    }
    g_free(home_themes);
    g_free(user_themes);

    GHashTableIter iter;
    gpointer key;
    g_hash_table_iter_init(&iter, set);
    while (g_hash_table_iter_next(&iter, &key, NULL))
        g_ptr_array_add(themes, g_strdup((const gchar *)key));
    g_hash_table_destroy(set);

    g_ptr_array_sort(themes, string_ptr_compare);
    return themes;
}

static gchar *
find_best_dark_theme(GPtrArray *themes, const gchar *current)
{
    if (current)
    {
        gchar *candidate = g_strdup_printf("%s-dark", current);
        for (guint i = 0; i < themes->len; i++)
            if (g_ascii_strcasecmp(candidate, g_ptr_array_index(themes, i)) == 0)
                return candidate;
        g_free(candidate);

        candidate = g_strdup_printf("%s-Dark", current);
        for (guint i = 0; i < themes->len; i++)
            if (g_strcmp0(candidate, g_ptr_array_index(themes, i)) == 0)
                return candidate;
        g_free(candidate);
    }

    const gchar *preferred[] = { "Adwaita-dark", "Adwaita-dark", "Greybird-dark", "Arc-Dark", "Materia-dark", NULL };
    for (guint p = 0; preferred[p]; p++)
        for (guint i = 0; i < themes->len; i++)
            if (g_strcmp0(preferred[p], g_ptr_array_index(themes, i)) == 0)
                return g_strdup(preferred[p]);

    for (guint i = 0; i < themes->len; i++)
    {
        const gchar *theme = g_ptr_array_index(themes, i);
        gchar *down = g_ascii_strdown(theme, -1);
        gboolean match = strstr(down, "dark") != NULL;
        g_free(down);
        if (match) return g_strdup(theme);
    }

    return current ? g_strdup(current) : g_strdup("Adwaita-dark");
}

static gchar *
find_best_light_theme(GPtrArray *themes, const gchar *current)
{
    if (current && *current)
        return g_strdup(current);
    if (themes->len > 0)
        return g_strdup(g_ptr_array_index(themes, 0));
    return g_strdup("Adwaita");
}

static gchar *
config_path_for_plugin(DarkModePlugin *dm, gboolean create)
{
#ifndef DARKMODE_STANDALONE
    if (dm->plugin)
        return xfce_panel_plugin_save_location(dm->plugin, create);
#else
    (void)dm;
    (void)create;
#endif
    gchar *dir = g_build_filename(g_get_user_config_dir(), APP_NAME, NULL);
    if (create)
        g_mkdir_with_parents(dir, 0700);
    gchar *path = g_build_filename(dir, "config.rc", NULL);
    g_free(dir);
    return path;
}

static gchar *
config_lookup_for_plugin(DarkModePlugin *dm)
{
#ifndef DARKMODE_STANDALONE
    if (dm->plugin)
        return xfce_panel_plugin_lookup_rc_file(dm->plugin);
#else
    (void)dm;
#endif
    return config_path_for_plugin(dm, FALSE);
}

static void
darkmode_load_config(DarkModePlugin *dm)
{
    GPtrArray *themes = list_gtk_themes();
    gchar *current = detect_current_gtk_theme();
    dm->dark = FALSE;
    dm->set_xfwm = TRUE;
    dm->set_modern_apps = TRUE;
    dm->light_theme = find_best_light_theme(themes, current);
    dm->dark_theme = find_best_dark_theme(themes, current);

    gchar *path = config_lookup_for_plugin(dm);
    if (path && g_file_test(path, G_FILE_TEST_EXISTS))
    {
        GKeyFile *kf = g_key_file_new();
        if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL))
        {
            gchar *s;
            s = g_key_file_get_string(kf, CONFIG_GROUP, "LightTheme", NULL);
            if (s && *s) { g_free(dm->light_theme); dm->light_theme = s; } else g_free(s);
            s = g_key_file_get_string(kf, CONFIG_GROUP, "DarkTheme", NULL);
            if (s && *s) { g_free(dm->dark_theme); dm->dark_theme = s; } else g_free(s);
            if (g_key_file_has_key(kf, CONFIG_GROUP, "Dark", NULL))
                dm->dark = g_key_file_get_boolean(kf, CONFIG_GROUP, "Dark", NULL);
            if (g_key_file_has_key(kf, CONFIG_GROUP, "SetXfwm", NULL))
                dm->set_xfwm = g_key_file_get_boolean(kf, CONFIG_GROUP, "SetXfwm", NULL);
            if (g_key_file_has_key(kf, CONFIG_GROUP, "SetModernApps", NULL))
                dm->set_modern_apps = g_key_file_get_boolean(kf, CONFIG_GROUP, "SetModernApps", NULL);
        }
        g_key_file_unref(kf);
    }

    g_free(path);
    g_free(current);
    g_ptr_array_unref(themes);
}

static void
darkmode_save_config(DarkModePlugin *dm)
{
    GKeyFile *kf = g_key_file_new();
    g_key_file_set_string(kf, CONFIG_GROUP, "LightTheme", dm->light_theme ? dm->light_theme : "");
    g_key_file_set_string(kf, CONFIG_GROUP, "DarkTheme", dm->dark_theme ? dm->dark_theme : "");
    g_key_file_set_boolean(kf, CONFIG_GROUP, "Dark", dm->dark);
    g_key_file_set_boolean(kf, CONFIG_GROUP, "SetXfwm", dm->set_xfwm);
    g_key_file_set_boolean(kf, CONFIG_GROUP, "SetModernApps", dm->set_modern_apps);

    gchar *path = config_path_for_plugin(dm, TRUE);
    gchar *data = g_key_file_to_data(kf, NULL, NULL);
    if (path && data)
        g_file_set_contents(path, data, -1, NULL);
    g_free(data);
    g_free(path);
    g_key_file_unref(kf);
}

static gchar *
detect_current_gtk_theme(void)
{
    if (program_exists("xfconf-query"))
    {
        char *argv[] = { "xfconf-query", "-c", "xsettings", "-p", "/Net/ThemeName", NULL };
        gchar *stdout_text = NULL;
        gint status = 0;
        if (g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                         &stdout_text, NULL, &status, NULL) && status == 0 && stdout_text)
        {
            g_strstrip(stdout_text);
            if (*stdout_text)
                return stdout_text;
        }
        g_free(stdout_text);
    }

    GtkSettings *settings = gtk_settings_get_default();
    if (settings)
    {
        gchar *theme = NULL;
        g_object_get(settings, "gtk-theme-name", &theme, NULL);
        if (theme && *theme)
            return theme;
        g_free(theme);
    }
    return NULL;
}

static void
update_button(DarkModePlugin *dm)
{
    const gchar *icon = dm->dark ? "weather-clear-night-symbolic" : "weather-clear-symbolic";
    const gchar *label = dm->dark ? "Dark" : "Light";
    const gchar *theme = dm->dark ? dm->dark_theme : dm->light_theme;
    gchar *tip = g_strdup_printf("Dark Mode Switcher\nCurrent mode: %s\nTheme: %s\nClick to switch. Right-click for Properties.",
                                 label, theme ? theme : "(none)");
    gtk_image_set_from_icon_name(GTK_IMAGE(dm->image), icon, GTK_ICON_SIZE_BUTTON);
    if (dm->label)
        gtk_label_set_text(GTK_LABEL(dm->label), label);
    gtk_widget_set_tooltip_text(dm->button, tip);
    g_free(tip);
}

static void
darkmode_apply(DarkModePlugin *dm, gboolean dark)
{
    const gchar *theme = dark ? dm->dark_theme : dm->light_theme;
    gchar *error = NULL;

    if (!theme || !*theme)
    {
        set_error(dm, "No %s theme selected", dark ? "dark" : "light");
        return;
    }

    /* GTK apps such as Thunar receive Xfce theme updates through xfsettingsd. */
    ensure_xfsettingsd();

    if (!xfconf_set_string("xsettings", "/Net/ThemeName", theme, &error))
    {
        set_error(dm, "Could not set Xfce GTK theme: %s", error ? error : "unknown error");
        g_free(error);
        return;
    }

    /* Xfce panel has its own dark-mode property; GTK theme alone may not recolor the panel. */
    xfconf_set_bool("xfce4-panel", "/panels/dark-mode", dark, NULL);

    if (dm->set_xfwm && theme_has_subdir(theme, "xfwm4"))
        xfconf_set_string("xfwm4", "/general/theme", theme, NULL);

    if (dm->set_modern_apps)
    {
        gsettings_set_literal("org.gnome.desktop.interface", "color-scheme", dark ? "'prefer-dark'" : "'default'");
        gsettings_set_string("org.gnome.desktop.interface", "gtk-theme", theme);
        gsettings_set_literal("org.gnome.desktop.interface", "gtk-application-prefer-dark-theme", dark ? "true" : "false");
        update_gtk_settings_file("gtk-3.0", theme, dark);
        update_gtk_settings_file("gtk-4.0", theme, dark);
    }

    /* Qt apps on Xfce commonly use qt5ct/qt6ct instead of GTK settings. */
    update_qt_settings(dark);

    dm->dark = dark;
    g_clear_pointer(&dm->last_error, g_free);
    update_button(dm);
    darkmode_save_config(dm);

    /* Xfce 4.18 often does not repaint panel light/dark CSS until panel reload. */
    reload_xfce_panel();
}

static void
show_error_dialog(GtkWindow *parent, const gchar *message)
{
    GtkWidget *d = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
                                          GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                          "%s", message);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}

static void
clicked_cb(GtkButton *button, gpointer user_data)
{
    (void)button;
    DarkModePlugin *dm = user_data;
    darkmode_apply(dm, !dm->dark);
    if (dm->last_error)
        show_error_dialog(GTK_WINDOW(gtk_widget_get_toplevel(dm->button)), dm->last_error);
}

static GtkWidget *
combo_new_with_themes(GPtrArray *themes, const gchar *active)
{
    GtkWidget *combo = gtk_combo_box_text_new_with_entry();
    gint active_index = -1;
    for (guint i = 0; i < themes->len; i++)
    {
        const gchar *theme = g_ptr_array_index(themes, i);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), theme);
        if (g_strcmp0(theme, active) == 0)
            active_index = (gint)i;
    }
    if (active_index >= 0)
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active_index);
    else if (active)
        gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combo))), active);
    return combo;
}

static gchar *
combo_get_text(GtkWidget *combo)
{
    gchar *text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
    if (!text)
    {
        GtkWidget *entry = gtk_bin_get_child(GTK_BIN(combo));
        text = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    }
    g_strstrip(text);
    return text;
}

static void
darkmode_show_configure(DarkModePlugin *dm, GtkWindow *parent)
{
    GPtrArray *themes = list_gtk_themes();
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Dark Mode Switcher Properties",
                                                    parent,
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "Cancel", GTK_RESPONSE_CANCEL,
                                                    "Save", GTK_RESPONSE_OK,
                                                    NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 460, -1);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_container_add(GTK_CONTAINER(content), grid);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>Choose the Xfce themes used for each mode</b>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_grid_attach(GTK_GRID(grid), title, 0, 0, 2, 1);

    GtkWidget *light_label = gtk_label_new("Light theme:");
    GtkWidget *dark_label = gtk_label_new("Dark theme:");
    gtk_label_set_xalign(GTK_LABEL(light_label), 0.0);
    gtk_label_set_xalign(GTK_LABEL(dark_label), 0.0);
    GtkWidget *light_combo = combo_new_with_themes(themes, dm->light_theme);
    GtkWidget *dark_combo = combo_new_with_themes(themes, dm->dark_theme);
    gtk_widget_set_hexpand(light_combo, TRUE);
    gtk_widget_set_hexpand(dark_combo, TRUE);

    GtkWidget *xfwm_check = gtk_check_button_new_with_label("Also switch xfwm4 window-border theme when the selected theme provides one");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(xfwm_check), dm->set_xfwm);
    GtkWidget *modern_check = gtk_check_button_new_with_label("Advertise color-scheme to modern apps (Chrome, Firefox, GTK4/libadwaita)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(modern_check), dm->set_modern_apps);

    GtkWidget *hint = gtk_label_new("Tip: install themes into ~/.themes or ~/.local/share/themes if they do not appear here. The editable boxes also accept a manual theme name.");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(hint), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(hint), "dim-label");

    gtk_grid_attach(GTK_GRID(grid), light_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), light_combo, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), dark_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), dark_combo, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), xfwm_check, 0, 3, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), modern_check, 0, 4, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), hint, 0, 5, 2, 1);

    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
    {
        gchar *light = combo_get_text(light_combo);
        gchar *dark = combo_get_text(dark_combo);
        if (!*light || !*dark)
        {
            show_error_dialog(GTK_WINDOW(dialog), "Please select both a light theme and a dark theme.");
            g_free(light);
            g_free(dark);
        }
        else
        {
            g_free(dm->light_theme);
            g_free(dm->dark_theme);
            dm->light_theme = light;
            dm->dark_theme = dark;
            dm->set_xfwm = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(xfwm_check));
            dm->set_modern_apps = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(modern_check));
            darkmode_save_config(dm);
            update_button(dm);
            darkmode_apply(dm, dm->dark);
            if (dm->last_error)
                show_error_dialog(GTK_WINDOW(dialog), dm->last_error);
        }
    }

    gtk_widget_destroy(dialog);
    g_ptr_array_unref(themes);
}

#ifndef DARKMODE_STANDALONE
static void
configure_signal_cb(XfcePanelPlugin *plugin, gpointer user_data)
{
    (void)plugin;
    DarkModePlugin *dm = user_data;
    darkmode_show_configure(dm, NULL);
}

static void
save_signal_cb(XfcePanelPlugin *plugin, gpointer user_data)
{
    (void)plugin;
    darkmode_save_config((DarkModePlugin *)user_data);
}

static void
free_signal_cb(XfcePanelPlugin *plugin, gpointer user_data)
{
    (void)plugin;
    DarkModePlugin *dm = user_data;
    darkmode_save_config(dm);
    g_free(dm->light_theme);
    g_free(dm->dark_theme);
    g_free(dm->last_error);
    g_free(dm);
}

static void
construct_plugin(XfcePanelPlugin *plugin)
{
    DarkModePlugin *dm = g_new0(DarkModePlugin, 1);
    dm->plugin = plugin;
    darkmode_load_config(dm);

    dm->button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(dm->button), GTK_RELIEF_NONE);
    gtk_widget_set_focus_on_click(dm->button, FALSE);
    dm->image = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(dm->button), dm->image);
    gtk_container_add(GTK_CONTAINER(plugin), dm->button);

    xfce_panel_plugin_add_action_widget(plugin, dm->button);
    xfce_panel_plugin_menu_show_configure(plugin);
    g_signal_connect(plugin, "configure-plugin", G_CALLBACK(configure_signal_cb), dm);
    g_signal_connect(plugin, "save", G_CALLBACK(save_signal_cb), dm);
    g_signal_connect(plugin, "free-data", G_CALLBACK(free_signal_cb), dm);
    g_signal_connect(dm->button, "clicked", G_CALLBACK(clicked_cb), dm);

    update_button(dm);
    gtk_widget_show_all(GTK_WIDGET(plugin));
}

XFCE_PANEL_PLUGIN_REGISTER(construct_plugin)
#else
static void
prefs_clicked_cb(GtkButton *button, gpointer user_data)
{
    DarkModePlugin *dm = user_data;
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button)));
    darkmode_show_configure(dm, parent);
}

static gboolean
idle_show_config_cb(gpointer user_data)
{
    darkmode_show_configure((DarkModePlugin *)user_data, NULL);
    return G_SOURCE_REMOVE;
}

static GtkWidget *
create_standalone_window(DarkModePlugin *dm)
{
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Dark Mode Switcher Test");
    gtk_window_set_default_size(GTK_WINDOW(win), 360, 140);
    gtk_container_set_border_width(GTK_CONTAINER(win), 18);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(win), box);

    GtkWidget *caption = gtk_label_new("Standalone preview of the Xfce panel plugin button:");
    gtk_label_set_xalign(GTK_LABEL(caption), 0.0);
    gtk_box_pack_start(GTK_BOX(box), caption, FALSE, FALSE, 0);

    dm->button = gtk_button_new();
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    dm->image = gtk_image_new();
    dm->label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(h), dm->image, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(h), dm->label, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(dm->button), h);
    gtk_box_pack_start(GTK_BOX(box), dm->button, FALSE, FALSE, 0);

    GtkWidget *prefs = gtk_button_new_with_label("Open Properties");
    gtk_box_pack_start(GTK_BOX(box), prefs, FALSE, FALSE, 0);
    g_signal_connect(dm->button, "clicked", G_CALLBACK(clicked_cb), dm);
    g_signal_connect(prefs, "clicked", G_CALLBACK(prefs_clicked_cb), dm);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    update_button(dm);
    return win;
}

int
main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    DarkModePlugin *dm = g_new0(DarkModePlugin, 1);
    darkmode_load_config(dm);
    GtkWidget *win = create_standalone_window(dm);
    gtk_widget_show_all(win);
    if (argc > 1 && g_strcmp0(argv[1], "--properties") == 0)
        g_idle_add(idle_show_config_cb, dm);
    gtk_main();
    darkmode_save_config(dm);
    g_free(dm->light_theme);
    g_free(dm->dark_theme);
    g_free(dm->last_error);
    g_free(dm);
    return 0;
}
#endif
