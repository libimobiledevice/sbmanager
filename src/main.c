/**
 * sbmanager -- Manage iPhone/iPod Touch SpringBoard icons from your computer!
 *
 * Copyright (C) 2009-2010 Nikias Bassen <nikias@gmx.li>
 * Copyright (C) 2009-2010 Martin Szulecki <opensuse@sukimashita.com>
 * Copyright (C) 2013-2016 Timothy Ward <gtwa001@gmail.com>
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
#include <glib/gi18n.h>

#include <gio/gio.h>
#include <locale.h>



#include <gtk/gtk.h>
#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h> 
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "sbmgr.h"
#include "utility.h"

GtkWidget *main_window;
GtkWidget *btn_reload; 
GtkWidget *btn_apply; 
/* GtkToolButton *btn_info; */
GtkWidget *image;
GtkWidget *buttonbox;
/* GtkWidget *vbox; */
 


char *match_uuid = NULL;
char *current_uuid = NULL;

/* static gboolean win_map_cb(GtkWidget *widget, GdkEvent *event, gpointer *data)
{
    debug_printf("%s: mapped\n", __func__);

    return TRUE;
} */

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

/* FIXME - GTK3 application update */
 static gboolean apply_button_clicked_cb(GtkButton *button, gpointer user_data)
{
    gtk_widget_set_sensitive(btn_reload, FALSE);
    gtk_widget_set_sensitive(btn_apply, FALSE);
    sbmgr_save(current_uuid);
    gtk_widget_set_sensitive(btn_reload, TRUE);
    gtk_widget_set_sensitive(btn_apply, TRUE);
    return TRUE;
} 

/* FIXME - GTK3 application update */
static gboolean info_button_clicked_cb(GtkButton *button, gpointer user_data)
 {
    const gchar *authors[] = {
        "Nikias Bassen <nikias@gmx.li>",
        "Martin Szulecki <opensuse@sukimashita.com>",
        "Timothy Ward <gtwa001@gmail.com>",
        NULL
    }; 


    const gchar *copyright =  "Copyright © 2009-2010 Nikias Bassen, Martin Szulecki, 2013-2016 Timothy Ward; All Rights Reserved.";
    const gchar *program_name = PACKAGE_NAME;
    const gchar *version = PACKAGE_VERSION;
    const gchar *comments = _("Manage iPhone/iPod Touch SpringBoard from the computer");
    const gchar *website = "https://github.com/gitsop01/sbmanager.git";
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

/* FIXME - GTK3 application update */

static void quit_program_cb(GtkWidget *widget, gpointer user_data)
{ 
    /* cleanup */
    sbmgr_finalize();
    idevice_event_unsubscribe();
    g_print("quit-called\n");
	gtk_main_quit();

/*	g_application_quit(app); */
} 

/* FIXME - GTK3 application update NOT REQUIRED FOR GTK3 */
 /* static gboolean quit_button_clicked_cb(GtkButton *button, gpointer user_data)
{
    quit_program_cb(GTK_WIDGET(button), user_data);
    return TRUE; 
} 
*/

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
	
        if (!current_uuid && (!match_uuid || !strcasecmp(match_uuid, event->udid))) {
			debug_printf("Device add event: adding device %s\n", event->udid);
			
            current_uuid = g_strdup(event->udid);

			const gchar *name3 = "devevcbthrd"; /* thread name added for debugging TW */
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



static void start(GtkApplication *app, gpointer user_data)
{
	printf( "Activated\n");
	
	/* Create the clutter widget */
    GtkWidget *sbmgr_widget = sbmgr_new();
    if (!sbmgr_widget) {
        return;
    }
    /* FIXME START GTK-INSPECTOR - interactive debugger */
	gtk_window_set_interactive_debugging(TRUE); 

     main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  	/* main_window =gtk_application_window_new(GTK_APPLICATION (app)); */
    gtk_window_set_resizable(GTK_WINDOW(main_window), FALSE);

    gtk_window_set_title(GTK_WINDOW(main_window), PACKAGE_NAME);

	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    gtk_container_add(GTK_CONTAINER(main_window), vbox);
    gtk_widget_show(vbox);

      /* create a toolbar */
    GtkWidget *toolbar = gtk_toolbar_new(); 
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0); 
    buttonbox=gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);

	
 
	/* 	btn_reload = (GtkWidget*)gtk_tool_button_new(gtk_image_new_from_icon_name("view-refresh",GTK_ICON_SIZE_BUTTON),NULL); */
	
	btn_reload = (GtkWidget*)gtk_tool_button_new(NULL , "Reload");
		

     gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(btn_reload), _("Reload icons from device")); 
     gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(btn_reload), -1); 
 
    /* btn_apply = (GtkWidget*)gtk_tool_button_new(gtk_image_new_from_icon_name("Apply",GTK_ICON_SIZE_BUTTON),NULL); */
 
	btn_apply = (GtkWidget*)gtk_tool_button_new(NULL, "Upload");

    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(btn_apply), _("Upload changes to device"));  
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(btn_apply), -1); 
 
	
 
   /* GtkToolItem *btn_info = gtk_tool_button_new(gtk_image_new_from_icon_name("dialoginformation",GTK_ICON_SIZE_BUTTON),NULL); */

	GtkToolItem *btn_info = gtk_tool_button_new(NULL, "Info");
    gtk_tool_item_set_tooltip_text(btn_info,_("Get info about this program")); 
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar),GTK_TOOL_ITEM(btn_info), -1); 

    GtkToolItem *spacer = gtk_tool_item_new();
    gtk_tool_item_set_expand(spacer, TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), spacer, -1); 

	/*  GtkToolItem *btn_quit = gtk_tool_button_new_from_stock(GTK_STOCK_QUIT); */

	/*   GtkWidget *btn_quit = gtk_image_new_from_icon_name("application-exit", GTK_ICON_SIZE_BUTTON);
    Gtk_tool_button_set_icon_widget(toolbar, *btn_quit);
    gtk_tool_item_set_tooltip_text(btn_quit, _("Quit this program")); 
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_quit, -1); */

    gtk_widget_set_sensitive(btn_reload, FALSE); 
    gtk_widget_set_sensitive(btn_apply, FALSE); 
    gtk_widget_show(toolbar); 

     /* set up signal handlers */
    g_signal_connect(btn_reload, "clicked", G_CALLBACK(reload_button_clicked_cb), NULL);
    g_signal_connect(btn_apply, "clicked", G_CALLBACK(apply_button_clicked_cb), NULL); 
    g_signal_connect(btn_info, "clicked", G_CALLBACK(info_button_clicked_cb), NULL); 
   /*  g_signal_connect(btn_quit, "clicked", G_CALLBACK(quit_button_clicked_cb), NULL); */


	/* insert sbmgr widget */
    gtk_box_pack_start(GTK_BOX(vbox), sbmgr_widget, TRUE, TRUE, 0);
    /* gtk_widget_show(sbmgr_widget); */
    gtk_widget_grab_focus(sbmgr_widget); 

    /* create a statusbar */
	 /*  statusbar = gtk_statusbar_new();
    :gtk_statusbar_set_has_resize_grip(GTK_STATUSBAR(statusbar), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), statusbar, FALSE, FALSE, 0);
    gtk_widget_show(statusbar); */

    /* attach to window signals */
 	/*   g_signal_connect(G_OBJECT(main_window), "map-event", G_CALLBACK(win_map_cb), NULL); */

    /* Show the window. This also sets the stage's bounding box. */
 
   gtk_widget_show_all(GTK_WIDGET(main_window));

    g_set_printerr_handler((GPrintFunc)gui_error_dialog);

    /* Stop the application when the window is closed */
	g_signal_connect(main_window, "destroy", G_CALLBACK(quit_program_cb), NULL);

    /* get notified when plug in/out events occur */
    idevice_event_subscribe(device_event_cb, NULL);
	
}






/* main */
/**static void print_usage(int argc, char **argv)
 * {
 *   char *name = NULL;
 *
 *   name = strrchr(argv[0], '/');
 *   printf("Usage: %s [OPTIONS]\n", (name ? name + 1 : argv[0]));
 *   printf("Manage SpringBoard icons of an iPhone/iPod Touch.\n\n");
 *   printf("  -d, --debug\t\tenable communication debugging\n");
 *   printf("  -D, --debug-app\tenable application debug messages\n");
 *   printf("  -u, --uuid UUID\ttarget specific device by its 40-digit device UUID\n");
 *   printf("  -h, --help\t\tprints usage information\n");
 *  printf("\n");
 * }
 */

static void initoptions (GApplication *app)
{
printf("in initoptions\n");
gboolean *debuglevel = FALSE;
gboolean debugapp = FALSE;
gchar uuid[40];


	const GOptionEntry options[] = {

		{ "debug", 'd', 0, G_OPTION_ARG_NONE, &debuglevel, "Enable communication debugging", NULL },
		{ "debug-app", 'D', 0, G_OPTION_ARG_NONE, &debugapp, "Enable application debug messages", NULL },
		{ "uuid  UUID", 'u', 0, G_OPTION_ARG_STRING, &uuid, "Target specific device by its 40-digit device UUID", NULL },
		{ "verbose", 'V', 0, G_OPTION_ARG_NONE, NULL, "Show verbose debugging information", NULL },
		{ "version", 'v', 0, G_OPTION_ARG_NONE, NULL, "Show version number", NULL },
		{ NULL }
	};


	GOptionContext *context;
 /*   gboolean parsing_ok; */

    
	context = g_option_context_new (" - Manage SpringBoard icons of an iPhone/iPod Touch");
	g_option_context_set_help_enabled (context, TRUE); 
	g_application_add_main_option_entries (G_APPLICATION (app), options); 
  /*  g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE); 
	g_option_context_add_group (context, gtk_get_option_group (TRUE)); */
	/*if (!g_option_context_parse (context, &argc, &argv, &error))
		{
			g_print ("option parsing commandline flags failed\n");
			exit (1);
		} */
}


static int localcommandline( GApplication *app, GApplicationCommandLine *cmdline)
{
printf("in handle_local_options\n");
/* GError *error; */
const gchar *debuglevel;

GVariantDict *options;

	options = g_application_command_line_get_options_dict ( *cmdline);

	if (g_variant_dict_contains (options, "verbose"))
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

	
	if (g_variant_dict_contains (options, "version")) {
		g_print ("sbmanager " VERSION "\n");
		return 0;
	}

	if (g_variant_dict_contains (options, "--debug")) {
		g_print ("sbmanager-debug \n");
		return 0;
	}
/*	if (!g_application_register (app, NULL, &error)) {
		g_printerr ("%s\n", error->message);
		return 1;
	} */

	if (g_variant_dict_lookup (options, "debug", "&s", &debuglevel)) {
		printf("Found debug in commandline from dictionary\n");
	}

	/*	g_action_group_activate_action (G_ACTION_GROUP (app),
						"set-mode",
						g_variant_new_string (mode)); 
		return 0; */
	/* } else if (g_variant_dict_lookup (options, "search", "&s", &search)) {
		g_action_group_activate_action (G_ACTION_GROUP (app),
						"search",
						g_variant_new_string (search));
		return 0; 
		} */
	

	return -1;
}
 /*	gint i;	*/
 /*	gchar **argv; 	*/
 /*	gint argc; 	*/
 /*	printf("in command-line\n"); */
/*	g_application_hold (app); */
 /*	argv = g_application_command_line_get_arguments (cmdline, &argc);	*/
  /*	g_application_command_line_get_options_dict (GApplicationCommandLine *cmdline); */
	/* FIXME need to ensure this is correct */
	
/*	printf("%s\n",*argv); */

/*	if (argc > 1)
*    {
*      if (g_strcmp0 (argv[1], "-D") == 0)
*       {
*        for (i = 0; i < argc; i++)
*        g_print (" %s", argv[i]);
*         g_print ("\n");
* 		}
*	}
*/

/*  g_application_command_line_print (cmdline, "this text is written back\n" 
												"to stdout of the caller\n"); */
 /*		for (i = 0; i < argc; i++) {
 			g_print("argument %d: %s\n", i, argv[i]);
			g_print("argument %d: %s\n", 1, argv[1]); */
		
	
  /* FIXME only the first argument the name of the program is being displayed */
 /**
 *			if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
 *			idevice_set_debug_level(1);
 *			continue;
 *	  } else if (!strcmp(argv[i], "-D") || !strcmp(argv[i], "--debug-app")) {
 *	        set_debug(TRUE);
 *	        continue;
 *	  } else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--uuid")) {
 *	        i++;
 *	       		if (!argv[i] || (strlen(argv[i]) != 40)) {
 *	       		 print_usage(argc, argv); 
 *       		return 0;
 *     		    }
 *	       match_uuid = g_strndup(argv[i], 40);	
 *		}
 */
 /*	} */
 /*	g_strfreev (argv); */
  /*  g_application_release (app); */
/*	return 0; */
/* } */

 /**	       continue;	
 * 	  } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {	
 *		    print_usage(argc, argv);	
 *		      return 0;	
 *		  } 
 *	}	
 * 
 *	g_strfreev (argv);	
 *		
 *	return 0;	
 *  }	
  */  

int main(int argc, char **argv)
{
    
	
 	int status;
	GApplication *app;
	gboolean test = FALSE;
	GError **error = NULL;

/*	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE); */


	/* start up the application window with dbus activation */
	
	app = g_application_new ("github.com.gitsop01.sbmanager", G_APPLICATION_HANDLES_COMMAND_LINE);
	 
	initoptions (app);

	/* FIXME the command-line signal is emitted when a commndline is not handled locally */
	 g_signal_connect (app, "command-line", G_CALLBACK (localcommandline), NULL);
	 g_signal_connect (app, "startup", G_CALLBACK (start), NULL);
	 g_application_set_inactivity_timeout (app, 10000);
	 
	
	test = g_application_register(app, NULL, error);	
	if(test == TRUE) {
		printf("g_application_register returned TRUE\n");
	}
	else if (test == FALSE) {
		printf("g_application_register returned FALSE\n");
	}
	
/*	 g_application_activate ( GApplication (app));   FIXME this function does not work */

/*	 g_signal_connect (app, "startup", G_CALLBACK (start), NULL); */
 	
	status = g_application_run (app, argc, argv);
 
	/* g_signal_connect (app, "activate", G_CALLBACK (start), NULL); */
	/* g_object_unref (app);    */
   
    /* Start the main loop, so we can respond to events */

   gtk_main(); 
  printf("status = %i\n",status);
   return status; 


}
