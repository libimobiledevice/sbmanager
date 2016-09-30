/**
 * sb.app.h -- Sbmanager app header
 *
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

#ifndef __SB_APP_H__
#define __SB_APP_H__

#include <gtk/gtk.h>
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
#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h> 
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "sbmgr.h"
#include "utility.h"


G_BEGIN_DECLS

#define SB_TYPE_APP         (sb_app_get_type ())
#define SB_APP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), SB_TYPE_APP, SbApp))
#define SB_APP_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), SB_TYPE_APP, SbAppClass))
#define SB_IS_APP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), SB_TYPE_APP))
#define SB_IS_APP_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), SB_TYPE_APP))
#define SB_APP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), SB_TYPE_APP, SbAppClass))

typedef struct _SbApp        SbApp;
typedef struct _SbAppClass   SbAppClass;

struct _SbApp {
        GtkApplication parent_instance;
};

struct _SbAppClass {
        GtkApplicationClass parent_class;
};


gboolean reload_button_clicked_cb(GtkButton *button, gpointer user_data);
gboolean apply_button_clicked_cb(GtkButton *button, gpointer user_data);




void gui_error_dialog(const gchar *string);
void finished_cb(gboolean success);
gpointer device_add_cb(gpointer user_data);
void update_device_info_cb(const char *device_name, const char *device_type);
void device_event_cb(const idevice_event_t *event, void *user_data);



GType sb_app_get_type (void) G_GNUC_CONST;

SbApp         *sb_app_new               (void);
void           app_quit              (SbApp *self);
SbApp 		*sb_app_start (GApplication *application);


G_END_DECLS

#endif /* __SB_APP_H__ */


