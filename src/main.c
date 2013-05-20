/**
 * sbmanager -- Manage iPhone/iPod Touch SpringBoard icons from your computer!
 *
 * Copyright (C) 2009-2010 Nikias Bassen <nikias@gmx.li>
 * Copyright (C) 2009-2010 Martin Szulecki <opensuse@sukimashita.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more profile.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 
 * USA
 */

#ifdef HAVE_CONFIG_H
 #include <config.h> /* for GETTEXT_PACKAGE */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/sbservices.h>
#include <plist/plist.h>
#include <time.h>
#include <sys/time.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>
#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "sbmgr.h"
#include "utility.h"

GtkWidget *main_window;
GtkWidget *btn_reload;
GtkWidget *btn_apply;


char *match_uuid = NULL;
char *current_uuid = NULL;

static gboolean win_map_cb(GtkWidget *widget, GdkEvent *event, gpointer *data)
{
    debug_printf("%s: mapped\n", __func__);

    return TRUE;
}

static void update_device_info_cb(const char *device_name, const char *device_type)
{
    if (device_name) {
        gchar *wndtitle = g_strdup_printf("%s - " PACKAGE_NAME, device_name);
        gtk_window_set_title(GTK_WINDOW(main_window), wndtitle);
        g_free(wndtitle);
    } else {
        gtk_window_set_title(GTK_WINDOW(main_window), PACKAGE_NAME);
    }
}

static void finished_cb(gboolean success)
{
    gtk_widget_set_sensitive(btn_reload, TRUE);
    gtk_widget_set_sensitive(btn_apply, TRUE);
    if (success) {
        printf("successfully loaded icons\n");
    } else {
        printf("there was an error loading the icons\n");
    }
}

static gboolean reload_button_clicked_cb(GtkButton *button, gpointer user_data)
{
    gtk_widget_set_sensitive(btn_reload, FALSE);
    gtk_widget_set_sensitive(btn_apply, FALSE);
    sbmgr_load(current_uuid, update_device_info_cb, finished_cb);
    return TRUE;
}

static gboolean apply_button_clicked_cb(GtkButton *button, gpointer user_data)
{
    gtk_widget_set_sensitive(btn_reload, FALSE);
    gtk_widget_set_sensitive(btn_apply, FALSE);
    sbmgr_save(current_uuid);
    gtk_widget_set_sensitive(btn_reload, TRUE);
    gtk_widget_set_sensitive(btn_apply, TRUE);
    return TRUE;
}

static gboolean info_button_clicked_cb(GtkButton *button, gpointer user_data)
{
    const gchar *authors[] = {
        "Nikias Bassen <nikias@gmx.li>",
        "Martin Szulecki <opensuse@sukimashita.com>",
        NULL
    };
    const gchar *copyright =  "Copyright © 2009-2010 Nikias Bassen, Martin Szulecki; All Rights Reserved.";
    const gchar *program_name = PACKAGE_NAME;
    const gchar *version = PACKAGE_VERSION;
    const gchar *comments = _("Manage iPhone/iPod Touch SpringBoard from the computer");
    const gchar *website = "http://cgit.sukimashita.com/sbmanager.git";
    const gchar *website_label = _("Project Site");
    const gchar *translators =
	"Español: Itxaka Serrano\n"
	"Français: Christophe Fergeau\n"
	"Italiano: Mario Polino\n"
	"日本語: Koichi Akabe\n"
	;

    gtk_show_about_dialog(GTK_WINDOW(main_window),
            "authors", authors,
            "copyright", copyright,
            "program-name", program_name,
            "version", version,
            "comments", comments,
            "website", website,
            "website-label", website_label,
            "translator-credits", translators,
            NULL);
    return TRUE;
}

static void quit_program_cb(GtkWidget *widget, gpointer user_data)
{
    /* cleanup */
    sbmgr_finalize();
    idevice_event_unsubscribe();
    gtk_main_quit();
}

static gboolean quit_button_clicked_cb(GtkButton *button, gpointer user_data)
{
    quit_program_cb(GTK_WIDGET(button), user_data);
    return TRUE;
}

static void gui_error_dialog(const gchar *string)
{
    GtkWidget *dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW(main_window), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "<b>%s</b>", _("Error"));
    gtk_window_set_title(GTK_WINDOW(dialog), PACKAGE_NAME);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", string);
    g_signal_connect_swapped (dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_widget_show(dialog);
}

static gpointer device_add_cb(gpointer user_data)
{
    const char *uuid = (const char*)user_data;
    sbmgr_load(uuid, update_device_info_cb, finished_cb);
    return NULL;
}

static void device_event_cb(const idevice_event_t *event, void *user_data)
{
    if (event->event == IDEVICE_DEVICE_ADD) {
		/* TW 27/04/13 Compiler error: ‘idevice_event_t’ has no member named uuid  it is udid in libmobiledevice.h */
        if (!current_uuid && (!match_uuid || !strcasecmp(match_uuid, event->udid))) {
			/* TW 27/04/13 Compiler error: ‘idevice_event_t’ has no member named uuid  it is udid in libmobiledevice.h */
            debug_printf("Device add event: adding device %s\n", event->udid);
			/* TW 27/04/13 Compiler error: ‘idevice_event_t’ has no member named uuid  it is udid in libmobiledevice.h */
            current_uuid = g_strdup(event->udid);

			/* g_thread_create’ is deprecated TW 27/04/13 */
            /*g_thread_create(device_add_cb, current_uuid, FALSE, NULL); */
			const gchar *name3 = "devevcbthrd"; /* threadname added for debugging */
			g_thread_new(name3, device_add_cb, current_uuid);

        } else {
			/* TW 27/04/13 Compiler error: ‘idevice_event_t’ has no member named uuid  it is udid in libmobiledevice.h */
            debug_printf("Device add event: ignoring device %s\n", event->udid);
        }
    } else if (event->event == IDEVICE_DEVICE_REMOVE) {
		/* TW 27/04/13 Compiler error: ‘idevice_event_t’ has no member named uuid  it is udid in libmobiledevice.h */
        if (current_uuid && !strcasecmp(current_uuid, event->udid)) {

			/* This section does not work TW 08/05/13 */
			/* TW 27/04/13 Compiler error: ‘idevice_event_t’ has no member named uuid  it is udid in libmobiledevice.h */
			debug_printf("Device remove event: removing device %s\n", event->udid);
            free(current_uuid);
            current_uuid = NULL;
            sbmgr_cleanup();
        } else {
			/* TW 27/04/13 Compiler error: ‘idevice_event_t’ has no member named uuid  it is udid in libmobiledevice.h */
            debug_printf("Device remove event: ignoring device %s\n", event->udid);
        }
    }
}

static void wnd_init()
{
    /* Create the clutter widget */
    GtkWidget *sbmgr_widget = sbmgr_new();
    if (!sbmgr_widget) {
        return;
    }

    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_resizable(GTK_WINDOW(main_window), FALSE);

    gtk_window_set_title(GTK_WINDOW(main_window), PACKAGE_NAME);

	/* gtk_vbox_new’ is deprecated TW 27/04/13 */
    /* GtkWidget *vbox = gtk_vbox_new(FALSE, 6); */
	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    gtk_container_add(GTK_CONTAINER(main_window), vbox);
    gtk_widget_show(vbox);

    /* create a toolbar */
    GtkWidget *toolbar = gtk_toolbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

    btn_reload = (GtkWidget*)gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH);
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(btn_reload), _("Reload icons from device"));
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(btn_reload), -1);

    btn_apply = (GtkWidget*)gtk_tool_button_new_from_stock(GTK_STOCK_APPLY);
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(btn_apply), _("Upload changes to device"));
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(btn_apply), -1);

    GtkToolItem *btn_info = gtk_tool_button_new_from_stock(GTK_STOCK_INFO);
    gtk_tool_item_set_tooltip_text(btn_info, _("Get info about this cool program"));
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_info, -1);

    GtkToolItem *spacer = gtk_tool_item_new();
    gtk_tool_item_set_expand(spacer, TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), spacer, -1);

    GtkToolItem *btn_quit = gtk_tool_button_new_from_stock(GTK_STOCK_QUIT);
    gtk_tool_item_set_tooltip_text(btn_quit, _("Quit this program"));
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_quit, -1);

    gtk_widget_set_sensitive(btn_reload, FALSE);
    gtk_widget_set_sensitive(btn_apply, FALSE);
    gtk_widget_show(toolbar);

    /* set up signal handlers */
    g_signal_connect(btn_reload, "clicked", G_CALLBACK(reload_button_clicked_cb), NULL);
    g_signal_connect(btn_apply, "clicked", G_CALLBACK(apply_button_clicked_cb), NULL);
    g_signal_connect(btn_info, "clicked", G_CALLBACK(info_button_clicked_cb), NULL);
    g_signal_connect(btn_quit, "clicked", G_CALLBACK(quit_button_clicked_cb), NULL);

    /* insert sbmgr widget */
    gtk_box_pack_start(GTK_BOX(vbox), sbmgr_widget, TRUE, TRUE, 0);
    gtk_widget_show(sbmgr_widget);
    gtk_widget_grab_focus(sbmgr_widget);

    /* create a statusbar */
/*    statusbar = gtk_statusbar_new();
    gtk_statusbar_set_has_resize_grip(GTK_STATUSBAR(statusbar), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), statusbar, FALSE, FALSE, 0);
    gtk_widget_show(statusbar);
*/
    /* attach to window signals */
    g_signal_connect(G_OBJECT(main_window), "map-event", G_CALLBACK(win_map_cb), NULL);

    /* Show the window. This also sets the stage's bounding box. */
    gtk_widget_show_all(GTK_WIDGET(main_window));

    g_set_printerr_handler((GPrintFunc)gui_error_dialog);

    /* Stop the application when the window is closed */
    g_signal_connect(main_window, "hide", G_CALLBACK(quit_program_cb), NULL);

    /* get notified when plug in/out events occur */
    idevice_event_subscribe(device_event_cb, NULL);
}

/* main */
static void print_usage(int argc, char **argv)
{
    char *name = NULL;

    name = strrchr(argv[0], '/');
    printf("Usage: %s [OPTIONS]\n", (name ? name + 1 : argv[0]));
    printf("Manage SpringBoard icons of an iPhone/iPod Touch.\n\n");
    printf("  -d, --debug\t\tenable communication debugging\n");
    printf("  -D, --debug-app\tenable application debug messages\n");
    printf("  -u, --uuid UUID\ttarget specific device by its 40-digit device UUID\n");
    printf("  -h, --help\t\tprints usage information\n");
    printf("\n");
}

int main(int argc, char **argv)
{
    int i;

    /* parse cmdline args */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
            idevice_set_debug_level(1);
            continue;
        } else if (!strcmp(argv[i], "-D") || !strcmp(argv[i], "--debug-app")) {
            set_debug(TRUE);
            continue;
        } else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--uuid")) {
            i++;
            if (!argv[i] || (strlen(argv[i]) != 40)) {
                print_usage(argc, argv);
                return 0;
            }
            match_uuid = g_strndup(argv[i], 40);
            continue;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_usage(argc, argv);
            return 0;
        } else {
            print_usage(argc, argv);
            return 0;
        }
    }

    /* Create the window and some child widgets */
    wnd_init();

    /* Start the main loop, so we can respond to events */
    gtk_main();

    return 0;
}
