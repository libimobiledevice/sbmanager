/*
 * nautilus-sbmanager.c
 * 
 * Copyright (C) 2010 Nikias Bassen <nikias@gmx.li>
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

#include "nautilus-sbmanager.h"

#include "../src/sbmgr.h"

#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-extension/nautilus-property-page-provider.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h> /* for atoi */
#include <string.h> /* for strcmp */
#include <unistd.h> /* for chdir */
#include <sys/stat.h>

static void nautilus_sbmanager_instance_init (NautilusSBManager      *cvs);
static void nautilus_sbmanager_class_init    (NautilusSBManagerClass *class);

static GType sbmanager_type = 0;

static gboolean loading = FALSE;

static void launch_sbmanager (NautilusMenuItem *item)
{
	GdkScreen *screen;
	GError *error = NULL;
	gchar *argvp[4] = {(char*)"sbmanager", (char*)"-u", NULL, NULL};
	argvp[2] = g_object_get_data (G_OBJECT (item), "NautilusSBManager::uuid");

	screen = g_object_get_data (G_OBJECT (item), "NautilusSBManager::screen");
	gdk_spawn_on_screen(screen, NULL, argvp, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);
	if (error) {
		g_error_free(error);
	}
}

static void launch_sbmanager_callback (NautilusMenuItem *item, const char *uuid)
{
	launch_sbmanager (item);
}

static NautilusMenuItem *launch_sbmanager_menu_item_new (const char *uuid, GdkScreen *screen)
{
	NautilusMenuItem *ret = NULL;
	char *action_name = g_strdup_printf("NautilusSBManager::launch_sbmanager");
	const char *name = _("Manage _SpringBoard");
	const char *tooltip = _("Launch SBManager to manage the SpringBoard of this device");

	ret = nautilus_menu_item_new (action_name, name, tooltip, "phone-apple-iphone");
	g_free (action_name);

	g_object_set_data (G_OBJECT (ret),
			   "NautilusSBManager::screen",
			   screen);
	g_object_set_data (G_OBJECT (ret),
			   "NautilusSBManager::uuid",
			   (gpointer)g_strdup(uuid));
	g_signal_connect (ret, "activate",
			  G_CALLBACK (launch_sbmanager_callback),
			  NULL);

	return ret;
}

GList *
nautilus_launch_sbmanager_get_file_items (NautilusMenuProvider *provider,
				       GtkWidget            *window,
				       GList                *files)
{
	gchar *uri;
	gchar *uri_scheme;
	GList *items;
	NautilusMenuItem *item;

	if (g_list_length (files) != 1
	    || ((nautilus_file_info_get_file_type (files->data) != G_FILE_TYPE_SHORTCUT)
	     && (nautilus_file_info_get_file_type (files->data) != G_FILE_TYPE_MOUNTABLE))) {
		return NULL;
	}

	items = NULL;
	uri = nautilus_file_info_get_activation_uri (files->data);
	uri_scheme = g_uri_parse_scheme (uri);
	if ((strcmp(uri_scheme, "afc") == 0) && g_find_program_in_path("sbmanager")) {
		gchar *uuid = g_strndup(uri + 6, 40);
		item = launch_sbmanager_menu_item_new (uuid, gtk_widget_get_screen (window));
		g_free(uuid);
		items = g_list_append (items, item);
	}
	g_free(uri_scheme);
	g_free (uri);

	return items;
}

static void
nautilus_launch_sbmanager_menu_provider_iface_init (NautilusMenuProviderIface *iface)
{
	iface->get_background_items = NULL;
	iface->get_file_items = nautilus_launch_sbmanager_get_file_items;
}

static void nautilus_sbmgr_load_finished(gboolean success)
{
	loading = FALSE;
}

static gboolean nautilus_sbmgr_expose_cb(GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
	if (loading == FALSE) {
		loading = TRUE;
		sbmgr_load((const char*)g_object_get_data(G_OBJECT (widget), "NautilusSBManager::uuid"), NULL, nautilus_sbmgr_load_finished);
	}
	return TRUE;
}

static gboolean nautilus_sbmgr_unrealize_cb(GtkWidget *widget, gpointer user_data)
{
	sbmgr_save((const char*)g_object_get_data(G_OBJECT (widget), "NautilusSBManager::uuid"));
	sbmgr_finalize();

	if (user_data) {
		GtkWidget *sbmgr_widget = (GtkWidget*)user_data;
		gtk_widget_destroy(sbmgr_widget);
	}

	return TRUE;
}

static GtkWidget *nautilus_sbmgr_new(const char *uuid)
{
	GtkWidget *sbmgr_widget = sbmgr_new();

	if (!sbmgr_widget) {
		return NULL;
	}

	GtkWidget *sbmgr_container = gtk_alignment_new(0.5, 0.0, 0.0, 0.0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(sbmgr_container), 10, 10, 10, 10);
	gtk_container_add(GTK_CONTAINER(sbmgr_container), sbmgr_widget);
	gtk_widget_show_all(sbmgr_container);

	g_object_set_data (G_OBJECT (sbmgr_container),
			   "NautilusSBManager::uuid",
			   (gpointer)g_strdup(uuid));

	g_signal_connect (G_OBJECT (sbmgr_container),
			  "expose-event",
			  G_CALLBACK(nautilus_sbmgr_expose_cb),
			  NULL);

	g_signal_connect (G_OBJECT (sbmgr_container),
			  "unrealize",
			  G_CALLBACK(nautilus_sbmgr_unrealize_cb),
			  sbmgr_widget);

	return sbmgr_container;
}

static NautilusPropertyPage *sbmanager_property_page_new(NautilusPropertyPageProvider *provider, const char *uuid)
{
	NautilusPropertyPage *ret;
	GtkWidget *page;

	page = nautilus_sbmgr_new(uuid);

	ret = nautilus_property_page_new ("sbmanager-page", gtk_label_new("SpringBoard"), page);

	return ret;
}

GList *nautilus_sbmanager_property_page (NautilusPropertyPageProvider *provider, GList *files)
{
	GList *pages;
	NautilusPropertyPage *page;
	gchar *uri;
	gchar *uri_scheme;

	if (g_list_length (files) != 1
	    || ((nautilus_file_info_get_file_type (files->data) != G_FILE_TYPE_SHORTCUT)
	     && (nautilus_file_info_get_file_type (files->data) != G_FILE_TYPE_MOUNTABLE))) {
		return NULL;
	}

	pages = NULL;
	uri = nautilus_file_info_get_activation_uri (files->data);
	uri_scheme = g_uri_parse_scheme (uri);
	if ((strcmp(uri_scheme, "afc") == 0) && g_find_program_in_path("sbmanager")) {
		gchar *uuid = g_strndup(uri + 6, 40);
		page = sbmanager_property_page_new(provider, uuid);
		g_free(uuid);
		pages = g_list_append(pages, page);
	}
	g_free(uri_scheme);
	g_free (uri);

	return pages;
}

static void
nautilus_sbmanager_property_page_provider_iface_init (NautilusPropertyPageProviderIface *iface)
{
	iface->get_pages = nautilus_sbmanager_property_page;
}

static void 
nautilus_sbmanager_instance_init (NautilusSBManager *cvs)
{
}

static void
nautilus_sbmanager_class_init (NautilusSBManagerClass *class)
{
}

static void
nautilus_sbmanager_class_finalize (NautilusSBManagerClass *class)
{
}

GType
nautilus_sbmanager_get_type (void) 
{
	return sbmanager_type;
}

void
nautilus_sbmanager_register_type (GTypeModule *module)
{
	static const GTypeInfo info = {
		sizeof (NautilusSBManagerClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) nautilus_sbmanager_class_init,
		(GClassFinalizeFunc) nautilus_sbmanager_class_finalize,
		NULL,
		sizeof (NautilusSBManager),
		0,
		(GInstanceInitFunc) nautilus_sbmanager_instance_init,
		NULL
	};

	static const GInterfaceInfo menu_provider_iface_info = {
		(GInterfaceInitFunc) nautilus_launch_sbmanager_menu_provider_iface_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo property_page_iface_info = {
		(GInterfaceInitFunc) nautilus_sbmanager_property_page_provider_iface_init,
		NULL,
		NULL
	};

	sbmanager_type = g_type_module_register_type (module,
						     G_TYPE_OBJECT,
						     "NautilusSBManager",
						     &info, 0);

	g_type_module_add_interface (module,
				     sbmanager_type,
				     NAUTILUS_TYPE_MENU_PROVIDER,
				     &menu_provider_iface_info);

	g_type_module_add_interface (module,
				     sbmanager_type,
				     NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER,
				     &property_page_iface_info);
}
