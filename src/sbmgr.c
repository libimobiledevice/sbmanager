/**
 * sbmgr.c
 * SBManager Widget implementation.
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

#include <glib.h>
#include <gtk/gtk.h>

#include "sbmgr.h"
#include "device.h"
#include "gui.h"

static device_info_cb_t device_info_callback = NULL;
static finished_cb_t finished_callback = NULL;

GtkWidget *sbmgr_new()
{
    if (!g_thread_supported())
        g_thread_init(NULL);

    /* initialize device communication environment */
    device_init();

    /* Create the clutter widget and return it */
    return gui_init();
}

static gpointer gui_pages_load_cb(gpointer user_data)
{
    const char *uuid = (const char*)user_data;
    gui_pages_load(uuid, device_info_callback, finished_callback);
    return NULL;
}

void sbmgr_load(const char *uuid, device_info_cb_t info_cb, finished_cb_t finished_cb)
{
    /* load icons */
    device_info_callback = info_cb;
    finished_callback = finished_cb;
    g_thread_create((GThreadFunc)gui_pages_load_cb, (gpointer)uuid, FALSE, NULL);
}

void sbmgr_save(const char *uuid)
{
    plist_t iconstate = gui_get_iconstate();
    if (iconstate) {
        GError *error = NULL;
        sbservices_client_t sbc;

        sbc = device_sbs_new(uuid, &error);

        if (error) {
            g_printerr("%s", error->message);
            g_error_free(error);
            error = NULL;
        }

        if (sbc) {
            device_sbs_set_iconstate(sbc, iconstate, &error);
            device_sbs_free(sbc);
	}
        plist_free(iconstate);

        if (error) {
            g_printerr("%s", error->message);
            g_error_free(error);
            error = NULL;
        }
    }
}

void sbmgr_cleanup()
{
    gui_pages_free();
}

void sbmgr_finalize()
{ 
    sbmgr_cleanup();
    gui_deinit();
}
