/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-win32-reloc.c: Support relocatable installation on Win32
 * Copyright 2005, Novell, Inc.
 *
 * Authors:
 *   Tor Lillqvist <tml@novell.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <windows.h>
#include <string.h>

#include <glib.h>
#include <libgnome/gnome-init.h>

/* localedir uses system codepage as it is passed to the non-UTF8ified
 * gettext library
 */
static char *localedir = NULL;

/* The others are in UTF-8 */
static char *gladedir;
static char *imagesdir;

static HMODULE hmodule;
G_LOCK_DEFINE_STATIC (mutex);

/* Silence gcc with a prototype. Yes, this is silly. */
BOOL WINAPI DllMain (HINSTANCE hinstDLL,
		     DWORD     fdwReason,
		     LPVOID    lpvReserved);

/* Minimal DllMain that just tucks away the DLL's HMODULE */
BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
	 DWORD     fdwReason,
	 LPVOID    lpvReserved)
{
        switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
                hmodule = hinstDLL;
                break;
        }
        return TRUE;
}

static char *
replace_prefix (const char *runtime_prefix,
                const char *configure_time_path)
{
        if (runtime_prefix &&
            strncmp (configure_time_path, GAL_PREFIX "/",
                     strlen (GAL_PREFIX) + 1) == 0) {
                return g_strconcat (runtime_prefix,
                                    configure_time_path + strlen (GAL_PREFIX),
                                    NULL);
        } else
                return g_strdup (configure_time_path);
}

static void
setup (void)
{
	char *full_prefix;  
	char *cp_prefix; 

        G_LOCK (mutex);
        if (localedir != NULL) {
                G_UNLOCK (mutex);
                return;
        }

        gnome_win32_get_prefixes (hmodule, &full_prefix, &cp_prefix);

        localedir = replace_prefix (cp_prefix, GAL_LOCALEDIR);
        g_free (cp_prefix);

        gladedir = replace_prefix (full_prefix, GAL_GLADEDIR);
        imagesdir = replace_prefix (full_prefix, GAL_IMAGESDIR);
        g_free (full_prefix);

	G_UNLOCK (mutex);
}

#include "e-util-private.h"	/* For prototypes */

#define GETTER(varbl)				\
const char *					\
_gal_get_##varbl (void)				\
{						\
        setup ();				\
        return varbl;				\
}

GETTER(localedir)
GETTER(gladedir)
GETTER(imagesdir)


