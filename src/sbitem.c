/**
 * sbitem.c
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

#ifdef HAVE_CONFIG_H
 #include <config.h>
#endif

#include <stdlib.h>

#include "sbitem.h"

char *sbitem_get_display_name(SBItem *item)
{
    char *strval = NULL;
    plist_t node = plist_dict_get_item(item->node, "displayName");
    if (node && plist_get_node_type(node) == PLIST_STRING) {
        plist_get_string_val(node, &strval);
    }
    return strval;
}


char *sbitem_get_display_identifier(SBItem *item)
{
    char *strval = NULL;
    plist_t node = plist_dict_get_item(item->node, "displayIdentifier");
    if (node && plist_get_node_type(node) == PLIST_STRING) {
        plist_get_string_val(node, &strval);
    }
    return strval;
}

SBItem *sbitem_new(plist_t icon_info)
{
    SBItem *item = NULL;

    if (plist_get_node_type(icon_info) != PLIST_DICT) {
        return item;
    }

    item = g_new0(SBItem, 1);
    item->node = plist_copy(icon_info);
    item->texture = NULL;
    item->drawn = FALSE;
    item->is_dock_item = FALSE;
    item->is_folder = FALSE;
    item->enabled = TRUE;
    item->subitems = NULL;

    return item;
}

SBItem *sbitem_new_with_subitems(plist_t icon_info, GList *subitems)
{
    SBItem *item = sbitem_new(icon_info);
    if (item) {
        item->subitems = subitems;
	item->is_folder = TRUE;
    }
    return item;
}

void sbitem_free(SBItem *item)
{
    if (item) {
        if (item->node) {
            plist_free(item->node);
        }
        if (item->texture && CLUTTER_IS_ACTOR(item->texture)) {
            ClutterActor *parent = clutter_actor_get_parent(item->texture);
            if (parent) {
                clutter_actor_destroy(parent);
                item->texture = NULL;
                item->label = NULL;
            } else {
                clutter_actor_destroy(item->texture);
                item->texture = NULL;
            }
        }
        if (item->texture_shadow && CLUTTER_IS_ACTOR(item->texture_shadow)) {
            clutter_actor_destroy(item->texture_shadow);
            item->texture_shadow = NULL;
        }
        if (item->label_shadow && CLUTTER_IS_ACTOR(item->label_shadow)) {
            clutter_actor_destroy(item->label_shadow);
            item->label_shadow = NULL;
        }

        if (item->subitems) {
            g_list_foreach(item->subitems, (GFunc)(g_func_sbitem_free), NULL);
            g_list_free(item->subitems);
        }
        if (item->label && CLUTTER_IS_ACTOR(item->label)) {
            clutter_actor_destroy(item->label);
            item->label = NULL;
        }
        free(item);
    }
}

void g_func_sbitem_free(SBItem *item, gpointer data)
{
    sbitem_free(item);
}

char *sbitem_get_icon_filename(SBItem *item)
{
    static gboolean create_dir = FALSE;
    const char *value;
    char *filename, *path;

    if (create_dir == FALSE) {
      path = g_build_filename (g_get_user_cache_dir (), "libimobiledevice", "icons", NULL);
      if (g_mkdir_with_parents (path, 0755) >= 0)
        create_dir = TRUE;
      g_free (path);
    }

    value = sbitem_get_display_identifier(item);
    if (!value)
        return NULL;

    filename = g_strdup_printf ("%s.png", value);
    path = g_build_filename (g_get_user_cache_dir (),"libimobiledevice", "icons", filename, NULL);
	/* fprintf(stderr,"\n%s:get-icon-filename = %s\n", __func__,path); THIS IS CORRECT TW 03/05/13 */
    g_free (filename);
    return path;
}
