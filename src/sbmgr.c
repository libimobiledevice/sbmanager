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
 *
 */

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <plist/plist.h>

#include "sbmgr.h"
#include "device.h"
#include "gui.h"
#include "utility.h"

static device_info_cb_t device_info_callback = NULL;
static finished_cb_t finished_callback = NULL;

GtkWidget *sbmgr_new()
{
    if (!g_thread_supported())
      /*  g_thread_init(NULL); g_thread_init is deprecated TW 26/04/13 */

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
	
	/* g_thread_create' is deprecated TW 26/04/13 */
    /* g_thread_create((GThreadFunc)gui_pages_load_cb, (gpointer)uuid, FALSE, NULL); */
	const gchar *name1 = "sbloadthd"; /* Name Added for debugging thread TW 20/05/13 */
	g_thread_new(name1, (GThreadFunc)gui_pages_load_cb, (gpointer)uuid);

}

static gboolean iconstate_changed_v1(plist_t current_state, plist_t new_state)
{
    if (!current_state || !new_state) {
        return FALSE;
    }
    /* compare new state with last saved state */
    uint32_t cur_cnt = plist_array_get_size(current_state);
    uint32_t new_cnt = plist_array_get_size(new_state);
    debug_printf("%s: old %d, new %d\n", __func__, cur_cnt, new_cnt);
    if (cur_cnt != new_cnt) {
        /* number of pages has changed! */
        debug_printf("%s: number of pages changed!\n", __func__);
        return TRUE;
    }

    /* now dig deeper */
    uint32_t i;
    for (i = 0; i < new_cnt; i++) {
        plist_t cur_pp = plist_array_get_item(current_state, i);
        plist_t new_pp = plist_array_get_item(new_state, i);
        uint32_t cur_row_cnt = plist_array_get_size(cur_pp);
        uint32_t new_row_cnt = plist_array_get_size(new_pp);	
        if (cur_row_cnt != new_row_cnt) {
            fprintf(stderr, "%s: number of rows on page %d changed: old %d, new %d, this is NOT expected!\n", __func__, i, cur_row_cnt, new_row_cnt);
            return FALSE;
        }
	uint32_t j;
        for (j = 0; j < cur_row_cnt; j++) {
            plist_t cur_row = plist_array_get_item(cur_pp, j);
            plist_t new_row = plist_array_get_item(new_pp, j);
            uint32_t cur_item_cnt = plist_array_get_size(cur_row);
            uint32_t new_item_cnt = plist_array_get_size(new_row);
            if (cur_item_cnt != new_item_cnt) {
                fprintf(stderr, "%s: number of items on page %d row %d changed: old %d, new %d, this is NOT expected!\n", __func__, i, j, cur_item_cnt, new_item_cnt);
                return FALSE;
            }
            uint32_t k;
            for (k = 0; k < cur_item_cnt; k++) {
                plist_t cur_node = plist_array_get_item(cur_row, k);
                plist_t new_node = plist_array_get_item(new_row, k);
		if (plist_get_node_type(cur_node) != plist_get_node_type(new_node)) {
                    /* one is an icon, the other one empty. this is a change! */
                    return TRUE;
                }
                if (plist_get_node_type(cur_node) == PLIST_DICT) {
                    /* ok, compare the displayIdentifier */
                    plist_t cur_di = plist_dict_get_item(cur_node, "displayIdentifier");
                    plist_t new_di = plist_dict_get_item(new_node, "displayIdentifier");
                    if (plist_compare_node_value(cur_di, new_di) == FALSE) {
                        debug_printf("%s: page %d row %d item %d changed!\n", __func__, i, j, k);
                        return TRUE;
                    }
                }
            }
        }
    }

    /* no change found */
    debug_printf("%s: no change!\n", __func__);
    return FALSE;
}

static gboolean iconstate_changed_v2(plist_t current_state, plist_t new_state)
{
    if (!current_state || !new_state) {
        return FALSE;
    }

    /* compare new state with last saved state */
    uint32_t cur_cnt = plist_array_get_size(current_state);
    uint32_t new_cnt = plist_array_get_size(new_state);
    debug_printf("%s: old %d, new %d\n", __func__, cur_cnt, new_cnt);
    if (cur_cnt != new_cnt) {
        /* number of pages has changed! */
        debug_printf("%s: number of pages changed!\n", __func__);
        return TRUE;
    }

    /* now dig deeper */
    uint32_t i;
    for (i = 0; i < new_cnt; i++) {
        plist_t cur_pp = plist_array_get_item(current_state, i);
        plist_t new_pp = plist_array_get_item(new_state, i);
        if (plist_get_node_type(plist_array_get_item(new_pp, 0)) == PLIST_ARRAY) {
            new_pp = plist_array_get_item(new_pp, 0);
        }
        if (plist_get_node_type(plist_array_get_item(cur_pp, 0)) == PLIST_ARRAY) {
            cur_pp = plist_array_get_item(cur_pp, 0);
        }

        uint32_t cur_item_cnt = plist_array_get_size(cur_pp);
        uint32_t new_item_cnt = plist_array_get_size(new_pp);	
        if (cur_item_cnt != new_item_cnt) {
            fprintf(stderr, "%s: number of items on page %d changed: old %d, new %d\n", __func__, i, cur_item_cnt, new_item_cnt);
            return TRUE;
        }
        uint32_t k;
        for (k = 0; k < cur_item_cnt; k++) {
            plist_t cur_node = plist_array_get_item(cur_pp, k);
            plist_t new_node = plist_array_get_item(new_pp, k);
            if (plist_get_node_type(cur_node) == PLIST_DICT) {
                /* ok, compare the displayIdentifier */
                plist_t cur_di = plist_dict_get_item(cur_node, "displayIdentifier");
                plist_t new_di = plist_dict_get_item(new_node, "displayIdentifier");
                if (!cur_di && !new_di) {
                    cur_di = plist_dict_get_item(cur_node, "displayName");
                    new_di = plist_dict_get_item(new_node, "displayName");
                    if (plist_compare_node_value(cur_di, new_di) == FALSE) {
                        debug_printf("%s: page %d folder item %d displayName changed!\n", __func__, i, k);
                        return TRUE;
                    }
                    cur_di = plist_dict_get_item(cur_node, "iconLists");
                    new_di = plist_dict_get_item(new_node, "iconLists");
                    cur_di = plist_array_get_item(cur_di, 0);
                    new_di = plist_array_get_item(new_di, 0);
                    if (plist_array_get_size(cur_di) != plist_array_get_size(new_di)) {
                        debug_printf("%s: page %d folder item %d subitem count changed!\n", __func__, i, k);
                        return TRUE;
                    }
                    uint32_t j;
                    for (j = 0; j < plist_array_get_size(cur_di); j++) {
                        plist_t cur_si = plist_array_get_item(cur_di, j);
                        plist_t new_si = plist_array_get_item(new_di, j);
                        if (plist_compare_node_value(cur_si, new_si) == FALSE) {
                            debug_printf("%s: page %d folder item %d subitem %d changed!\n", __func__, i, k, j);
                            return TRUE;
                        }
                    }
                } else if ((!cur_di && new_di) || (cur_di && !new_di)) {
                    debug_printf("%s: page %d item %d changed!\n", __func__, i, k);
                    return TRUE;
                } else {
                    if (plist_compare_node_value(cur_di, new_di) == FALSE) {
                        debug_printf("%s: page %d item %d changed!\n", __func__, i, k);
                        return TRUE;
                    }
                }
            }
        }
    }

    /* no change found */
    debug_printf("%s: no change!\n", __func__);
    return FALSE;
}

static gboolean iconstate_changed(plist_t current_state, plist_t new_state, const char *format_version)
{
    if (format_version && (strcmp(format_version, "2") == 0)) {
        return iconstate_changed_v2(current_state, new_state);
    } else {
        return iconstate_changed_v1(current_state, new_state);
    }
}

void sbmgr_save(const char *uuid)
{
    GError *error = NULL;
    sbservices_client_t sbc;
    uint32_t osversion = 0;

    sbc = device_sbs_new(uuid, &osversion, &error);

    if (error) {
        g_printerr("%s", error->message);
        g_error_free(error);
        error = NULL;
    }

    if (sbc) {
        plist_t current_state = NULL;
        const char *fmt_version = NULL;
        if (osversion >= 0x04000000) {
            fmt_version = "2";
        }
        plist_t iconstate = gui_get_iconstate(fmt_version);
        if (iconstate) {
            device_sbs_get_iconstate(sbc, &current_state, fmt_version, &error);
            if (error) {
                g_printerr("%s", error->message);
                g_error_free(error);
                error = NULL;
            }
            if (iconstate_changed(current_state, iconstate, fmt_version) == TRUE) {
                device_sbs_set_iconstate(sbc, iconstate, &error);
            }
            if (current_state) {
                plist_free(current_state);
            }
            plist_free(iconstate);
	}
        device_sbs_free(sbc);
    }

    if (error) {
        g_printerr("%s", error->message);
        g_error_free(error);
        error = NULL;
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
