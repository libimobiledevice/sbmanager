/**
 * iconstate.h
 * Serialization of iconstate from/to GList's (header file)
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

#ifndef ICONSTATE_H
#define ICONSTATE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <plist/plist.h>
#include <glib.h>

#include "iconstate.h"

GList * iconstate_to_g_list(plist_t iconstate, GError **error);
plist_t g_list_to_iconstate(GList *iconstate, GError **error);

#endif
