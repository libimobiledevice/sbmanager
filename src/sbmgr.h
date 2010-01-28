/**
 * sbmgr.h
 * SBManager Widget definitions.
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

#ifndef SBMGR_H
#define SBMGR_H

#include <gtk/gtk.h>

typedef void (*device_info_cb_t)(const char *device_name, const char *device_type);
typedef void (*finished_cb_t)(gboolean success);

GtkWidget *sbmgr_new();
void sbmgr_load(const char *uuid, device_info_cb_t info_callback, finished_cb_t finished_callback);
void sbmgr_save(const char *uuid);
void sbmgr_cleanup();
void sbmgr_finalize();

#endif
