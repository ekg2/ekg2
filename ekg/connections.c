/*
 *  Asynchronous read/write handling for connections
 *
 *  (C) Copyright 2011 EKG2 team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2.h"

struct ekg_connection {
	GSocketConnection *conn;
	GDataInputStream *instream;
	GDataOutputStream *outstream;
	GCancellable *cancellable;

	gpointer priv_data;
	ekg_input_callback_t callback;
	ekg_failure_callback_t failure_callback;
	ekg_input_type_t in_type;
};

static GSList *connections = NULL;

static void setup_async_read(struct ekg_connection *c);
static void setup_async_write(struct ekg_connection *c);

static struct ekg_connection *get_connection_by_outstream(GDataOutputStream *s) {
	GSList *el;

	gint conn_find_outstream(gconstpointer list_elem, gconstpointer comp_elem) {
		const struct ekg_connection *c = list_elem;

		return c->outstream == comp_elem ? 0 : -1;
	}

	el = g_slist_find_custom(connections, s, conn_find_outstream);
	return el ? el->data : NULL;
}

static void done_async_read(GObject *obj, GAsyncResult *res, gpointer user_data) {
	struct ekg_connection *c = user_data;
	GError *err = NULL;
	gssize rsize;

	rsize = g_buffered_input_stream_fill_finish(G_BUFFERED_INPUT_STREAM(obj), res, &err);

	if (rsize == -1) {
		debug_error("done_async_read(), read failed: %s\n", err ? err->message : NULL);
		c->failure_callback(c->instream, err, c->priv_data);
		/* XXX: cleanup */
		g_error_free(err);
		return;
	}

	switch (c->in_type) {
		case EKG_INPUT_RAW:
			if (rsize == 0) {
				/* XXX: EOF */
				return;
			}

			c->callback(c->instream, c->priv_data);
			break;
		default:
			g_assert_not_reached();
	}

	setup_async_read(c);
}

static void setup_async_read(struct ekg_connection *c) {
	g_buffered_input_stream_fill_async(
			G_BUFFERED_INPUT_STREAM(c->instream),
			-1, /* fill the buffer */
			G_PRIORITY_DEFAULT,
			c->cancellable,
			done_async_read,
			c);
}

static void failed_write(struct ekg_connection *c) {
	/* XXX? */
}

static void done_async_write(GObject *obj, GAsyncResult *res, gpointer user_data) {
	struct ekg_connection *c = user_data;
	GError *err = NULL;
	gboolean ret;

	ret = g_output_stream_flush_finish(G_OUTPUT_STREAM(obj), res, &err);

	if (!ret) {
		debug_error("done_async_write(), write failed: %s\n", err ? err->message : NULL);
		/* XXX */
		failed_write(c);
		g_error_free(err);
		return;
	}

	/* XXX: anything to do? */
}

static void setup_async_write(struct ekg_connection *c) {
	g_output_stream_flush_async(
			G_OUTPUT_STREAM(c->outstream),
			G_PRIORITY_DEFAULT,
			c->cancellable, /* XXX */
			done_async_write,
			c);
}

GDataOutputStream *ekg_connection_add(
		GSocketConnection *conn,
		GInputStream *raw_instream,
		GOutputStream *raw_outstream,
		ekg_input_type_t in_type,
		ekg_input_callback_t callback,
		ekg_failure_callback_t failure_callback,
		gpointer priv_data)
{
	struct ekg_connection *c = g_slice_new(struct ekg_connection);

	c->conn = conn;
	c->instream = g_data_input_stream_new(raw_instream);
	c->outstream = g_data_output_stream_new(raw_outstream);
	c->cancellable = g_cancellable_new();

	c->callback = callback;
	c->failure_callback = failure_callback;
	c->priv_data = priv_data;
	c->in_type = in_type;

		/* disallow any blocking writes */
	g_buffered_output_stream_set_auto_grow(G_BUFFERED_OUTPUT_STREAM(c->outstream), TRUE);

	connections = g_slist_prepend(connections, c);
	setup_async_read(c);

	return c->outstream;
}

void ekg_connection_write(GDataOutputStream *f, const gchar *format, ...) {
	static GString *buf = NULL;
	va_list args;
	gsize out;
	GError *err = NULL;
	struct ekg_connection *c = get_connection_by_outstream(f);

	if (G_LIKELY(format)) {
		if (!buf)
			buf = g_string_sized_new(120);

		va_start(args, format);
		g_string_vprintf(buf, format, args);
		va_end(args);
		
		out = g_output_stream_write(G_OUTPUT_STREAM(f), buf->str, buf->len, NULL, &err);

		if (out < buf->len) {
			debug_error("ekg_connection_write() failed (wrote %d out of %d): %s\n",
					out, buf->len, err ? err->message : "(no error?!)");
			failed_write(c);
			g_error_free(err);

			return;
		}
	}

	setup_async_write(c);
}
