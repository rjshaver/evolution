/*
 * evolution-secure-button.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "e-mail-format-extensions.h"

#include <glib/gi18n-lib.h>

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <e-util/e-util.h>

#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
#include "certificate-viewer.h"
#include "e-cert-db.h"
#endif

#include <camel/camel.h>

typedef struct _EMailFormatterSecureButton {
	GObject parent;
} EMailFormatterSecureButton;

typedef struct _EMailFormatterSecureButtonClass {
	GObjectClass parent_class;
} EMailFormatterSecureButtonClass;

static void e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface);
static void e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailFormatterSecureButton,
	e_mail_formatter_secure_button,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_formatter_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_formatter_extension_interface_init));

static const gchar *formatter_mime_types[] = { "application/vnd.evolution.widget.secure-button", NULL };

static const struct {
	const gchar *icon, *shortdesc, *description;
} smime_sign_table[5] = {
	{ "stock_signature-bad", N_("Unsigned"), N_("This message is not signed. There is no guarantee that this message is authentic.") },
	{ "stock_signature-ok", N_("Valid signature"), N_("This message is signed and is valid meaning that it is very likely that this message is authentic.") },
	{ "stock_signature-bad", N_("Invalid signature"), N_("The signature of this message cannot be verified, it may have been altered in transit.") },
	{ "stock_signature", N_("Valid signature, but cannot verify sender"), N_("This message is signed with a valid signature, but the sender of the message cannot be verified.") },
	{ "stock_signature-bad", N_("Signature exists, but need public key"), N_("This message is signed with a signature, but there is no corresponding public key.") },

};

static const struct {
	const gchar *icon, *shortdesc, *description;
} smime_encrypt_table[4] = {
	{ "stock_lock-broken", N_("Unencrypted"), N_("This message is not encrypted. Its content may be viewed in transit across the Internet.") },
	{ "stock_lock-ok", N_("Encrypted, weak"), N_("This message is encrypted, but with a weak encryption algorithm. It would be difficult, but not impossible for an outsider to view the content of this message in a practical amount of time.") },
	{ "stock_lock-ok", N_("Encrypted"), N_("This message is encrypted.  It would be difficult for an outsider to view the content of this message.") },
	{ "stock_lock-ok", N_("Encrypted, strong"), N_("This message is encrypted, with a strong encryption algorithm. It would be very difficult for an outsider to view the content of this message in a practical amount of time.") },
};

static const GdkRGBA smime_sign_colour[5] = {
	{ 0 }, { 0.53, 0.73, 0.53, 1 }, { 0.73, 0.53, 0.53, 1 }, { 0.91, 0.82, 0.13, 1 }, { 0 },
};

static gboolean
emfe_secure_button_format (EMailFormatterExtension *extension,
                           EMailFormatter *formatter,
                           EMailFormatterContext *context,
                           EMailPart *part,
                           CamelStream *stream,
                           GCancellable *cancellable)
{
	gchar *str;

	if ((context->mode != E_MAIL_FORMATTER_MODE_NORMAL) &&
	    (context->mode != E_MAIL_FORMATTER_MODE_RAW))
		return FALSE;

	str = g_strdup_printf (
		"<object type=\"application/vnd.evolution.widget.secure-button\" "
		"height=\"20\" width=\"100%%\" data=\"%s\" id=\"%s\"></object>",
		part->id, part->id);

	camel_stream_write_string (stream, str, cancellable, NULL);

	g_free (str);
	return TRUE;
}

#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
static void
viewcert_clicked (GtkWidget *button,
                  GtkWidget *parent)
{
	CamelCipherCertInfo *info = g_object_get_data((GObject *) button, "e-cert-info");
	ECert *ec = NULL;

	if (info->cert_data)
		ec = e_cert_new (CERT_DupCertificate (info->cert_data));

	if (ec != NULL) {
		GtkWidget *w = certificate_viewer_show (ec);

		/* oddly enough certificate_viewer_show doesn't ... */
		gtk_widget_show (w);
		g_signal_connect (
			w, "response",
			G_CALLBACK (gtk_widget_destroy), NULL);

		if (w && parent)
			gtk_window_set_transient_for (
				(GtkWindow *) w, (GtkWindow *) parent);

		g_object_unref (ec);
	} else {
		g_warning("can't find certificate for %s <%s>",
			info->name ? info->name : "",
			info->email ? info->email : "");
	}
}
#endif

static void
info_response (GtkWidget *widget,
               guint button,
               gpointer user_data)
{
	gtk_widget_destroy (widget);
}

static void
add_cert_table (GtkWidget *grid,
                GQueue *certlist,
                gpointer user_data)
{
	GList *head, *link;
	GtkTable *table;
	gint n = 0;

	table = (GtkTable *) gtk_table_new (certlist->length, 2, FALSE);

	head = g_queue_peek_head_link (certlist);

	for (link = head; link != NULL; link = g_list_next (link)) {
		CamelCipherCertInfo *info = link->data;
		gchar *la = NULL;
		const gchar *l = NULL;

		if (info->name) {
			if (info->email && strcmp (info->name, info->email) != 0)
				l = la = g_strdup_printf("%s <%s>", info->name, info->email);
			else
				l = info->name;
		} else {
			if (info->email)
				l = info->email;
		}

		if (l) {
			GtkWidget *w;
#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
			ECert *ec = NULL;
#endif
			w = gtk_label_new (l);
			gtk_misc_set_alignment ((GtkMisc *) w, 0.0, 0.5);
			g_free (la);
			gtk_table_attach (table, w, 0, 1, n, n + 1, GTK_FILL, GTK_FILL, 3, 3);
#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
			w = gtk_button_new_with_mnemonic(_("_View Certificate"));
			gtk_table_attach (table, w, 1, 2, n, n + 1, 0, 0, 3, 3);
			g_object_set_data((GObject *)w, "e-cert-info", info);
			g_signal_connect (
				w, "clicked",
				G_CALLBACK (viewcert_clicked), grid);

			if (info->cert_data)
				ec = e_cert_new (CERT_DupCertificate (info->cert_data));

			if (ec == NULL)
				gtk_widget_set_sensitive (w, FALSE);
			else
				g_object_unref (ec);
#else
			w = gtk_label_new (_("This certificate is not viewable"));
			gtk_table_attach (table, w, 1, 2, n, n + 1, 0, 0, 3, 3);
#endif
			n++;
		}
	}

	gtk_container_add (GTK_CONTAINER (grid), GTK_WIDGET (table));
}

static void
format_cert_infos (GQueue *cert_infos,
                   GString *output_buffer)
{
	GQueue valid = G_QUEUE_INIT;
	GList *head, *link;

	head = g_queue_peek_head_link (cert_infos);

	/* Make sure we have a valid CamelCipherCertInfo before
	 * appending anything to the output buffer, so we don't
	 * end up with "()". */
	for (link = head; link != NULL; link = g_list_next (link)) {
		CamelCipherCertInfo *cinfo = link->data;

		if ((cinfo->name != NULL && *cinfo->name != '\0') ||
		    (cinfo->email != NULL && *cinfo->email != '\0')) {
			g_queue_push_tail (&valid, cinfo);
		}
	}

	if (g_queue_is_empty (&valid))
		return;

	g_string_append (output_buffer, " (");

	while (!g_queue_is_empty (&valid)) {
		CamelCipherCertInfo *cinfo;

		cinfo = g_queue_pop_head (&valid);

		if (cinfo->name != NULL && *cinfo->name != '\0') {
			g_string_append (output_buffer, cinfo->name);

			if (cinfo->email != NULL && *cinfo->email != '\0') {
				g_string_append (output_buffer, " <");
				g_string_append (output_buffer, cinfo->email);
				g_string_append (output_buffer, ">");
			}

		} else if (cinfo->email != NULL && *cinfo->email != '\0') {
			g_string_append (output_buffer, cinfo->email);
		}

		if (!g_queue_is_empty (&valid))
			g_string_append (output_buffer, ", ");
	}

	g_string_append_c (output_buffer, ')');
}

static void
secure_button_clicked_cb (GtkWidget *widget,
                          EMailPart *part)
{
	GtkBuilder *builder;
	GtkWidget *grid, *w;
	GtkWidget *dialog;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "mail-dialogs.ui");

	dialog = e_builder_get_widget(builder, "message_security_dialog");

	grid = e_builder_get_widget(builder, "signature_grid");
	w = gtk_label_new (_(smime_sign_table[part->validity->sign.status].description));
	gtk_misc_set_alignment ((GtkMisc *) w, 0.0, 0.5);
	gtk_label_set_line_wrap ((GtkLabel *) w, TRUE);
	gtk_container_add (GTK_CONTAINER (grid), w);
	if (part->validity->sign.description) {
		GtkTextBuffer *buffer;

		buffer = gtk_text_buffer_new (NULL);
		gtk_text_buffer_set_text (
			buffer, part->validity->sign.description,
			strlen (part->validity->sign.description));
		w = g_object_new (gtk_scrolled_window_get_type (),
				 "hscrollbar_policy", GTK_POLICY_AUTOMATIC,
				 "vscrollbar_policy", GTK_POLICY_AUTOMATIC,
				 "shadow_type", GTK_SHADOW_IN,
				 "expand", TRUE,
				 "child", g_object_new(gtk_text_view_get_type(),
						       "buffer", buffer,
						       "cursor_visible", FALSE,
						       "editable", FALSE,
						       "width_request", 500,
						       "height_request", 160,
						       NULL),
				 NULL);
		g_object_unref (buffer);

		gtk_container_add (GTK_CONTAINER (grid), w);
	}

	if (!g_queue_is_empty (&part->validity->sign.signers))
		add_cert_table (
			grid, &part->validity->sign.signers, NULL);

	gtk_widget_show_all (grid);

	grid = e_builder_get_widget(builder, "encryption_grid");
	w = gtk_label_new (_(smime_encrypt_table[part->validity->encrypt.status].description));
	gtk_misc_set_alignment ((GtkMisc *) w, 0.0, 0.5);
	gtk_label_set_line_wrap ((GtkLabel *) w, TRUE);
	gtk_container_add (GTK_CONTAINER (grid), w);
	if (part->validity->encrypt.description) {
		GtkTextBuffer *buffer;

		buffer = gtk_text_buffer_new (NULL);
		gtk_text_buffer_set_text (
			buffer, part->validity->encrypt.description,
			strlen (part->validity->encrypt.description));
		w = g_object_new (gtk_scrolled_window_get_type (),
				 "hscrollbar_policy", GTK_POLICY_AUTOMATIC,
				 "vscrollbar_policy", GTK_POLICY_AUTOMATIC,
				 "shadow_type", GTK_SHADOW_IN,
				 "expand", TRUE,
				 "child", g_object_new(gtk_text_view_get_type(),
						       "buffer", buffer,
						       "cursor_visible", FALSE,
						       "editable", FALSE,
						       "width_request", 500,
						       "height_request", 160,
						       NULL),
				 NULL);
		g_object_unref (buffer);

		gtk_container_add (GTK_CONTAINER (grid), w);
	}

	if (!g_queue_is_empty (&part->validity->encrypt.encrypters))
		add_cert_table (grid, &part->validity->encrypt.encrypters, NULL);

	gtk_widget_show_all (grid);

	g_object_unref (builder);

	g_signal_connect (
		dialog, "response",
		G_CALLBACK (info_response), NULL);

	gtk_widget_show (dialog);
}

static GtkWidget *
emfe_secure_button_get_widget (EMailFormatterExtension *extension,
                               EMailPartList *context,
                               EMailPart *part,
                               GHashTable *params)
{
	GtkWidget *box, *button, *layout, *widget;
	const gchar *icon_name;
	gchar *description;
	GString *buffer;
	buffer = g_string_new ("");

	if (part->validity->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE) {
		const gchar *desc;
		gint status;

		status = part->validity->sign.status;
		desc = smime_sign_table[status].shortdesc;

		g_string_append (buffer, gettext (desc));

		format_cert_infos (&part->validity->sign.signers, buffer);
	}

	if (part->validity->encrypt.status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE) {
		const gchar *desc;
		gint status;

		if (part->validity->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE)
			g_string_append (buffer, "\n");

		status = part->validity->encrypt.status;
		desc = smime_encrypt_table[status].shortdesc;
		g_string_append (buffer, gettext (desc));
	}

	description = g_string_free (buffer, FALSE);

	/* FIXME: need to have it based on encryption and signing too */
	if (part->validity->sign.status != 0)
		icon_name = smime_sign_table[part->validity->sign.status].icon;
	else
		icon_name = smime_encrypt_table[part->validity->encrypt.status].icon;

	box = gtk_event_box_new ();
	if (part->validity->sign.status != 0)
		gtk_widget_override_background_color (box, GTK_STATE_FLAG_NORMAL,
			&smime_sign_colour[part->validity->sign.status]);

	layout = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_container_add (GTK_CONTAINER (box), layout);

	button = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (layout), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
		G_CALLBACK (secure_button_clicked_cb), part);

	widget = gtk_image_new_from_icon_name (
			icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_button_set_image (GTK_BUTTON (button), widget);

	widget = gtk_label_new (description);
	gtk_box_pack_start (GTK_BOX (layout), widget, FALSE, FALSE, 0);

	gtk_widget_show_all (box);

	g_free (description);
	return box;
}

static const gchar *
emfe_secure_button_get_display_name (EMailFormatterExtension *extension)
{
	return NULL;
}

static const gchar *
emfe_secure_button_get_description (EMailFormatterExtension *extension)
{
	return NULL;
}

static const gchar **
emfe_secure_button_mime_types (EMailExtension *extension)
{
	return formatter_mime_types;
}

static void
e_mail_formatter_secure_button_class_init (EMailFormatterSecureButtonClass *klass)
{
	e_mail_formatter_secure_button_parent_class = g_type_class_peek_parent (klass);
}

static void
e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface)
{
	iface->format = emfe_secure_button_format;
	iface->get_widget = emfe_secure_button_get_widget;
	iface->get_display_name = emfe_secure_button_get_display_name;
	iface->get_description = emfe_secure_button_get_description;
}

static void
e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = emfe_secure_button_mime_types;
}

static void
e_mail_formatter_secure_button_init (EMailFormatterSecureButton *extension)
{

}
