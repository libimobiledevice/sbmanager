/**
 * device.c
 * Device communication functions.
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
#include <errno.h>
#include <plist/plist.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include <libiphone/libiphone.h>
#include <libiphone/lockdown.h>
#include <libiphone/sbservices.h>

#include "device.h"

static GMutex *libiphone_mutex = NULL;
static GQuark device_domain = 0;

void device_init()
{
    libiphone_mutex = g_mutex_new();
    device_domain = g_quark_from_string("libiphone");
}

sbservices_client_t device_sbs_new(const char *uuid, GError **error)
{
    sbservices_client_t sbc = NULL;
    iphone_device_t phone = NULL;
    lockdownd_client_t client = NULL;
    uint16_t port = 0;

    printf("%s: %s\n", __func__, uuid);

    g_mutex_lock(libiphone_mutex);
    if (IPHONE_E_SUCCESS != iphone_device_new(&phone, uuid)) {
        if (error)
            *error = g_error_new(device_domain, ENODEV, _("No device found, is it plugged in?"));
        g_mutex_unlock(libiphone_mutex);
        return sbc;
    }

    if (LOCKDOWN_E_SUCCESS != lockdownd_client_new_with_handshake(phone, &client, "sbmanager")) {
        if (error)
            *error = g_error_new(device_domain, EIO, _("Could not connect to lockdownd!"));
        goto leave_cleanup;
    }

    if ((lockdownd_start_service(client, "com.apple.springboardservices", &port) != LOCKDOWN_E_SUCCESS) || !port) {
        if (error)
            *error = g_error_new(device_domain, EIO, _("Could not start com.apple.springboardservices service! Remind that this feature is only supported in OS 3.1 and later!"));
        goto leave_cleanup;
    }
    if (sbservices_client_new(phone, port, &sbc) != SBSERVICES_E_SUCCESS) {
        if (error)
            *error = g_error_new(device_domain, EIO, _("Could not connect to springboardservices!"));
        goto leave_cleanup;
    }

  leave_cleanup:
    if (client) {
        lockdownd_client_free(client);
    }
    iphone_device_free(phone);
    g_mutex_unlock(libiphone_mutex);

    return sbc;
}

void device_sbs_free(sbservices_client_t sbc)
{
    if (sbc) {
        sbservices_client_free(sbc);
    }
}

gboolean device_sbs_get_iconstate(sbservices_client_t sbc, plist_t *iconstate, GError **error)
{
    plist_t iconstate_loc = NULL;
    gboolean ret = FALSE;

    *iconstate = NULL;
    if (sbc) {
        if (sbservices_get_icon_state(sbc, &iconstate_loc) != SBSERVICES_E_SUCCESS || !iconstate_loc) {
            if (error)
                *error = g_error_new(device_domain, EIO, _("Could not get icon state!"));
            goto leave_cleanup;
        }
        if (plist_get_node_type(iconstate_loc) != PLIST_ARRAY) {
            if (error)
                *error = g_error_new(device_domain, EIO, _("icon state is not an array as expected!"));
            goto leave_cleanup;
        }
        *iconstate = iconstate_loc;
	ret = TRUE;
    }

  leave_cleanup:
    if ((ret == FALSE) && iconstate_loc) {
        plist_free(iconstate_loc);
    }
    return ret;
}

gboolean device_sbs_save_icon(sbservices_client_t sbc, char *display_identifier, char *filename, GError **error)
{
    gboolean res = FALSE;
    char *png = NULL;
    uint64_t pngsize = 0;

    if ((sbservices_get_icon_pngdata(sbc, display_identifier, &png, &pngsize) == SBSERVICES_E_SUCCESS) && (pngsize > 0)) {
        /* save png icon to disk */
        FILE *f = fopen(filename, "w");
        fwrite(png, 1, pngsize, f);
        fclose(f);
        res = TRUE;
    } else {
        if (error)
            *error = g_error_new(device_domain, EIO, _("Could not get icon png data for '%s'"), display_identifier);
    }
    if (png) {
        free(png);
    }
    return res;
}

gboolean device_sbs_set_iconstate(sbservices_client_t sbc, plist_t iconstate, GError **error)
{
    gboolean result = FALSE;

    printf("About to upload new iconstate...\n");

    if (sbc) {
        if (sbservices_set_icon_state(sbc, iconstate) != SBSERVICES_E_SUCCESS) {
            if (error)
                *error = g_error_new(device_domain, EIO, _("Could not set new icon state!"));
            goto leave_cleanup;
        }

        printf("Successfully uploaded new iconstate\n");
        result = TRUE;
    }

  leave_cleanup:
    return result;
}

device_info_t device_info_new()
{
    return g_new0(struct device_info_int, 1);
}

void device_info_free(device_info_t device_info)
{
    if (device_info) {
        if (device_info->device_name) {
            free(device_info->device_name);
	}
        if (device_info->device_type) {
            free(device_info->device_type);
        }
	free(device_info);
    }
}

static guint battery_info_get_current_capacity(plist_t battery_info)
{
    uint64_t current_capacity = 0;
    plist_t node = NULL;

    if (battery_info == NULL)
        return (guint)current_capacity;

    node = plist_dict_get_item(battery_info, "BatteryCurrentCapacity");
    if (node != NULL) {
        plist_get_uint_val(node, &current_capacity);
    }
    return (guint)current_capacity;
}

gboolean device_get_info(const char *uuid, device_info_t *device_info, GError **error)
{
    uint64_t interval = 60;
    plist_t node = NULL;
    iphone_device_t phone = NULL;
    lockdownd_client_t client = NULL;
    gboolean res = FALSE;

    printf("%s: %s\n", __func__, uuid);

    if (!device_info) {
	return res;
    }

    printf("%s\n", __func__);

    g_mutex_lock(libiphone_mutex);
    if (IPHONE_E_SUCCESS != iphone_device_new(&phone, uuid)) {
        *error = g_error_new(device_domain, ENODEV, _("No device found, is it plugged in?"));
        goto leave_cleanup;
    }

    if (LOCKDOWN_E_SUCCESS != lockdownd_client_new_with_handshake(phone, &client, "sbmanager")) {
        *error = g_error_new(device_domain, EIO, _("Could not connect to lockdownd!"));
        goto leave_cleanup;
    }

    if (!*device_info) {
        /* make new device info */
        *device_info = device_info_new();
    }

    if ((*device_info)->device_name) {
	free((*device_info)->device_name);
	(*device_info)->device_name = NULL;
    }
    if ((*device_info)->device_type) {
	free((*device_info)->device_type);
	(*device_info)->device_type = NULL;
    }

    /* get device name */
    lockdownd_get_device_name(client, &((*device_info)->device_name));

    /* get device type */
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
                    (*device_info)->device_type = g_strdup(devtypes[i][1]);
                    break;
                }
            }
        }
	plist_free(node);
    } 

    /* get battery poll interval */
    node = NULL;
    lockdownd_get_value(client, "com.apple.mobile.iTunes", "BatteryPollInterval", &node);
    plist_get_uint_val(node, &interval);
    (*device_info)->battery_poll_interval = (guint)interval;
    plist_free(node);

    /* get current battery capacity */
    node = NULL;
    lockdownd_get_value(client, "com.apple.mobile.battery", NULL, &node);
    (*device_info)->battery_capacity = battery_info_get_current_capacity(node);
    plist_free(node);

    res = TRUE;

  leave_cleanup:
    if (client) {
        lockdownd_client_free(client);
    }
    iphone_device_free(phone);
    g_mutex_unlock(libiphone_mutex);

    return res;
}
