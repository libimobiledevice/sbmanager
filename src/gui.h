/**
 * gui.h
 * GUI definitions.
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

#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include <plist/plist.h>
#include "sbmgr.h"

typedef struct {
    char *uuid;
    device_info_t device_info;
} SBManagerData;

GtkWidget *gui_init();
void gui_deinit();
void gui_pages_load(const char *uuid, device_info_cb_t info_callback, finished_cb_t finshed_callback);
void gui_pages_free();

plist_t gui_get_iconstate(const char *format_version);


#endif
