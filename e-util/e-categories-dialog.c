/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

#include "e-categories-dialog.h"
#include "e-categories-editor.h"
#include "e-categories-selector.h"
#include "e-category-completion.h"
#include "e-category-editor.h"

#define E_CATEGORIES_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CATEGORIES_DIALOG, ECategoriesDialogPrivate))

G_DEFINE_TYPE (ECategoriesDialog, e_categories_dialog, GTK_TYPE_DIALOG)

struct _ECategoriesDialogPrivate {
	GtkWidget *categories_editor;
};

static void
entry_changed_cb (GtkEntry *entry,
                  ECategoriesDialog *dialog)
{
	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK, TRUE);
}

static void
e_categories_dialog_class_init (ECategoriesDialogClass *class)
{
	g_type_class_add_private (class, sizeof (ECategoriesDialogPrivate));
}

static void
e_categories_dialog_init (ECategoriesDialog *dialog)
{
	GtkWidget *dialog_content;
	GtkWidget *categories_editor;

	dialog->priv = E_CATEGORIES_DIALOG_GET_PRIVATE (dialog);

	categories_editor = e_categories_editor_new ();
	dialog->priv->categories_editor = categories_editor;

	dialog_content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);
	gtk_box_pack_start (
		GTK_BOX (dialog_content), categories_editor, TRUE, TRUE, 0);
	gtk_box_set_spacing (GTK_BOX (dialog_content), 12);

	g_signal_connect (
		categories_editor, "entry-changed",
		G_CALLBACK (entry_changed_cb), dialog);

	gtk_dialog_add_buttons (
		GTK_DIALOG (dialog),
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Categories"));

	gtk_widget_show_all (categories_editor);
}

/**
 * e_categories_dialog_new:
 * @categories: Comma-separated list of categories
 *
 * Creates a new #ECategoriesDialog widget and sets the initial selection
 * to @categories.
 *
 * Returns: a new #ECategoriesDialog
 **/
GtkWidget *
e_categories_dialog_new (const gchar *categories)
{
	ECategoriesDialog *dialog;

	dialog = g_object_new (E_TYPE_CATEGORIES_DIALOG, NULL);

	if (categories)
		e_categories_dialog_set_categories (dialog, categories);

	return GTK_WIDGET (dialog);
}

/**
 * e_categories_dialog_get_categories:
 * @dialog: An #ECategoriesDialog
 *
 * Gets a comma-separated list of the categories currently selected
 * in the dialog.
 *
 * Returns: a comma-separated list of categories. Free returned
 * pointer with g_free().
 **/
gchar *
e_categories_dialog_get_categories (ECategoriesDialog *dialog)
{
	gchar *categories;

	g_return_val_if_fail (E_IS_CATEGORIES_DIALOG (dialog), NULL);

	categories = e_categories_editor_get_categories (
		E_CATEGORIES_EDITOR (dialog->priv->categories_editor));

	return categories;
}

/**
 * e_categories_dialog_set_categories:
 * @dialog: An #ECategoriesDialog
 * @categories: Comma-separated list of categories
 *
 * Sets the list of categories selected on the dialog.
 **/
void
e_categories_dialog_set_categories (ECategoriesDialog *dialog,
                                    const gchar *categories)
{
	g_return_if_fail (E_IS_CATEGORIES_DIALOG (dialog));

	e_categories_editor_set_categories (
		E_CATEGORIES_EDITOR (dialog->priv->categories_editor),
		categories);
}
