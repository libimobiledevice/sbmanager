/**
 * utility.h
 * Debugging and helper function definitions.
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

#ifndef UTILITY_H
#define UTILITY_H
#include <glib.h>

/* debug */
void set_debug(gboolean debug);
void debug_printf(const char *format, ...);

/* helper */
gboolean elapsed_ms(struct timeval *tv, guint ms);

#endif
