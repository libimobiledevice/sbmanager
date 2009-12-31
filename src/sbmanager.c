/**
 * sbmanager -- Manage iPhone/iPod Touch SpringBoard icons from your computer!
 *
 * Copyright (C) 2009 Nikias Bassen <nikias@gmx.li>
 * Copyright (C) 2009 Martin Szulecki <opensuse@sukimashita.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libiphone/libiphone.h>
#include <libiphone/lockdown.h>
#include <libiphone/sbservices.h>
#include <plist/plist.h>
#include <time.h>
#include <sys/time.h>
#include <glib.h>

#include <gtk/gtk.h>
#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define STAGE_WIDTH 320
#define STAGE_HEIGHT 480
#define DOCK_HEIGHT 90

const char CLOCK_FONT[] = "FreeSans Bold 12px";
ClutterColor clock_text_color = { 255, 255, 255, 210 };

const char ITEM_FONT[] = "FreeSans Bold 10px";
ClutterColor item_text_color = { 255, 255, 255, 210 };
ClutterColor dock_item_text_color = { 255, 255, 255, 255 };

typedef struct {
    GtkWidget *window;
    char *uuid;
    plist_t battery;
    char *device_name;
    char *device_type;
} SBManagerApp;

typedef struct {
    plist_t node;
    ClutterActor *texture;
    ClutterActor *label;
    gboolean is_dock_item;
} SBItem;

const ClutterActorBox dock_area = { 0.0, STAGE_HEIGHT - DOCK_HEIGHT, STAGE_WIDTH, STAGE_HEIGHT };

const ClutterActorBox sb_area = { 0.0, 16.0, STAGE_WIDTH, STAGE_HEIGHT - DOCK_HEIGHT - 16.0 };

const ClutterActorBox left_trigger = { -30.0, 16.0, -8.0, STAGE_HEIGHT - DOCK_HEIGHT - 16.0 };
const ClutterActorBox right_trigger = { STAGE_WIDTH + 8.0, 16.0, STAGE_WIDTH + 30.0, STAGE_HEIGHT - DOCK_HEIGHT - 16.0 };

ClutterActor *stage = NULL;
ClutterActor *the_dock = NULL;
ClutterActor *the_sb = NULL;
ClutterActor *type_label = NULL;
ClutterActor *clock_label = NULL;
ClutterActor *battery_level = NULL;
ClutterActor *page_indicator = NULL;
ClutterActor *page_indicator_group = NULL;

GMutex *selected_mutex = NULL;
SBItem *selected_item = NULL;

gfloat start_x = 0.0;
gfloat start_y = 0.0;

gboolean move_left = TRUE;

GList *dockitems = NULL;
GList *sbpages = NULL;

guint num_dock_items = 0;

int current_page = 0;
struct timeval last_page_switch;

static void dock_align_icons(gboolean animated);
static void sb_align_icons(guint page_num, gboolean animated);
static void redraw_icons();

gboolean debug_app = FALSE;
static void debug_printf(const char *format, ...);

static void debug_printf(const char *format, ...)
{
    if (debug_app) {
        va_list args;
        va_start (args, format);
        vprintf(format, args);
        va_end (args);
    }
}

static gboolean elapsed_ms(struct timeval *tv, guint ms)
{
    struct timeval now;
    guint64 v1,v2;

    if (!tv) return FALSE;

    gettimeofday(&now, NULL);

    v1 = now.tv_sec*1000000 + now.tv_usec;
    v2 = tv->tv_sec*1000000 + tv->tv_usec;

    if (v1 < v2) {
        return TRUE;
    }

    if ((v1 - v2)/1000 > ms) {
        return TRUE;
    } else {
        return FALSE;
    }
}

static char *sbitem_get_display_name(SBItem *item)
{
    char *strval = NULL;
    plist_t node = plist_dict_get_item(item->node, "displayName");
    if (node && plist_get_node_type(node) == PLIST_STRING) {
        plist_get_string_val(node, &strval);
    }
    return strval;
}

static void sbitem_free(SBItem *a)
{
    if (a) {
        if (a->node) {
            plist_free(a->node);
        }
        if (a->texture) {
            free(a->texture);
        }
    }
}

static void sbpage_free(GList *sbitems)
{
    if (sbitems) {
        g_list_foreach(sbitems, (GFunc) (sbitem_free), NULL);
        g_list_free(sbitems);
    }
}

static void get_icon_for_node(plist_t node, GList **list, sbservices_client_t sbc)
{
    char *png = NULL;
    uint64_t pngsize = 0;
    SBItem *di = NULL;
    plist_t valuenode = NULL;
    if (plist_get_node_type(node) != PLIST_DICT) {
        return;
    }
    valuenode = plist_dict_get_item(node, "displayIdentifier");
    if (valuenode && (plist_get_node_type(valuenode) == PLIST_STRING)) {
        char *value = NULL;
        char *icon_filename = NULL;
        plist_get_string_val(valuenode, &value);
        debug_printf("retrieving icon for '%s'\n", value);
        if ((sbservices_get_icon_pngdata(sbc, value, &png, &pngsize) == SBSERVICES_E_SUCCESS) && (pngsize > 0)) {
            icon_filename = g_strdup_printf("/tmp/%s.png", value);
            FILE *f = fopen(icon_filename, "w");
            GError *err = NULL;
            fwrite(png, 1, pngsize, f);
            fclose(f);
            ClutterActor *actor = clutter_texture_new();
            clutter_texture_set_load_async(CLUTTER_TEXTURE(actor), TRUE);
            clutter_texture_set_from_file(CLUTTER_TEXTURE(actor), icon_filename, &err);
            g_free(icon_filename);
            if (actor) {
                plist_t nn = plist_dict_get_item(node, "displayName");
                di = g_new0(SBItem, 1);
                di->node = plist_copy(node);
                di->texture = actor;
                if (nn && (plist_get_node_type(nn) == PLIST_STRING)) {
                    char *txtval = NULL;
                    plist_get_string_val(nn, &txtval);
                    actor = clutter_text_new_with_text(ITEM_FONT, txtval);
                    di->label = actor;
                }
                *list = g_list_append(*list, di);
            }
            if (err) {
                fprintf(stderr, "ERROR: %s\n", err->message);
                g_error_free(err);
            }
        } else {
            fprintf(stderr, "ERROR: could not get icon for '%s'\n", value);
        }
        free(value);
    }
    if (png) {
        free(png);
    }
}

static void page_indicator_group_align()
{
    gint count = clutter_group_get_n_children(CLUTTER_GROUP(page_indicator_group));
    gint i;
    gfloat xpos = 0.0;

    if (count <= 0)
        return;

    for (i = 0; i < count; i++) {
        ClutterActor *dot = clutter_group_get_nth_child(CLUTTER_GROUP(page_indicator_group), i);
        clutter_actor_set_position(dot, xpos, 0.0);
        if (i == current_page) {
            clutter_actor_set_opacity(dot, 200);
        } else {
            clutter_actor_set_opacity(dot, 100);
        }
        xpos += clutter_actor_get_width(dot);
    }

    clutter_actor_set_x(page_indicator_group, (STAGE_WIDTH - xpos) / 2.0);
}

static void sbpages_set_current_page(int pageindex)
{
    gint count = clutter_group_get_n_children(CLUTTER_GROUP(page_indicator_group));

    if ((pageindex < 0) || (pageindex >= count))
        return;

    current_page = pageindex;

    page_indicator_group_align();

    clutter_actor_animate(the_sb, CLUTTER_EASE_IN_OUT_CUBIC, 400, "x", (gfloat) (-(current_page * STAGE_WIDTH)), NULL);
}

static void sbpages_show_next_page()
{
    sbpages_set_current_page(current_page+1);
}

static void sbpages_show_previous_page()
{
    sbpages_set_current_page(current_page-1);
}

static gboolean page_indicator_clicked(ClutterActor *actor, ClutterEvent *event, gpointer data)
{
    sbpages_set_current_page(GPOINTER_TO_UINT(data));

    return TRUE;
}

static gboolean get_icons(gpointer data)
{
    SBManagerApp *app = (SBManagerApp*)data;
    iphone_device_t phone = NULL;
    lockdownd_client_t client = NULL;
    sbservices_client_t sbc = NULL;
    int port = 0;
    plist_t iconstate = NULL;
    int total;

    if (sbpages) {
        g_list_foreach(sbpages, (GFunc)(sbpage_free), NULL);
        g_list_free(sbpages);
        sbpages = NULL;
    }
    if (dockitems) {
        sbpage_free(dockitems);
        dockitems = NULL;
    }

    if (IPHONE_E_SUCCESS != iphone_device_new(&phone, app->uuid)) {
        fprintf(stderr, "No iPhone found, is it plugged in?\n");
        return FALSE;
    }

    if (LOCKDOWN_E_SUCCESS != lockdownd_client_new(phone, &client)) {
        fprintf(stderr, "Could not connect to lockdownd. Exiting.\n");
        goto leave_cleanup;
    }

    if ((lockdownd_start_service(client, "com.apple.springboardservices", &port) != LOCKDOWN_E_SUCCESS) || !port) {
        fprintf(stderr,
                "Could not start com.apple.springboardservices service! Remind that this feature is only supported in OS 3.1 and later!\n");
        goto leave_cleanup;
    }
    if (sbservices_client_new(phone, port, &sbc) != SBSERVICES_E_SUCCESS) {
        fprintf(stderr, "Could not connect to springboardservices!\n");
        goto leave_cleanup;
    }
    if (sbservices_get_icon_state(sbc, &iconstate) != SBSERVICES_E_SUCCESS || !iconstate) {
        fprintf(stderr, "ERROR: Could not get icon state!\n");
        goto leave_cleanup;
    }
    if (plist_get_node_type(iconstate) != PLIST_ARRAY) {
        fprintf(stderr, "ERROR: icon state is not an array as expected!\n");
        goto leave_cleanup;
    }
    total = plist_array_get_size(iconstate);

    if (total < 1) {
        fprintf(stderr, "ERROR: No icons returned in icon state\n");
        goto leave_cleanup;
    } else {
        int i;
        int count;
        plist_t dock = plist_array_get_item(iconstate, 0);
        if ((plist_get_node_type(dock) != PLIST_ARRAY)
            || (plist_array_get_size(dock) < 1)) {
            fprintf(stderr, "ERROR: error getting outer dock icon array!\n");
            goto leave_cleanup;
        }
        dock = plist_array_get_item(dock, 0);
        if (plist_get_node_type(dock) != PLIST_ARRAY) {
            fprintf(stderr, "ERROR: error getting inner dock icon array!\n");
            goto leave_cleanup;
        }
        count = plist_array_get_size(dock);
        num_dock_items = count;
        for (i = 0; i < count; i++) {
            plist_t node = plist_array_get_item(dock, i);
            get_icon_for_node(node, &dockitems, sbc);
        }
        if (total > 1) {
            /* get all icons for the other pages */
            int p, r, rows;
            for (p = 1; p < total; p++) {
                plist_t npage = plist_array_get_item(iconstate, p);
                GList *page = NULL;
                if ((plist_get_node_type(npage) != PLIST_ARRAY)
                    || (plist_array_get_size(npage) < 1)) {
                    fprintf(stderr, "ERROR: error getting outer page icon array!\n");
                    goto leave_cleanup;
                }
                rows = plist_array_get_size(npage);
                for (r = 0; r < rows; r++) {
                    debug_printf("page %d, row %d\n", p, r);

                    plist_t nrow = plist_array_get_item(npage, r);
                    if (plist_get_node_type(nrow) != PLIST_ARRAY) {
                        fprintf(stderr, "ERROR: error getting page row icon array!\n");
                        goto leave_cleanup;
                    }
                    count = plist_array_get_size(nrow);
                    for (i = 0; i < count; i++) {
                        plist_t node = plist_array_get_item(nrow,
                                                            i);
                        get_icon_for_node(node, &page, sbc);
                    }
                }
                if (page) {
                    sbpages = g_list_append(sbpages, page);
                    if (page_indicator) {
                        ClutterActor *actor = clutter_clone_new(page_indicator);
                        clutter_actor_unparent(actor);
                        clutter_actor_set_reactive(actor, TRUE);
                        g_signal_connect(actor,
                                         "button-press-event",
                                         G_CALLBACK(page_indicator_clicked), GUINT_TO_POINTER(p - 1));
                        clutter_container_add_actor(CLUTTER_CONTAINER(page_indicator_group), actor);
                        page_indicator_group_align();
                    }
                }
            }
        }
    }

    redraw_icons();

  leave_cleanup:
    if (iconstate) {
        plist_free(iconstate);
    }
    if (sbc) {
        sbservices_client_free(sbc);
    }
    if (client) {
        lockdownd_client_free(client);
    }
    iphone_device_free(phone);

    return FALSE;
}

static void clock_set_time(ClutterActor *label, time_t t)
{
    struct tm *curtime = localtime(&t);
    gchar *ctext = g_strdup_printf("%02d:%02d", curtime->tm_hour, curtime->tm_min);
    clutter_text_set_text(CLUTTER_TEXT(label), ctext);
    clutter_actor_set_position(label, (clutter_actor_get_width(stage) - clutter_actor_get_width(label)) / 2, 2);
    g_free(ctext);
}

static void clock_update_cb(ClutterTimeline *timeline, gint msecs, SBManagerApp *app)
{
    clock_set_time(clock_label, time(NULL));
}

static void actor_get_abs_center(ClutterActor *actor, gfloat *center_x, gfloat *center_y)
{
    *center_x = 0.0;
    *center_y = 0.0;
    if (!actor)
        return;
    clutter_actor_get_scale_center(actor, center_x, center_y);
    *center_x += clutter_actor_get_x(actor);
    *center_y += clutter_actor_get_y(actor);
}

static gboolean item_button_press(ClutterActor *actor, ClutterButtonEvent *event, gpointer user_data)
{
    if (!user_data) {
        return FALSE;
    }

    if (selected_item) {
        /* do not allow a button_press event without a prior release */
        return FALSE;
    }

    SBItem *item = (SBItem*)user_data;

    char *strval = NULL;
    plist_t node = plist_dict_get_item(item->node, "displayName");
    if (node && plist_get_node_type(node) == PLIST_STRING) {
        plist_get_string_val(node, &strval);
    }

    g_mutex_lock(selected_mutex);
    debug_printf("%s: %s mouse pressed\n", __func__, strval);

    if (actor) {
        gfloat diffx = 0.0;
        gfloat diffy = 0.0;
        ClutterActor *sc = clutter_actor_get_parent(actor);
        if (item->is_dock_item) {
            GList *children = clutter_container_get_children(CLUTTER_CONTAINER(sc));
            if (children) {
                ClutterActor *icon = g_list_nth_data(children, 0);
                ClutterActor *label = g_list_nth_data(children, 1);
                clutter_text_set_color(CLUTTER_TEXT(label), &item_text_color);
                clutter_actor_set_y(label, clutter_actor_get_y(icon) + 62.0);
                g_list_free(children);
            }
            diffx = dock_area.x1;
            diffy = dock_area.y1;
        } else {
            diffx = sb_area.x1 - (current_page * STAGE_WIDTH);
            diffy = sb_area.y1;
        }
        clutter_actor_reparent(sc, stage);
        clutter_actor_set_position(sc, clutter_actor_get_x(sc) + diffx, clutter_actor_get_y(sc) + diffy);
        clutter_actor_raise_top(sc);
        clutter_actor_set_scale_full(sc, 1.2, 1.2,
                                     clutter_actor_get_x(actor) +
                                     clutter_actor_get_width(actor) / 2,
                                     clutter_actor_get_y(actor) + clutter_actor_get_height(actor) / 2);
        clutter_actor_set_opacity(sc, 160);
        selected_item = item;
        start_x = event->x;
        start_y = event->y;
    }
    g_mutex_unlock(selected_mutex);

    return TRUE;
}

static gboolean item_button_release(ClutterActor *actor, ClutterButtonEvent *event, gpointer user_data)
{
    if (!user_data) {
        return FALSE;
    }

    SBItem *item = (SBItem*)user_data;
    char *strval = NULL;
    plist_t node = plist_dict_get_item(item->node, "displayName");
    if (node && plist_get_node_type(node) == PLIST_STRING) {
        plist_get_string_val(node, &strval);
    }

    g_mutex_lock(selected_mutex);
    debug_printf("%s: %s mouse released\n", __func__, strval);

    if (actor) {
        ClutterActor *sc = clutter_actor_get_parent(actor);
        clutter_actor_set_scale_full(sc, 1.0, 1.0,
                                     clutter_actor_get_x(actor) +
                                     clutter_actor_get_width(actor) / 2,
                                     clutter_actor_get_y(actor) + clutter_actor_get_height(actor) / 2);
        clutter_actor_set_opacity(sc, 255);
        if (item->is_dock_item) {
            GList *children = clutter_container_get_children(CLUTTER_CONTAINER(sc));
            if (children) {
                ClutterActor *icon = g_list_nth_data(children, 0);
                ClutterActor *label = g_list_nth_data(children, 1);
                clutter_text_set_color(CLUTTER_TEXT(label), &dock_item_text_color);
                clutter_actor_set_y(label, clutter_actor_get_y(icon) + 67.0);
                g_list_free(children);
            }
            clutter_actor_reparent(sc, the_dock);
            clutter_actor_set_position(sc,
                                       clutter_actor_get_x(sc) - dock_area.x1, clutter_actor_get_y(sc) - dock_area.y1);
        } else {
            clutter_actor_reparent(sc, the_sb);
            clutter_actor_set_position(sc,
                                       clutter_actor_get_x(sc) +
                                       (current_page * STAGE_WIDTH) - sb_area.x1, clutter_actor_get_y(sc) - sb_area.y1);
        }
    }

    selected_item = NULL;
    dock_align_icons(TRUE);
    sb_align_icons(current_page, TRUE);
    start_x = 0.0;
    start_y = 0.0;

    g_mutex_unlock(selected_mutex);

    return TRUE;
}

static void dock_align_icons(gboolean animated)
{
    if (!dockitems)
        return;
    gint count = g_list_length(dockitems);
    if (count == 0) {
        return;
    }
    gfloat spacing = 16.0;
    gfloat ypos = 8.0;
    gfloat xpos = 0.0;
    gint i = 0;
    if (count > 4) {
        spacing = 3.0;
    }
    gfloat totalwidth = count * 60.0 + spacing * (count - 1);
    xpos = (STAGE_WIDTH - totalwidth) / 2.0;

    /* set positions */
    for (i = 0; i < count; i++) {
        SBItem *item = g_list_nth_data(dockitems, i);
        ClutterActor *icon = clutter_actor_get_parent(item->texture);
        if (!icon) {
            continue;
        }

        if (item != selected_item) {
            if (animated) {
                clutter_actor_animate(icon, CLUTTER_EASE_OUT_QUAD, 250, "x", xpos, "y", ypos, NULL);
            } else {
                clutter_actor_set_position(icon, xpos, ypos);
            }
        }

        xpos += 60;
        if (i < count - 1) {
            xpos += spacing;
        }
    }
}

static void sb_align_icons(guint page_num, gboolean animated)
{
    if (!sbpages)
        return;
    if (g_list_length(sbpages) == 0) {
        printf("%s: no pages? that's strange...\n", __func__);
        return;
    }
    GList *pageitems = g_list_nth_data(sbpages, page_num);
    if (!pageitems) {
        printf("%s: no items on page %d\n", __func__, page_num);
        return;
    }
    gint count = g_list_length(pageitems);

    gfloat ypos = 16.0;
    gfloat xpos = 16.0 + (page_num * STAGE_WIDTH);
    gint i = 0;

    /* set positions */
    for (i = 0; i < count; i++) {
        SBItem *item = g_list_nth_data(pageitems, i);
        if (!item) {
            printf("%s: item is null for i=%d\n", __func__, i);
            continue;
        }
        if (!item->texture) {
            printf("%s(%d,%d): i=%d item->texture is null\n", __func__, page_num, animated, i);
            continue;
        }
        ClutterActor *icon = clutter_actor_get_parent(item->texture);
        if (!icon) {
            continue;
        }

        if (item != selected_item) {
            if (animated) {
                clutter_actor_animate(icon, CLUTTER_EASE_OUT_QUAD, 250, "x", xpos, "y", ypos, NULL);
            } else {
                clutter_actor_set_position(icon, xpos, ypos);
            }
        }

        if (((i + 1) % 4) == 0) {
            xpos = 16.0 + (page_num * STAGE_WIDTH);
            if (ypos + 88.0 < sb_area.y2 - sb_area.y1) {
                ypos += 88.0;
            }
        } else {
            xpos += 76;
        }
    }
}

static void redraw_icons()
{
    guint i;
    guint j;
    gfloat ypos;
    gfloat xpos;

    if (dockitems) {
        xpos = 0.0;
        ypos = 0.0;
        debug_printf("%s: drawing dock icons\n", __func__);
        for (i = 0; i < g_list_length(dockitems); i++) {
            SBItem *item = (SBItem*)g_list_nth_data(dockitems, i);
            if (item && item->texture && item->node) {
                item->is_dock_item = TRUE;
                ClutterActor *grp = clutter_group_new();
                ClutterActor *actor = item->texture;
                clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor);
                clutter_actor_set_position(actor, xpos, ypos);
                clutter_actor_set_reactive(actor, TRUE);
                g_signal_connect(actor, "button-press-event", G_CALLBACK(item_button_press), item);
                g_signal_connect(actor, "button-release-event", G_CALLBACK(item_button_release), item);
                clutter_actor_show(actor);
                actor = item->label;
                clutter_actor_set_position(actor, xpos + (59.0 - clutter_actor_get_width(actor)) / 2, ypos + 67.0);
                clutter_text_set_color(CLUTTER_TEXT(actor), &dock_item_text_color);
                clutter_actor_show(actor);
                clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor);
                clutter_container_add_actor(CLUTTER_CONTAINER(the_dock), grp);
                dock_align_icons(FALSE);
            }
        }
    }
    clutter_stage_ensure_redraw(CLUTTER_STAGE(stage));
    if (sbpages) {
        debug_printf("%s: %d pages\n", __func__, g_list_length(sbpages));
        for (j = 0; j < g_list_length(sbpages); j++) {
            GList *cpage = g_list_nth_data(sbpages, j);
            ypos = 0.0;
            xpos = 0.0;
            debug_printf("%s: drawing page icons for page %d\n", __func__, j);
            for (i = 0; i < g_list_length(cpage); i++) {
                SBItem *item = (SBItem*)g_list_nth_data(cpage, i);
                if (item && item->texture && item->node) {
                    item->is_dock_item = FALSE;
                    ClutterActor *grp = clutter_group_new();
                    ClutterActor *actor = item->texture;
                    clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor);
                    clutter_actor_set_position(actor, xpos, ypos);
                    clutter_actor_set_reactive(actor, TRUE);
                    g_signal_connect(actor, "button-press-event", G_CALLBACK(item_button_press), item);
                    g_signal_connect(actor, "button-release-event", G_CALLBACK(item_button_release), item);
                    clutter_actor_show(actor);
                    actor = item->label;
                    clutter_text_set_color(CLUTTER_TEXT(actor), &item_text_color);
                    clutter_actor_set_position(actor, xpos + (59.0 - clutter_actor_get_width(actor)) / 2, ypos + 62.0);
                    clutter_actor_show(actor);
                    clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor);
                    clutter_container_add_actor(CLUTTER_CONTAINER(the_sb), grp);
                    sb_align_icons(j, FALSE);
                }
            }
        }
    }
    clutter_stage_ensure_redraw(CLUTTER_STAGE(stage));
}

static GList *insert_into_icon_list(GList *iconlist, SBItem *newitem, gfloat item_x, gfloat item_y)
{
    if (!newitem) {
        return iconlist;
    }
    if (!iconlist) {
        /* for empty lists just add the element */
        return g_list_append(iconlist, newitem);
    }
    gint i;
    gint count = g_list_length(iconlist);
    gint newpos = count;
    if (count <= 0) {
        return iconlist;
    }

    for (i = 0; i < count; i++) {
        SBItem *item = g_list_nth_data(iconlist, i);
        ClutterActor *icon = clutter_actor_get_parent(item->texture);
        gfloat xpos = clutter_actor_get_x(icon);
        gfloat ypos = clutter_actor_get_y(icon);

        gint nrow = (ypos - 16) / 88;
        gint irow = (item_y - 16) / 88;

        xpos += nrow*STAGE_WIDTH;
        gfloat ixpos = item_x + irow*STAGE_WIDTH;

        if (move_left) {
            if (ixpos < xpos + 40) {
                newpos = i;
                break;
            }
        } else {
            if (ixpos < xpos - 10) {
                newpos = i;
                break;
            }
        }
    }

    return g_list_insert(iconlist, newitem, newpos);
}

static gboolean stage_motion(ClutterActor *actor, ClutterMotionEvent *event, gpointer user_data)
{
    /* check if an item has been raised */
    if (!selected_item) {
        return FALSE;
    }

    ClutterActor *icon = clutter_actor_get_parent(selected_item->texture);

    clutter_actor_move_by(icon, (event->x - start_x), (event->y - start_y));

    if (event->x-start_x > 0) {
        move_left = FALSE;
    } else {
        move_left = TRUE;
    }

    start_x = event->x;
    start_y = event->y;

    gfloat center_x;
    gfloat center_y;
    actor_get_abs_center(icon, &center_x, &center_y);

    if (clutter_actor_box_contains(&left_trigger, center_x-30, center_y)) {
        if (current_page > 0) {
            if (elapsed_ms(&last_page_switch, 1000)) {
                sbpages_show_previous_page();
                gettimeofday(&last_page_switch, NULL);
            }
            return TRUE;
        }
    } else if (clutter_actor_box_contains(&right_trigger, center_x+30, center_y)) {
        if (current_page < (gint)(g_list_length(sbpages)-1)) {
            if (elapsed_ms(&last_page_switch, 1000)) {
                sbpages_show_next_page();
                gettimeofday(&last_page_switch, NULL);
            }
            return TRUE;
        }
    }

    if (selected_item->is_dock_item) {
        dockitems = g_list_remove(dockitems, selected_item);
        if (center_y >= dock_area.y1) {
            debug_printf("icon from dock moving inside the dock!\n");
            selected_item->is_dock_item = TRUE;
            dockitems =
                insert_into_icon_list(dockitems, selected_item, (center_x - dock_area.x1), (center_y - dock_area.y1));
        } else {
            debug_printf("icon from dock moving outside the dock!\n");
            selected_item->is_dock_item = FALSE;
        }
    } else {
        GList *pageitems = g_list_nth_data(sbpages, current_page);
        sbpages = g_list_remove(sbpages, pageitems);
        pageitems = g_list_remove(pageitems, selected_item);
        if (center_y >= dock_area.y1 && (g_list_length(dockitems) < num_dock_items)) {
            debug_printf("regular icon is moving inside the dock!\n");
            selected_item->is_dock_item = TRUE;
        } else {
            debug_printf("regular icon is moving!\n");
            pageitems =
                insert_into_icon_list(pageitems, selected_item, (center_x - sb_area.x1) + (current_page * STAGE_WIDTH), (center_y - sb_area.y1));
        }
        sbpages = g_list_insert(sbpages, pageitems, current_page);
    }
    dock_align_icons(TRUE);
    sb_align_icons(current_page, TRUE);

    return TRUE;
}

static gboolean form_map(GtkWidget *widget, GdkEvent *event, SBManagerApp *app)
{
    debug_printf("%s: mapped\n", __func__);
    clutter_stage_ensure_redraw(CLUTTER_STAGE(stage));

    return TRUE;
}

static gboolean form_focus_change(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
    if (!user_data) {
        return FALSE;
    }

    ClutterActor *actor = (ClutterActor*)user_data;

    if (event->in) {
        clutter_timeline_start(CLUTTER_TIMELINE(actor));
    } else {
        clutter_timeline_pause(CLUTTER_TIMELINE(actor));
    }

    return TRUE;
}

static gboolean set_icons(gpointer data)
{
    SBManagerApp *app = (SBManagerApp*)data;

    iphone_device_t phone = NULL;
    lockdownd_client_t client = NULL;
    sbservices_client_t sbc = NULL;
    int port = 0;

    gboolean result = FALSE;
    plist_t iconstate = NULL;
    plist_t pdockarray = NULL;
    plist_t pdockitems = NULL;
    guint i;

    if (!dockitems || !sbpages) {
        printf("missing dockitems or sbpages\n");
        return result;
    }

    printf("About to upload new iconstate...\n");

    guint count = g_list_length(dockitems);
    pdockitems = plist_new_array();
    for (i = 0; i < count; i++) {
        SBItem *item = g_list_nth_data(dockitems, i);
        if (!item) {
            continue;
        }
        plist_t valuenode = plist_dict_get_item(item->node, "displayIdentifier");
        if (!valuenode) {
            printf("could not get displayIdentifier\n");
            continue;
        }

        plist_t pitem = plist_new_dict();
        plist_dict_insert_item(pitem, "displayIdentifier", plist_copy(valuenode));
        plist_array_append_item(pdockitems, pitem);
    }
    for (i = count; i < num_dock_items; i++) {
        plist_array_append_item(pdockitems, plist_new_bool(0));
    }
    pdockarray = plist_new_array();
    plist_array_append_item(pdockarray, pdockitems);

    iconstate = plist_new_array();
    plist_array_append_item(iconstate, pdockarray);

    for (i = 0; i < g_list_length(sbpages); i++) {
        GList *page = g_list_nth_data(sbpages, i);
        if (page) {
            guint j;
            count = g_list_length(page);
            if (count <= 0) {
                continue;
            }
            plist_t ppage = plist_new_array();
            plist_t row = NULL;
            for (j = 0; j < 16; j++) {
                SBItem *item = g_list_nth_data(page, j);
                if ((j % 4) == 0) {
                    row = plist_new_array();
                    plist_array_append_item(ppage, row);
                }
                if (item && item->node) {
                    plist_t valuenode = plist_dict_get_item(item->node,
                                                            "displayIdentifier");
                    if (!valuenode) {
                        printf("could not get displayIdentifier\n");
                        continue;
                    }

                    plist_t pitem = plist_new_dict();
                    plist_dict_insert_item(pitem, "displayIdentifier", plist_copy(valuenode));
                    plist_array_append_item(row, pitem);
                } else {
                    plist_array_append_item(row, plist_new_bool(0));
                }
            }
            plist_array_append_item(iconstate, plist_copy(ppage));
            plist_free(ppage);
        }
    }

    if (IPHONE_E_SUCCESS != iphone_device_new(&phone, app->uuid)) {
        fprintf(stderr, "No iPhone found, is it plugged in?\n");
        return result;
    }

    if (LOCKDOWN_E_SUCCESS != lockdownd_client_new(phone, &client)) {
        fprintf(stderr, "Could not connect to lockdownd. Exiting.\n");
        goto leave_cleanup;
    }

    if ((lockdownd_start_service(client, "com.apple.springboardservices", &port) != LOCKDOWN_E_SUCCESS) || !port) {
        fprintf(stderr,
                "Could not start com.apple.springboardservices service! Remind that this feature is only supported in OS 3.1 and later!\n");
        goto leave_cleanup;
    }
    if (sbservices_client_new(phone, port, &sbc) != SBSERVICES_E_SUCCESS) {
        fprintf(stderr, "Could not connect to springboardservices!\n");
        goto leave_cleanup;
    }
    if (sbservices_set_icon_state(sbc, iconstate) != SBSERVICES_E_SUCCESS) {
        fprintf(stderr, "ERROR: Could not set new icon state!\n");
        goto leave_cleanup;
    }

    printf("Successfully uploaded new iconstate\n");
    result = TRUE;

  leave_cleanup:
    if (iconstate) {
        plist_free(iconstate);
    }
    if (sbc) {
        sbservices_client_free(sbc);
    }
    if (client) {
        lockdownd_client_free(client);
    }
    iphone_device_free(phone);

    return result;
}

static gboolean button_clicked(GtkButton *button, gpointer user_data)
{
    set_icons(user_data);

    return TRUE;
}

static guint battery_init(SBManagerApp *app)
{
    guint interval = 60;
    iphone_device_t phone = NULL;
    lockdownd_client_t client = NULL;
    plist_t info_plist = NULL;

    if (IPHONE_E_SUCCESS != iphone_device_new(&phone, app->uuid)) {
        fprintf(stderr, "No iPhone found, is it plugged in?\n");
        goto leave_cleanup;
    }

    if (LOCKDOWN_E_SUCCESS != lockdownd_client_new(phone, &client)) {
        fprintf(stderr, "Could not connect to lockdownd. Exiting.\n");
        goto leave_cleanup;
    }

    lockdownd_get_value(client, "com.apple.mobile.iTunes", "BatteryPollInterval", &info_plist);
    plist_get_uint_val(info_plist, (uint64_t*)&interval);
    plist_free(info_plist);

    printf("Have to poll battery every %d seconds...\n", interval);

  leave_cleanup:
    if (client) {
        lockdownd_client_free(client);
    }
    iphone_device_free(phone);

    /* Let's default to the default */
    return interval;
}

static guint battery_get_current_capacity(SBManagerApp *app)
{
    uint64_t current_capacity = 0;
    plist_t node = NULL;

    if (app->battery == NULL)
        return current_capacity;

    node = plist_dict_get_item(app->battery, "BatteryCurrentCapacity");
    if (node != NULL) {
        plist_get_uint_val(node, &current_capacity);
        plist_free(node);
    }

    return (guint) current_capacity;
}

static gboolean battery_update_cb(gpointer data)
{
    SBManagerApp *app = (SBManagerApp*)data;
    iphone_device_t phone = NULL;
    lockdownd_client_t client = NULL;
    guint capacity = 0;

    printf("Updating battery information...\n");

    if (IPHONE_E_SUCCESS != iphone_device_new(&phone, app->uuid)) {
        fprintf(stderr, "No iPhone found, is it plugged in?\n");
        goto leave_cleanup;
    }

    if (LOCKDOWN_E_SUCCESS != lockdownd_client_new(phone, &client)) {
        fprintf(stderr, "Could not connect to lockdownd. Exiting.\n");
        goto leave_cleanup;
    }

    lockdownd_get_value(client, "com.apple.mobile.battery", NULL, &app->battery);

    capacity = battery_get_current_capacity(app);
    printf("Battery capacity is at %d%%\n", capacity);

    clutter_actor_set_size(battery_level, (guint) (((double) (capacity) / 100.0) * 15), 6);
    clutter_actor_set_position(battery_level, 298, 6);

  leave_cleanup:
    if (client) {
        lockdownd_client_free(client);
    }
    iphone_device_free(phone);

    return TRUE;
}

static gboolean get_device_info(SBManagerApp *app)
{
    iphone_device_t phone = NULL;
    lockdownd_client_t client = NULL;
    plist_t node;
    gboolean res = FALSE;

    if (IPHONE_E_SUCCESS != iphone_device_new(&phone, app->uuid)) {
        fprintf(stderr, "No iPhone found, is it plugged in?\n");
        goto leave_cleanup;
    }

    if (LOCKDOWN_E_SUCCESS != lockdownd_client_new(phone, &client)) {
        fprintf(stderr, "Could not connect to lockdownd. Exiting.\n");
        goto leave_cleanup;
    }

    lockdownd_get_device_name(client, &app->device_name);

    lockdownd_get_value(client, NULL, "ProductType", &node);
    if (node) {
        char *devtype = NULL;
        const char *devtypes[6][2] = {
            {"iPhone1,1", "iPhone"},
            {"iPhone1,2", "iPhone 3G"},
            {"iPhone2,1", "iPhone 3GS"},
            {"iPod1,1", "iPod Touch"},
            {"iPod2,1", "iPod touch (2G)"},
            {"iPod3,1", "iPod Touch (3G)"}
        };
        plist_get_string_val(node, &devtype);
        if (devtype) {
            int i;
            for (i = 0; i < 6; i++) {
                if (g_str_equal(devtypes[i][0], devtype)) {
                    app->device_type = g_strdup(devtypes[i][1]);
                    break;
                }
            }
        }
    }

    res = TRUE;

  leave_cleanup:
    if (client) {
        lockdownd_client_free(client);
    }
    iphone_device_free(phone);

    return res;
}

static gboolean stage_key_press(ClutterActor *actor, ClutterEvent *event, gpointer user_data)
{
    if (!user_data || (event->type != CLUTTER_KEY_PRESS)) {
        return FALSE;
    }

    guint symbol = clutter_event_get_key_symbol(event);
    switch(symbol) {
        case CLUTTER_Right:
        sbpages_show_next_page();
        break;
        case CLUTTER_Left:
        sbpages_show_previous_page();
        break;
        default:
        return FALSE;
    }
    return TRUE;
}

static void print_usage(int argc, char **argv)
{
    char *name = NULL;

    name = strrchr(argv[0], '/');
    printf("Usage: %s [OPTIONS]\n", (name ? name + 1 : argv[0]));
    printf("Manage SpringBoard icons of an iPhone/iPod Touch.\n\n");
    printf("  -d, --debug\t\tenable communication debugging\n");
    printf("  -D, --debug-app\tenable application debug messages\n");
    printf("  -u, --uuid UUID\ttarget specific device by its 40-digit device UUID\n");
    printf("  -h, --help\t\tprints usage information\n");
    printf("\n");
}

int main(int argc, char **argv)
{
    SBManagerApp *app;
    ClutterTimeline *timeline;
    ClutterColor stage_color = { 0x00, 0x00, 0x00, 0xff };  /* Black */
    ClutterActor *actor;
    int i;

    app = g_new0(SBManagerApp, 1);
    if (!app) {
        printf("Error: out of memory!\n");
        return -1;
    }

    gettimeofday(&last_page_switch, NULL);

    app->uuid = NULL;

    /* parse cmdline args */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
            iphone_set_debug_mask(DBGMASK_ALL);
            iphone_set_debug_level(1);
            continue;
        } else if (!strcmp(argv[i], "-D") || !strcmp(argv[i], "--debug-app")) {
            debug_app = TRUE;
            continue;
        } else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--uuid")) {
            i++;
            if (!argv[i] || (strlen(argv[i]) != 40)) {
                print_usage(argc, argv);
                return 0;
            }
            app->uuid = g_strndup(argv[i], 40);
            continue;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_usage(argc, argv);
            return 0;
        } else {
            print_usage(argc, argv);
            return 0;
        }
    }

    if (!get_device_info(app)) {
        g_error("Could not read information from device.");
        return 0;
    }

    if (gtk_clutter_init(&argc, &argv) != CLUTTER_INIT_SUCCESS) {
        g_error("Unable to initialize GtkClutter");
    }

    if (!g_thread_supported())
        g_thread_init(NULL);

    /* Create the window and some child widgets: */
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_resizable(GTK_WINDOW(app->window), FALSE);
    gchar *wndtitle = g_strdup_printf("%s - SBManager", app->device_name);
    gtk_window_set_title(GTK_WINDOW(app->window), wndtitle);
    g_free(wndtitle);
    GtkWidget *vbox = gtk_vbox_new(FALSE, 6);
    gtk_container_add(GTK_CONTAINER(app->window), vbox);
    gtk_widget_show(vbox);
    GtkWidget *button = gtk_button_new_with_label("Upload changes to device");
    gtk_box_pack_end(GTK_BOX(vbox), button, FALSE, FALSE, 0);
    gtk_widget_show(button);

    g_signal_connect(button, "clicked", G_CALLBACK(button_clicked), app);

    /* Stop the application when the window is closed: */
    g_signal_connect(app->window, "hide", G_CALLBACK(gtk_main_quit), app);

    /* Create the clutter widget: */
    GtkWidget *clutter_widget = gtk_clutter_embed_new();
    gtk_box_pack_start(GTK_BOX(vbox), clutter_widget, TRUE, TRUE, 0);
    gtk_widget_show(clutter_widget);

    /* Set the size of the widget, because we should not set the size of its
     * stage when using GtkClutterEmbed.
     */
    gtk_widget_set_size_request(clutter_widget, STAGE_WIDTH, STAGE_HEIGHT);

    /* Get the stage and set its size and color: */
    stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(clutter_widget));

    clutter_stage_set_color(CLUTTER_STAGE(stage), &stage_color);

    /* dock background */
    GError *err = NULL;
    actor = clutter_texture_new();
    clutter_texture_set_load_async(CLUTTER_TEXTURE(actor), TRUE);
    clutter_texture_set_from_file(CLUTTER_TEXTURE(actor), SBMGR_DATA "/background.png", &err);
    if (err) {
        g_error_free(err);
        err = NULL;
    }
    if (actor) {
        clutter_actor_set_position(actor, 0, 0);
        clutter_actor_show(actor);
        clutter_group_add(CLUTTER_GROUP(stage), actor);
    } else {
        fprintf(stderr, "could not load background.png\n");
    }

    /* device type widget */
    type_label = clutter_text_new_full(CLOCK_FONT, app->device_type, &clock_text_color);
    clutter_group_add(CLUTTER_GROUP(stage), type_label);
    clutter_actor_set_position(type_label, 3.0, 2.0);

    /* clock widget */
    clock_label = clutter_text_new_full(CLOCK_FONT, "00:00", &clock_text_color);
    clutter_group_add(CLUTTER_GROUP(stage), clock_label);

    /* page indicator group for holding the page indicator dots */
    page_indicator_group = clutter_group_new();
    clutter_group_add(CLUTTER_GROUP(stage), page_indicator_group);

    /* alignment will be done when new indicators are added */
    clutter_actor_set_position(page_indicator_group, 0, STAGE_HEIGHT - DOCK_HEIGHT - 18);

    /* page indicator (dummy), will be cloned when the pages are created */
    page_indicator = clutter_texture_new();
    clutter_texture_set_load_async(CLUTTER_TEXTURE(page_indicator), TRUE);
    clutter_texture_set_from_file(CLUTTER_TEXTURE(page_indicator), SBMGR_DATA "/dot.png", &err);
    if (err) {
        fprintf(stderr, "Could not load texture " SBMGR_DATA "/dot.png" ": %s\n", err->message);
        g_error_free(err);
        err = NULL;
    }
    if (page_indicator) {
        clutter_actor_hide(page_indicator);
        clutter_container_add_actor(CLUTTER_CONTAINER(stage), page_indicator);
    }

    /* a group for the springboard icons */
    the_sb = clutter_group_new();
    clutter_group_add(CLUTTER_GROUP(stage), the_sb);
    clutter_actor_set_position(the_sb, 0, 16);

    /* a group for the dock icons */
    the_dock = clutter_group_new();
    clutter_group_add(CLUTTER_GROUP(stage), the_dock);
    clutter_actor_set_position(the_dock, dock_area.x1, dock_area.y1);

    /* Show the stage: */
    clutter_actor_show(stage);

    /* Create a timeline to manage animation */
    timeline = clutter_timeline_new(200);
    clutter_timeline_set_loop(timeline, TRUE);  /* have it loop */

    /* fire a callback for frame change */
    g_signal_connect(timeline, "completed", G_CALLBACK(clock_update_cb), app);

    /* and start it */
    clutter_timeline_start(timeline);

    g_signal_connect(stage, "motion-event", G_CALLBACK(stage_motion), app);
    g_signal_connect(stage, "key-press-event", G_CALLBACK(stage_key_press), app);

    g_signal_connect(G_OBJECT(app->window), "map-event", G_CALLBACK(form_map), app);

    g_signal_connect(G_OBJECT(app->window), "focus-in-event", G_CALLBACK(form_focus_change), timeline);
    g_signal_connect(G_OBJECT(app->window), "focus-out-event", G_CALLBACK(form_focus_change), timeline);

    selected_mutex = g_mutex_new();

    /* Show the window. This also sets the stage's bounding box. */
    gtk_widget_show_all(GTK_WIDGET(app->window));

    /* Position and update the clock */
    clock_set_time(clock_label, time(NULL));
    clutter_actor_show(clock_label);

    /* Load BatteryPollInterval and register battery state read timeout */
    g_timeout_add_seconds(battery_init(app), (GSourceFunc) battery_update_cb, app);

    /* battery capacity */
    battery_level = clutter_rectangle_new_with_color(clutter_color_new(0xff, 0xff, 0xff, 0x9f));
    battery_update_cb(app);
    clutter_group_add(CLUTTER_GROUP(stage), battery_level);

    /* Load icons in an idle loop */
    g_idle_add((GSourceFunc) get_icons, app);

    /* Start the main loop, so we can respond to events: */
    gtk_main();

    return 0;
}
