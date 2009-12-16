/*
sbmanager -- Manage iPhone/iPod Touch SpringBoard icons from your computer!

Copyright (C) 2009      Nikias Bassen <nikias@gmx.li>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <libiphone/libiphone.h>
#include <libiphone/lockdown.h>
#include <libiphone/sbservices.h>
#include <plist/plist.h>
#include <time.h>
#include <glib.h>

#include <gtk/gtk.h>
#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "../data/data.h"

#define ITEM_FONT "FreeSans Bold 10px"

typedef struct {
    GtkWidget *window;
} SBManagerApp;

typedef struct {
    plist_t node;
    ClutterActor *texture;
    ClutterActor *label;
    gboolean is_dock_item;
} SBItem;

ClutterActor *stage = NULL;
ClutterActor *clock_label = NULL;
ClutterColor text_color = {255, 255, 255, 210};

GMutex *selected_mutex = NULL;
ClutterActor *selected = NULL;
SBItem *selected_item = NULL;

gfloat start_x = 0.0;
gfloat start_y = 0.0;

GList *dockitems = NULL;
GList *sbpages = NULL;

GList *this_page = NULL;
int current_page = 0;

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
	g_list_foreach(sbitems, (GFunc)(sbitem_free), NULL);
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
	di = g_new0(SBItem, 1);
	*list = g_list_append(*list, di);
	return;
    }
    valuenode = plist_dict_get_item(node, "displayIdentifier");
    if (valuenode && (plist_get_node_type(valuenode) == PLIST_STRING)) {
	char *value = NULL;
	plist_get_string_val(valuenode, &value);
	printf("retrieving icon for '%s'\n", value);
	if ((sbservices_get_icon_pngdata(sbc, value, &png, &pngsize) == SBSERVICES_E_SUCCESS) && (pngsize > 0)) {
	    FILE *f = fopen("/tmp/temp.png", "w");
	    GError *err = NULL;
	    fwrite(png, 1, pngsize, f);
	    fclose(f);
	    ClutterActor *actor = clutter_texture_new_from_file("/tmp/temp.png", &err);
	    if (actor) {
		plist_t nn = plist_dict_get_item(node, "displayName");
		di = g_new0(SBItem, 1);
		di->node = plist_copy(node);
		di->texture = actor;
		if (nn && (plist_get_node_type(nn) == PLIST_STRING)) {
		    char *txtval = NULL;
		    plist_get_string_val(nn, &txtval);
		    actor = clutter_text_new_full(ITEM_FONT, txtval, &text_color);
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
}

static void get_icons(const char *uuid)
{
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

    if (IPHONE_E_SUCCESS != iphone_device_new(&phone, uuid)) {
	fprintf(stderr, "No iPhone found, is it plugged in?\n");
	return;
    }

    if (LOCKDOWN_E_SUCCESS != lockdownd_client_new(phone, &client)) {
	fprintf(stderr, "Could not connect to lockdownd. Exiting.\n");
	goto leave_cleanup;
    }

    if ((lockdownd_start_service(client, "com.apple.springboardservices", &port) != LOCKDOWN_E_SUCCESS) || !port) {
	fprintf(stderr, "Could not start com.apple.springboardservices service! Remind that this feature is only supported in OS 3.1 and later!\n");
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
	if ((plist_get_node_type(dock) != PLIST_ARRAY) || (plist_array_get_size(dock) < 1)) {
	    fprintf(stderr, "ERROR: error getting outer dock icon array!\n");
	    goto leave_cleanup;
	}
	dock = plist_array_get_item(dock, 0);
	if (plist_get_node_type(dock) != PLIST_ARRAY) {
	    fprintf(stderr, "ERROR: error getting inner dock icon array!\n");
	    goto leave_cleanup;
	}
	count = plist_array_get_size(dock);
	for (i = 0; i < count; i++) {
	    plist_t node = plist_array_get_item(dock, i);
	    get_icon_for_node(node, &dockitems, sbc);
	}
	if (total > 1) {
	    /* get all icons for the other pages */
	    int p,r,rows;
	    for (p = 1; p < total; p++) {
		plist_t npage = plist_array_get_item(iconstate, p);
		GList *page = NULL;
		if ((plist_get_node_type(npage) != PLIST_ARRAY) || (plist_array_get_size(npage) < 1)) {
		    fprintf(stderr, "ERROR: error getting outer page icon array!\n");
		    goto leave_cleanup;
		}
		rows = plist_array_get_size(npage);
		for (r = 0; r < rows; r++) {
		    printf("page %d, row %d\n", p, r);

		    plist_t nrow = plist_array_get_item(npage, r);
		    if (plist_get_node_type(nrow) != PLIST_ARRAY) {
			fprintf(stderr, "ERROR: error getting page row icon array!\n");
			goto leave_cleanup;
		    }
		    count = plist_array_get_size(nrow);
		    for (i = 0; i < count; i++) {
			plist_t node = plist_array_get_item(nrow, i);
			get_icon_for_node(node, &page, sbc);
		    }
		}
		if (page) {
		    sbpages = g_list_append(sbpages, page);
		}
	    }
	}
    }

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

    return;
}

static void clock_cb (ClutterTimeline *timeline, gint msecs, SBManagerApp *app)
{
    time_t t = time(NULL);
    struct tm *curtime = localtime(&t);
    gchar *ctext = g_strdup_printf("%02d:%02d", curtime->tm_hour, curtime->tm_min);
    clutter_text_set_text(CLUTTER_TEXT(clock_label), ctext);
    clutter_actor_set_position(clock_label, (clutter_actor_get_width(stage)-clutter_actor_get_width(clock_label)) / 2, 2);
    g_free(ctext);
}

static gboolean item_button_press (ClutterActor *actor, ClutterButtonEvent *event, gpointer user_data)
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
    printf("%s: %s mouse pressed\n", __func__, strval);

    if (actor) {
	ClutterActor *sc = clutter_actor_get_parent(actor);
	if (item->is_dock_item) {
	    GList *children = clutter_container_get_children(CLUTTER_CONTAINER(sc));
	    if (children) {
		ClutterActor *icon = g_list_nth_data(children, 0);
		ClutterActor *label = g_list_nth_data(children, 1);
		clutter_actor_set_y(label, clutter_actor_get_y(icon) + 62.0);
		g_list_free(children);
	    }
	}
	clutter_actor_set_scale_full(sc, 1.2, 1.2, clutter_actor_get_x(actor) + clutter_actor_get_width(actor)/2, clutter_actor_get_y(actor) + clutter_actor_get_height(actor)/2);
	clutter_actor_raise_top(sc);
	clutter_actor_set_opacity(sc, 160);
	selected = sc;
	selected_item = item;
	start_x = event->x;
	start_y = event->y;
    }
    g_mutex_unlock(selected_mutex);

    return TRUE;
}

static gboolean item_button_release (ClutterActor *actor, ClutterButtonEvent *event, gpointer user_data)
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
    printf("%s: %s mouse released\n", __func__, strval);

    if (actor) {
	ClutterActor *sc = clutter_actor_get_parent(actor);
	if (item->is_dock_item) {
	    GList *children = clutter_container_get_children(CLUTTER_CONTAINER(sc));
	    if (children) {
		ClutterActor *icon = g_list_nth_data(children, 0);
		ClutterActor *label = g_list_nth_data(children, 1);
		clutter_actor_set_y(label, clutter_actor_get_y(icon) + 67.0);
		g_list_free(children);
	    }
	}
	clutter_actor_set_scale_full(sc, 1.0, 1.0, clutter_actor_get_x(actor) + clutter_actor_get_width(actor)/2, clutter_actor_get_y(actor) + clutter_actor_get_height(actor)/2);
	clutter_actor_set_opacity(sc, 255);
    }

    selected = NULL;
    selected_item = NULL;
    start_x = 0.0;
    start_y = 0.0;

    g_mutex_unlock(selected_mutex);

    return TRUE;
}

static void redraw_icons(SBManagerApp *app)
{
    guint i;
    gfloat ypos;
    gfloat xpos;

    if (dockitems) {
	ypos = 398.0;
	xpos = 16.0;
  	printf("%s: drawing dock icons\n", __func__);
	for (i = 0; i < g_list_length(dockitems); i++) {
	    SBItem *item = (SBItem*)g_list_nth_data(dockitems, i);
	    if (item && item->texture && item->node) {
		item->is_dock_item = TRUE;
		ClutterActor *grp = clutter_group_new();
		ClutterActor *actor = item->texture;
		clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor);
		clutter_actor_set_position(actor, xpos, ypos);
		clutter_actor_set_reactive(actor, TRUE);
		g_signal_connect(actor, "button-press-event", G_CALLBACK (item_button_press), item);
		g_signal_connect(actor, "button-release-event", G_CALLBACK (item_button_release), item);
		clutter_actor_show(actor);
		actor = item->label;
		clutter_actor_set_position(actor, xpos+(59.0 - clutter_actor_get_width(actor))/2, ypos+67.0);
		clutter_actor_show(actor);
		clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor);
		clutter_container_add_actor(CLUTTER_CONTAINER(stage), grp);
	    }
	    xpos += 76;
	}
    }
    clutter_stage_ensure_redraw(CLUTTER_STAGE(stage));
    if (sbpages) {
	ypos = 32.0;
	xpos = 16.0;
	printf("%s: %d pages\n", __func__, g_list_length(sbpages));
  	printf("%s: drawing page icons for page %d\n", __func__, current_page);
	this_page = g_list_nth_data(sbpages, current_page);
	for (i = 0; i < g_list_length(this_page); i++) {
	    SBItem *item = (SBItem*)g_list_nth_data(this_page, i);
	    if (item && item->texture && item->node) {
		item->is_dock_item = FALSE;
		ClutterActor *grp = clutter_group_new();
		ClutterActor *actor = item->texture;
		clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor);
		clutter_actor_set_position(actor, xpos, ypos);
		clutter_actor_set_reactive(actor, TRUE);
		g_signal_connect(actor, "button-press-event", G_CALLBACK (item_button_press), item);
		g_signal_connect(actor, "button-release-event", G_CALLBACK (item_button_release), item);
		clutter_actor_show(actor);
		actor = item->label;
		clutter_actor_set_position(actor, xpos+(59.0 - clutter_actor_get_width(actor))/2, ypos+62.0);
		clutter_actor_show(actor);
		clutter_container_add_actor(CLUTTER_CONTAINER(grp), actor);
		clutter_container_add_actor(CLUTTER_CONTAINER(stage), grp);
	    }
	    if (((i+1) % 4) == 0) {
		xpos = 16.0;
		ypos += 88.0;
	    } else {
		xpos += 76.0;
	    }
	}
    }
    clutter_stage_ensure_redraw(CLUTTER_STAGE(stage));
}

static gboolean stage_motion (ClutterActor *actor, ClutterMotionEvent *event, gpointer user_data)
{
    /* check if an item has been raised */ 
    if (!selected || !selected_item) {
	return FALSE;
    }

/*    gfloat oldx = clutter_actor_get_x(selected);
    gfloat oldy = clutter_actor_get_y(selected);

    clutter_actor_set_position(selected, oldx + (event->x - start_x), oldy + (event->y - start_y));
*/
    clutter_actor_move_by(selected, (event->x - start_x), event->y - start_y);

    start_x = event->x;
    start_y = event->y;

    if (selected_item->is_dock_item) {
	printf("an icon from the dock is moving\n");
    } else {
	printf("a regular icon is moving\n");
    }

    return TRUE;
}

static gboolean form_map(GtkWidget *widget, GdkEvent *event, SBManagerApp *app)
{
    printf("%s: mapped\n", __func__);
    clutter_stage_ensure_redraw(CLUTTER_STAGE(stage));

    redraw_icons(app);

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

int main(int argc, char **argv)
{
    SBManagerApp *app;
    ClutterTimeline *timeline;
    ClutterColor stage_color = { 0x00, 0x00, 0x00, 0xff }; /* Black */
    ClutterActor *actor;

    app = g_new0 (SBManagerApp, 1);
    if (!app) {
	printf("Error: out of memory!\n");
	return -1;
    }

    if (gtk_clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS) {
	g_error ("Unable to initialize GtkClutter");
    }

    if (!g_thread_supported())
		g_thread_init(NULL);

    get_icons(NULL);

    /* Create the window and some child widgets: */
    app->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW(app->window), "SpringBoard Manager");
    GtkWidget *vbox = gtk_vbox_new (FALSE, 6);
    gtk_container_add (GTK_CONTAINER (app->window), vbox);
    gtk_widget_show (vbox);
    GtkWidget *button = gtk_button_new_with_label ("Upload changes to device");
    gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    gtk_widget_show (button);

    /* Stop the application when the window is closed: */
    g_signal_connect (app->window, "hide", G_CALLBACK (gtk_main_quit), app);

    /* Create the clutter widget: */
    GtkWidget *clutter_widget = gtk_clutter_embed_new ();
    gtk_box_pack_start (GTK_BOX (vbox), clutter_widget, TRUE, TRUE, 0);
    gtk_widget_show (clutter_widget);

    /* Set the size of the widget, because we should not set the size of its
     * stage when using GtkClutterEmbed.
     */ 
    gtk_widget_set_size_request (clutter_widget, 320, 480);

    /* Get the stage and set its size and color: */
    stage = gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (clutter_widget));

    clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

    /* dock background */
    GError *err = NULL;
    actor = clutter_texture_new_from_file(BGPIC, &err);
    if (err) {
	g_error_free(err);
    }
    if (actor) {
	clutter_actor_set_position(actor, 0, 0);
	clutter_actor_show(actor);
	clutter_group_add (CLUTTER_GROUP(stage), actor);
    } else {
	fprintf(stderr, "could not load background.png\n");
    }

    /* clock widget */
    actor = clutter_text_new_full ("FreeSans Bold 12px", "00:00", &text_color);
    gint xpos = (clutter_actor_get_width(stage)-clutter_actor_get_width(actor))/2;
    clutter_actor_set_position(actor, xpos, 2);
    clutter_actor_show(actor);
    clutter_group_add (CLUTTER_GROUP (stage), actor);
    clock_label = actor;

    /* Show the stage: */
    clutter_actor_show (stage);

    /* Create a timeline to manage animation */
    timeline = clutter_timeline_new (200);
    clutter_timeline_set_loop(timeline, TRUE);   /* have it loop */

    /* fire a callback for frame change */
    g_signal_connect(timeline, "completed",  G_CALLBACK (clock_cb), app);

    /* and start it */
    clutter_timeline_start (timeline);

    /* Show the window: */
    gtk_widget_show_all (GTK_WIDGET (app->window));

    g_signal_connect(stage, "motion-event", G_CALLBACK (stage_motion), app);

    g_signal_connect( G_OBJECT(app->window), "map-event", G_CALLBACK (form_map), app);

    g_signal_connect( G_OBJECT(app->window), "focus-in-event", G_CALLBACK (form_focus_change), timeline);
    g_signal_connect( G_OBJECT(app->window), "focus-out-event", G_CALLBACK (form_focus_change), timeline);

    selected_mutex = g_mutex_new();

    /* Start the main loop, so we can respond to events: */
    gtk_main ();

    return 0;
}
