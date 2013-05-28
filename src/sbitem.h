/**
 * sbitem.h
 * SpringBoard Item representation
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

#ifndef SBITEM_H
#define SBITEM_H

#include <glib.h>
#include <clutter/clutter.h>
#include <plist/plist.h>

typedef struct {
    plist_t node;
    ClutterActor *texture; 
    ClutterActor *texture_shadow;
    ClutterActor *label;
    ClutterActor *label_shadow;
    gboolean drawn;
    gboolean is_dock_item;
    gboolean is_folder;
    gboolean enabled;
    GList *subitems;
} SBItem;

char *sbitem_get_display_name(SBItem *item);
char *sbitem_get_display_identifier(SBItem *item);
char *sbitem_get_icon_filename(SBItem *item);

SBItem *sbitem_new(plist_t icon_info);
SBItem *sbitem_new_with_subitems(plist_t icon_info, GList *subitems);
void sbitem_free(SBItem *item);

void g_func_sbitem_free(SBItem *item, gpointer data);

#endif
