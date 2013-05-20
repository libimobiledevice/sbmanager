/**
 * gui.c
 * GUI implementations.
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
 #include <config.h> /* for GETTEXT_PACKAGE */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <plist/plist.h>
#include <time.h>
#include <sys/time.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>
#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "sbmgr.h"
#include "utility.h"
#include "device.h"
#include "sbitem.h"
#include "gui.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b)) 
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define STAGE_WIDTH 320
#define STAGE_HEIGHT 480
#define DOCK_HEIGHT 90
#define MAX_PAGE_ITEMS (gint)(device_info->home_screen_icon_rows*device_info->home_screen_icon_columns+device_info->home_screen_icon_dock_max_count)
#define ICON_SPACING 18
#define PAGE_X_OFFSET(i) ((gfloat)(i)*(gfloat)(stage_area.x2))

#define ICON_MOVEMENT_DURATION 250
#define FOLDER_ANIM_DURATION 500

const char CLOCK_FONT[] = "FreeSans Bold 12px";
ClutterColor clock_text_color = { 255, 255, 255, 210 };

const char ITEM_FONT[] = "FreeSans Bold 10px";
ClutterColor item_text_color = { 255, 255, 255, 210 };
ClutterColor dock_item_text_color = { 255, 255, 255, 255 };
ClutterColor stage_color = { 0x00, 0x00, 0x00, 0x00 };  /* last 00 was ff TW 06/05/13 Black */
ClutterColor battery_color = { 0xff, 0xff, 0xff, 0x9f }; /* White */
ClutterColor spinner_color = { 0xff, 0xff, 0xff, 0xf0 };
ClutterColor label_shadow_color = { 0x00, 0x00, 0x00, 0xe0 };

const char FOLDER_LARGE_FONT[] = "FreeSans Bold 18px";

GtkWidget *clutter_gtk_widget;

ClutterActorBox stage_area = { 0.0, 0.0, STAGE_WIDTH, STAGE_HEIGHT };
ClutterActorBox dock_area = { 0.0, STAGE_HEIGHT - DOCK_HEIGHT, STAGE_WIDTH, STAGE_HEIGHT };
ClutterActorBox sb_area = { 0.0, ICON_SPACING, STAGE_WIDTH, STAGE_HEIGHT - DOCK_HEIGHT - ICON_SPACING };
ClutterActorBox left_trigger = { -30.0, ICON_SPACING, -8.0, STAGE_HEIGHT - DOCK_HEIGHT - ICON_SPACING };
ClutterActorBox right_trigger = { STAGE_WIDTH + 8.0, ICON_SPACING, STAGE_WIDTH + 30.0, STAGE_HEIGHT - DOCK_HEIGHT - ICON_SPACING };

ClutterActor *stage = NULL;
ClutterActor *wallpaper = NULL;
ClutterActor *the_dock = NULL;
ClutterActor *the_sb = NULL;
ClutterActor *type_label = NULL;
ClutterActor *clock_label = NULL;
ClutterActor *battery_level = NULL;
ClutterActor *page_indicator = NULL;
ClutterActor *page_indicator_group = NULL;
ClutterActor *fade_rectangle = NULL;
ClutterActor *spinner = NULL;
ClutterActor *icon_shadow = NULL;
ClutterTimeline *spinner_timeline = NULL;
ClutterTimeline *clock_timeline = NULL;

/* static GMutex *selected_mutex = NULL; */
SBItem *selected_item = NULL;

SBItem *selected_folder = NULL;

ClutterActor *folder_marker = NULL;

ClutterActor *aniupper = NULL;
ClutterActor *anilower = NULL;
ClutterActor *folder = NULL;
gfloat split_pos = 0.0;

/* static GMutex *icon_loader_mutex = NULL; */
static int icons_loaded = 0;
static int total_icons = 0;

gfloat start_x = 0.0;
gfloat start_y = 0.0;

gboolean move_left = TRUE;

GList *dockitems = NULL;
GList *sbpages = NULL;

guint num_dock_items = 0;

sbservices_client_t sbc = NULL;
uint32_t osversion = 0;
device_info_t device_info = NULL;

static finished_cb_t finished_callback = NULL;
static device_info_cb_t device_info_callback = NULL;

int current_page = 0;
struct timeval last_page_switch;

static int gui_deinitialized = 0;
static int clutter_threads_initialized = 0;
static int clutter_initialized = 0;

static void gui_page_indicator_group_add(GList *page, int page_index);
static void gui_page_align_icons(guint page_num, gboolean animated);
static void gui_folder_align_icons(SBItem *item, gboolean animated);

/* helper */
static void sbpage_free(GList *sbitems, gpointer data)
{
    if (sbitems) {
        g_list_foreach(sbitems, (GFunc) (g_func_sbitem_free), NULL);
        g_list_free(sbitems);

		 /* clutter_group_remove_all(CLUTTER_GROUP(page_indicator_group)); */
	 	/* clutter_group_remove_all  Deprecated TW 19/04/13 */
		clutter_actor_remove_all_children(CLUTTER_ACTOR(page_indicator_group));
       
	}
}

static void pages_free()
{
    if (sbpages) {
        g_list_foreach(sbpages, (GFunc)(sbpage_free), NULL);
        g_list_free(sbpages);
        sbpages = NULL;
    }
    if (dockitems) {
        sbpage_free(dockitems, NULL);
        dockitems = NULL;
    }
    if (wallpaper) {
        clutter_actor_destroy(wallpaper);
        wallpaper = NULL;
        item_text_color.alpha = 210;
    }
}

static gboolean item_enable(gpointer item)
{
	if (item) {
		((SBItem*)item)->enabled = TRUE;
	}
	return FALSE;
}

static void clutter_actor_get_abs_center(ClutterActor *actor, gfloat *center_x, gfloat *center_y)
{
    *center_x = 0.0;
    *center_y = 0.0;
    if (!actor)
        return;

	/* clutter_actor_get_scale_center is deprecated TW 19/04/13 */
   /* clutter_actor_get_scale_center(actor, center_x, center_y); */	
    clutter_actor_get_pivot_point(actor, center_x, center_y);

    *center_x += clutter_actor_get_x(actor);
    *center_y += clutter_actor_get_y(actor);
}

static GList *iconlist_insert_item_at(GList *iconlist, SBItem *newitem, gfloat item_x, gfloat item_y, int pageindex, int icons_per_row)
{
    if (!newitem) {
        return iconlist;
    }
    
    debug_printf("%s: count items %d\n", __func__, g_list_length(iconlist));
    
    if (!iconlist) {
        debug_printf("%s: appending item\n", __func__);
        /* for empty lists just add the element */
        return g_list_append(iconlist, newitem);
    }
    gint i;
    gint count = g_list_length(iconlist);
    gint newpos = count;
    if (count <= 0) {
        return iconlist;
    }

    gfloat xpageoffset = PAGE_X_OFFSET(pageindex);

    gfloat spacing = ICON_SPACING;
    if (icons_per_row > (gint)device_info->home_screen_icon_columns) {
        spacing = 3;
    }

    gfloat xpos = spacing + xpageoffset;
    gfloat ypos = ICON_SPACING;
    gfloat oxpos = xpos;

    for (i = 0; i < count; i++) {
        oxpos = xpos;

        gint nrow = (ypos - ICON_SPACING) / 88;
        gint irow = (item_y - ICON_SPACING) / 88;

        oxpos += nrow*stage_area.x2;
        gfloat ixpos = item_x + irow*stage_area.x2;

        /* if required, add spacing */
        if (!move_left)
            oxpos += spacing;

        if (ixpos < oxpos + 75) {
            newpos = i;
            break;
        }

        if (((i + 1) % icons_per_row) == 0) {
            xpos = spacing + xpageoffset;
            if (ypos + 88.0 < sb_area.y2 - sb_area.y1) {
                ypos += 88.0;
            }
        } else {
            xpos += 75;
            xpos += spacing;
        }
    }

    debug_printf("%s: newpos:%d\n", __func__, newpos);

    /* do we have a full page? */
    if ((count >= MAX_PAGE_ITEMS) && (icons_per_row == (gint)device_info->home_screen_icon_columns)) {
        debug_printf("%s: full page detected\n", __func__);
        /* remove overlapping item from current page */
        SBItem *last_item = g_list_nth_data(iconlist, MAX_PAGE_ITEMS-1);
        iconlist = g_list_remove(iconlist, last_item);
        /* animate it to new position */
		
		fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-get-parent\n", __func__); 
        ClutterActor *actor = clutter_actor_get_parent(last_item->texture);
		fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-get-parent\n", __func__);

		/* clutter_actor_animate is deprecated TW 20/04/13 */
     	/* clutter_actor_animate(actor, CLUTTER_EASE_OUT_QUAD, 250, "x", ICON_SPACING + PAGE_X_OFFSET(pageindex + 1), "y", ICON_SPACING, NULL); */
		clutter_actor_set_easing_mode (actor, CLUTTER_EASE_OUT_QUAD);
		clutter_actor_set_easing_duration (actor, 250);
		clutter_actor_set_position (actor, ICON_SPACING + PAGE_X_OFFSET(pageindex + 1), ICON_SPACING);
	

        /* first, we need to get the pages that we have to manipulate */
        gint page_count = g_list_length(sbpages);
        gint last_index = pageindex;
        for (i = pageindex; i < page_count; i++) {
            GList *thepage = g_list_nth_data(sbpages, i);
            if (g_list_length(thepage) < (device_info->home_screen_icon_columns*device_info->home_screen_icon_rows)) {
                last_index = i;
                break;
            }
        }
        if ((gint)g_list_length(g_list_nth_data(sbpages, last_index)) >= MAX_PAGE_ITEMS) {
            /* it's the last page that is full, so we need to add a new page */
            debug_printf("last page is full, appending page\n");
            sbpages = g_list_append(sbpages, NULL);
            last_index++;
        }
        debug_printf("will alter pages %d to %d (%d pages)\n", pageindex, last_index, (last_index - pageindex) + 1);
        /* now manipulate the lists in reverse order */
        for (i = last_index; i >= pageindex; i--) {
            GList *thepage = g_list_nth_data(sbpages, i);
            sbpages = g_list_remove(sbpages, thepage);
            GList *prevpage = NULL;
            if (i > pageindex) {
                prevpage = g_list_nth_data(sbpages, i-1);
            }
            gint thepage_count = g_list_length(thepage);
            while (thepage_count >= MAX_PAGE_ITEMS) {
                thepage = g_list_remove(thepage, g_list_nth_data(thepage, thepage_count-1));
                thepage_count = g_list_length(thepage);
            }
            if (prevpage) {
                SBItem *prev_page_item = g_list_nth_data(prevpage, MAX_PAGE_ITEMS-1);
                /* animate this item to fix drawing error */
				
				fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-get-parent\n", __func__);
                actor = clutter_actor_get_parent(prev_page_item->texture);
				fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-get-parent\n", __func__);

       	        /* clutter_actor_animate is deprecated TW 20/04/13 */
				/* clutter_actor_animate(actor, CLUTTER_LINEAR, 100, "x", ICON_SPACING + PAGE_X_OFFSET(i + 1), "y", ICON_SPACING, NULL); */
				clutter_actor_set_easing_mode (actor, CLUTTER_LINEAR);
				clutter_actor_set_easing_duration (actor, 100);
				clutter_actor_set_position (actor, ICON_SPACING + PAGE_X_OFFSET(i + 1), ICON_SPACING);

                thepage = g_list_prepend(thepage, prev_page_item);
            } else {
                thepage = g_list_prepend(thepage, last_item);
            }
            sbpages = g_list_insert(sbpages, thepage, i);
        }
    }
    return g_list_insert(iconlist, newitem, newpos >= MAX_PAGE_ITEMS ? 15:newpos);
}

/* clock */
static void clock_set_time(ClutterActor *label, time_t t)
{
    struct tm *curtime = localtime(&t);
    gchar *ctext = g_strdup_printf("%02d:%02d", curtime->tm_hour, curtime->tm_min);
    clutter_text_set_text(CLUTTER_TEXT(label), ctext);
    clutter_actor_set_position(label, (clutter_actor_get_width(stage) - clutter_actor_get_width(label)) / 2, 2);
    g_free(ctext);
}

static void clock_update_cb(ClutterTimeline *timeline, gint msecs, gpointer data)
{
    clock_set_time(clock_label, time(NULL));
}

/* gui */
static void gui_fade_init() 
{ 
    /* ClutterColor fade_color = { 0x00, 0x00, 0x00, 0xff }; BLACK */
 
	/* clutter_rectange_new_with_color is deprecated TW 20/04/13 */
	/*	fade_rectangle = clutter_rectangle_new_with_color(&fade_color); */
    fade_rectangle = clutter_actor_new(); 

	/* clutter_container_add_actor is deprecated TW 20/04/13 */
    /* clutter_container_add_actor(CLUTTER_CONTAINER (stage), fade_rectangle); */
	
	/* fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-add-child\n", __func__); */
	clutter_actor_add_child(CLUTTER_ACTOR(stage), fade_rectangle); 
    /* fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-add-child\n", __func__); */
	
	clutter_actor_set_position(fade_rectangle, 0, 0);
	/* fprintf(stderr,"\nERROR: above clutter_set_size1: LINE 355 \n"); TW 01/05/13 */
    clutter_actor_set_size(fade_rectangle, stage_area.x2, stage_area.y2); 
    clutter_actor_set_opacity(fade_rectangle, 0); 
} 

static void gui_fade_stop()
{
	/* fprintf(stderr,"\ngui-fade-stop\n"); TEST TW 03/05/13 */
	/* clutter_actor_raise_top' is deprecated TW 21/04/13 */
    /* clutter_actor_raise_top(fade_rectangle); */
	 /* fprintf(stderr,"\ngui-fade-stop(): above clutter_actor_set_child_above_sibling1: LINE 365 \n");  TEST TW 03/05/13 */
	
	/* Checked that self is stage and fade_rectange is child  TW 18/05/13 */
	
	/* Clutter-CRITICAL **: clutter_actor_insert_child_above: assertion `child->priv->parent == NULL' failed */
	/* Runtime error TW 10/05/13 */
	
	  clutter_actor_set_child_above_sibling(stage, fade_rectangle, ((void *)0) ); /*  TEST TW 10/05/13 */

	/* clutter_actor_animate is deprecated TW 20/04/13 */
    /* clutter_actor_animate(CLUTTER_ACTOR(fade_rectangle), CLUTTER_EASE_OUT_QUAD, 500, "opacity", 0, NULL); */
	clutter_actor_set_easing_mode (CLUTTER_ACTOR(fade_rectangle), CLUTTER_EASE_OUT_QUAD); 
	clutter_actor_set_easing_duration (CLUTTER_ACTOR(fade_rectangle), 500); 
	clutter_actor_set_opacity (CLUTTER_ACTOR(fade_rectangle), 0); 

    clutter_actor_set_reactive(fade_rectangle, FALSE); 
} 

static void gui_fade_start() 
{
	/* fprintf(stderr,"\ngui-fade-start\n");  TEST TW 03/05/13 */
   clutter_actor_set_reactive(fade_rectangle, TRUE); 

	/* clutter_actor_raise_top is deprecated TW 21/04/13 */
    /* clutter_actor_raise_top(fade_rectangle); */
	/* clutter_actor_insert_child_above(fade_rectangle, stage,((void *)0) ); Maybe this is wrong TW 06/05/13 */
	
	/* Clutter-CRITICAL **: clutter_actor_insert_child_above: assertion `child->priv->parent == NULL' failed */
	/* Runtime error TW 10/05/13  FIXED 18/05/13 Wrong funcion - should have been this one */
    
     /* fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-set-child-above-sibling\n", __func__); */

	 clutter_actor_set_child_above_sibling(stage, fade_rectangle, ((void *)0) );

     /* fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-set-child-above-sibling\n", __func__); */

	/* clutter_actor_animate is deprecated TW 20/04/13 */
    /*clutter_actor_animate(CLUTTER_ACTOR(fade_rectangle), CLUTTER_EASE_OUT_QUAD, 500, "opacity", 180, NULL); */
	clutter_actor_set_easing_mode (CLUTTER_ACTOR(fade_rectangle), CLUTTER_EASE_OUT_QUAD); 
	clutter_actor_set_easing_duration (CLUTTER_ACTOR(fade_rectangle), 500); 
	clutter_actor_set_opacity (CLUTTER_ACTOR(fade_rectangle), 180); 

} 

static gboolean spinner_spin_cb(gpointer data)
{
    int i;
    for (i = 0; i < 12; i++) {
		
		/* clutter_group_get_nth_child is deprecated TW 20/04/13 */
		/* ClutterActor *actor = clutter_group_get_nth_child(CLUTTER_GROUP(spinner), i); */
	   ClutterActor *actor = clutter_actor_get_child_at_index(CLUTTER_ACTOR(spinner), i);

       clutter_actor_set_opacity(actor, clutter_actor_get_opacity(actor)-30); 
    }
    return TRUE;
}

static void gui_spinner_init()
{
	/* clutter_rectange_new_with_color is deprecated TW 20/04/13 */
    /* ClutterActor *spinner_element = clutter_rectangle_new_with_color(&spinner_color); */
	ClutterActor *spinner_element = clutter_actor_new();
	

	/* fprintf(stderr,"\nERROR: above clutter_set_size2: LINE 414 \n"); */
    clutter_actor_set_size(spinner_element, 2.0, 8.0);
    clutter_actor_hide(spinner_element);

	/* clutter_container_add_actor' is deprecated TW 21/04/13 */
    /* clutter_group_add(CLUTTER_GROUP(stage), spinner_element); */
	
	/* fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-add-child\n", __func__); */
	clutter_actor_add_child(CLUTTER_ACTOR(stage), spinner_element);
	/* fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-add-child\n", __func__); */

	/* clutter_group_new' is deprecated TW 20/04/13 */   
	/* spinner = clutter_group_new(); */
	spinner = clutter_actor_new();

    int i;
    for (i = 0; i < 12; i++) {
        ClutterActor *actor = clutter_clone_new(spinner_element);
		
		/* clutter_container_add_actor' is deprecated TW 21/04/13 */
        /* clutter_group_add(CLUTTER_GROUP(spinner), actor); */

		/* fprintf(stderr,"\n%s:ERROR: above2 clutter-actor-add-child\n", __func__); */
		clutter_actor_add_child(CLUTTER_ACTOR(spinner), actor);
		/* fprintf(stderr,"\n%s:ERROR: below2 clutter-actor-add-child\n", __func__); */

        clutter_actor_set_position(actor, ICON_SPACING, 0.0);
        clutter_actor_set_opacity(actor, (guint8)(((gfloat)(i)/12.0)*255));
		
		/* clutter_actor_set_rotation is deprecated TW 21/04/13 */
        /* clutter_actor_set_rotation(actor, CLUTTER_Z_AXIS, i*30, 1.0, 15.0, 0); */
		clutter_actor_set_rotation_angle(actor, CLUTTER_Z_AXIS, i*30);

        clutter_actor_show(actor);
    }
    clutter_actor_hide(spinner);

	/* clutter_container_add_actor' is deprecated TW 21/04/13 */
    /* clutter_group_add(CLUTTER_GROUP(stage), spinner); */
	
	/* fprintf(stderr,"\n%s:ERROR: above3 clutter-actor-add-child\n", __func__); */
	clutter_actor_add_child(CLUTTER_ACTOR(stage), spinner);
	/* fprintf(stderr,"\n%s:ERROR: below3 clutter-actor-add-child\n", __func__); */

    clutter_actor_set_position(spinner, (stage_area.x2-32.0)/2, (stage_area.y2-64.0)/2);
    spinner_timeline = clutter_timeline_new(100);

	/* clutter_timeline_set_loop' is deprecated TW 21/04/13 */
    /* clutter_timeline_set_loop(spinner_timeline, TRUE); */
	clutter_timeline_set_repeat_count(spinner_timeline, -1);

    g_signal_connect(spinner_timeline, "completed", G_CALLBACK(spinner_spin_cb), NULL);
}

static void gui_spinner_start()
{
    clutter_actor_show(spinner);

	/* clutter_actor_raise_top is deprecated TW 21/04/13 */
    /* clutter_actor_raise_top(spinner); */
	/* clutter_actor_set_child_above_sibling(spinner, stage,((void *)0) ); */
	/* fprintf(stderr, "\ngui-spinner-start-above clutter_actor_insert_child_above LINE 479\n");  TEST TE 03/05/13 */
	/* clutter_actor_insert_child_above(spinner, stage, ((void *)0) );  TEST TW 06/05/13 */

    clutter_timeline_start(spinner_timeline);
}

static void gui_spinner_stop()
{
    clutter_timeline_stop(spinner_timeline);
    clutter_actor_hide(spinner);
}

static void gui_dock_align_icons(gboolean animated)
{
    if (!dockitems)
        return;
    gint count = g_list_length(dockitems);
    if (count == 0) {
        return;
    }
    gfloat spacing = ICON_SPACING;
    gfloat ypos = ICON_SPACING/2;
    gfloat xpos = 0.0;
    gint i = 0;
    if (count > (gint)device_info->home_screen_icon_columns) {
        spacing = 3.0;
    }
    gfloat totalwidth = count * device_info->home_screen_icon_width + spacing * (count - 1);
    xpos = (stage_area.x2 - totalwidth) / 2.0;

    /* set positions */
    for (i = 0; i < count; i++) {
        SBItem *item = g_list_nth_data(dockitems, i);
        if (!item || !item->texture) {
            continue;
        }

		/* fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-get-parent\n", __func__); */
        ClutterActor *icon = clutter_actor_get_parent(item->texture);
		/* fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-get-parent\n", __func__); */

        if (!icon) {
            continue;
        }

        if (item != selected_item) {
            if (animated) {
				/* clutter_actor_animate is deprecated TW 20/04/13 */				
                /* clutter_actor_animate(icon, CLUTTER_EASE_OUT_QUAD, ICON_MOVEMENT_DURATION, "x", xpos, "y", ypos, NULL); */
				clutter_actor_set_easing_mode (icon, CLUTTER_EASE_OUT_QUAD);
				clutter_actor_set_easing_duration (icon, ICON_MOVEMENT_DURATION);
				clutter_actor_set_position (icon, xpos, ypos);

            } else {
                clutter_actor_set_position(icon, xpos, ypos);
            }
        }

        xpos += device_info->home_screen_icon_width;
        if (i < count - 1) {
            xpos += spacing;
        }
    }
}

static void gui_page_align_icons(guint page_num, gboolean animated)
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

    gfloat ypos = ICON_SPACING;
    gfloat xpos = ICON_SPACING + PAGE_X_OFFSET(page_num);

    gint i = 0;
    gfloat item_offset = (device_info->home_screen_icon_height+ICON_SPACING);

    /* set positions */
    for (i = 0; i < count; i++) {
        SBItem *item = g_list_nth_data(pageitems, i);
        if (!item) {
            debug_printf("%s: item is null for i=%d\n", __func__, i);
            continue;
        }
        if (!item->texture) {
            debug_printf("%s(%d,%d): i=%d item->texture is null\n", __func__, page_num, animated, i);
            continue;
        }

		/* fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-get-parent\n", __func__); */

		/* Clutter-CRITICAL **: clutter_actor_get_parent: assertion `CLUTTER_IS_ACTOR (self)' failed */
		/* Runtime error when moving icons with mouse or click on icon TW 13/05/13 */

        ClutterActor *icon = clutter_actor_get_parent(item->texture);
		/* fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-get-parent\n", __func__); */

        if (!icon) {
            continue;
        }

        if (item != selected_item) {
            if (animated) {
				/* clutter_actor_animate is deprecated TW 20/04/13 */
                /* clutter_actor_animate(icon, CLUTTER_EASE_OUT_QUAD, ICON_MOVEMENT_DURATION, "x", xpos, "y", ypos, NULL); */
				clutter_actor_set_easing_mode (icon, CLUTTER_EASE_OUT_QUAD);
				clutter_actor_set_easing_duration (icon, ICON_MOVEMENT_DURATION);
				clutter_actor_set_position (icon, xpos, ypos);
            } else {
                clutter_actor_set_position(icon, xpos, ypos);
            }
        }

        if (((i + 1) % device_info->home_screen_icon_columns) == 0) {
            xpos = ICON_SPACING + PAGE_X_OFFSET(page_num);
            if (ypos + item_offset < sb_area.y2 - sb_area.y1) {
                ypos += (device_info->home_screen_icon_height + ICON_SPACING);
            }
        } else {
            xpos += device_info->home_screen_icon_width + (stage_area.x2 - (ICON_SPACING*2) - (device_info->home_screen_icon_columns*device_info->home_screen_icon_width)) / (device_info->home_screen_icon_columns-1);
        }
    }
}

static void gui_page_indicator_group_align()
{
	/* clutter_group_get_n_children' is deprecated TW 20/04/13 */
    /* gint count = clutter_group_get_n_children(CLUTTER_GROUP(page_indicator_group)); */
	gint count = clutter_actor_get_n_children(CLUTTER_ACTOR(page_indicator_group));

    gint i;
    gfloat xpos = 0.0;

    if (count <= 0)
        return;

    for (i = 0; i < count; i++) {

		/* clutter_group_get_nth_child is deprecated TW 20/04/13 */
        /* ClutterActor *dot = clutter_group_get_nth_child(CLUTTER_GROUP(page_indicator_group), i); */
		ClutterActor *dot = clutter_actor_get_child_at_index(CLUTTER_ACTOR(page_indicator_group), i);

        clutter_actor_set_position(dot, xpos, 0.0);
        clutter_actor_set_name(dot, g_strdup_printf("%d", i));
        if (i == current_page) {
            clutter_actor_set_opacity(dot, 200);
        } else {
            clutter_actor_set_opacity(dot, 100);
        }
        xpos += clutter_actor_get_width(dot);
    }

    clutter_actor_set_x(page_indicator_group, (stage_area.x2 - xpos) / 2.0);
}

static gboolean page_indicator_clicked_cb(ClutterActor *actor, ClutterButtonEvent *event, gpointer data);

static void gui_page_indicator_group_add(GList *page, int page_index)
{
    debug_printf("%s: adding page indicator for page %d\n", __func__, page_index);
    if (page_indicator) {
        ClutterActor *actor = clutter_clone_new(CLUTTER_ACTOR(page_indicator));

		/* clutter_actor_unparent' is deprecated TW 20/04/13 */
		/* clutter_actor_unparent(actor); */
		/* ClutterActor *parent = clutter_actor_get_parent(actor);   TW 24/04/13  Do not think this is required */
		
				
		fprintf(stderr,"\n%s:above1-clutter-actor-remove-child: stage, actor\n", __func__);
		clutter_actor_remove_child(stage, actor); /* TEST TW 06/05/13  maybe stage is self and actor is child TW 28/04/13 */
		fprintf(stderr,"\n%s:below1-clutter-actor-remove-child: stage, actor\n", __func__);

        clutter_actor_set_reactive(actor, TRUE);
        g_signal_connect(actor, "button-press-event", G_CALLBACK(page_indicator_clicked_cb), NULL);

		/* clutter_container_add_actor is deprecated TW 20/04/13 */
        /* clutter_container_add_actor(CLUTTER_CONTAINER(page_indicator_group), actor); */
		
		/* fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-add-child\n", __func__); */
		clutter_actor_add_child(CLUTTER_ACTOR(page_indicator_group), actor);
		/* fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-add-child\n", __func__); */

        gui_page_indicator_group_align();
    }
}

static void gui_page_indicator_group_remove(GList *page, int page_index)
{
    debug_printf("%s: removing page indicator for page %d\n", __func__, page_index);
    if (page_indicator) {

		/* clutter_group_get_nth_child is deprecated TW 20/04/13 */
        /* ClutterActor *actor = clutter_group_get_nth_child(CLUTTER_GROUP(page_indicator_group), page_index); */
		ClutterActor *actor = clutter_actor_get_child_at_index(CLUTTER_ACTOR(page_indicator_group), page_index);		

        /* afaik, this also removes it from the container */
        clutter_actor_destroy(actor);
        gui_page_indicator_group_align();
    }
}

static void gui_pages_remove_empty()
{
    gint count = g_list_length(sbpages);
    gint i;
    GList *page = NULL;

    for (i = 0; i < count; i++) {
        page = g_list_nth_data(sbpages, i);
        debug_printf("%s: checking page %d itemcount %d\n", __func__, i, g_list_length(page));
        if (g_list_length(page) == 0) {
            debug_printf("%s: removing page %d\n", __func__, i);
            gui_page_indicator_group_remove(page, i);
        }
    }
    sbpages = g_list_remove_all(sbpages, NULL);
}

static void gui_set_current_page(int pageindex, gboolean animated)
{
    /* clutter_group_get_n_children' is deprecated TW 20/04/13 */
	/* gint count = clutter_group_get_n_children(CLUTTER_GROUP(page_indicator_group)); */
	gint count = clutter_actor_get_n_children(CLUTTER_ACTOR(page_indicator_group));
	/* fprintf(stderr,"\n%s:page-indicator-group number of children = %i\n", __func__, count);  TEST TW 12/05/13 */
    if ((pageindex < 0) || (pageindex >= count))
        return;

    /* make sure the page has correct aligned icons */
    gui_page_align_icons(pageindex, FALSE);

    current_page = pageindex;

    gui_page_indicator_group_align();

    if (animated) {
		/* clutter_actor_animate is deprecated TW 20/04/13 */
        /* clutter_actor_animate(the_sb, CLUTTER_EASE_IN_OUT_CUBIC, 400, "x", (gfloat) (-PAGE_X_OFFSET(current_page)), NULL); */
		clutter_actor_set_easing_mode (the_sb, CLUTTER_EASE_IN_OUT_CUBIC);
		clutter_actor_set_easing_duration (the_sb, 400);
		clutter_actor_set_x (the_sb, (gfloat)(-PAGE_X_OFFSET(current_page)));
    } else {
        clutter_actor_set_x(the_sb, (gfloat)(-PAGE_X_OFFSET(current_page)));
    }
}

static void gui_show_next_page()
{
    gui_set_current_page(current_page+1, TRUE);
}

static void gui_show_previous_page()
{
    gui_set_current_page(current_page-1, TRUE);
}

static plist_t sbitem_get_subitems(SBItem *item)
{
    plist_t result = plist_new_array();
    if (item && item->subitems) {
        guint i;
        for (i = 0; i < g_list_length(item->subitems); i++) {
            SBItem *subitem = g_list_nth_data(item->subitems, i);
            plist_t node = plist_dict_get_item(subitem->node, "displayIdentifier");
            if (!node) {
                printf("could not get displayIdentifier\n");
                continue;
            }
            plist_array_append_item(result, plist_copy(node));
        }
    }
    return result;
}

static plist_t sbitem_to_plist(SBItem *item)
{
    plist_t result = plist_new_dict();
    if (!item) {
        return result;
    }
    plist_t node;
    if (item->is_folder) {
        node = plist_dict_get_item(item->node, "displayName");
        if (!node) {
            printf("could not get displayName for folder!\n");
            return result;
        }
        plist_dict_insert_item(result, "displayName", plist_copy(node));
        plist_t iconlists = plist_new_array();
        plist_array_append_item(iconlists, sbitem_get_subitems(item));
        plist_dict_insert_item(result, "iconLists", iconlists);
    } else {
        node = plist_dict_get_item(item->node, "displayIdentifier");
        if (!node) {
            printf("could not get displayIdentifier\n");
            return result;
        }
        plist_dict_insert_item(result, "displayIdentifier", plist_copy(node));
    }
    return result;
}

plist_t gui_get_iconstate(const char *format_version)
{
    plist_t iconstate = NULL;
    plist_t pdockarray = NULL;
    plist_t pdockitems = NULL;
    guint i;

    int use_version = 1;
    if (format_version && (strcmp(format_version, "2") == 0)) {
        use_version = 2;
    }

    guint count = g_list_length(dockitems);
    pdockitems = plist_new_array();
    for (i = 0; i < count; i++) {
        SBItem *item = g_list_nth_data(dockitems, i);
        if (item && item->node) {
            plist_array_append_item(pdockitems, sbitem_to_plist(item));
        }
    }

    if (use_version == 1) {
        for (i = count; i < num_dock_items; i++) {
            plist_array_append_item(pdockitems, plist_new_bool(0));
        }
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
            if (use_version == 2) {
                row = plist_new_array();
                plist_array_append_item(ppage, row);
            }
            for (j = 0; j < (device_info->home_screen_icon_columns*device_info->home_screen_icon_rows); j++) {
                SBItem *item = g_list_nth_data(page, j);
                if (use_version == 1) {
                    if ((j % device_info->home_screen_icon_columns) == 0) {
                        row = plist_new_array();
                        plist_array_append_item(ppage, row);
                    }
                }
                if (item && item->node) {
                    plist_array_append_item(row, sbitem_to_plist(item));
                } else {
                    if (use_version == 1)
                        plist_array_append_item(row, plist_new_bool(0));
                }
            }
            plist_array_append_item(iconstate, plist_copy(ppage));
            plist_free(ppage);
        }
    }

    return iconstate;
}

/* input */
static gboolean stage_motion_cb(ClutterActor *actor, ClutterMotionEvent *event, gpointer user_data)
{
    /* check if an item has been raised */
    if (!selected_item) {
        return FALSE;
    }

	fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-get-parent\n", __func__);
	
	/* Clutter-CRITICAL **: clutter_actor_get_parent: assertion `CLUTTER_IS_ACTOR (self)' failed */
	/* Runtime error when moving icons with mouse or click on icon TW 13/05/13 */
    ClutterActor *icon = clutter_actor_get_parent(selected_item->texture);
	
	fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-get-parent\n", __func__);
    
    fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-move-by\n", __func__);

    /* Clutter-CRITICAL **: clutter_actor_move_by: assertion `CLUTTER_IS_ACTOR (self)' failed */
    /* Runtime error when moving icons on stage or click on icons TW 18/05/13 */
    
    clutter_actor_move_by(icon, (event->x - start_x), (event->y - start_y));
    fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-move-by\n", __func__);

    if (event->x-start_x > 0) {
        move_left = FALSE;
    } else {
        move_left = TRUE;
    }

    start_x = event->x;
    start_y = event->y;

    gfloat center_x;
    gfloat center_y;
    clutter_actor_get_abs_center(icon, &center_x, &center_y);

    if (!selected_folder && clutter_actor_box_contains(&left_trigger, center_x - (device_info->home_screen_icon_width / 2), center_y)) {
        if (current_page > 0) {
            if (elapsed_ms(&last_page_switch, 1000)) {
                gui_show_previous_page();
                gettimeofday(&last_page_switch, NULL);
            }
        }
    } else if (!selected_folder && clutter_actor_box_contains(&right_trigger, center_x + (device_info->home_screen_icon_width / 2), center_y)) {
        if (current_page < (gint)(g_list_length(sbpages)-1)) {
            if (elapsed_ms(&last_page_switch, 1000)) {
                gui_show_next_page();
                gettimeofday(&last_page_switch, NULL);
            }
        }
    }

    if (selected_folder) {
        selected_folder->subitems = g_list_remove(selected_folder->subitems, selected_item);
        selected_folder->subitems = iconlist_insert_item_at(selected_folder->subitems, selected_item, (center_x - 0.0), (center_y - split_pos - clutter_actor_get_y(aniupper)), 0, 4);
        gui_folder_align_icons(selected_folder, TRUE);
    } else if (selected_item->is_dock_item) {
        dockitems = g_list_remove(dockitems, selected_item);
        if (center_y >= dock_area.y1) {
            debug_printf("%s: icon from dock moving inside the dock!\n", __func__);
            selected_item->is_dock_item = TRUE;
            dockitems =
                iconlist_insert_item_at(dockitems, selected_item, (center_x - dock_area.x1), (center_y - dock_area.y1), 0, num_dock_items);
            gui_dock_align_icons(TRUE);
        } else {
            debug_printf("%s: icon from dock moving outside the dock!\n", __func__);
            selected_item->is_dock_item = FALSE;
            gui_page_align_icons(current_page, TRUE);
        }
    } else {
        int p = current_page;
        int i;
        GList *pageitems = NULL;
        debug_printf("%s: current_page %d\n", __func__, p);
        /* remove selected_item from all pages */
        int count = g_list_length(sbpages);
        for (i = 0; i < count; i++) {
            pageitems = g_list_nth_data(sbpages, i);
            sbpages = g_list_remove(sbpages, pageitems);
            pageitems = g_list_remove(pageitems, selected_item);
            sbpages = g_list_insert(sbpages, pageitems, i);
        }
        /* get current page */
        pageitems = g_list_nth_data(sbpages, p);
        /* remove current page from pages list as we will alter it */
        sbpages = g_list_remove(sbpages, pageitems);
        if (center_y >= dock_area.y1 && (g_list_length(dockitems) < num_dock_items)) {
            debug_printf("%s: regular icon is moving inside the dock!\n", __func__);
            selected_item->is_dock_item = TRUE;
        } else {
            debug_printf("%s: regular icon is moving!\n", __func__);
            pageitems =
                iconlist_insert_item_at(pageitems, selected_item, (center_x - sb_area.x1) + PAGE_X_OFFSET(p), (center_y - sb_area.y1), p, 4);
        }
        /* insert back current page */
        sbpages = g_list_insert(sbpages, pageitems, p);
        gui_dock_align_icons(TRUE);
        gui_page_align_icons(p, TRUE);
    }

    return TRUE;
}

static gboolean page_indicator_clicked_cb(ClutterActor *actor, ClutterButtonEvent *event, gpointer data)
{
    if (event->click_count > 1) {
        return FALSE;
    }
    const gchar *index_str = clutter_actor_get_name(actor);
    int pageindex = strtol(index_str, NULL, 10);
    debug_printf("page indicator for page %d has been clicked\n", pageindex);
    gui_set_current_page(pageindex, TRUE);
    return TRUE;
}

static void gui_folder_redraw_subitems(SBItem *item)
{
    if (!item)
        return;

    ClutterActor *minigrp = NULL;

	fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-get-parent\n", __func__);
    ClutterActor *grp = clutter_actor_get_parent(item->texture);
	fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-get-parent\n", __func__);

	/* clutter_group_get_n_children' is deprecated TW 20/04/13 */
    /* guint cnt = clutter_group_get_n_children(CLUTTER_GROUP(grp)); */
	guint cnt = clutter_actor_get_n_children(CLUTTER_ACTOR(grp));

    guint i;
    for (i = 0; i < cnt; i++) {

		/* clutter_group_get_nth_child is deprecated TW 20/04/13 */
        /* ClutterActor *act = clutter_group_get_nth_child(CLUTTER_GROUP(grp), i); */
		ClutterActor *act = clutter_actor_get_child_at_index(CLUTTER_ACTOR(grp), i);

        const char *nm = clutter_actor_get_name(act);
        if (nm && !strcmp(nm, "mini")) {
            minigrp = act;
	    break;
        }
    }
    if (minigrp) {
        clutter_actor_destroy(minigrp);
    }

	/* clutter_group_new is deprecated TW 20/04/13 */
    /* minigrp = clutter_group_new(); */
	minigrp = clutter_actor_new();

    clutter_actor_set_name(minigrp, "mini");

	/* clutter_container_add_actor is deprecated TW 20/04/13 */
    /* clutter_container_add_actor(CLUTTER_CONTAINER(grp), minigrp); */
	
	fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-add-child\n", __func__);
	clutter_actor_add_child(CLUTTER_ACTOR(grp), minigrp);
	fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-add-child\n", __func__);

    for (i = 0; i < g_list_length(item->subitems); i++) {
        SBItem *subitem = (SBItem*)g_list_nth_data(item->subitems, i);
        if (subitem && subitem->texture && subitem->node) {
            ClutterActor *suba = clutter_clone_new(subitem->texture);

			/* clutter_actor_unparent' is deprecated TW 20/04/13 */
			/* clutter_actor_unparent(suba); */
			
			fprintf(stderr,"\n%s:ERROR: above2 clutter-actor-get-parent\n", __func__);
			ClutterActor *parent = clutter_actor_get_parent(suba); /* TW 24/04/13 */
			fprintf(stderr,"\n%s:ERROR: below2 clutter-actor-get-parent\n", __func__);

			fprintf(stderr, "\n%sERROR:above1-clutter-actor-remove-child: parent, suba\n", __func__); /* TEST TW 28/04/13 */
			clutter_actor_remove_child(parent, suba);
			fprintf(stderr, "\n%sERROR:below1-clutter-actor-remove-child: parent, suba\n", __func__); /* TEST TW 28/04/13 */

			/* clutter_container_add_actor is deprecated TW 20/04/13 */
            /* clutter_container_add_actor(CLUTTER_CONTAINER(minigrp), suba); */
			fprintf(stderr,"\n%s:ERROR: above2 clutter-actor-add-child\n", __func__);
			clutter_actor_add_child(CLUTTER_ACTOR(minigrp), suba);
			fprintf(stderr,"\n%s:ERROR: below2 clutter-actor-add-child\n", __func__);

            clutter_actor_set_scale(suba, 0.22, 0.22);
            clutter_actor_set_position(suba, 8.0 + (i%3)*ICON_SPACING, 8.0 + ((double)(int)((int)i/(int)3))*ICON_SPACING);
            if (i < 9)
                clutter_actor_show(suba);
            else
                clutter_actor_hide(suba);
        }
    }
}

static void gui_folder_align_icons(SBItem *item, gboolean animated)
{
    if (!item || !item->subitems)
        return;

    gint count = g_list_length(item->subitems);

    gfloat ypos = 8.0 + ICON_SPACING + ICON_SPACING;
    gfloat xpos = (ICON_SPACING / 2);
    gint i = 0;

    /* set positions */
    for (i = 0; i < count; i++) {
        SBItem *si = g_list_nth_data(item->subitems, i);
        if (!si) {
            debug_printf("%s: item is null for i=%d\n", __func__, i);
            continue;
        }
        if (!si->texture) {
            debug_printf("%s: i=%d item->texture is null\n", __func__, i);
            continue;
        }

		fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-get-parent\n", __func__);
        ClutterActor *icon = clutter_actor_get_parent(si->texture);
		fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-get-parent\n", __func__);

        if (!icon) {
            continue;
        }

        if (si != selected_item) {
            if (animated) {
				/* clutter_actor_animate is deprecated TW 20/04/13 */
                /* clutter_actor_animate(icon, CLUTTER_EASE_OUT_QUAD, 250, "x", xpos, "y", ypos, NULL); */
				clutter_actor_set_easing_mode (icon, CLUTTER_EASE_OUT_QUAD);
				clutter_actor_set_easing_duration (icon, 250);
				clutter_actor_set_position (icon, xpos, ypos);

            } else {
                clutter_actor_set_position(icon, xpos, ypos);
            }
        }

        if (((i + 1) % 4) == 0) {
            xpos = ICON_SPACING;
            ypos += 88.0;
        } else {
            xpos += 76.0;
        }
    }
}

static gboolean folderview_close_finish(gpointer user_data)
{
    SBItem *item = (SBItem*)user_data;

	/* clutter_group_get_nth_child is deprecated TW 20/04/13 */
    /* ClutterActor *label = clutter_group_get_nth_child(CLUTTER_GROUP(folder), 2); */
	ClutterActor *label = clutter_actor_get_child_at_index(CLUTTER_ACTOR(folder), 2);

    const gchar *oldname = clutter_text_get_text(CLUTTER_TEXT(item->label));
    const gchar *newname = clutter_text_get_text(CLUTTER_TEXT(label));
    if (g_str_equal(oldname, newname) == FALSE) {
        gfloat oldwidth = clutter_actor_get_width(item->label);
        clutter_text_set_text(CLUTTER_TEXT(item->label), newname);
        plist_dict_remove_item(item->node, "displayName");
        plist_dict_insert_item(item->node, "displayName", plist_new_string(newname));
        gfloat newwidth = clutter_actor_get_width(item->label);
        gfloat xshift = -(newwidth-oldwidth)/2;
        clutter_actor_move_by(item->label, xshift, 0);
        if (item->label_shadow) {
            clutter_text_set_text(CLUTTER_TEXT(item->label_shadow), newname);
            clutter_actor_move_by(item->label_shadow, xshift, 0);
        }
    }
    clutter_actor_show(item->label);
    if (item->label_shadow) {
        clutter_actor_show(item->label_shadow);
    }

	fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-get-parent\n", __func__);
    ClutterActor *newparent = clutter_actor_get_parent(item->texture);
	fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-get-parent\n", __func__);	

    GList *subitems = item->subitems;
    guint i;
    for (i = 0; i < g_list_length(subitems); i++) {
        SBItem *si = g_list_nth_data(subitems, i);

		fprintf(stderr,"\n%s:ERROR: above2 clutter-actor-get-parent\n", __func__);
        ClutterActor *actor = clutter_actor_get_parent(si->texture);
		fprintf(stderr,"\n%s:ERROR: below2 clutter-actor-get-parent\n", __func__);
		
		/* clutter_actor_reparent is deprecated TW 21/04/13 */
        /* clutter_actor_reparent(actor, newparent); */
		
		fprintf(stderr, "\n%sERROR:above1-clutter-actor-remove-child: actor, newparent\n", __func__); /* TEST TW 28/04/13 */
		clutter_actor_remove_child(actor, newparent);
		fprintf(stderr, "\n%sERROR:below1-clutter-actor-remove-child: actor, newparent\n", __func__); /* TEST TW 28/04/13 */
        clutter_actor_hide(actor);
    }

    gui_folder_redraw_subitems(item);

    clutter_actor_destroy(folder);
    folder = NULL;
    clutter_actor_destroy(aniupper);
    aniupper = NULL;
    clutter_actor_destroy(anilower);
    anilower = NULL;
    split_pos = 0.0;

    /* un-dim sb and dock items */
    GList *page = g_list_nth_data(sbpages, current_page);
    SBItem *it;
    ClutterActor *act;
    for (i = 0; i < g_list_length(page); i++) {
        it = g_list_nth_data(page, i);

		fprintf(stderr,"\n%s:ERROR: above3 clutter-actor-get-parent\n", __func__);
        act = clutter_actor_get_parent(it->texture);
		fprintf(stderr,"\n%s:ERROR: below3 clutter-actor-get-parent\n", __func__);
        clutter_actor_set_opacity(act, 255);
    }
    for (i = 0; i < g_list_length(dockitems); i++) {
        it = g_list_nth_data(dockitems, i);

		fprintf(stderr,"\n%s:ERROR: above4 clutter-actor-get-parent\n", __func__);
        act = clutter_actor_get_parent(it->texture);
		fprintf(stderr,"\n%s:ERROR: below4 clutter-actor-get-parent\n", __func__);

        clutter_actor_set_opacity(act, 255);
    }

    /* show page indicators */
    clutter_actor_show(page_indicator_group);

    clutter_actor_set_reactive(item->texture, TRUE);

    selected_folder = NULL;

    return FALSE;
}

static void folderview_close(SBItem *folderitem)
{
    clutter_actor_set_reactive(folderitem->texture, FALSE);

    clutter_actor_set_reactive(aniupper, FALSE);
    clutter_actor_set_reactive(anilower, FALSE);

	/* clutter_actor_raise_top is deprecated TW 21/04/13 */    
	/* clutter_actor_raise_top(aniupper); */
	/* TEST TW 28/04/13 */
	fprintf(stderr,"\nERROR: clutter_actor_set_child_above_sibling2: LINE 1147 \n"); /* TEST TW 28/04/13 */
	clutter_actor_set_child_above_sibling(aniupper, stage,((void *)0) );
    

	/* clutter_actor_raise_top is deprecated TW 21/04/13 */
	/* clutter_actor_raise_top(anilower); */
	
	fprintf(stderr,"\nERROR: clutter_actor_set_child_above_sibling3: LINE 1154 \n"); /* TEST TW 28/04/13 */
	clutter_actor_insert_child_above(anilower, stage,((void *)0) );
	
	/* clutter_actor_animate is deprecated TW 20/04/13 */
    /* clutter_actor_animate(aniupper, CLUTTER_EASE_IN_OUT_QUAD, FOLDER_ANIM_DURATION, "y", 0.0, NULL); */
	clutter_actor_set_easing_mode (aniupper, CLUTTER_EASE_OUT_QUAD);
	clutter_actor_set_easing_duration (aniupper, FOLDER_ANIM_DURATION);
	clutter_actor_set_y(aniupper, 0.0);
	
	/* clutter_actor_animate is deprecated TW 20/04/13 */
	/* clutter_actor_animate(anilower, CLUTTER_EASE_IN_OUT_QUAD, FOLDER_ANIM_DURATION, "y", 0.0, NULL); */
	clutter_actor_set_easing_mode (anilower, CLUTTER_EASE_OUT_QUAD);
	clutter_actor_set_easing_duration (anilower, FOLDER_ANIM_DURATION);
	clutter_actor_set_y(anilower, 0.0);
	
	/* clutter_actor_animate is deprecated TW 20/04/13 */
    /* clutter_actor_animate(folder, CLUTTER_EASE_IN_OUT_QUAD, FOLDER_ANIM_DURATION, "y", split_pos, NULL); */
	clutter_actor_set_easing_mode (folder, CLUTTER_EASE_OUT_QUAD);
	clutter_actor_set_easing_duration (folder, FOLDER_ANIM_DURATION);
	clutter_actor_set_y(folder, split_pos);

    clutter_threads_add_timeout(FOLDER_ANIM_DURATION, (GSourceFunc)folderview_close_finish, (gpointer)folderitem);
}

static gboolean folderview_close_cb(ClutterActor *actor, ClutterButtonEvent *event, gpointer user_data)
{
    /* discard double clicks */
    if (event->click_count > 1) {
        return FALSE;
    }

    SBItem *item = (SBItem*)user_data;

    folderview_close(item);

    return TRUE;
}

static gboolean folderview_open_finish(gpointer user_data)
{
	/* clutter_group_get_nth_child is deprecated TW 20/04/13 */
    /* ClutterActor *marker = clutter_group_get_nth_child(CLUTTER_GROUP(folder), 2); */
	ClutterActor *marker = clutter_actor_get_child_at_index(CLUTTER_ACTOR(folder), 2);

	/* clutter_actor_raise_top is deprecated TW 21/04/13 */
    /* clutter_actor_raise_top(folder); */
	fprintf(stderr,"\nERROR: clutter_actor_set_child_above_sibling4 LINE 1200 \n"); /* TEST TW 28/04/13 */
	clutter_actor_insert_child_above(folder, stage,((void *)0) );

    clutter_actor_show(marker);

    g_signal_connect(aniupper, "button-press-event", G_CALLBACK(folderview_close_cb), user_data);
    g_signal_connect(anilower, "button-press-event", G_CALLBACK(folderview_close_cb), user_data);

    return FALSE;
}

static void folderview_open(SBItem *item)
{
    GList *page = g_list_nth_data(sbpages, current_page);
    guint i;
    SBItem *it;
    ClutterActor *act;
    gfloat ypos = 0;
    gfloat xpos = 0;

    gboolean is_dock_folder = FALSE;

    selected_folder = item;

    ClutterActor *fldr = NULL;

    /* dim the springboard icons */
    for (i = 0; i < g_list_length(page); i++) {
        it = g_list_nth_data(page, i);

		fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-get-parent\n", __func__);
        act = clutter_actor_get_parent(it->texture);
		fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-get-parent\n", __func__);

        if (item == it) {
            clutter_actor_set_opacity(act, 255);
            ypos = 24.0+(gfloat)((int)(i / 4)+1) * 88.0;
            xpos = 16 + ((i % 4))*76.0;
            clutter_actor_hide(it->label);
            if (it->label_shadow) {
                clutter_actor_hide(it->label_shadow);
            }
            fldr = act;
        } else {
            clutter_actor_set_opacity(act, 64);
        }
    }

    /* dim the dock icons */
    guint count = g_list_length(dockitems);
    for (i = 0; i < count; i++) {
        it = g_list_nth_data(dockitems, i);
		
		fprintf(stderr,"\n%s:ERROR: above2 clutter-actor-get-parent\n", __func__);
        act = clutter_actor_get_parent(it->texture);
		fprintf(stderr,"\n%s:ERROR: below2 clutter-actor-get-parent\n", __func__);

        if (item == it) {
            clutter_actor_set_opacity(act, 255);
	    ypos = stage_area.y1 - DOCK_HEIGHT - ICON_SPACING;
	    gfloat spacing = ICON_SPACING;
	    if (count > 4) {
		spacing = 3.0; 
	    }
            gfloat totalwidth = count*57.0 + (count-1) * spacing;
	    xpos = (stage_area.x2 - totalwidth)/2.0 + (i*57.0) + (i*spacing);
            clutter_actor_hide(it->label);
            if (it->label_shadow) {
                clutter_actor_hide(it->label_shadow);
            }
	    is_dock_folder = TRUE;
            fldr = act;
        } else {
            clutter_actor_set_opacity(act, 64);
        }
    }

    /* hide page indicators */
    clutter_actor_hide(page_indicator_group);

    if (item->texture_shadow) {
        clutter_actor_hide(item->texture_shadow);
    }

    /* make snapshot from the stage */
    guchar *shot = clutter_stage_read_pixels(CLUTTER_STAGE(stage), 0, 0, stage_area.x2, stage_area.y2);
    if (!shot) {
        printf("Error creating stage snapshot!\n");
        return;
    }

    if (item->texture_shadow) {
        clutter_actor_show(item->texture_shadow);
    }

    /* upper */
	
	/* clutter_group_new is deprecated TW 20/04/13 */
    /* aniupper = clutter_group_new(); */
	aniupper = clutter_actor_new();

	/* clutter_container_add_actor is deprecated TW 20/04/13 */
    /* clutter_container_add_actor(CLUTTER_CONTAINER(stage), aniupper); */
	fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-add-child\n", __func__);
	clutter_actor_add_child(CLUTTER_ACTOR(stage), aniupper);
	fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-add-child\n", __func__);

   	/* clutter_texture_new' is deprecated TW 21/04/13 */
	/* act = clutter_texture_new(); */
	ClutterContent *act1 = clutter_image_new(); /* ClutterContent TW 24/04/13 */

	/* clutter_texture_set_from_rgb_data' is deprecated TW 21/04/13 */
    /* clutter_texture_set_from_rgb_data(CLUTTER_TEXTURE(act), shot, TRUE, stage_area.x2, ypos, stage_area.x2*4, 4, CLUTTER_TEXTURE_NONE, NULL); */
	clutter_image_set_data(CLUTTER_IMAGE(act1), shot, 2, stage_area.x2, ypos, stage_area.x2*4, NULL);	

	/* clutter_container_add_actor is deprecated TW 20/04/13 */
    /* clutter_container_add_actor(CLUTTER_CONTAINER(aniupper), act); */
	clutter_actor_set_content(CLUTTER_ACTOR(aniupper), act1);

    clutter_actor_set_position(aniupper, 0, 0);
    clutter_actor_set_reactive(aniupper, TRUE);
    clutter_actor_show(aniupper);
	
	/* clutter_actor_raise_top is deprecated TW 21/04/13 */
    /* clutter_actor_raise_top(aniupper); */
	fprintf(stderr,"\nERROR: clutter_actor_set_child_above_sibling5: LINE 1315 \n"); /* TEST TW 28/04/13 */
	clutter_actor_set_child_above_sibling(stage, aniupper,((void *)0) );

    /* lower */
	
	/* clutter_group_new is deprecated TW 20/04/13 */
    /* anilower = clutter_group_new(); */
	anilower = clutter_actor_new();

	/* clutter_container_add_actor is deprecated TW 20/04/13 */
    /* clutter_container_add_actor(CLUTTER_CONTAINER(stage), anilower); */
	fprintf(stderr,"\n%s:ERROR: above2 clutter-actor-add-child\n", __func__);
	clutter_actor_add_child(CLUTTER_ACTOR(stage), anilower);
	fprintf(stderr,"\n%s:ERROR: below2 clutter-actor-add-child\n", __func__);

    /* clutter_texture_new' is deprecated TW 21/04/13 */
	/* act = clutter_texture_new(); */
	ClutterContent *act2 = clutter_image_new();	/* ClutterContent TW 24/04/13 */

	/* clutter_texture_set_from_rgb_data' is deprecated TW 21/04/13 */    
	/* clutter_texture_set_from_rgb_data(CLUTTER_TEXTURE(act), shot, TRUE, stage_area.x2, stage_area.y2, stage_area.x2*4, 4, CLUTTER_TEXTURE_NONE, NULL); */
	clutter_image_set_data(CLUTTER_IMAGE(act2), shot, 2, stage_area.x2, ypos, stage_area.x2*4, NULL);

    clutter_actor_set_clip(anilower, 0.0, ypos, (gfloat)(stage_area.x2), (gfloat)(stage_area.y2)-ypos);

	/* clutter_container_add_actor is deprecated TW 20/04/13 */
    /* clutter_container_add_actor(CLUTTER_CONTAINER(anilower), act); */
	clutter_actor_set_content(CLUTTER_ACTOR(anilower), act2);

    clutter_actor_set_position(anilower, 0, 0);
    clutter_actor_set_reactive(anilower, TRUE);
    clutter_actor_show(anilower);

	/* clutter_actor_raise_top is deprecated TW 21/04/13 */
    /* clutter_actor_raise_top(anilower); */
	fprintf(stderr,"\nERROR: clutter_actor_set_child_above_sibling6 LINE 1348\n"); /* TEST TW 28/04/13 */
	clutter_actor_set_child_above_sibling(stage, anilower,((void *)0) );

    /* add a clone of the original folder icon */
    ClutterActor *act3 = clutter_clone_new(fldr);
    if (is_dock_folder) {

		/* clutter_container_add_actor is deprecated TW 20/04/13 */
        /* clutter_container_add_actor(CLUTTER_CONTAINER(anilower), act);*/
		
		fprintf(stderr,"\n%s:ERROR: above3 clutter-actor-add-child\n", __func__);
		clutter_actor_add_child(CLUTTER_ACTOR(anilower), act3);
		fprintf(stderr,"\n%s:ERROR: below3 clutter-actor-add-child\n", __func__);

        clutter_actor_set_position(act3, xpos, ypos+20.0);
    } else {

		/* clutter_container_add_actor is deprecated TW 20/04/13 */
        /* clutter_container_add_actor(CLUTTER_CONTAINER(aniupper), act); */
		
		fprintf(stderr,"\n%s:ERROR: above4 clutter-actor-add-child\n", __func__);
		clutter_actor_add_child(CLUTTER_ACTOR(aniupper), act3);
		fprintf(stderr,"\n%s:ERROR: below4 clutter-actor-add-child\n", __func__);

        clutter_actor_set_position(act3, xpos, ypos-80.0);
    }

    /* create folder container */

	/* clutter_group_new is deprecated TW 20/04/13 */
    /* folder = clutter_group_new(); */
	folder = clutter_actor_new();

	/* clutter_container_add_actor is deprecated TW 20/04/13 */
    /* clutter_container_add_actor(CLUTTER_CONTAINER(stage), folder); */

	fprintf(stderr,"\n%s:ERROR: above5 clutter-actor-add-child\n", __func__);
	clutter_actor_add_child(CLUTTER_ACTOR(stage), folder);
	fprintf(stderr,"\n%s:ERROR: below5 clutter-actor-add-child\n", __func__);

	/* clutter_actor_raise_top is deprecated TW 21/04/13 */
    /* clutter_actor_raise_top(folder); */
	fprintf(stderr,"\nERROR: clutter_actor_set_child_above_sibling7: LINE 1381 \n"); /* TEST TW 28/04/13 */
	clutter_actor_set_child_above_sibling(stage, folder,((void *)0) );

    clutter_actor_set_position(folder, 0, ypos);
    clutter_actor_show(folder);

    /* folder background rect */
    /* ClutterColor folderbg = {0x70, 0x70, 0x70, 255}; */ /* TW 24/04/13 */

	/* clutter_rectange_new_with_color is deprecated TW 20/04/13 */
   
    /*  *act = clutter_rectangle_new_with_color(&folderbg); */
	ClutterActor *act11 = clutter_actor_new();

    /* ClutterColor folderbd = {0xe0, 0xe0, 0xe0, 255}; */ /* TW 24/04/13 */
	
	/* clutter_rectangle_set_border_color' is deprecated TW 24/04/13 */
	/* clutter_rectangle_set_border_color(CLUTTER_RECTANGLE(act), &folderbd); */
	
	/* clutter_rectangle_set_border_width' is deprecated TW 24/03/13 */
    /* clutter_rectangle_set_border_width(CLUTTER_RECTANGLE(act), 1); */

	fprintf(stderr,"\nERROR: above clutter_set_size3: LINE 1402 \n");
    clutter_actor_set_size(act11, stage_area.x2, 1);
    clutter_actor_set_position(act11, 0, 0);
    clutter_actor_set_reactive(act11, TRUE);

	/* clutter_container_add_actor is deprecated TW 20/04/13 */
    /* clutter_container_add_actor(CLUTTER_CONTAINER(folder), act); */

	fprintf(stderr,"\n%s:ERROR: above6 clutter-actor-add-child\n", __func__);
	clutter_actor_add_child(CLUTTER_ACTOR(folder), act11);
	fprintf(stderr,"\n%s:ERROR: below6 clutter-actor-add-child\n", __func__);

    clutter_actor_show(act11);

    /* create folder name label */
    /* ClutterColor rcolor = {255, 255, 255, 255}; */ /* TW 24/04/13 */

	/* clutter_rectange_new_with_color is deprecated TW 20/04/13 */
    /* ClutterActor *trect = clutter_rectangle_new_with_color(&rcolor); */
	ClutterActor *trect = clutter_actor_new();

	/* clutter_container_add_actor is deprecated TW 20/04/13 */
    /* clutter_container_add_actor(CLUTTER_CONTAINER(folder), trect); */

	fprintf(stderr,"\n%s:ERROR: above7 clutter-actor-add-child\n", __func__);
	clutter_actor_add_child(CLUTTER_ACTOR(folder), trect);
	fprintf(stderr,"\n%s:ERROR: below7 clutter-actor-add-child\n", __func__);


    clutter_actor_set_position(trect, 16.0, 8.0);

	fprintf(stderr,"\nERROR: above clutter_set_size4: LINE 1426 \n");
    clutter_actor_set_size(trect, (gfloat)(stage_area.x2)-32.0, 24.0);

    const gchar *ltext = clutter_text_get_text(CLUTTER_TEXT(item->label));
    ClutterColor lcolor = {0, 0, 0, 255};
    ClutterActor *lbl = clutter_text_new_full(FOLDER_LARGE_FONT, ltext, &lcolor);

	/* clutter_container_add_actor is deprecated TW 20/04/13 */
    /* clutter_container_add_actor(CLUTTER_CONTAINER(folder), lbl); */

	fprintf(stderr,"\n%s:ERROR: above8 clutter-actor-add-child\n", __func__);
	clutter_actor_add_child(CLUTTER_ACTOR(folder), lbl);
	fprintf(stderr,"\n%s:ERROR: below8 clutter-actor-add-child\n", __func__);


    clutter_actor_set_position(lbl, 16.0, 8.0);
    clutter_actor_set_width(lbl, (gfloat)(stage_area.x2)-32.0);

	/* clutter_actor_raise is deprecated TW 21/04/13 */
    /* clutter_actor_raise(lbl, trect); */
	fprintf(stderr,"\nERROR: clutter_actor_set_child_above_sibling8: LINE 1442 \n"); /* TEST TW 28/04/13 */
	clutter_actor_set_child_above_sibling(lbl, trect,((void *)0) );

    clutter_actor_grab_key_focus(lbl);
    clutter_text_set_editable(CLUTTER_TEXT(lbl), TRUE);
    clutter_text_set_selectable(CLUTTER_TEXT(lbl), TRUE);
    clutter_text_set_single_line_mode(CLUTTER_TEXT(lbl), TRUE);
    clutter_text_set_line_wrap(CLUTTER_TEXT(lbl), FALSE);
    clutter_actor_set_reactive(lbl, TRUE);
    ClutterColor selcolor = {0, 0, 0xa0, 200};
    ClutterColor curcolor = {0, 0, 0xa0, 255};
    clutter_text_set_selection_color(CLUTTER_TEXT(lbl), &selcolor);
    clutter_text_set_cursor_color(CLUTTER_TEXT(lbl), &curcolor);

    /* calculate height */
    gfloat fh = 8.0 + 18.0 + 8.0;
    if (item->subitems && (g_list_length(item->subitems) > 0)) {
        fh += (((g_list_length(item->subitems)-1)/device_info->home_screen_icon_columns) + 1)*88.0;
    } else {
        fh += 88.0;
    }

    /* folder marker */
    ClutterActor *marker = clutter_clone_new(folder_marker);
	
	/* clutter_actor_unparent' is deprecated TW 20/04/13 */    
	/* clutter_actor_unparent(marker); */

	fprintf(stderr,"\n%s:ERROR: above3 clutter-actor-get-parent\n", __func__);
	ClutterActor *parent = clutter_actor_get_parent(marker); /* TW 24/04/13 */
	fprintf(stderr,"\n%s:ERROR: below3 clutter-actor-get-parent\n", __func__);

	fprintf(stderr, "\n%sERROR:above1-clutter-actor-remove-child: parent, marker\n", __func__); /* TEST TW 28/04/13 */
	clutter_actor_remove_child(parent, marker);
	fprintf(stderr, "\n%sERROR:below1-clutter-actor-remove-child: parent, marker\n", __func__); /* TEST TW 28/04/13 */

	/* clutter_container_add_actor is deprecated TW 20/04/13 */
    /* clutter_container_add_actor(CLUTTER_CONTAINER(folder), marker); */

	fprintf(stderr,"\n%s:ERROR: above9 clutter-actor-add-child\n", __func__);
	clutter_actor_add_child(CLUTTER_ACTOR(folder), marker);
	fprintf(stderr,"\n%s:ERROR: below9 clutter-actor-add-child\n", __func__);

    if (is_dock_folder) {

	/* clutter_actor_set_rotation is deprecated TW 21/04/13 */
	/* clutter_actor_set_rotation(marker, CLUTTER_Z_AXIS, 180.0, 29.0, 8.0, 0.0); */
	clutter_actor_set_rotation_angle(marker, CLUTTER_Z_AXIS, 180.0);

	clutter_actor_set_position(marker, xpos, fh-2.0);
        clutter_actor_hide(marker);
    } else {
        clutter_actor_set_position(marker, xpos, -14.0);
        clutter_actor_show(marker);
    }

    /* reparent the icons to the folder */
    for (i = 0; i < g_list_length(item->subitems); i++) {
        SBItem *si = g_list_nth_data(item->subitems, i);
		
		fprintf(stderr,"\n%s:ERROR: above4 clutter-actor-get-parent\n", __func__);
        ClutterActor *a = clutter_actor_get_parent(si->texture);
		fprintf(stderr,"\n%s:ERROR: below4 clutter-actor-get-parent\n", __func__);

		/* clutter_actor_reparent is deprecated TW 21/04/13 */
        /* clutter_actor_reparent(a, folder); */

		fprintf(stderr, "\n%sERROR:above2-clutter-actor-remove-child: a, folder\n", __func__); /* TEST TW 28/04/13 */
		clutter_actor_remove_child(a, folder);
		fprintf(stderr, "\n%sERROR:below2-clutter-actor-remove-child: a, folder\n", __func__); /* TEST TW 28/04/13 */
	
        clutter_actor_set_position(a, 0, 0);
        clutter_actor_show(a);
    }

    /* align folder icons */
    gui_folder_align_icons(item, FALSE);

    /* distance to move upwards */
    gfloat move_up_by = 0;
    if (is_dock_folder) {
	move_up_by = fh;
    } else {
        if ((ypos + fh) > (stage_area.y2 - DOCK_HEIGHT/2)) {
            move_up_by = (ypos + fh) - (stage_area.y2 - DOCK_HEIGHT/2);
	}
    }
	
	/* clutter_actor_raise_top is deprecated TW 21/04/13 */
    /* clutter_actor_raise_top(folder); */
	fprintf(stderr,"\nERROR: clutter_actor_set_child_above_sibling9: LINE 1521 \n"); /* TEST TW 28/04/13 */
	clutter_actor_set_child_above_sibling(stage, folder,((void *)0) );
	
	/* clutter_actor_raise_top is deprecated TW 21/04/13 */
    /* clutter_actor_raise_top(anilower); */
	fprintf(stderr,"\nERROR: clutter_actor_set_child_above_sibling10: LINE 1526 \n"); /* TEST TW 28/04/13 */
	clutter_actor_set_child_above_sibling(stage, anilower,((void *)0) );

    /* now animate the actors */
	
	/* clutter_actor_animate is deprecated TW 20/04/13 */
    /* clutter_actor_animate(act, CLUTTER_EASE_IN_OUT_QUAD, FOLDER_ANIM_DURATION, "height", fh, NULL); */
	
	/* NOTE TW 11/05/13 THIS NEEDS TO BE FIXED */
	/* clutter_actor_set_easing_mode (act, CLUTTER_EASE_IN_OUT_QUAD);act flagged by compiler TW 26/04/13 */
	/* clutter_actor_set_easing_duration (CLUTTER_ACTOR(act), FOLDER_ANIM_DURATION);act flagged by compiler TW 26/04/13 */
	/* clutter_actor_set_height (act, fh); act flagged by compiler TW 26/04/13 */

	/* clutter_actor_animate is deprecated TW 20/04/13 */
    /* clutter_actor_animate(folder, CLUTTER_EASE_IN_OUT_QUAD, FOLDER_ANIM_DURATION, "y", ypos-move_up_by, NULL); */
	clutter_actor_set_easing_mode (folder, CLUTTER_EASE_IN_OUT_QUAD);
	clutter_actor_set_easing_duration (folder, FOLDER_ANIM_DURATION);
	clutter_actor_set_y(folder, ypos-move_up_by);
	
	/* clutter_actor_animate is deprecated TW 20/04/13 */
    /* clutter_actor_animate(aniupper, CLUTTER_EASE_IN_OUT_QUAD, FOLDER_ANIM_DURATION, "y", (gfloat) -move_up_by, NULL); */
	clutter_actor_set_easing_mode (aniupper, CLUTTER_EASE_IN_OUT_QUAD);
	clutter_actor_set_easing_duration (aniupper, FOLDER_ANIM_DURATION);
	clutter_actor_set_y(aniupper, (gfloat) -move_up_by );

	/* clutter_actor_animate is deprecated TW 20/04/13 */
    /* clutter_actor_animate(anilower, CLUTTER_EASE_IN_OUT_QUAD, FOLDER_ANIM_DURATION, "y", (gfloat) fh-move_up_by, NULL); */
	clutter_actor_set_easing_mode (anilower, CLUTTER_EASE_IN_OUT_QUAD);
	clutter_actor_set_easing_duration (anilower, FOLDER_ANIM_DURATION);
	clutter_actor_set_y(anilower, (gfloat) fh-move_up_by);

    free(shot);

    split_pos = ypos;

    clutter_threads_add_timeout(FOLDER_ANIM_DURATION, (GSourceFunc)folderview_open_finish, item);
}

static gboolean item_button_press_cb(ClutterActor *actor, ClutterButtonEvent *event, gpointer user_data)
{
    if (!user_data) {
        return FALSE;
    }

    if (selected_item) {
        /* do not allow a button_press event without a prior release */
        return FALSE;
    }

    /* discard double clicks */
    if (event->click_count > 2) {
        return FALSE;
    }

    SBItem *item = (SBItem*)user_data;

    if (event->click_count == 2) {
        if (item->is_folder) {
            folderview_open(item);
            return TRUE;
        } else {
            return FALSE;
        }
    }

    if (!item->enabled) {
        return FALSE;
    }

    char *strval = sbitem_get_display_name(item);

   /* g_mutex_lock(selected_mutex); */
    debug_printf("%s: %s mouse pressed\n", __func__, strval);

    if (actor) {
        gfloat diffx = 0.0;
        gfloat diffy = 0.0;

		fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-get-parent\n", __func__);
        ClutterActor *sc = clutter_actor_get_parent(actor);
		fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-get-parent\n", __func__);

        if (item->is_dock_item) {
            clutter_text_set_color(CLUTTER_TEXT(item->label), &item_text_color);
            clutter_actor_set_y(item->label, clutter_actor_get_y(item->texture) + device_info->home_screen_icon_height);
            if (item->label_shadow) {
                clutter_actor_set_y(item->label_shadow, clutter_actor_get_y(item->texture) + device_info->home_screen_icon_height + 1.0);
            }
            diffx = dock_area.x1;
            diffy = dock_area.y1;
        } else {
            diffx = sb_area.x1 - PAGE_X_OFFSET(current_page);
            diffy = sb_area.y1;
        }

		/* clutter_actor_reparent' is deprecated TW 21/04/13 */
        /* clutter_actor_reparent(sc, stage); */
		/* NOTE TW 10/05/13 sc could be sb_area or dock_area or stage */
		/*Clutter-CRITICAL **: clutter_actor_remove_child: assertion `self != child' failed */
 		
		/* Runtime error TW 10/05/13 */
		
		clutter_actor_remove_child(sc, stage);
		
		fprintf(stderr, "\n%s ERROR:above1 - clutter-actor-remove-child: sc, stage\n",__func__); /* TEST TW 12/05/13 */
		clutter_actor_remove_child(sc, actor); /* TEST TW 12/05/13 */
		fprintf(stderr, "\n%s ERROR:below1 - clutter-actor-remove-child: sc, stage\n",__func__); /* TEST TW 12/05/13 */
        
		clutter_actor_set_position(sc, clutter_actor_get_x(sc) + diffx, clutter_actor_get_y(sc) + diffy);

		/* clutter_actor_raise_top is deprecated TW 21/04/13 */
        /* clutter_actor_raise_top(sc); */
				
		/* Clutter-CRITICAL **: clutter_actor_insert_child_above: assertion `self != child' failed */
		/* Runtime error TW 10/05/13 */
		
		fprintf(stderr,"\n%sERROR:above1: clutter-actor-insert-child-above:\n", __func__); /* TEST TW 28/04/13 */
		clutter_actor_insert_child_above(stage, sc,((void *)0));
        fprintf(stderr,"\n%sERROR:below1: clutter-actor-insert-child-above:\n", __func__); /* TEST TW 28/04/13 */

		/* clutter_actor_set_scale_full' is deprecated TW 21/04/13		
		clutter_actor_set_scale_full(sc, 1.2, 1.2,
                                     clutter_actor_get_x(actor) +
                                     clutter_actor_get_width(actor) / 2,
                                     clutter_actor_get_y(actor) + clutter_actor_get_height(actor) / 2); */
		clutter_actor_set_scale(sc, 1.2, 1.2);


        clutter_actor_set_opacity(sc, 160);
        selected_item = item;
        start_x = event->x;
        start_y = event->y;
    }
    /* g_mutex_unlock(selected_mutex); */

    /* add pages and page indicators as needed */
    GList *page = NULL;
    gui_page_indicator_group_add(page, g_list_length(sbpages));
    sbpages = g_list_append(sbpages, page);

    return TRUE;
}

static gboolean item_button_release_cb(ClutterActor *actor, ClutterButtonEvent *event, gpointer user_data)
{
    if (!user_data) {
        return FALSE;
    }

    /* discard double clicks */
    if (event->click_count > 1) {
        return FALSE;
    }

    SBItem *item = (SBItem*)user_data;
    if (!item->enabled) {
        return FALSE;
    }
    item->enabled = FALSE;

    char *strval = sbitem_get_display_name(item);

    /* remove empty pages and page indicators as needed */
    gui_pages_remove_empty();
    int count = g_list_length(sbpages);
    if (current_page >= count) {
        gui_set_current_page(count-1, FALSE);
    }

    /* g_mutex_lock(selected_mutex); */
    debug_printf("%s: %s mouse released\n", __func__, strval);

    if (actor) {
		
		fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-get-parent\n", __func__);
        ClutterActor *sc = clutter_actor_get_parent(actor);
		fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-get-parent\n", __func__);
		
		/* clutter_actor_set_scale_full' is deprecated TW 21/04/13        
		clutter_actor_set_scale_full(sc, 1.0, 1.0,
                                     clutter_actor_get_x(actor) +
                                     clutter_actor_get_width(actor) / 2,
                                     clutter_actor_get_y(actor) + clutter_actor_get_height(actor) / 2); */
		clutter_actor_set_scale(sc, 1.0, 1.0);

        clutter_actor_set_opacity(sc, 255);
        if (item->is_dock_item) {
            clutter_text_set_color(CLUTTER_TEXT(item->label), &dock_item_text_color);
            clutter_actor_set_y(item->label, clutter_actor_get_y(item->texture) + device_info->home_screen_icon_height);
            if (item->label_shadow) {
                clutter_actor_set_y(item->label_shadow, clutter_actor_get_y(item->texture) + device_info->home_screen_icon_height + 1.0);
            }

			/* clutter_actor_reparent is deprecated TW 21/04/13 */
            /* clutter_actor_reparent(sc, the_dock); */

			fprintf(stderr, "\n%s ERROR:above1 - clutter-actor-remove-child: the-dock, sc\n",__func__);
			clutter_actor_remove_child(the_dock, sc);
			fprintf(stderr, "\n%s ERROR:below1 - clutter-actor-remove-child: the-dock, sc\n",__func__);

            clutter_actor_set_position(sc, clutter_actor_get_x(sc) - dock_area.x1, clutter_actor_get_y(sc) - dock_area.y1);
        } else {

			/* clutter_actor_reparent is deprecated TW 21/04/13 */
            /* clutter_actor_reparent(sc, the_sb); */

			fprintf(stderr, "\n%s ERROR:above2 -clutter-actor-remove-child: the_sb, sc \n",__func__); /* TEST TW 28/04/13 */
			clutter_actor_remove_child(the_sb, sc);
			fprintf(stderr, "\n%s ERROR:below2 -clutter-actor-remove-child: the_sb, sc \n",__func__); /* TEST TW 28/04/13 */

            clutter_actor_set_position(sc,
                                       clutter_actor_get_x(sc) +
                                       PAGE_X_OFFSET(current_page) - sb_area.x1, clutter_actor_get_y(sc) - sb_area.y1);
        }
    }

    selected_item = NULL;
    gui_dock_align_icons(TRUE);
    gui_page_align_icons(current_page, TRUE);
    start_x = 0.0;
    start_y = 0.0;

    clutter_threads_add_timeout(ICON_MOVEMENT_DURATION, (GSourceFunc)item_enable, (gpointer)item);

    /* g_mutex_unlock(selected_mutex); */

    return TRUE;
}

static gboolean stage_key_press_cb(ClutterActor *actor, ClutterEvent *event, gpointer user_data)
{
    if (!user_data || (event->type != CLUTTER_KEY_PRESS)) {
        return FALSE;
    }

    guint symbol = clutter_event_get_key_symbol(event);
    switch(symbol) {
        case CLUTTER_Right:
        gui_show_next_page();
        break;
        case CLUTTER_Left:
        gui_show_previous_page();
        break;
        default:
        return FALSE;
    }
    return TRUE;
}

static gboolean subitem_button_press_cb(ClutterActor *actor, ClutterButtonEvent *event, gpointer user_data)
{
    if (!user_data) {
        return FALSE;
    }

    if (selected_item) {
        /* do not allow a button_press event without a prior release */
        return FALSE;
    }

    /* discard double clicks */
    if (event->click_count > 1) {
        return FALSE;
    }

    SBItem *item = (SBItem*)user_data;
    if (!item->enabled) {
        return FALSE;
    }

    char *strval = sbitem_get_display_name(item);

    /* g_mutex_lock(selected_mutex); */
    debug_printf("%s: %s mouse pressed\n", __func__, strval);

    if (actor) {
        gfloat diffx = 0.0;
        gfloat diffy = 0.0;

		fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-get-parent\n", __func__);
        ClutterActor *sc = clutter_actor_get_parent(actor);
		fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-get-parent\n", __func__);

        diffy = split_pos + clutter_actor_get_y(aniupper);

        /* clutter_actor_reparent is deprecated TW 21/04/13 */
		/* clutter_actor_reparent(sc, stage); */
		
		fprintf(stderr, "\n%sERROR:above1-clutter-actor-remove-child: stage, sc\n",__func__); /* TEST TW 28/04/13 */	
		clutter_actor_remove_child(stage, sc);
		fprintf(stderr, "\n%sERROR:below1-clutter-actor-remove-child: stage, sc\n",__func__); /* TEST TW 28/04/13 */	

        clutter_actor_set_position(sc, clutter_actor_get_x(sc) + diffx, clutter_actor_get_y(sc) + diffy);

		/* clutter_actor_raise_top is deprecated TW 21/04/13 */		
		/* clutter_actor_raise_top(sc); */
		fprintf(stderr,"\nERROR: clutter_actor_set_child_above_sibling12: LINE 1796 \n"); /* TEST TW 28/04/13 */
		clutter_actor_insert_child_above(stage, sc, ((void *)0));


		/* clutter_actor_set_scale_full' is deprecated TW 21/04/13 
		clutter_actor_set_scale_full(sc, 1.2, 1.2,
                                     clutter_actor_get_x(actor) +
                                     clutter_actor_get_width(actor) / 2,
                                     clutter_actor_get_y(actor) + clutter_actor_get_height(actor) / 2); */
		clutter_actor_set_scale(sc, 1.2, 1.2);

        clutter_actor_set_opacity(sc, 160);
        selected_item = item;
        start_x = event->x;
        start_y = event->y;
    }
    /* g_mutex_unlock(selected_mutex); */

    return TRUE;
}

static gboolean subitem_button_release_cb(ClutterActor *actor, ClutterButtonEvent *event, gpointer user_data)
{
    if (!user_data) {
        return FALSE;
    }

    /* discard double clicks */
    if (event->click_count > 1) {
        return FALSE;
    }

    SBItem *item = (SBItem*)user_data;
    if (!item->enabled) {
        return FALSE;
    }
    item->enabled = FALSE;

    char *strval = sbitem_get_display_name(item);

    /* g_mutex_lock(selected_mutex);*/
    debug_printf("%s: %s mouse released\n", __func__, strval);

    if (actor) {

		fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-get-parent\n", __func__);
        ClutterActor *sc = clutter_actor_get_parent(actor);
		fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-get-parent\n", __func__);

		/* clutter_actor_set_scale_full' is deprecated TW 21/04/13 
        clutter_actor_set_scale_full(sc, 1.0, 1.0,
                                     clutter_actor_get_x(actor) +
                                     clutter_actor_get_width(actor) / 2,
                                     clutter_actor_get_y(actor) + clutter_actor_get_height(actor) / 2); */
		clutter_actor_set_scale(sc, 1.0, 1.0);

        clutter_actor_set_opacity(sc, 255);

        /* clutter_actor_reparent is deprecated TW 21/04/13 */
		/* clutter_actor_reparent(sc, folder); */

		fprintf(stderr, "\n%sERROR:above1-clutter-actor-remove-child: sc, folder \n", __func__); /* TEST TW 28/04/13 */
		clutter_actor_remove_child(sc, folder);
		fprintf(stderr, "\n%sERROR:below1-clutter-actor-remove-child: sc, folder \n", __func__); /* TEST TW 28/04/13 */

        clutter_actor_set_position(sc,
                                       clutter_actor_get_x(sc), clutter_actor_get_y(sc) - (split_pos + clutter_actor_get_y(aniupper)));
    }

    selected_item = NULL;
    gui_folder_align_icons(selected_folder, TRUE);
    start_x = 0.0;
    start_y = 0.0;

    clutter_threads_add_timeout(ICON_MOVEMENT_DURATION, (GSourceFunc)item_enable, (gpointer)item);

    /* g_mutex_unlock(selected_mutex); */

    if (selected_folder)
        gui_folder_redraw_subitems(selected_folder);

    return TRUE;
}

static void gui_folder_draw_subitems(SBItem *item)
{
	/* fprintf(stderr,"\n%s above1-clutter-actor-get-parent--item->texture\n", __func__); */
    ClutterActor *grp = clutter_actor_get_parent(item->texture);
    /* fprintf(stderr,"\n%s below1-clutter-actor-get-parent--item->texture\n", __func__); */

	/* clutter_group_new is deprecated TW 20/04/13 */
	/* ClutterActor *minigrp = clutter_group_new(); */
	ClutterActor *minigrp = clutter_actor_new();

    clutter_actor_set_name(minigrp, "mini");

	/* clutter_container_add_actor is deprecated TW 20/04/13 */
    /* clutter_container_add_actor(CLUTTER_CONTAINER(grp), minigrp); */
	
	/* fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-add-child\n", __func__); */
	clutter_actor_add_child(CLUTTER_ACTOR(grp), minigrp);
	/* fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-add-child\n", __func__); */

    guint i;
    for (i = 0; i < g_list_length(item->subitems); i++) {
        SBItem *subitem = (SBItem*)g_list_nth_data(item->subitems, i);
        if (subitem && subitem->texture && !subitem->drawn && subitem->node) {
            subitem->is_dock_item = FALSE;

			/* clutter_group_new is deprecated TW 20/04/13 */
            /* ClutterActor *sgrp = clutter_group_new(); */
			ClutterActor *sgrp = clutter_actor_new();

            ClutterActor *actor;
            // icon shadow
            actor = subitem->texture_shadow;
            if (actor) {

				/* clutter_container_add_actor is deprecated TW 20/04/13 */
                /* clutter_container_add_actor(CLUTTER_CONTAINER(sgrp), actor); */

				/* fprintf(stderr,"\n%s:ERROR: above2 clutter-actor-add-child\n", __func__); */
				clutter_actor_add_child(CLUTTER_ACTOR(sgrp), actor);
				/* fprintf(stderr,"\n%s:ERROR: below2 clutter-actor-add-child\n", __func__); */

                clutter_actor_set_position(actor, -12.0, -12.0);
                clutter_actor_show(actor);
            }
            // label shadow
            actor = subitem->label_shadow;
            if (actor) {
				
				/* clutter_container_add_actor is deprecated TW 20/04/13 */
                /* clutter_container_add_actor(CLUTTER_CONTAINER(sgrp), actor); */
				
				/* fprintf(stderr,"\n%s:ERROR: above3 clutter-actor-add-child\n", __func__); */
				clutter_actor_add_child(CLUTTER_ACTOR(sgrp), actor);
				/* fprintf(stderr,"\n%s:ERROR: below3 clutter-actor-add-child\n", __func__); */

                clutter_actor_set_position(actor, (device_info->home_screen_icon_width - clutter_actor_get_width(actor)) / 2 + 1.0, device_info->home_screen_icon_height + 1.0);
                clutter_actor_show(actor);
            }

            actor = subitem->texture;

			/* clutter_container_add_actor is deprecated TW 20/04/13 */
            /* clutter_container_add_actor(CLUTTER_CONTAINER(sgrp), actor); */
			
			/*fprintf(stderr,"%s: Clutter=actor-add-child LINE 1941\n", __func__);  TEST TW 10/05/13 */
			
			/* Clutter-CRITICAL **: clutter_actor_add_child: assertion `child->priv->parent == NULL' failed */
			/* Runtime error TW 10/05/13 */
			
			/*fprintf(stderr,"\n%s:ERROR: above4 clutter-actor-add-child\n", __func__); */
			clutter_actor_add_child(CLUTTER_ACTOR(sgrp), actor);
			/* fprintf(stderr,"\n%s:ERROR: below4 clutter-actor-add-child\n", __func__); */

			/* fprintf(stderr,"%s: Clutter=actor-add-child LINE 1943\n", __func__);  TEST TW 10/05/13 */

            clutter_actor_set_position(actor, 0.0, 0.0);
            clutter_actor_set_reactive(actor, TRUE);
            g_signal_connect(actor, "button-press-event", G_CALLBACK(subitem_button_press_cb), subitem);
            g_signal_connect(actor, "button-release-event", G_CALLBACK(subitem_button_release_cb), subitem);
            clutter_actor_show(actor);

            /* setup label */
            actor = subitem->label;
            clutter_actor_set_position(actor, (device_info->home_screen_icon_width - clutter_actor_get_width(actor)) / 2, device_info->home_screen_icon_height);
            clutter_text_set_color(CLUTTER_TEXT(actor), &item_text_color);
            clutter_actor_show(actor);

			/* clutter_container_add_actor is deprecated TW 20/04/13 */
            /* clutter_container_add_actor(CLUTTER_CONTAINER(sgrp), actor); */
            /* clutter_container_add_actor(CLUTTER_CONTAINER(grp), sgrp); */
			
			/* fprintf(stderr,"\n%s:ERROR: above5 clutter-actor-add-child\n", __func__); */
			clutter_actor_add_child(CLUTTER_ACTOR(sgrp), actor);
			/* fprintf(stderr,"\n%s:ERROR: below5 clutter-actor-add-child\n", __func__); */

			/* fprintf(stderr,"\n%s:ERROR: above6 clutter-actor-add-child\n", __func__); */
			clutter_actor_add_child(CLUTTER_ACTOR(grp), sgrp);
			/* fprintf(stderr,"\n%s:ERROR: below6 clutter-actor-add-child\n", __func__); */
			
            clutter_actor_hide(sgrp);

            ClutterActor *suba = clutter_clone_new(subitem->texture);

			if(suba == NULL) {
				fprintf(stderr, "\nERROR:suba = clutter_clone_new = NULL: LINE 1954 \n"); /* TEST TW 28/04/13 */
			}

			/* clutter_actor_unparent' is deprecated TW 20/04/13 */
            /* clutter_actor_unparent(suba); */

			/* fprintf(stderr,"\n%s above2-clutter-actor-get-parent-suba\n", __func__); */
			/* ClutterActor *parent = clutter_actor_get_parent(suba);  I do not think we need this TW 06/05/13 */
			/* fprintf(stderr,"\n%s below2-clutter-actor-get-parent-suba\n", __func__); */

			
			/* fprintf(stderr,"\n%s above2-clutter-actor-remove-child: parent, suba\n", __func__); */
			/* clutter_actor_remove_child(parent, suba);  I do not think we need this TW 06/05/13 */
			/* fprintf(stderr,"\n%s below2-clutter-actor-remove-child: parent, suba\n", __func__); */

			/* clutter_container_add_actor is deprecated TW 20/04/13 */
            /* clutter_container_add_actor(CLUTTER_CONTAINER(minigrp), suba); */
			
			/* fprintf(stderr,"\n%s:ERROR: above7 clutter-actor-add-child\n", __func__); */
			clutter_actor_add_child(CLUTTER_ACTOR(minigrp), suba);
			/* fprintf(stderr,"\n%s:ERROR: below7 clutter-actor-add-child\n", __func__); */

            clutter_actor_set_scale(suba, 0.22, 0.22);
            clutter_actor_set_position(suba, 8.0 + (i%3)*ICON_SPACING, 8.0 + ((double)(int)((int)i/3))*ICON_SPACING);
            if (i < 9)
                clutter_actor_show(suba);
            else
                clutter_actor_hide(suba);
            subitem->drawn = TRUE;
        }
    }
}

static void gui_show_icons()
{
    guint i;
    guint j;
    gfloat ypos;
    gfloat xpos;

    if (dockitems) {
        xpos = 0.0;
        ypos = 0.0;
        debug_printf("%s: showing dock icons\n", __func__);
        for (i = 0; i < g_list_length(dockitems); i++) {
            SBItem *item = (SBItem*)g_list_nth_data(dockitems, i);
            if (item && item->texture && !item->drawn && item->node) {
                item->is_dock_item = TRUE;

				/* clutter_group_new is deprecated TW 20/04/13 */
                /* ClutterActor *grp = clutter_group_new(); */
				ClutterActor *grp = clutter_actor_new();

                ClutterActor *actor;
                // icon shadow
                actor = item->texture_shadow;
                if (actor) {

					/* clutter_container_add_actor is deprecated TW 20/04/13 */
                    /* clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor); */
					/* fprintf(stderr,"\n clutter-actor-add-child: LINE 2006\n"); TEST TW 03/05/13 */

					/* fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-add-child\n", __func__); */
					clutter_actor_add_child(CLUTTER_ACTOR(grp), actor);
					/* fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-add-child\n", __func__); */

					clutter_actor_set_position(actor, xpos-12, ypos-12);
                }
                // label shadow
                actor = item->label_shadow;
                if (actor) {

					/* clutter_container_add_actor is deprecated TW 20/04/13 */
                    /* clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor); */
					/* fprintf(stderr,"\n clutter-actor-add-child: LINE 2017\n"); TEST TW 03/05/13 */

					/* fprintf(stderr,"\n%s:ERROR: above2 clutter-actor-add-child\n", __func__); */
					clutter_actor_add_child(CLUTTER_ACTOR(grp), actor);
					/* fprintf(stderr,"\n%s:ERROR: below2 clutter-actor-add-child\n", __func__); */
				
                    clutter_actor_set_position(actor, xpos + (device_info->home_screen_icon_width - clutter_actor_get_width(actor)) / 2 + 1.0, ypos + device_info->home_screen_icon_height + 1.0);
                }
                actor = item->texture;
				/* fprintf(stderr,"\ngui-show-icons: actor->texture = %s\n",item->texture);  TEST TW 06/05/13 */
				if((item->texture) == NULL){
				fprintf(stderr, "ERROR item-texture = NULL"); /* TEST TW 06/05/13 */
				}
				/* clutter_container_add_actor is deprecated TW 20/04/13 */
                /* clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor); */
				/* fprintf(stderr,"\ngui-show-icons: clutter-actor-add-child: LINE 2040\n");  TEST TW 03/05/13 */
				
		/* Clutter-CRITICAL **: clutter_actor_add_child: assertion `child->priv->parent == NULL' failed		*/		
		/* Runtime warning 09/05/13 */

				/* fprintf(stderr,"\n%s:ERROR: above3 clutter-actor-add-child\n", __func__); */		
				clutter_actor_add_child(CLUTTER_ACTOR(grp), actor); 
				/* fprintf(stderr,"\n%s:ERROR: below3 clutter-actor-add-child\n", __func__); */

                clutter_actor_set_position(actor, xpos, ypos);
                clutter_actor_set_reactive(actor, TRUE);
                g_signal_connect(actor, "button-press-event", G_CALLBACK(item_button_press_cb), item);
                g_signal_connect(actor, "button-release-event", G_CALLBACK(item_button_release_cb), item);
                clutter_actor_show(actor);
                actor = item->label;
                clutter_actor_set_position(actor, xpos + (device_info->home_screen_icon_width - clutter_actor_get_width(actor)) / 2, ypos + device_info->home_screen_icon_height);
                clutter_text_set_color(CLUTTER_TEXT(actor), &dock_item_text_color);


				/* clutter_container_add_actor is deprecated TW 20/04/13 */
                /* clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor); */
                /* clutter_container_add_actor(CLUTTER_CONTAINER(the_dock), grp); */
				
				/* fprintf(stderr,"\n%s:ERROR: above4 clutter-actor-add-child\n", __func__); */
				clutter_actor_add_child(CLUTTER_ACTOR(grp), actor);
				/* fprintf(stderr,"\n%s:ERROR: below4 clutter-actor-add-child\n", __func__); */

				/* fprintf(stderr,"\n%s:ERROR: above5 clutter-actor-add-child\n", __func__); */
				clutter_actor_add_child(CLUTTER_ACTOR(the_dock), grp);
				/* fprintf(stderr,"\n%s:ERROR: below5 clutter-actor-add-child\n", __func__); */

				item->drawn = TRUE;
            }
            /* process subitems */
            if (item->texture && item->is_folder && item->subitems) {
                gui_folder_draw_subitems(item);
            }
        }
        gui_dock_align_icons(FALSE);
    }
    clutter_stage_ensure_redraw(CLUTTER_STAGE(stage));
    if (sbpages) {
        debug_printf("%s: processing %d pages\n", __func__, g_list_length(sbpages));
        for (j = 0; j < g_list_length(sbpages); j++) {
            GList *cpage = g_list_nth_data(sbpages, j);
            ypos = 0.0;
            xpos = 0.0;
            debug_printf("%s: showing page icons for page %d\n", __func__, j);
            for (i = 0; i < g_list_length(cpage); i++) {
                SBItem *item = (SBItem*)g_list_nth_data(cpage, i);
                if (item && item->texture && !item->drawn && item->node) {
                    item->is_dock_item = FALSE;

					/* clutter_group_new is deprecated TW 20/04/13 */
                    /* ClutterActor *grp = clutter_group_new(); */
					ClutterActor *grp = clutter_actor_new();

                    ClutterActor *actor;

                    // icon shadow
                    actor = item->texture_shadow;
                    if (actor) {

						/* clutter_container_add_actor is deprecated TW 20/04/13 */
                        /* clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor); */
						
						
						/*  Clutter-CRITICAL **: clutter_actor_add_child: assertion `child->priv->parent == NULL' failed */
						/* Runtime error TW 10/08/13 */

						/* fprintf(stderr,"\n%s:ERROR: above6 clutter-actor-add-child\n", __func__); */
						clutter_actor_add_child(CLUTTER_ACTOR(grp), actor);
						/* fprintf(stderr,"\n%s:ERROR: below6 clutter-actor-add-child\n", __func__); */

                        clutter_actor_set_position(actor, xpos-12.0, ypos-12.0);
                    }

                    // label shadow
                    actor = item->label_shadow;
                    if (actor) {

						/* clutter_container_add_actor is deprecated TW 20/04/13 */
                        /* clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor); */
						
						/* fprintf(stderr,"\n%s:ERROR: above7 clutter-actor-add-child\n", __func__); */
						clutter_actor_add_child(CLUTTER_ACTOR(grp), actor);
						/* fprintf(stderr,"\n%s:ERROR: below7 clutter-actor-add-child\n", __func__); */

                        clutter_actor_set_position(actor, xpos + (device_info->home_screen_icon_width - clutter_actor_get_width(actor)) / 2 + 1.0, ypos + device_info->home_screen_icon_height + 1.0);
                    }
                    actor = item->texture;

					/* clutter_container_add_actor is deprecated TW 20/04/13 */
                    /* clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor); */
					
					
					
					/*  Clutter-CRITICAL **: clutter_actor_add_child: assertion `child->priv->parent == NULL' failed */
					/* Runlime error TW 10/05/13 */

					/* fprintf(stderr,"\n%s:ERROR: above8 clutter-actor-add-child\n", __func__); */
					clutter_actor_add_child(CLUTTER_ACTOR(grp), actor); 
					/* fprintf(stderr,"\n%s:ERROR: below8 clutter-actor-add-child\n", __func__); */
					
					clutter_actor_set_position(actor, xpos, ypos);
                    clutter_actor_set_reactive(actor, TRUE);
                    g_signal_connect(actor, "button-press-event", G_CALLBACK(item_button_press_cb), item);
                    g_signal_connect(actor, "button-release-event", G_CALLBACK(item_button_release_cb), item);
                    clutter_actor_show(actor);
                    actor = item->label;
                    clutter_text_set_color(CLUTTER_TEXT(actor), &item_text_color);
                    clutter_actor_set_position(actor, xpos + (device_info->home_screen_icon_width - clutter_actor_get_width(actor)) / 2, ypos + device_info->home_screen_icon_height);

					/* clutter_container_add_actor is deprecated TW 20/04/13 */
                    /* clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor); */
                    /* clutter_container_add_actor(CLUTTER_CONTAINER(the_sb), grp); */
					
					/* fprintf(stderr,"\n%s:ERROR: above9 clutter-actor-add-child\n", __func__); */
					clutter_actor_add_child(CLUTTER_ACTOR(grp), actor);
					/* fprintf(stderr,"\n%s:ERROR: below9 clutter-actor-add-child\n", __func__); */

					/* fprintf(stderr,"\n%s:ERROR: above10 clutter-actor-add-child\n", __func__); */
					clutter_actor_add_child(CLUTTER_ACTOR(the_sb), grp);
					/* fprintf(stderr,"\n%s:ERROR: below10 clutter-actor-add-child\n", __func__); */

					
		    item->drawn = TRUE;
                }
                /* process subitems */
                if (item->texture && item->is_folder && item->subitems) {
                    gui_folder_draw_subitems(item);
                }
            }
            gui_page_align_icons(j, FALSE);
        }
    }
    clutter_stage_ensure_redraw(CLUTTER_STAGE(stage));
}
			/* altered the arguments from ClutterTexture *texture, to ClutterActor *actor TEST TW 07/05/13 */
static void sbitem_texture_load_finished(ClutterActor *actor, gpointer error, gpointer data) 
{ 
    SBItem *item = (SBItem *)data; 
	/* fprintf(stderr,"\n executing: sbitem_texture_load_finished-start\n"); TEST TW 07/05/13 */
	   if (item->texture_shadow) {
		fprintf(stderr,"\nsbitem_texture_load_finished: in item->texture_shadow LINE 2189\n"); /* TEST TW 08/05/13 */ 
       clutter_actor_show(item->texture_shadow); 
   } 
	/* fprintf(stderr,"\nsbitem_texture_load_finished: in item->label LINE 2157\n"); */
	 
	/* Clutter-CRITICAL **: clutter_actor_show: assertion `CLUTTER_IS_ACTOR (self)' failed */
	/* Runtime error TW 10/05/13 */
    /* clutter_actor_show(item->label); */
	   if (item->label_shadow) {
		fprintf(stderr,"\nsbitem_texture_load_finished: in item->texture_label_shadow LINE 2198\n"); /* TEST TW 08/05/13 */ 
       clutter_actor_show(item->label_shadow); 
	  } 
} 

static gboolean sbitem_texture_new(gpointer data)
{
	ClutterContent *image;
	GdkPixbuf *pixbuf;
	
    SBItem *item = (SBItem *)data;
    char *icon_filename;
    if (item->is_folder) {
        icon_filename = g_strdup(SBMGR_DATA "/folder.png");
    } else {
        icon_filename = sbitem_get_icon_filename(item);
    }
    GError *err = NULL;

    /* create and load texture */

	/* clutter_texture_new' is deprecated TW 21/04/13 */
    /* ClutterActor *actor = clutter_texture_new(); */
	
	 ClutterActor *actor = clutter_actor_new(); 
	
	/* clutter_texture_set_load_async' is deprecated TW 24/04/13 */    
	/* clutter_texture_set_load_async(CLUTTER_TEXTURE(actor), TRUE); */
	pixbuf = gdk_pixbuf_new_from_file(icon_filename, &err); 
	  
	image = clutter_image_new (); 
	  
	clutter_image_set_data (CLUTTER_IMAGE (image),
                          gdk_pixbuf_get_pixels (pixbuf),
                          gdk_pixbuf_get_has_alpha (pixbuf)
                            ? COGL_PIXEL_FORMAT_RGBA_8888
                            : COGL_PIXEL_FORMAT_RGB_888,
                          gdk_pixbuf_get_width (pixbuf),
                          gdk_pixbuf_get_height (pixbuf),
                          gdk_pixbuf_get_rowstride (pixbuf),
                          NULL); 
	 g_object_unref (pixbuf); 
	
	 clutter_actor_set_content (actor, image);
	 
	/* TW TEST 05/05/13 this does load all icon's on to wallpaper on top of each other */
	/* clutter_actor_add_child(stage, actor); */
	/* NOTE ACTORS are not added to stage THIS IS PROBABLY THE PROBLEM TW 10/05/13 FIXED 18/05/13 */

	
	
	/* sbitem_texture_load_finished(CLUTTER_ACTOR(actor), (gpointer)err, (gpointer)item); REMOVED FOR TEST TW 19/05/13 */	
     g_signal_connect(CLUTTER_ACTOR(actor), "actor-added", G_CALLBACK(sbitem_texture_load_finished), (gpointer)item);
	/* fprintf(stderr,"\nERROR: above clutter_set_size5: LINE 2194 \n"); TEST TW 03/05/13 */
    clutter_actor_set_size(actor, device_info->home_screen_icon_width, device_info->home_screen_icon_height);
    clutter_actor_set_scale(actor, 1.0, 1.0);
	
    /* create item */
    item->texture = actor;
	/* actor = item->texture; Tried for test does not work TW 10/05/13 */
	/* item->texture = clutter_actor_get_content(CLUTTER_ACTOR (actor));  TEST TW 10/05/13 */
	if(item->texture == NULL){
	fprintf(stderr, "\nERROR:item->texture = NULL\n");
	}

    if (wallpaper) {
        actor = clutter_clone_new(icon_shadow);
        clutter_actor_hide(actor);
		/* fprintf(stderr,"\nERROR: above clutter_set_size6: LINE 2204 \n"); TEST TW 03/05/13 */
        clutter_actor_set_size(actor, device_info->home_screen_icon_width+24.0, device_info->home_screen_icon_height+24.0);
        item->texture_shadow = actor;
    }

    char *txtval = sbitem_get_display_name(item);
    if (txtval) {
        item->label = clutter_text_new_with_text(ITEM_FONT, txtval);
        clutter_actor_hide(item->label);
        if (wallpaper) {
            item->label_shadow = clutter_text_new_full(ITEM_FONT, txtval, &label_shadow_color);
            clutter_actor_hide(item->label_shadow);
        }
    }
    if (err) {
        fprintf(stderr, "ERROR: %s\n", err->message);
        g_error_free(err);
    }

	/* clutter_texture_set_from_file is deprecated TW 24/04/13 */
    /* clutter_texture_set_from_file(CLUTTER_TEXTURE(item->texture), icon_filename, &err); */
	if(item->texture){
		pixbuf = gdk_pixbuf_new_from_file(icon_filename, &err);
	  
		image = clutter_image_new ();
	  
			clutter_image_set_data (CLUTTER_IMAGE (image),
                          gdk_pixbuf_get_pixels (pixbuf),
                          gdk_pixbuf_get_has_alpha (pixbuf)
                            ? COGL_PIXEL_FORMAT_RGBA_8888
                            : COGL_PIXEL_FORMAT_RGB_888,
                          gdk_pixbuf_get_width (pixbuf),
                          gdk_pixbuf_get_height (pixbuf),
                          gdk_pixbuf_get_rowstride (pixbuf),
                          NULL);
		g_object_unref (pixbuf);
		
		clutter_actor_set_content (actor, image);	
		g_object_unref(image);
	}

    /* FIXME: Optimize! Do not traverse whole iconlist, just this icon */
    gui_show_icons();

   /* g_mutex_lock(icon_loader_mutex); */
    icons_loaded++;
   /* g_mutex_unlock(icon_loader_mutex); */

    return FALSE;
}

static gpointer sbitem_thread_load_texture(gpointer data)
{
    SBItem *item = (SBItem *)data;
    char *icon_filename = sbitem_get_icon_filename(item);
    char *display_identifier = sbitem_get_display_identifier(item);
    GError *err = NULL;

    debug_printf("%s: loading icon texture for '%s'\n", __func__, display_identifier);

    if (device_sbs_save_icon(sbc, display_identifier, icon_filename, &err)) {
        /* load texture in the clutter main loop */
        clutter_threads_add_idle((GSourceFunc)sbitem_texture_new, item);
    } else {
        fprintf(stderr, "ERROR: %s\n", err->message);
        g_error_free(err);
    }
    g_free(icon_filename);

    return NULL;
}

static guint gui_load_icon_row(plist_t items, GList **row)
{
    int i;
    int count;
    int icon_count = 0;
    SBItem *item = NULL;

    count = plist_array_get_size(items);
    for (i = 0; i < count; i++) {
        plist_t icon_info = plist_array_get_item(items, i);
        plist_t subitems = plist_dict_get_item(icon_info, "iconLists");
        if (subitems) {
            /* this is a folder, so we need to load the subitems */
            GList *folderitems = NULL;
            if (plist_get_node_type(subitems) == PLIST_ARRAY) {
                subitems = plist_array_get_item(subitems, 0);
                if (plist_get_node_type(subitems) == PLIST_ARRAY) {
                    icon_count += gui_load_icon_row(subitems, &folderitems);
                }
            }
            if (folderitems) {
                item = sbitem_new_with_subitems(icon_info, folderitems);
                if (item != NULL) {
                    clutter_threads_add_idle((GSourceFunc)sbitem_texture_new, item);
                    *row = g_list_append(*row, item);
                    icon_count++;
                }
            }
        } else {
            item = sbitem_new(icon_info);
            if (item != NULL) {
                /* load texture of icon in a new thread */
				
				/* g_thread_create' is deprecated TW 25/04/13 */
                /* g_thread_create(sbitem_thread_load_texture, item, FALSE, NULL); */
				const gchar *name = "sbitthread";
				g_thread_new(name, sbitem_thread_load_texture, item);

                *row = g_list_append(*row, item);
                icon_count++;
            }
        }
    }

    return icon_count;
}

static void gui_set_iconstate(plist_t iconstate, const char *format_version)
{
    int total;

    /* get total number of pages */
    if (plist_get_node_type(iconstate) != PLIST_ARRAY) {
        fprintf(stderr, "ERROR: Invalid icon state format!\n");
        return;
    }

    total = plist_array_get_size(iconstate);
    if (total < 1) {
        fprintf(stderr, "ERROR: No icons returned in icon state\n");
        return;
    } else {
        plist_t dock = plist_array_get_item(iconstate, 0);
        if ((plist_get_node_type(dock) != PLIST_ARRAY)
                || (plist_array_get_size(dock) < 1)) {
            fprintf(stderr, "ERROR: error getting outer dock icon array!\n");
            return;
        }

        if (!format_version || (strcmp(format_version, "2") != 0)) {
            dock = plist_array_get_item(dock, 0);
            if (plist_get_node_type(dock) != PLIST_ARRAY) {
                fprintf(stderr, "ERROR: error getting inner dock icon array!\n");
                return;
            }
        }

        /* load dock icons */
        debug_printf("%s: processing dock\n", __func__);
        total_icons += gui_load_icon_row(dock, &dockitems);
        num_dock_items = g_list_length(dockitems);
        if (total > 1) {
            /* get all page icons */
            int p, r, rows;
            for (p = 1; p < total; p++) {
                plist_t npage = plist_array_get_item(iconstate, p);
                GList *page = NULL;
                if ((plist_get_node_type(npage) != PLIST_ARRAY)
                        || (plist_array_get_size(npage) < 1)) {
                        fprintf(stderr, "ERROR: error getting outer page icon array!\n");
                        return;
                }

                if (!format_version || (strcmp(format_version, "2") != 0)) {
                    /* rows */
                    rows = plist_array_get_size(npage);
                    for (r = 0; r < rows; r++) {
                        debug_printf("%s: processing page %d, row %d\n", __func__, p, r);

                        plist_t nrow = plist_array_get_item(npage, r);
                        if (plist_get_node_type(nrow) != PLIST_ARRAY) {
                            fprintf(stderr, "ERROR: error getting page row icon array!\n");
                            return;
                        }
                        total_icons += gui_load_icon_row(nrow, &page);
                    }
                } else {
                    total_icons += gui_load_icon_row(npage, &page);
                }

                if (page) {
                        sbpages = g_list_append(sbpages, page);
                        gui_page_indicator_group_add(page, p - 1);
                }
            }
        }
    }
}

static void gui_disable_controls() 
{
    gui_fade_start(); 
    gui_spinner_start();
}

static void gui_enable_controls()
{
    gui_spinner_stop();
    gui_fade_stop(); 
}

static gboolean wait_icon_load_finished(gpointer user_data)
{
    gboolean res = TRUE;
  /*  g_mutex_lock(icon_loader_mutex); */
    debug_printf("%d of %d icons loaded (%d%%)\n", icons_loaded, total_icons, (int)(100*((double)icons_loaded/(double)total_icons)));
    if (icons_loaded >= total_icons) {
        gui_enable_controls();
        res = FALSE;
        if (finished_callback) {
            finished_callback(TRUE);
            finished_callback = NULL;
        }
    }
  /*  g_mutex_unlock(icon_loader_mutex); */
    return res;
}

#ifdef HAVE_LIBIMOBILEDEVICE_1_1
static void gui_set_wallpaper(const char *wp)
{
    GError *err = NULL;
    wallpaper = NULL;
	ClutterContent *image = NULL;
	GdkPixbuf *pixbuf;
	
	/* clutter_texture_new' is deprecated TW 21/04/13 */
    /* ClutterActor *actor = clutter_texture_new(); */
	/* ClutterImage *actor = clutter_image_new(); */
	ClutterActor *actor = clutter_actor_new();

	/* clutter_texture_set_load_async' is deprecated TW 24/04/13 */
    /* clutter_texture_set_load_async(CLUTTER_TEXTURE(actor), TRUE); */

	/* clutter_texture_set_from_file is deprecated TW 24/04/13 */
    /* clutter_texture_set_from_file(CLUTTER_TEXTURE(actor), wp, &err); */
	pixbuf = gdk_pixbuf_new_from_file(wp, &err);
	image = clutter_image_new ();
	  
	clutter_image_set_data (CLUTTER_IMAGE (image),
                          gdk_pixbuf_get_pixels (pixbuf),
                          gdk_pixbuf_get_has_alpha (pixbuf)
                            ? COGL_PIXEL_FORMAT_RGBA_8888
                            : COGL_PIXEL_FORMAT_RGB_888,
                          gdk_pixbuf_get_width (pixbuf),
                          gdk_pixbuf_get_height (pixbuf),
                          gdk_pixbuf_get_rowstride (pixbuf),
                          NULL);
	clutter_actor_set_content (actor, image);

	g_object_unref (pixbuf);
    if (err) {
        g_error_free(err);
        err = NULL;
        return;
    }
	/* fprintf(stderr,"\nERROR: above clutter_set_size7: LINE 2462 \n"); TEST TW 03/05/13 */
    clutter_actor_set_size(actor, stage_area.x2, stage_area.y2);
    clutter_actor_set_position(actor, 0, 18); /* WAS 0,0 but changed to 0,18 so that wallpaper does not cover clock etc TW 10/05/12 */
    clutter_actor_show(actor);

	/* clutter_container_add_actor' is deprecated TW 21/04/13 */
    /* clutter_group_add(CLUTTER_GROUP(stage), actor); */
	
	/* NOTE This adds the phone wallpaper to stage TW 07/05/13 */
	 /* clutter_actor_add_child(CLUTTER_ACTOR(stage), actor);   This is removed for test only TW 10/05/13 */

	/* clutter_actor_lower_bottom' is deprecated  TW 22/04/13 */  
	/* clutter_actor_lower_bottom(actor); */
	
	/* THIS adds wallpaper to stage but loads it over the top of icons TW 18/05/13 */
	 /* clutter_actor_insert_child_below(CLUTTER_ACTOR(stage), actor, ((void *)0));  TEST TW 05/05/13 */

    wallpaper = actor;
    item_text_color.alpha = 255;
}
#endif

static gboolean gui_pages_init_cb(gpointer user_data)
{
    const char *uuid = (const char*)user_data;
    GError *error = NULL;
    plist_t iconstate = NULL;

    gui_disable_controls();
    icons_loaded = 0;
    total_icons = 0;

    if (selected_folder) {
        folderview_close_finish(selected_folder);
    }

    pages_free();

    /* connect to sbservices */
    if (!sbc)
        sbc = device_sbs_new(uuid, &osversion, &error);

    if (error) {
        g_printerr("%s", error->message);
        g_error_free(error);
        error = NULL;
    }

    if (sbc) {
        const char *fmt_version = NULL;
#ifdef HAVE_LIBIMOBILEDEVICE_1_1
        if (osversion >= 0x04000000) {
            fmt_version = "2";
        }

        /* Load wallpaper if available */
        if (osversion >= 0x03020000) {
            char *path;
            path = device_sbs_save_wallpaper(sbc, uuid, &error);
            if (path == NULL) {
              g_printerr("%s", error->message);
              g_error_free(error);
              error = NULL;
	    } else {
	      gui_set_wallpaper(path); /* This only sets path to wallpaper off phone in users .cache directory TW */
	    }
	    g_free (path);
        }
#endif
        /* Load icon data */
        if (device_sbs_get_iconstate(sbc, &iconstate, fmt_version, &error)) {
            gui_set_iconstate(iconstate, fmt_version);
            plist_free(iconstate);
        }
    }

    if (error) {
        g_printerr("%s", error->message);
        g_error_free(error);
        error = NULL;
    }
	/* increased time to 2500 from 500 for test BUT NOT REQUIRED TW 08/05/13 */
    clutter_threads_add_timeout(500, (GSourceFunc)wait_icon_load_finished, NULL); 

    return FALSE;
}

static gboolean update_device_info_cb(gpointer user_data)
{
    device_info_t di = (device_info_t)user_data;
    if (di) {
        clutter_text_set_text(CLUTTER_TEXT(type_label), di->device_type);
    } else {
        clutter_text_set_text(CLUTTER_TEXT(type_label), NULL);
    }
    return FALSE;
}

static gboolean update_battery_info_cb(gpointer user_data)
{
    const char *uuid = (const char*)user_data;
    GError *error = NULL;
    gboolean res = TRUE;

    if (gui_deinitialized) {
        return FALSE;
    }

    if (device_poll_battery_capacity(uuid, &device_info, &error)) {
		/* fprintf(stderr,"\nERROR: above clutter_set_size8: LINE 2567 \n"); TEST TW 03/05/13 */
        clutter_actor_set_size(battery_level, (guint) (((double) (device_info->battery_capacity) / 100.0) * 15), 6);
        if (device_info->battery_capacity == 100) {
            res = FALSE;
        }
    }
    return res;
}

static gboolean init_battery_info_cb(gpointer user_data)
{
	/* fprintf(stderr,"\nERROR: above clutter_set_size9: LINE 2578 \n"); TEST TW 03/05/13 */
    clutter_actor_set_size(battery_level, (guint) (((double) (device_info->battery_capacity) / 100.0) * 15), 6);
    return FALSE;
}

void gui_pages_free()
{
    clutter_threads_add_timeout(0, (GSourceFunc)(update_device_info_cb), NULL);
    pages_free();
    if (sbc) {
        device_sbs_free(sbc);
	sbc = NULL;
	osversion = 0;
    }
}

static void gui_update_layout(device_info_t info) {
    if (!info)
        return;

    /* calculate stage boundry */
    stage_area.x1 = 0.0;
    stage_area.y1 = 0.0;
    stage_area.x2 = (info->home_screen_icon_width * MAX(info->home_screen_icon_columns, info->home_screen_icon_dock_max_count));
    stage_area.x2 += (ICON_SPACING * MAX(info->home_screen_icon_columns, info->home_screen_icon_dock_max_count));
    stage_area.x2 += ICON_SPACING + 2;

    stage_area.y2 = (info->home_screen_icon_height * info->home_screen_icon_rows);
    stage_area.y2 += ((ICON_SPACING*2) * info->home_screen_icon_rows);
    stage_area.y2 += DOCK_HEIGHT;

    printf("%s: stage_area x: %f, y: %f, width: %f, height: %f\n", __func__, stage_area.x1, stage_area.y1, stage_area.x2, stage_area.y2);

    /* update areas */
    dock_area.x1 = 0.0;
    dock_area.y1 = stage_area.y2 - DOCK_HEIGHT - (ICON_SPACING / 2);
    dock_area.x2 = stage_area.x2;
    dock_area.y2 = stage_area.y2;

    printf("%s: dock_area x: %f, y: %f, width: %f, height: %f\n", __func__, dock_area.x1, dock_area.y1, dock_area.x2, dock_area.y2);

    sb_area.x1 = 0.0;
    sb_area.y1 = ICON_SPACING;
    sb_area.x2 = stage_area.x2;
    sb_area.y2 = dock_area.y1;

    printf("%s: sb_area x: %f, y: %f, width: %f, height: %f\n", __func__, sb_area.x1, sb_area.y1, sb_area.x2, sb_area.y2);

    /* update triggers */
    left_trigger.x1 = -ICON_SPACING - 2;
    left_trigger.y1 = ICON_SPACING;
    left_trigger.x2 = -(ICON_SPACING / 2);
    left_trigger.y2 = stage_area.y2 - DOCK_HEIGHT - ICON_SPACING;

    printf("%s: left_trigger x: %f, y: %f, width: %f, height: %f\n", __func__, left_trigger.x1, left_trigger.y1, left_trigger.x2, left_trigger.y2);

    right_trigger.x1 = stage_area.x2 + (ICON_SPACING / 2);
    right_trigger.y1 = ICON_SPACING;
    right_trigger.x2 = stage_area.x2 + (ICON_SPACING*2);
    right_trigger.y2 = stage_area.y2 - DOCK_HEIGHT - ICON_SPACING;

    printf("%s: right_trigger x: %f, y: %f, width: %f, height: %f\n", __func__, right_trigger.x1, right_trigger.y1, right_trigger.x2, right_trigger.y2);

    /* update widget to new layout */
    gtk_widget_set_size_request(clutter_gtk_widget, stage_area.x2, stage_area.y2);
    clutter_actor_set_position(the_dock, dock_area.x1, dock_area.y1);
    clutter_actor_set_position(page_indicator_group, 0, stage_area.y2 - DOCK_HEIGHT - ICON_SPACING);
    clutter_actor_set_position(the_sb, sb_area.x1, sb_area.y1);
    clutter_actor_set_position(battery_level, stage_area.x2 - 22, 6);
    clutter_actor_set_position(spinner, (stage_area.x2 - 32.0) / 2, (stage_area.y2 - 64.0) / 2);
	/* fprintf(stderr,"\nERROR: above clutter_set_size9: LINE 2648 \n");  TEST TW 03/05/13 */
    clutter_actor_set_size(fade_rectangle, stage_area.x2, stage_area.y2);

#ifdef HAVE_LIBIMOBILEDEVICE_1_1
	/* fprintf(stderr,"\nERROR: above clutter_set_size10: LINE 2653 \n");  TEST TW 03/05/13 */
    
	/* This returns a Runtime error */
	/* Clutter-CRITICAL **: clutter_actor_set_size: assertion `CLUTTER_IS_ACTOR (self)' failed */
	
	/* clutter_actor_set_size(wallpaper, stage_area.x2, stage_area.y2); */

	/* fprintf(stderr, "\nactor-set-size-wallpaper- stage-area.x2 = %f stage-area.y2 = %f- LINE:2655\n",stage_area.x2, stage_area.y2);  TEST TW 03/05/13 */
#endif
}

static gboolean device_info_cb(gpointer user_data)
{
    GError *error = NULL;
    const char *uuid = (const char*)user_data;
    if (device_get_info(uuid, &device_info, &error)) {
        /* Update layout */
        gui_update_layout(device_info);
        /* Update device info */
        clutter_threads_add_idle((GSourceFunc)update_device_info_cb, device_info);
        /* Update battery information */
        clutter_threads_add_idle((GSourceFunc)init_battery_info_cb, NULL);
        /* Register battery state read timeout */
        clutter_threads_add_timeout(device_info->battery_poll_interval * 1000, (GSourceFunc)update_battery_info_cb, (gpointer)uuid);

	if (device_info_callback) {
            device_info_callback(device_info->device_name, device_info->device_name);
            device_info_callback = NULL;
	}
    } else {
        if (error) {
            g_printerr("%s", error->message);
            g_error_free(error);
        } else {
            g_printerr(_("Unknown error occurred"));
        }
        if (finished_callback) {
            finished_callback(FALSE);
	    finished_callback = NULL;
        }
    }
    return FALSE;
}

void gui_pages_load(const char *uuid, device_info_cb_t info_cb, finished_cb_t finished_cb)
{
    printf("%s: %s\n", __func__, uuid);
    finished_callback = finished_cb;
    device_info_callback = info_cb;

    /* Load icons */
    clutter_threads_add_idle((GSourceFunc)gui_pages_init_cb, (gpointer)uuid);

    /* Load device information */

	/* g_thread_create' is deprecated TW 24/04/13 */
    /* g_thread_create((GThreadFunc)device_info_cb, (gpointer)uuid, FALSE, NULL); */
	const gchar *name1 = "loadthread";
	g_thread_new(name1, (GThreadFunc)device_info_cb, (gpointer)uuid);
}

GtkWidget *gui_init()
{
    device_info = device_info_new();
    ClutterActor *actor;
	ClutterContent *image, *image1, *image2, *image3 = NULL; 
	GdkPixbuf *pixbuf, *pixbuf1, *pixbuf2, *pixbuf3 = NULL; 
	
    /* if (!g_thread_supported()) */
       
		/* g_thread_init' is deprecated	No longer necessary TW 24/03/13 */	
		/* g_thread_init(NULL); */

   /*  if (icon_loader_mutex == NULL) */
		/* 'g_mutex_new' is deprecated TW 24/04/13 */
    	/*    icon_loader_mutex = g_mutex_new(); */

    /* initialize clutter threading environment */
    if (!clutter_threads_initialized) {
		/* 'clutter_threads_init' is deprecated TW 26/04/13 */
      	/*  clutter_threads_init(); */
        clutter_threads_initialized = 1;
    }

    if (!clutter_initialized) {
        g_setenv ("CLUTTER_VBLANK", "none", FALSE);
        if (gtk_clutter_init(NULL, NULL) != CLUTTER_INIT_SUCCESS) {
            g_error("Unable to initialize GtkClutter");
            return NULL;
        }
        clutter_initialized = 1;
    }

    gettimeofday(&last_page_switch, NULL);

    /* Create the clutter widget */
    GtkWidget *clutter_widget = clutter_gtk_widget = gtk_clutter_embed_new();

    /* Set the size of the widget, because we should not set the size of its
     * stage when using GtkClutterEmbed.
     */
    gtk_widget_set_size_request(clutter_widget, stage_area.x2, stage_area.y2);

    /* Set the stage background color */
    stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(clutter_widget));

	/* clutter_stage_set_color is deprecated TW 23/04/13 */
    /* clutter_stage_set_color(CLUTTER_STAGE(stage), &stage_color); */
	
	clutter_actor_set_background_color(CLUTTER_ACTOR(stage), &stage_color);

    /* attach to stage signals */
    g_signal_connect(stage, "motion-event", G_CALLBACK(stage_motion_cb), NULL);
    g_signal_connect(stage, "key-press-event", G_CALLBACK(stage_key_press_cb), NULL);

    /* Load ui background */
    GError *err = NULL;

	/* clutter_texture_new' is deprecated TW 21/04/13 */
    /* actor = clutter_texture_new(); */
	 actor = clutter_actor_new();
	 
	/* clutter_texture_set_load_async' is deprecated TW 24/04/13 */    
	/* clutter_texture_set_load_async(CLUTTER_TEXTURE(actor), TRUE); */
    	
	/* clutter_texture_set_from_file is deprecated TW 24/04/13 */
	/* clutter_texture_set_from_file(CLUTTER_TEXTURE(actor), SBMGR_DATA "/background.png", &err); */
	
	/* image = gtk_image_new_from_file(SBMGR_DATA "/background.png"); */
	

	pixbuf = gdk_pixbuf_new_from_file ("/usr/local/share/sbmanager/background.png", NULL);
	/* fprintf(stderr,"\n%s\n", __func__);  TEST TW 10/05/13 */
	
	/*char *iconfilename; */
	/* iconfilename = g_strdup(SBMGR_DATA "/background.png"); */

	/* fprintf(stderr,"\n gui-init: iconfilename = %s\n", iconfilename); */

	/* pixbuf = gdk_pixbuf_new_from_file (iconfilename, &err);  TEST TW 10/08/13 */
	/* pixbuf = gdk_pixbuf_new_from_file (SBMGR_DATA "/background.png", NULL); */
  	if(pixbuf == NULL){
		fprintf(stderr, "\n%spixbuf error reading file on background LINE 2854\n", __func__);
	}
	image = clutter_image_new ();
	  
	clutter_image_set_data (CLUTTER_IMAGE (image),
                          gdk_pixbuf_get_pixels (pixbuf),
                          gdk_pixbuf_get_has_alpha (pixbuf)
                            ? COGL_PIXEL_FORMAT_RGBA_8888
                            : COGL_PIXEL_FORMAT_RGB_888,
                          gdk_pixbuf_get_width (pixbuf),
                          gdk_pixbuf_get_height (pixbuf),
                          gdk_pixbuf_get_rowstride (pixbuf),
                          NULL);

	g_object_unref (pixbuf);

    gfloat width = 0;
    gfloat height = 0;
    /* int ptr */

    /*gint addres1 = &width */
    clutter_content_get_preferred_size(image, &width, &height); 
    
    /*fprintf(stderr,"\n%s - preferred width = %f, and preferred height of image = %f\n", __func__, width, height); */
    
    clutter_actor_set_size(actor, width, height);
	
    clutter_actor_set_content (actor, image);	
	
	g_object_unref (image);

	/* gtk_widget_show(image); This does not work TW 30/04/13 */

	/* FIXME none of the springboard icons, background, page indicators, folder markers, and icon shadow are not being displayed TW 30/04/13 */
	/* The problem is that the original clutter functions used are deprecated and the code has to be changed to */
	/* do what is intended. Maybe gtk-clutter functions maybe gtk functions TW 30/04/13 then fix all the other errors */

    if (err) {
	g_printerr("%s", err->message);
	fprintf(stderr,"\n%s\n",err->message);
        g_error_free(err);
        err = NULL; 
    }
    if (actor) {
        clutter_actor_set_position(actor, 0, 0);
		clutter_actor_show(actor);
	
		/* clutter_container_add_actor' is deprecated TW 21/04/13 */
        /* clutter_group_add(CLUTTER_GROUP(stage), actor); */
		
		 clutter_actor_add_child(CLUTTER_ACTOR(stage), actor); 

		/* clutter_actor_show(stage);  TEST TW 04/05/13 this does not make the actor visible on stage */
		
		
    } else {
        fprintf(stderr, "could not load background.png\n");
    }

    /* Create device type widget */
     type_label = clutter_text_new_full(CLOCK_FONT, "", &clock_text_color); 

	/* clutter_container_add_actor' is deprecated TW 21/04/13 */
    /* clutter_group_add(CLUTTER_GROUP(stage), type_label); */
	
	/* fprintf(stderr,"\n%s:ERROR: above1 clutter-actor-add-child\n", __func__); */
	 clutter_actor_add_child(CLUTTER_ACTOR(stage), type_label); 
	/* fprintf(stderr,"\n%s:ERROR: below1 clutter-actor-add-child\n", __func__); */

    clutter_actor_set_position(type_label, 3.0, 2.0);

    /* clock widget */
    clock_label = clutter_text_new_full(CLOCK_FONT, "00:00", &clock_text_color);

	/* clutter_container_add_actor' is deprecated TW 21/04/13 */
    /* clutter_group_add(CLUTTER_GROUP(stage), clock_label); */

	/* fprintf(stderr,"\n%s:ERROR: above2 clutter-actor-add-child\n", __func__); */
	clutter_actor_add_child(CLUTTER_ACTOR(stage), clock_label);
	/* fprintf(stderr,"\n%s:ERROR: below2 clutter-actor-add-child\n", __func__); */

    /* page indicator group for holding the page indicator dots */
	
	/* clutter_group_new is deprecated TW 20/04/13 */
    /* page_indicator_group = clutter_group_new(); */
	page_indicator_group = clutter_actor_new();

	/* clutter_container_add_actor' is deprecated TW 21/04/13 */
    /* clutter_group_add(CLUTTER_GROUP(stage), page_indicator_group); */

	/* fprintf(stderr,"\n%s:ERROR: above3 clutter-actor-add-child\n", __func__); */
	clutter_actor_add_child(CLUTTER_ACTOR(stage), page_indicator_group); 
	/* fprintf(stderr,"\n%s:ERROR: below3 clutter-actor-add-child\n", __func__); */

    /* alignment will be done when new indicators are added */
    clutter_actor_set_position(page_indicator_group, 0, stage_area.y2 - DOCK_HEIGHT - ICON_SPACING); 

    /* page indicator (dummy), will be cloned when the pages are created */
    
	/* clutter_texture_new' is deprecated TW 21/04/13 */
	/* page_indicator = clutter_texture_new(); */
    page_indicator = clutter_actor_new(); 

	/* clutter_texture_set_load_async' is deprecated TW 24/04/13 */    
	/* clutter_texture_set_load_async(CLUTTER_TEXTURE(page_indicator), TRUE); */

	/* clutter_texture_set_from_file is deprecated TW 24/04/13 */
    /* clutter_texture_set_from_file(CLUTTER_TEXTURE(page_indicator), SBMGR_DATA "/dot.png", &err); */
	/* gdk_pixbuf_new_from_file(SBMGR_DATA "/dot.png", &err); */

	pixbuf1 = gdk_pixbuf_new_from_file ("/usr/local/share/sbmanager/dot.png", NULL); 
  
	image1 = clutter_image_new (); 
	  
	clutter_image_set_data (CLUTTER_IMAGE (image1), 
                          gdk_pixbuf_get_pixels (pixbuf1), 
                          gdk_pixbuf_get_has_alpha (pixbuf1) 
                            ? COGL_PIXEL_FORMAT_RGBA_8888 
                            : COGL_PIXEL_FORMAT_RGB_888, 
                          gdk_pixbuf_get_width (pixbuf1), 
                          gdk_pixbuf_get_height (pixbuf1), 
                          gdk_pixbuf_get_rowstride (pixbuf1), 
                          NULL); 
	g_object_unref (pixbuf1); 

    gfloat width1 = 0;
    gfloat height1 = 0;
   
    clutter_content_get_preferred_size(image1, &width1, &height1); 

    clutter_actor_set_size(page_indicator, width1, height1);
    
    clutter_actor_set_content (page_indicator, image1); 

    g_object_unref (image1); 
	
    if (err) { 
        fprintf(stderr, "Could not load texture " SBMGR_DATA "/dot.png" ": %s\n", err->message); 
        g_error_free(err); 
        err = NULL; 
    } 
    if (page_indicator) { 
         clutter_actor_hide(page_indicator); 
		/* fprintf(stderr, "\nif-page-indicator\n");  TW TEST 03/05/13 */

		/* clutter_container_add_actor is deprecated TW 20/04/13 */
        /* clutter_container_add_actor(CLUTTER_CONTAINER(stage), page_indicator); */
		
        
        /* fprintf(stderr,"\n%s:ERROR: above4 clutter-actor-add-child\n", __func__); */
		 clutter_actor_add_child(CLUTTER_ACTOR(stage), page_indicator); 
        /* fprintf(stderr,"\n%s:ERROR: below4 clutter-actor-add-child\n", __func__); */

		/* clutter_actor_show(stage);  TEST TW 04/05/13 */	

   } 

    /* icon shadow texture dummy, cloned when drawing the icons */
    
	/* clutter_texture_new' is deprecated TW 21/04/13 */
	/* icon_shadow = clutter_texture_new(); */
	icon_shadow = clutter_actor_new(); 

	/* clutter_texture_set_load_async' is deprecated TW 24/04/13 */
    /* clutter_texture_set_load_async(CLUTTER_TEXTURE(icon_shadow), TRUE); */

	/* clutter_texture_set_from_file is deprecated TW 24/04/13 */
    /* clutter_texture_set_from_file(CLUTTER_TEXTURE(icon_shadow), SBMGR_DATA "/iconshadow.png", &err); */
	/*gtk_image_new_from_file(SBMGR_DATA "/iconshadow.png"); TEST */
	
	pixbuf2 = gdk_pixbuf_new_from_file ("/usr/local/share/sbmanager/iconshadow.png", NULL); 
  
	image2 = clutter_image_new (); 
	  
	clutter_image_set_data (CLUTTER_IMAGE (image2), 
                          gdk_pixbuf_get_pixels (pixbuf2), 
                          gdk_pixbuf_get_has_alpha (pixbuf2) 
                           ? COGL_PIXEL_FORMAT_RGBA_8888 
                           : COGL_PIXEL_FORMAT_RGB_888, 
                          gdk_pixbuf_get_width (pixbuf2), 
                          gdk_pixbuf_get_height (pixbuf2), 
                          gdk_pixbuf_get_rowstride (pixbuf2), 
                          NULL); 
	g_object_unref (pixbuf2); 
    
    gfloat width2 = 0;
    gfloat height2 = 0;
   
    clutter_content_get_preferred_size(image2, &width2, &height2); 

    clutter_actor_set_size(icon_shadow, width2, height2);
    
    clutter_actor_set_content (icon_shadow, image2); 

    g_object_unref (image2); 
	
    if (err) { 
        fprintf(stderr, "Could not load texture " SBMGR_DATA "/iconshadow.png" ": %s\n", err->message); 
        g_error_free(err); 
        err = NULL; 
    } 
    if (icon_shadow) { 
        clutter_actor_hide(icon_shadow); 

		/* clutter_container_add_actor is deprecated TW 20/04/13 */
        /* clutter_container_add_actor(CLUTTER_CONTAINER(stage), icon_shadow); */
		/* fprintf(stderr,"\n%s:ERROR: above5 clutter-actor-add-child\n", __func__); */
		clutter_actor_add_child(CLUTTER_ACTOR(stage), icon_shadow); 
		/* fprintf(stderr,"\n%s:ERROR: below5 clutter-actor-add-child\n", __func__); */

    } 

    /* folder marker */

	/* clutter_texture_new' is deprecated TW 21/04/13 */
    /* folder_marker = clutter_texture_new(); */
	folder_marker = clutter_actor_new(); 

	/* clutter_texture_set_load_async' is deprecated TW 24/04/13 */    
	/* clutter_texture_set_load_async(CLUTTER_TEXTURE(folder_marker), TRUE); */
    
	/* clutter_texture_set_from_file is deprecated TW 24/04/13 */
	/* clutter_texture_set_from_file(CLUTTER_TEXTURE(folder_marker), SBMGR_DATA "/foldermarker.png", &err); */
	
	/* gtk_image_new_from_file(SBMGR_DATA "/foldermarker.png"); */
	
	pixbuf3 = gdk_pixbuf_new_from_file ("/usr/local/share/sbmanager/foldermarker.png", NULL);  
  
	image3 = clutter_image_new (); 
	  
	clutter_image_set_data (CLUTTER_IMAGE (image3), 
                          gdk_pixbuf_get_pixels (pixbuf3), 
                          gdk_pixbuf_get_has_alpha (pixbuf3) 
                            ? COGL_PIXEL_FORMAT_RGBA_8888 
                            : COGL_PIXEL_FORMAT_RGB_888, 
                          gdk_pixbuf_get_width (pixbuf3), 
                          gdk_pixbuf_get_height (pixbuf3), 
                          gdk_pixbuf_get_rowstride (pixbuf3), 
                          NULL); 
	g_object_unref (pixbuf3); 
    
    gfloat width3 = 0;
    gfloat height3 = 0;
   
    clutter_content_get_preferred_size(image3, &width3, &height3); 

    clutter_actor_set_size(folder_marker, width3, height3);	
    
    clutter_actor_set_content (folder_marker, image3); 

    g_object_unref (image3); 

    if (err) { 
        fprintf(stderr, "Could not load texture " SBMGR_DATA "/foldermarker.png" ": %s\n", err->message); 
        g_error_free(err); 
        err = NULL;  
    }  
    if (folder_marker) { 
         clutter_actor_hide(folder_marker); 
/*		clutter_actor_show(folder_marker);  TEST TW 12/05/13 */
		/* clutter_container_add_actor is deprecated TW 20/04/13 */
        /* clutter_container_add_actor(CLUTTER_CONTAINER(stage), folder_marker); */
		/* fprintf(stderr,"\n%s:ERROR: above6 clutter-actor-add-child\n", __func__); */
		clutter_actor_add_child(CLUTTER_ACTOR(stage), folder_marker); 
		/* fprintf(stderr,"\n%s:ERROR: below6 clutter-actor-add-child\n", __func__); */

    } 

    /* a group for the springboard icons */
    
	/* clutter_group_new is deprecated TW 20/04/13 */
	/* the_sb = clutter_group_new(); */
	the_sb = clutter_actor_new();

	/* clutter_container_add_actor' is deprecated TW 21/04/13 */
    /* clutter_group_add(CLUTTER_GROUP(stage), the_sb); */

	/* fprintf(stderr,"\n%s:ERROR: above7 clutter-actor-add-child\n", __func__); */
	clutter_actor_add_child(CLUTTER_ACTOR(stage), the_sb);
	/* fprintf(stderr,"\n%s:ERROR: below7 clutter-actor-add-child\n", __func__); */

    clutter_actor_set_position(the_sb, sb_area.x1, sb_area.y1);

    /* a group for the dock icons */
    
	/* clutter_group_new is deprecated TW 20/04/13 */
	/* the_dock = clutter_group_new(); */
	the_dock = clutter_actor_new();


	/* clutter_container_add_actor' is deprecated TW 21/04/13 */
    /* clutter_group_add(CLUTTER_GROUP(stage), the_dock); */
	/* fprintf(stderr,"\n%s:ERROR: above8 clutter-actor-add-child\n", __func__); */
	clutter_actor_add_child(CLUTTER_ACTOR(stage), the_dock);
	/* fprintf(stderr,"\n%s:ERROR: below8 clutter-actor-add-child\n", __func__); */

    clutter_actor_set_position(the_dock, dock_area.x1, dock_area.y1);

	gui_fade_init(); 
    gui_spinner_init();

    /* Show the stage */
    clutter_actor_show(stage);

    /* Create a timeline to manage animation */
    clock_timeline = clutter_timeline_new(200);

	/* clutter_timeline_set_loop' is deprecated TW 21/04/13 */
    /* clutter_timeline_set_loop(clock_timeline, TRUE); */ /* have it loop */
	clutter_timeline_set_repeat_count(clock_timeline, -1);  /* have it loop */

    /* fire a callback for frame change */
    g_signal_connect(clock_timeline, "completed", G_CALLBACK(clock_update_cb), NULL);

    /* and start it */
    clutter_timeline_start(clock_timeline);

     /* if (selected_mutex == NULL) */
		/* 'g_mutex_new' is deprecated TW 24/04/13 */
        /* selected_mutex = g_mutex_new(); */

    /* Position and update the clock */
    clock_set_time(clock_label, time(NULL));
    clutter_actor_show(clock_label);

    /* battery capacity */

	/* clutter_rectange_new_with_color is deprecated TW 20/04/13 */
    /* battery_level = clutter_rectangle_new_with_color(&battery_color); */
	battery_level = clutter_actor_new();

    /* clutter_container_add_actor' is deprecated TW 21/04/13 */
	/* clutter_group_add(CLUTTER_GROUP(stage), battery_level); */

	/* fprintf(stderr,"\n%s:ERROR: above9 clutter-actor-add-child\n", __func__); */
	 clutter_actor_add_child(CLUTTER_ACTOR(stage), battery_level);
	/* fprintf(stderr,"\n%s:ERROR: below9 clutter-actor-add-child\n", __func__); */

     clutter_actor_set_position(battery_level, stage_area.x2 - 22, 6); /* REMOVED FOR TEST TW ON 18/05/13 */

    return clutter_widget;
}

void gui_deinit()
{
    clutter_timeline_stop(clock_timeline);
    device_info_free(device_info);
    gui_deinitialized = 1;
}
