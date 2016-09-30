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
#include <sb.app.h>

GtkWidget *main_window;

void gui_error_dialog(const gchar *string)
{
    GtkWidget *dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW(main_window), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "<b>%s</b>", _("Error"));
    gtk_window_set_title(GTK_WINDOW(dialog), PACKAGE_NAME);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", string);
    g_signal_connect_swapped (dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog); 
    gtk_widget_show(dialog);
}

gpointer device_add_cb(gpointer user_data)
{
    const char *uuid = (const char*)user_data;
    sbmgr_load(uuid, update_device_info_cb, finished_cb);
    return NULL;
}



int main(int argc, char **argv)
{

 	int status;
	SbApp   *application;
	gboolean test = FALSE;
	GError **error = NULL;

/*	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE); */


	/* start up the application window with dbus activation */
	/* FIXME START GTK-INSPECTOR - interactive debugger */
	 gtk_window_set_interactive_debugging(TRUE); 

	application = sb_app_new();
	test = g_application_register (G_APPLICATION (application), NULL, error);
	if(test == TRUE) {
		printf("g_application_register returned TRUE\n");
	}
	else if (test == FALSE) {
		printf("g_application_register returned FALSE\n");
	}
	
	g_application_hold (G_APPLICATION (application));

	status = g_application_run (G_APPLICATION (application), argc, argv);

	g_object_unref (application);

    debug_printf("status = %i\n",status);
    return status;
}
