/**
 * utility.c
 * Debugging and helper function implementation.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "utility.h"

/* debug */
static gboolean debug_app = FALSE;

void set_debug(gboolean debug)
{
    debug_app = debug;
}

void debug_printf(const char *format, ...)
{
    if (debug_app) {
        va_list args;
        va_start (args, format);
        vprintf(format, args);
        va_end (args);
    }
}

/* helper */
gboolean elapsed_ms(struct timeval *tv, guint ms)
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
