/**
 * sample-control-factory.c
 *
 * Copyright 1999, Helix Code, Inc.
 * 
 * Author:
 *   Nat Friedman (nat@nat.org)
 *
 */

#include <config.h>
#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-init.h>
#include <liboaf/liboaf.h>
#include <bonobo/bonobo-main.h>
#include <glade/glade.h>
#include <gal/widgets/e-cursors.h>

#include "addressbook.h"
#include "addressbook-component.h"
#include "e-address-widget.h"
#include "e-address-popup.h"
#include "addressbook/gui/widgets/e-minicard-control.h"
#include "select-names/e-select-names-factory.h"


static void
init_corba (int *argc, char **argv)
{
	gnome_init_with_popt_table ("evolution-addressbook", "0.0",
				    *argc, argv, oaf_popt_options, 0, NULL);

	oaf_init (*argc, argv);
}

static void
init_bonobo (int argc, char **argv)
{
	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("Could not initialize Bonobo"));

	glade_gnome_init ();
}

int
main (int argc, char **argv)
{
	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);
	
	init_corba (&argc, argv);

	
	init_bonobo (argc, argv);

	/* FIXME: Messy names here.  This file should be `main.c'.  `addressbook.c' should
           be `addressbook-control-factory.c' and the functions should be called
           `addressbook_control_factory_something()'.  And `addressbook-component.c'
           should be `addressbook-component-factory.c'.  */

	addressbook_factory_init ();
	addressbook_component_factory_init ();

	e_select_names_factory_init ();
	
	e_minicard_control_factory_init ();

	e_address_widget_factory_init ();
	e_address_popup_factory_init ();

	e_cursors_init();

#if 0
	g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

	bonobo_main ();

	return 0;
}
