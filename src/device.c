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
 *
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

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/sbservices.h>

#include "device.h"

/* static GMutex *idevice_mutex = NULL; */ /* Static GMutex moved to actual Function that uses it TW 28/04/13 */
static GQuark device_domain = 0;

void device_init()
{
	/* Changed from g_mutex_new to g_mutex_init due deprecated. TW 17/04/13 */
   /* idevice_mutex = g_mutex_new(); */
	/* idevice_mutex = g_mutex_init(); */  /* g_mutex_init  moved to actual Function that uses it TW 28/04/13 */
    device_domain = g_quark_from_string("libimobiledevice");
}

static gboolean device_connect(const char *uuid, idevice_t *phone, lockdownd_client_t *client, GError **error) {
    gboolean res = FALSE;

    if (!client || !phone) {
        return res;
    }

    if (IDEVICE_E_SUCCESS != idevice_new(phone, uuid)) {
        *error = g_error_new(device_domain, ENODEV, _("No device found, is it plugged in?"));
        return res;
    }

    if (LOCKDOWN_E_SUCCESS != lockdownd_client_new_with_handshake(*phone, client, "sbmanager")) {
        *error = g_error_new(device_domain, EIO, _("Could not connect to lockdownd!"));
        return res;
    }

    res = TRUE;

    return res;
}

sbservices_client_t device_sbs_new(const char *uuid, uint32_t *osversion, GError **error)
{
	/* static GMutex *idevice_mutex = NULL; */
    sbservices_client_t sbc = NULL;
    idevice_t phone = NULL;
    lockdownd_client_t client = NULL;
   /* uint16_t port = 0; */
    lockdownd_service_descriptor_t service = NULL;

    printf("%s: %s\n", __func__, uuid);
	
	/* g_mutex_init(idevice_mutex);  Added here to test TW 28/04/13 */

   /* g_mutex_lock(idevice_mutex); */
    if (!device_connect(uuid, &phone, &client, error)) {
        goto leave_cleanup;
    }

    plist_t version = NULL;
    if (osversion && lockdownd_get_value(client, NULL, "ProductVersion", &version) == LOCKDOWN_E_SUCCESS) {
        if (plist_get_node_type(version) == PLIST_STRING) {
            char *version_string = NULL;
            plist_get_string_val(version, &version_string);
            if (version_string) {
                /* parse version */
                int maj = 0;
                int min = 0;
                int rev = 0;
                sscanf(version_string, "%d.%d.%d", &maj, &min, &rev);
                free(version_string);
                *osversion = ((maj & 0xFF) << 24) + ((min & 0xFF) << 16) + ((rev & 0xFF) << 8);
            }
        }
    }
									/* service changed from &port to match libimobiledevice TW 17/04/13 */
    if ((lockdownd_start_service(client, "com.apple.springboardservices", &service) != LOCKDOWN_E_SUCCESS) || !service) {
        if (error)
            *error = g_error_new(device_domain, EIO, _("Could not start com.apple.springboardservices service! Remind that this feature is only supported in OS 3.1 and later!"));
        goto leave_cleanup;
    }					/* port changed to service to match libimobiledevice TW 19/04/13 */
    if (sbservices_client_new(phone, service, &sbc) != SBSERVICES_E_SUCCESS) {
        if (error)
            *error = g_error_new(device_domain, EIO, _("Could not connect to springboardservices!"));
        goto leave_cleanup;
    }

  leave_cleanup:
    if (client) {
        lockdownd_client_free(client);
    }
    idevice_free(phone);
  /*  g_mutex_unlock(idevice_mutex); */

    return sbc;
}

void device_sbs_free(sbservices_client_t sbc)
{
    if (sbc) {
        sbservices_client_free(sbc);
    }
}

gboolean device_sbs_get_iconstate(sbservices_client_t sbc, plist_t *iconstate, const char *format_version, GError **error)
{
    plist_t iconstate_loc = NULL;
    gboolean ret = FALSE;

    *iconstate = NULL;
    if (sbc) {
        sbservices_error_t err;
#ifdef HAVE_LIBIMOBILEDEVICE_1_1
        err = sbservices_get_icon_state(sbc, &iconstate_loc, format_version);
#else
        err = sbservices_get_icon_state(sbc, &iconstate_loc);
#endif
        if (err != SBSERVICES_E_SUCCESS || !iconstate_loc) {
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
        res = g_file_set_contents (filename, png, pngsize, error);
		/* fprintf(stderr,"\n device_sbs_save_icon file name = %s \n",filename); TW 01/05/13 */
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

#ifdef HAVE_LIBIMOBILEDEVICE_1_1
char *device_sbs_save_wallpaper(sbservices_client_t sbc, const char *uuid, GError **error)
{
    char *res = NULL;
    char *png = NULL;
    uint64_t pngsize = 0;

    if ((sbservices_get_home_screen_wallpaper_pngdata(sbc, &png, &pngsize) == SBSERVICES_E_SUCCESS) && (pngsize > 0)) {
        /* save png icon to disk */
        char *path;
        char *filename;

	path = g_build_filename (g_get_user_cache_dir (),
				 "libimobiledevice",
				 "wallpaper", NULL);
	g_mkdir_with_parents (path, 0755);
	g_free (path);

	filename = g_strdup_printf ("%s.png", uuid);
	path = g_build_filename (g_get_user_cache_dir (),
				 "libimobiledevice",
				 "wallpaper",
				 filename, NULL);
        g_free (filename);

        if (g_file_set_contents (path, png, pngsize, error) == FALSE) {
          g_free (filename);
          free (png);
          return NULL;
	}
	res = path;
    } else {
        if (error)
            *error = g_error_new(device_domain, EIO, _("Could not get wallpaper png data"));
    }
    if (png) {
        free(png);
    }
    return res;
}
#endif

device_info_t device_info_new()
{
    device_info_t device_info = g_new0(struct device_info_int, 1);

    /* initialize default values */
    device_info->home_screen_icon_columns = 4;
    device_info->home_screen_icon_dock_max_count = 4;
    device_info->home_screen_icon_height = 57;
    device_info->home_screen_icon_rows = 4;
    device_info->home_screen_icon_width = 57;
    device_info->icon_folder_columns = 4;
    device_info->icon_folder_max_pages = 1;
    device_info->icon_folder_rows = 3;
    device_info->icon_state_saves = 1;

    return device_info;
}

void device_info_free(device_info_t device_info)
{
    if (device_info) {
        if (device_info->uuid) {
            free(device_info->uuid);
        }
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

gboolean device_poll_battery_capacity(const char *uuid, device_info_t *device_info, GError **error) {
   /* static GMutex *idevice_mutex = NULL; */
	plist_t node = NULL;
    idevice_t phone = NULL;
    lockdownd_client_t client = NULL;
    gboolean res = FALSE;

    printf("%s: %s\n", __func__, uuid);

    if (!device_info) {
        return res;
    }

    printf("%s\n", __func__);

	
    /* g_mutex_lock(idevice_mutex); */
    if (!device_connect(uuid, &phone, &client, error)) {
        goto leave_cleanup;
    }

    if (!*device_info) {
        /* make new device info */
        *device_info = device_info_new();
    }

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
    idevice_free(phone);
    /* g_mutex_unlock(idevice_mutex); */

    return res;
}

static void device_dump_info(device_info_t info) {
    printf("%s: Device Information\n", __func__);

    printf("%s: UUID: %s\n", __func__, info->uuid);
    printf("%s: Name: %s\n", __func__, info->device_name);
    printf("%s: Type: %s\n", __func__, info->device_type);

    printf("%s: Battery\n", __func__);
    printf("%s: PollInterval: %d\n", __func__, info->battery_poll_interval);
    printf("%s: CurrentCapacity: %d\n", __func__, info->battery_capacity);

    printf("%s: HomeScreen Settings\n", __func__);

    printf("%s: IconColumns: %d\n", __func__, info->home_screen_icon_columns);
    printf("%s: IconRows: %d\n", __func__, info->home_screen_icon_rows);
    printf("%s: IconDockMaxCount: %d\n", __func__, info->home_screen_icon_dock_max_count);
    printf("%s: IconWidth: %d\n", __func__, info->home_screen_icon_width);
    printf("%s: IconHeight: %d\n", __func__, info->home_screen_icon_height);

    printf("%s: IconFolder Settings\n", __func__);
    printf("%s: IconWidth: %d\n", __func__, info->home_screen_icon_width);
    printf("%s: IconHeight: %d\n", __func__, info->home_screen_icon_height);
}

gboolean device_get_info(const char *uuid, device_info_t *device_info, GError **error)
{
	/* static GMutex *idevice_mutex = NULL; */
    uint8_t boolean = FALSE;
    uint64_t integer = 60;
    plist_t node = NULL;
    idevice_t phone = NULL;
    lockdownd_client_t client = NULL;
    gboolean res = FALSE;

    printf("%s: %s\n", __func__, uuid);

    if (!device_info) {
	return res;
    }

    printf("%s\n", __func__);

   /* g_mutex_lock(idevice_mutex); */
    if (!device_connect(uuid, &phone, &client, error)) {
        goto leave_cleanup;
    }

    if (!*device_info) {
        /* make new device info */
        *device_info = device_info_new();
    }

    (*device_info)->uuid = strdup(uuid);

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
        const char *devtypes[18][2] = {
            {"iPhone1,1", "iPhone"},
            {"iPhone1,2", "iPhone 3G"},
            {"iPhone2,1", "iPhone 3GS"},
            {"iPhone3,1", "iPhone 4"},
            {"iPhone3,3", "iPhone 4 CDMA"},
            {"iPhone4,1", "iPhone 4S"},
            {"iPad1,1", "iPad"},
            {"iPad2,1", "iPad 2"},
            {"iPad2,2", "iPad 2 3G GSM"},
            {"iPad2,3", "iPad 2 3G CDMA"},
            {"iPad2,4", "iPad"},
            {"iPad3,1", "iPad (3rd Gen)"},
            {"iPad3,2", "iPad 4G LTE (3rd Gen)"},
            {"iPad3,3", "iPad 4G LTE (3rd Gen)"},
            {"iPod1,1", "iPod Touch"},
            {"iPod2,1", "iPod Touch (2G)"},
            {"iPod3,1", "iPod Touch (3G)"},
            {"iPod4,1", "iPod Touch (4G)"}
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

    /* get layout information */
    node = NULL;
    plist_t value = NULL;
    lockdownd_get_value(client, "com.apple.mobile.iTunes", NULL, &node);

    value = plist_dict_get_item(node, "HomeScreenIconColumns");
    plist_get_uint_val(value, &integer);
    (*device_info)->home_screen_icon_columns = (guint)integer;

    value = plist_dict_get_item(node, "HomeScreenIconDockMaxCount");
    plist_get_uint_val(value, &integer);
    (*device_info)->home_screen_icon_dock_max_count = (guint)integer;

    value = plist_dict_get_item(node, "HomeScreenIconHeight");
    plist_get_uint_val(value, &integer);
    (*device_info)->home_screen_icon_height = (guint)integer;

    value = plist_dict_get_item(node, "HomeScreenIconRows");
    plist_get_uint_val(value, &integer);
    (*device_info)->home_screen_icon_rows = (guint)integer;

    value = plist_dict_get_item(node, "HomeScreenIconWidth");
    plist_get_uint_val(value, &integer);
    (*device_info)->home_screen_icon_width = (guint)integer;

    value = plist_dict_get_item(node, "IconFolderColumns");
    plist_get_uint_val(value, &integer);
    (*device_info)->icon_folder_columns = (guint)integer;

    value = plist_dict_get_item(node, "IconFolderMaxPages");
    plist_get_uint_val(value, &integer);
    (*device_info)->icon_folder_max_pages = (guint)integer;

    value = plist_dict_get_item(node, "IconFolderRows");
    plist_get_uint_val(value, &integer);
    (*device_info)->icon_folder_rows = (guint)integer;

    value = plist_dict_get_item(node, "IconStateSaves");
    plist_get_bool_val(value, &boolean);
    (*device_info)->icon_state_saves = (gboolean)boolean;

    /* get battery poll interval */
    value = plist_dict_get_item(node, "BatteryPollInterval");
    plist_get_uint_val(value, &integer);
    (*device_info)->battery_poll_interval = (guint)integer;

    plist_free(node);
    value = NULL;

    /* get current battery capacity */
    node = NULL;
    lockdownd_get_value(client, "com.apple.mobile.battery", NULL, &node);
    (*device_info)->battery_capacity = battery_info_get_current_capacity(node);
    plist_free(node);

    res = TRUE;

    device_dump_info((*device_info));

  leave_cleanup:
    if (client) {
        lockdownd_client_free(client);
    }
    idevice_free(phone);
   /* g_mutex_unlock(idevice_mutex); */

    return res;
}
