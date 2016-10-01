/**
 * sb.app.c -- Sbmanager gappliction setup 
 *
 * Copyright (C) 2009-2010 Nikias Bassen <nikias@gmx.li>
 * Copyright (C) 2009-2010 Martin Szulecki <opensuse@sukimashita.com>
 * Copyright (C) 2016 Timothy Ward <gtwa001@gmail.com>
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
#include "sb.app.h"

typedef struct {

} SbAppPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (SbApp, sb_app, GTK_TYPE_APPLICATION);



GtkWidget *main_window; 
GtkWidget *btn_reload;
GtkWidget *btn_apply;
GtkWidget *image;
GtkWidget *buttonbox;


char *match_uuid = NULL;
char *current_uuid = NULL;

/*
 * static void
 *app_quit_cb (GSimpleAction *simple, GVariant *parameter, gpointer user_data)
 * {
 *
 *   // cleanup 
 *   sbmgr_finalize();
 *   idevice_event_unsubscribe();
 *   debug_printf("quit-called\n");
 *	gtk_widget_destroy(main_window);
 * }
 */

static void
about_cb (GSimpleAction *simple, GVariant *parameter, gpointer user_data)
{
    GtkApplication *app;
    GtkWindow *parent;

   const gchar *authors[] = {
        "Nikias Bassen <nikias@gmx.li>",
        "Martin Szulecki <opensuse@sukimashita.com>",
        "Timothy Ward <gtwa001@gmail.com>",
        NULL
    };


    const gchar *copyright =  "Copyright © 2009-2010 Nikias Bassen, Martin Szulecki, 2013-2016 Timothy Ward, All Rights Reserved.";
    const gchar *program_name = PACKAGE_NAME;
    const gchar *comments = _("Manage iPhone/iPod Touch SpringBoard from the computer");
    const gchar *website = "https://github.com/gitsop01/sbmanager.git";
    const gchar *website_label = _("Project Site");
	const gchar *translators =
	"Español: Itxaka Serrano\n"
	"Français: Christophe Fergeau\n"
	"Italiano: Mario Polino\n"
	"日本語: Koichi Akabe\n"
	;

	 app = GTK_APPLICATION (user_data);
     parent = gtk_application_get_active_window (GTK_APPLICATION (app));

    gtk_show_about_dialog(parent,
            "authors", authors,
            "copyright", copyright,
            "program-name", program_name,
            "version", PACKAGE_VERSION,
            "comments", comments,
            "website", website,
            "website-label", website_label,
			"license-type", GTK_LICENSE_GPL_2_0,
            "translator-credits", translators,
            NULL);

}



static void
sb_app_startup (GApplication *application)
{

      /*   SbApp *app = SB_APP (application); */
     /*   SbAppPrivate *priv = dh_app_get_instance_private (app); */

              /* Chain up parent's startup */
        G_APPLICATION_CLASS (sb_app_parent_class)->startup (application);

}





SbApp *
sb_app_new (void)
{
        return g_object_new (SB_TYPE_APP,
                           "application-id",   "github.com.gitsop01.sbmanager",
                           "flags",            G_APPLICATION_HANDLES_COMMAND_LINE,
                            "register-session", TRUE,
                            NULL);
}


static gboolean  option_version;
static gchar *option_uuid;


static GOptionEntry options[] = {
        { "debug", 'd',      0, G_OPTION_ARG_NONE, NULL, N_("Enable communication debugging"), NULL },
        { "debug-app", 'D',  0, G_OPTION_ARG_NONE, NULL, N_("Enable application debug messages"), NULL },
        { "Verbose", 'V',    0, G_OPTION_ARG_NONE, NULL, N_("Show verbose debugging information"), NULL },
        { "version", 'v',    0, G_OPTION_ARG_NONE, &option_version, N_("Show the version number and exit"), NULL },
        { "uuid  UUID", 'u', 0, G_OPTION_ARG_STRING, &option_uuid, N_("Target specific device by its 40-digit device UUID"), NULL },
        { NULL }
};

static gint
sb_app_handle_local_options (GApplication *app, GVariantDict *Options)
{
  debug_printf("In sb-app-handle-local-options\n");
  if (option_version)
    {
      g_print ("%s Version = %s\n", g_get_application_name (), PACKAGE_VERSION);
      exit (0);
    }

  return -1;
}

static gint
sb_app_command_line (GApplication            *app,
                     GApplicationCommandLine *command_line)
{
        gboolean *option_debug = FALSE;
        gboolean *option_debug_app = FALSE;
        gboolean *option_verbose = FALSE;
 /*       static gchar *option_uuid[40]; */
         
        debug_printf("In sb-app-command line\n");
        GVariantDict *Options;

        Options = g_application_command_line_get_options_dict (command_line);
        g_variant_dict_lookup (Options, "debug", "b", &option_debug);
        g_variant_dict_lookup (Options, "debug-app", "b", &option_debug_app);
        g_variant_dict_lookup (Options, "verbose", "b", &option_verbose);
        g_variant_dict_lookup (Options, "version", "b", &option_version);
        g_variant_dict_lookup (Options, "uuid", "&s",&option_uuid);

        if (option_debug) {
           idevice_set_debug_level(1);
        } else if (option_debug_app) {
           set_debug(TRUE);    
        } else if (option_verbose) {
                /* FIXME add verbose debugging */
        }

		/* else if ((strlen(option_uuid) != 40)) { */
        /*   		 g_print ("UUID is not 40 characters long"); */
        /* 	return 0; */
		/*  } */
        /* match_uuid = g_strndup(option_uuid, 40);	*/


		debug_printf("Finished command-line\n");
        return 0;
}

void
sb_app_init (SbApp *app)
{

        gtk_window_set_default_icon_name ("phone");

        g_application_add_main_option_entries (G_APPLICATION (app), options);
		debug_printf("Finished sb_app_init\n");
}



/** static void
*sbapp_quit_cb (GSimpleAction *action,
*         GVariant      *parameter,
*         gpointer       user_data)
* {
*        SbApp *app = SB_APP (user_data);
*        GList *l;
*
*         Remove all windows registered in the application
*        while ((l = gtk_application_get_windows (GTK_APPLICATION (app)))) {
*                gtk_application_remove_window (GTK_APPLICATION (app),
*                                              GTK_WINDOW (l->data));
*        }
*}
**/

SbApp *
sb_app_start (GApplication *application)
{

	debug_printf( "in-sb-app-start\n");
	
	/* Create the clutter widget */
    GtkWidget *sbmgr_widget = sbmgr_new();
    if (!sbmgr_widget) {
        printf("Failed to create Clutter widget");
    }
  
     main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	/* gtk_application_add_window (GTK_APPLICATION (application), GTK_WINDOW (main_window)); */

    gtk_window_set_resizable(GTK_WINDOW(main_window), FALSE);

    gtk_window_set_title(GTK_WINDOW(main_window), PACKAGE_NAME);

	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    gtk_container_add(GTK_CONTAINER(main_window), vbox);
    gtk_widget_show(vbox);

      /* create a toolbar */
    GtkWidget *toolbar = gtk_toolbar_new(); 
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0); 
    buttonbox=gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	btn_reload = (GtkWidget*)gtk_tool_button_new(NULL , "Reload");
	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(btn_reload), _("Reload icons from device")); 
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(btn_reload), -1);
 
	btn_apply = (GtkWidget*)gtk_tool_button_new(NULL, "Upload");
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(btn_apply), _("Upload changes to device"));  
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(btn_apply), -1);
 
	
	GtkToolItem *btn_info = gtk_tool_button_new(NULL, "Info");
    gtk_tool_item_set_tooltip_text(btn_info,_("Get info about this program")); 
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar),GTK_TOOL_ITEM(btn_info), -1); 
    GtkToolItem *spacer = gtk_tool_item_new();
    gtk_tool_item_set_expand(spacer, TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), spacer, -1); 

    gtk_widget_set_sensitive(btn_reload, FALSE); 
    gtk_widget_set_sensitive(btn_apply, FALSE); 
    gtk_widget_show(toolbar); 

     /* set up signal handlers */
    g_signal_connect(btn_reload, "clicked", G_CALLBACK(reload_button_clicked_cb), NULL);
    g_signal_connect(btn_apply, "clicked", G_CALLBACK(apply_button_clicked_cb), NULL); 
	g_signal_connect(btn_info, "clicked", G_CALLBACK(about_cb), NULL);

	/* insert sbmgr widget */
    gtk_box_pack_start(GTK_BOX(vbox), sbmgr_widget, TRUE, TRUE, 0);
    /* gtk_widget_show(sbmgr_widget); */
    gtk_widget_grab_focus(sbmgr_widget); 


   gtk_widget_show_all(GTK_WIDGET(main_window));

    g_set_printerr_handler((GPrintFunc)gui_error_dialog);

    /* Stop the application when the window is closed */
	g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_widget_destroyed), &main_window);
  /*  g_signal_connect(main_window, "destroy", G_GALLBACK(app_quit_cb), NULL); */

    /* get notified when plug in/out events occur */
    idevice_event_subscribe(device_event_cb, NULL);
	return 0;
}


static void
sb_app_activate (GApplication *application)
{
	    debug_printf("In sb-app-activate\n");
        sb_app_start (G_APPLICATION (application));
}



void update_device_info_cb(const char *device_name, const char *device_type)
{
    if (device_name) {
        gchar *wndtitle = g_strdup_printf("%s - " PACKAGE_NAME, device_name);
        gtk_window_set_title(GTK_WINDOW(main_window), wndtitle);
        g_free(wndtitle);
    } else {
        gtk_window_set_title(GTK_WINDOW(main_window), PACKAGE_NAME);
    }
}



void finished_cb(gboolean success)
{
    gtk_widget_set_sensitive(btn_reload, TRUE);
    gtk_widget_set_sensitive(btn_apply, TRUE);
    if (success) {
        printf("successfully loaded icons\n");
    } else {
        printf("there was an error loading the icons\n");
    }
} 

     /* FIXME Reload should clear the pages on the Springboard or only reload the icons that have changed position */
gboolean reload_button_clicked_cb(GtkButton *button, gpointer user_data)
{
    gtk_widget_set_sensitive(btn_reload, FALSE);
    gtk_widget_set_sensitive(btn_apply, FALSE);
    sbmgr_load(current_uuid, update_device_info_cb, finished_cb);
    return TRUE;
}


gboolean apply_button_clicked_cb(GtkButton *button, gpointer user_data)
{
    gtk_widget_set_sensitive(btn_reload, FALSE);
    gtk_widget_set_sensitive(btn_apply, FALSE);
    sbmgr_save(current_uuid);
    gtk_widget_set_sensitive(btn_reload, TRUE);
    gtk_widget_set_sensitive(btn_apply, TRUE);
    return TRUE;
} 

void device_event_cb(const idevice_event_t *event, void *user_data)
{
    if (event->event == IDEVICE_DEVICE_ADD) {
        if (!current_uuid && (!match_uuid || !strcasecmp(match_uuid, event->udid))) {
			debug_printf("Device add event: adding device %s\n", event->udid);
            current_uuid = g_strdup(event->udid);

			const gchar *name3 = "devevcbthrd";
			g_thread_new(name3, device_add_cb, current_uuid);

        } else {

            debug_printf("Device add event: ignoring device %s\n", event->udid);
        }
    } else if (event->event == IDEVICE_DEVICE_REMOVE) {

        if (current_uuid && !strcasecmp(current_uuid, event->udid)) {
			debug_printf("Device remove event: removing device %s\n", event->udid);
            free(current_uuid);
            current_uuid = NULL;
            sbmgr_cleanup();
        } else {
			debug_printf("Device remove event: ignoring device %s\n", event->udid);
        }
    }
}


static void
sb_app_dispose (GObject *object)
{
     /*   SbApp *app = SB_APP (object); */
/*        SbAppPrivate *priv = sb_app_get_instance_private (app); */


        G_OBJECT_CLASS (sb_app_parent_class)->dispose (object);
}


static void
sb_app_class_init (SbAppClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

       application_class->startup = sb_app_startup;
        application_class->activate = sb_app_activate;
       application_class->handle_local_options = sb_app_handle_local_options;
        application_class->command_line = sb_app_command_line;

         object_class->dispose = sb_app_dispose;
}

