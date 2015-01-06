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
#include "ekg/internal.h"

#ifdef HAVE_LIBGNUTLS
#	include <errno.h> /* EAGAIN for transport wrappers */
#	include <gnutls/gnutls.h>

#	define NEED_SLAVERY 1
#endif

#define EKG_CONNECTION_ERROR ekg_connection_error_quark()
GQuark ekg_connection_error_quark() {
	return g_quark_from_static_string("ekg-connection-error-quark");
}

struct ekg_connection;

typedef void (*ekg_flush_handler_t) (struct ekg_connection *conn);

struct ekg_connection {
	GSocketConnection *conn;
	GDataInputStream *instream;
	GDataOutputStream *outstream;
	GCancellable *cancellable;

	gpointer priv_data;
	ekg_input_callback_t callback;
	ekg_failure_callback_t failure_callback;

	ekg_flush_handler_t flush_handler;

	GString *wr_buffer;

#if NEED_SLAVERY
	struct ekg_connection *master;
	struct ekg_connection *slave;
#endif
};

static GSList *connections = NULL;

static void setup_async_read(struct ekg_connection *c);
static gboolean setup_async_connect(GSocketClient *sock, struct ekg_connection_starter *cs);

#ifdef HAVE_LIBGNUTLS
static void ekg_gnutls_new_session(
		GSocketClient *sockclient,
		GSocketConnection *sock,
		struct ekg_connection_starter *cs);

#define EKG_GNUTLS_ERROR ekg_gnutls_error_quark()
static G_GNUC_CONST GQuark ekg_gnutls_error_quark() {
	return g_quark_from_static_string("ekg-gnutls-error-quark");
}
#endif

static void ekg_connection_remove(struct ekg_connection *c) {
	if (g_input_stream_has_pending(G_INPUT_STREAM(c->instream))) {
		debug_warn("ekg_connection_remove(%x) input stream has pending!\n", c);
		g_input_stream_clear_pending(G_INPUT_STREAM(c->instream));
	}
#if 0 /* XXX */
	g_assert(!g_output_stream_has_pending(
				G_OUTPUT_STREAM(c->outstream)));
#endif

	connections = g_slist_remove(connections, c);

	g_string_free(c->wr_buffer, TRUE);
	g_object_unref(c->cancellable);
	g_object_unref(c->instream);
	g_object_unref(c->outstream);
	g_slice_free(struct ekg_connection, c);
}

static gint __conn_find_outstream(gconstpointer list_elem, gconstpointer comp_elem) {
	const struct ekg_connection *c = list_elem;

	return c->outstream == comp_elem ? 0 : -1;
}


static struct ekg_connection *get_connection_by_outstream(GDataOutputStream *s) {
	GSList *el;

	el = g_slist_find_custom(connections, s, __conn_find_outstream);
	return el ? el->data : NULL;
}

#if NEED_SLAVERY
static gint __conn_find_slaveless_conn(gconstpointer list_elem, gconstpointer comp_elem) {
	const struct ekg_connection *c = list_elem;

	return (!c->slave && c->conn == comp_elem) ? 0 : -1;
}

static struct ekg_connection *get_slave_connection_by_conn(GSocketConnection *c) {
	GSList *el;

	el = g_slist_find_custom(connections, c, __conn_find_slaveless_conn);
	return el ? el->data : NULL;
}
#endif


static void done_async_read(GObject *obj, GAsyncResult *res, gpointer user_data) {
	struct ekg_connection *c = user_data;
	GError *err = NULL;
	gssize rsize;
	GBufferedInputStream *instr = G_BUFFERED_INPUT_STREAM(obj);

	rsize = g_buffered_input_stream_fill_finish(instr, res, &err);

	if (rsize <= 0) {
		if (rsize == -1) /* error */
			debug_error("done_async_read(), read failed: %s\n", err ? err->message : NULL);
		else { /* EOF */
#if NEED_SLAVERY
			if (c->master) /* let the master handle it */
				return;
#endif
			debug_function("done_async_read(), EOF\n");
			if (g_buffered_input_stream_get_available(instr) > 0)
				c->callback(c->instream, c->priv_data);

			err = g_error_new_literal(
					EKG_CONNECTION_ERROR,
					EKG_CONNECTION_ERROR_EOF,
					"Connection terminated");
		}

		c->failure_callback(c->instream, err, c->priv_data);
		ekg_connection_remove(c);
		g_error_free(err);
		return;
	}

	debug_function("done_async_read(): read %d bytes\n", rsize);

	c->callback(c->instream, c->priv_data);
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
	GOutputStream *of = G_OUTPUT_STREAM(obj);

	ret = g_output_stream_flush_finish(of, res, &err);

	if (!ret) {
		debug_error("done_async_write(), write failed: %s\n", err ? err->message : NULL);
		/* XXX */
		failed_write(c);
		g_error_free(err);
		return;
	}

	if (c->wr_buffer->len > 0) {
		/* the stream should not have any pending writes ATM
		 * we need to ensure that to have the data written to stream
		 * rather than re-appended to the buffer*/
		g_assert(!g_output_stream_has_pending(of));

		ekg_connection_write_buf(G_DATA_OUTPUT_STREAM(of),
				c->wr_buffer->str, c->wr_buffer->len);
		g_string_truncate(c->wr_buffer, 0);
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
		ekg_input_callback_t callback,
		ekg_failure_callback_t failure_callback,
		gpointer priv_data)
{
	struct ekg_connection *c = g_slice_new(struct ekg_connection);
	GOutputStream *bout = g_buffered_output_stream_new(raw_outstream);

	c->conn = conn;
	c->instream = g_data_input_stream_new(raw_instream);
	c->outstream = g_data_output_stream_new(bout);
	c->cancellable = g_cancellable_new();
	c->wr_buffer = g_string_new("");

	c->callback = callback;
	c->failure_callback = failure_callback;
	c->priv_data = priv_data;

#if NEED_SLAVERY
	c->master = get_slave_connection_by_conn(conn);
	c->slave = NULL;

		/* be a good slave.. er, servant */
	if (G_UNLIKELY(c->master)) {
		struct ekg_connection *ci;
		c->master->slave = c;

		/* shift flush handlers (if set)
		 * this is required in order to be able to easily set flush
		 * handlers for future slaves */
		for (ci = c;
				ci->master && (ci->master->flush_handler != setup_async_write);
				ci = ci->master)
			ci->flush_handler = ci->master->flush_handler;
		ci->flush_handler = setup_async_write;
	} else
#endif
		c->flush_handler = setup_async_write;

		/* LF works fine for CRLF */
	g_data_input_stream_set_newline_type(c->instream, G_DATA_STREAM_NEWLINE_TYPE_LF);
		/* disallow any blocking writes */
	g_buffered_output_stream_set_auto_grow(G_BUFFERED_OUTPUT_STREAM(bout), TRUE);

	connections = g_slist_prepend(connections, c);
#if NEED_SLAVERY
	if (G_LIKELY(!c->master))
#endif
		setup_async_read(c);

	return c->outstream;
}

void ekg_disconnect_by_outstream(GDataOutputStream *f) {
	struct ekg_connection *c = get_connection_by_outstream(f);

	if (!c) {
		debug_warn("ekg_disconnect_by_outstream() - connection not found\n");
		return;
	}

	debug_function("ekg_disconnect_by_outstream(%x)\n",c);

	ekg_connection_remove(c);
}

void ekg_connection_write_buf(GDataOutputStream *f, gconstpointer buf, gsize len) {
	struct ekg_connection *c = get_connection_by_outstream(f);
	GError *err = NULL;
	gssize out;
	GOutputStream *of = G_OUTPUT_STREAM(f);

	/* we can't write to buffer if it has pending ops;
	 * yes, it is stupid. */
	if (g_output_stream_has_pending(of)) {
		c->wr_buffer = g_string_append_len(c->wr_buffer, buf, len);
		return;
	}

	out = g_output_stream_write(of, buf, len, NULL, &err);
	if (out != len) {
		debug_error("ekg_connection_write_string() failed (wrote %d out of %d): %s\n",
				out, len, err ? err->message : "(no error?!)");
		failed_write(c);
		g_error_free(err);

		return;
	}

	debug_function("ekg_connection_write_buf(), wrote %d bytes\n", out);

	c->flush_handler(c);
}

void ekg_connection_write(GDataOutputStream *f, const gchar *format, ...) {
	static GString *buf = NULL;
	va_list args;

	if (G_LIKELY(format)) {
		if (!buf)
			buf = g_string_sized_new(120);

		va_start(args, format);
		g_string_vprintf(buf, format, args);
		va_end(args);

		ekg_connection_write_buf(f, buf->str, buf->len);
	} else {
		struct ekg_connection *c = get_connection_by_outstream(f);
		c->flush_handler(c);
	}
}

struct ekg_connection_starter {
	GCancellable *cancellable;

	gchar *bind_hostname;

	gchar *service;
	gchar *domain;

	gchar **servers;
	gchar **current_server;
	guint16 defport;

	gboolean use_tls;

	ekg_connection_callback_t callback;
	ekg_connection_failure_callback_t failure_callback;
	gpointer priv_data;
};

static void failed_async_connect(
		GSocketClient *sock,
		GError *err,
		struct ekg_connection_starter *cs)
{
	debug_error("done_async_connect(), connect failed: %s\n",
			err ? err->message : "(reason unknown)");
	if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED) || !setup_async_connect(sock, cs)) {
		cs->failure_callback(err, cs->priv_data);
		ekg_connection_starter_free(cs);
		g_object_unref(sock);
	}
}

static void succeeded_async_connect(
		GSocketClient *sock,
		GSocketConnection *conn,
		struct ekg_connection_starter *cs,
		GInputStream *instream,
		GOutputStream *outstream)
{
	cs->callback(conn, instream, outstream, cs->priv_data);
	ekg_connection_starter_free(cs);
	g_object_unref(sock);
}

static void done_async_connect(GObject *obj, GAsyncResult *res, gpointer user_data) {
	GSocketClient *sock = G_SOCKET_CLIENT(obj);
	struct ekg_connection_starter *cs = user_data;
	GSocketConnection *conn;
	GError *err = NULL;
	
	conn = g_socket_client_connect_finish(sock, res, &err);
	if (conn) {
#ifdef HAVE_LIBGNUTLS
		if (cs->use_tls) {
			ekg_gnutls_new_session(sock, conn, cs);
		} else
#endif
		{
			GIOStream *cio = G_IO_STREAM(conn);
			succeeded_async_connect(
					sock, conn, cs,
					g_io_stream_get_input_stream(cio),
					g_io_stream_get_output_stream(cio));
		}
	} else {
		failed_async_connect(sock, err, cs);
		g_error_free(err);
	}
}

static gboolean setup_async_connect(GSocketClient *sock, struct ekg_connection_starter *cs) {
	if (*(cs->current_server)) {
		debug_function("setup_async_connect(), trying %s (defport: %d)\n",
				*(cs->current_server), cs->defport);
		g_socket_client_connect_to_host_async(
				sock, *(cs->current_server), cs->defport,
				cs->cancellable,
				done_async_connect,
				cs);
		cs->current_server++;
		return TRUE;
	} else
		return FALSE;
}

ekg_connection_starter_t ekg_connection_starter_new(guint16 defport) {
	struct ekg_connection_starter *cs = g_slice_new0(struct ekg_connection_starter);

	cs->defport = defport;

	return cs;
}

void ekg_connection_starter_free(ekg_connection_starter_t cs) {
	g_free(cs->bind_hostname);
	g_free(cs->service);
	g_free(cs->domain);
	g_strfreev(cs->servers);
	g_object_unref(cs->cancellable);
	g_slice_free(struct ekg_connection_starter, cs);
}

void ekg_connection_starter_bind(
		ekg_connection_starter_t cs,
		const gchar *hostname)
{
	g_free(cs->bind_hostname);
	cs->bind_hostname = g_strdup(hostname);
}

void ekg_connection_starter_set_srv_resolver(
		ekg_connection_starter_t cs,
		const gchar *service,
		const gchar *domain)
{
	g_free(cs->service);
	g_free(cs->domain);
	cs->service = g_strdup(service);
	cs->domain = g_strdup(domain);
}

void ekg_connection_starter_set_servers(
		ekg_connection_starter_t cs,
		const gchar *servers)
{
	g_strfreev(cs->servers);
	cs->servers = g_strsplit(servers, ",", 0);
}

void ekg_connection_starter_set_use_tls(
		ekg_connection_starter_t cs,
		gboolean use_tls) /* XXX */
{
	cs->use_tls = use_tls;
}

GCancellable *ekg_connection_starter_run(
		ekg_connection_starter_t cs,
		GSocketClient *sock,
		ekg_connection_callback_t callback,
		ekg_connection_failure_callback_t failure_callback,
		gpointer priv_data)
{
	cs->callback = callback;
	cs->failure_callback = failure_callback;
	cs->priv_data = priv_data;

	cs->cancellable = g_cancellable_new();
	cs->current_server = cs->servers;

	if (cs->bind_hostname) {
		GResolver *res = g_resolver_get_default();
		GList *addrs;
		GError *err = NULL;

		addrs = g_resolver_lookup_by_name(
				res,
				cs->bind_hostname,
				NULL,
				&err);

		if (!addrs) {
				/* XXX: delay calling that */
			failed_async_connect(sock, err, cs);
			g_error_free(err);
			return cs->cancellable;
		}

		g_socket_client_set_local_address(sock,
				g_inet_socket_address_new(
					G_INET_ADDRESS(g_list_nth_data(addrs, 0)), 0));

		g_resolver_free_addresses(addrs);
		g_object_unref(res);
	}

		/* if we have the domain name, try SRV lookup first */
	if (cs->domain) {
		g_assert(cs->service);

			/* fallback to domainname lookup if 'servers' not set */
		if (!cs->servers || !cs->servers[0])
			ekg_connection_starter_set_servers(cs, cs->domain);

		debug_function("ekg_connection_start(), trying _%s._tcp.%s\n",
				cs->service, cs->domain);
		g_socket_client_connect_to_service_async(
				sock, cs->domain, cs->service,
				cs->cancellable,
				done_async_connect,
				cs);
	} else /* otherwise, just begin with servers */
		g_assert(setup_async_connect(sock, cs));

	return cs->cancellable;
}

#ifdef HAVE_LIBGNUTLS
struct ekg_gnutls_connection {
	struct ekg_connection *connection;
	GMemoryInputStream *instream;
	GMemoryOutputStream *outstream;

	GError *connection_error;
	gnutls_session_t session;
	gnutls_certificate_credentials_t cred;
};

struct ekg_gnutls_connection_starter {
	struct ekg_connection_starter *parent;
	struct ekg_gnutls_connection *conn;
	GSocketClient *sockclient;
};

static void ekg_gnutls_free_connection(struct ekg_gnutls_connection *conn) {
	gnutls_deinit(conn->session);
	gnutls_certificate_free_credentials(conn->cred);
	g_slice_free(struct ekg_gnutls_connection, conn);
}

static void ekg_gnutls_free_connection_starter(struct ekg_gnutls_connection_starter *gcs) {
	g_slice_free(struct ekg_gnutls_connection_starter, gcs);
}

static gssize ekg_gnutls_pull(gnutls_transport_ptr_t connptr, gpointer buf, gsize len) {
	struct ekg_gnutls_connection *conn = connptr;
	GBufferedInputStream *s = G_BUFFERED_INPUT_STREAM(conn->connection->instream);
	gsize avail_bytes = g_buffered_input_stream_get_available(s);

	/* XXX: EOF? */

	g_assert(len > 0);

	if (avail_bytes == 0) {
		if (conn->connection_error)
			return 0; /* EOF */

		gnutls_transport_set_errno(conn->session, EAGAIN);
		return -1;
	} else {
		GError *err = NULL;
		gssize ret = g_input_stream_read(
				G_INPUT_STREAM(s),
				buf,
				MIN(avail_bytes, len),
				NULL,
				&err);
		
		if (ret == -1) {
			debug_error("ekg_gnutls_pull() failed: %s\n", err->message);
			g_error_free(err);
		}

		return ret;
	}

	g_assert_not_reached();
}

static gssize ekg_gnutls_push(gnutls_transport_ptr_t connptr, gconstpointer buf, gsize len) {
	struct ekg_gnutls_connection *conn = connptr;

	g_assert(len > 0);

		/* XXX: handle failures better? */
	ekg_connection_write_buf(conn->connection->outstream, buf, len);
	return len;
}

static void ekg_gnutls_handle_data_failure(GDataInputStream *s, GError *err, gpointer data) {
	struct ekg_gnutls_connection *gc = data;

	gc->connection_error = g_error_copy(err);
}

static void ekg_gnutls_handle_data(GDataInputStream *s, gpointer data) {
	struct ekg_gnutls_connection *gc = data;
	ssize_t ret;
	char buf[4096];

	g_assert(gc->connection->slave);

	do {
		ret = gnutls_record_recv(gc->session, buf, sizeof(buf));

		if (ret > 0)
			g_memory_input_stream_add_data(
					gc->instream,
					g_memdup(buf, ret),
					ret,
					g_free);
		else if (ret != GNUTLS_E_INTERRUPTED && ret != GNUTLS_E_AGAIN) {
			GError *err;
			
			if (ret != 0)
				err = g_error_new_literal(EKG_GNUTLS_ERROR,
						ret, gnutls_strerror(ret));
			else {
				debug_function("ekg_gnutls_handle_data(), got EOF from gnutls\n");
				err = g_error_new_literal(
						EKG_CONNECTION_ERROR,
						EKG_CONNECTION_ERROR_EOF,
						"Connection terminated");
			}

			gc->connection->slave->failure_callback(
					gc->connection->slave->instream,
					err,
					gc->connection->slave->priv_data);
			g_error_free(err);
			ekg_connection_remove(gc->connection->slave);
			ekg_connection_remove(gc->connection);
			return;
		}
	} while (ret > 0 || ret == GNUTLS_E_INTERRUPTED);

		/* not necessarily async but be lazy */
	if (!g_input_stream_has_pending(G_INPUT_STREAM(gc->connection->slave->instream)))
		setup_async_read(gc->connection->slave);
}

static void ekg_gnutls_flush(struct ekg_connection *c) {
	struct ekg_gnutls_connection *gc = c->master->priv_data;
	GMemoryOutputStream *dos = gc->outstream;
	ssize_t ret;
	gconstpointer bufp;
	gsize bufleft;

		/* GMemoryOutputStream is not supposed to fail */
	g_assert(g_output_stream_flush(
			G_OUTPUT_STREAM(c->outstream),
			NULL,
			NULL));

		/* now pass the written data to gnutls */
	bufp = g_memory_output_stream_get_data(dos);
	bufleft = g_memory_output_stream_get_data_size(dos);
	do {
		ret = gnutls_record_send(gc->session, bufp, bufleft);

		if (ret > 0) {
			g_assert(ret <= bufleft);
			bufp += ret;
			bufleft -= ret;
		} else if (ret != GNUTLS_E_INTERRUPTED && ret != GNUTLS_E_AGAIN) {
			debug_error("gnutls_flush(), write failed: %s\n",
					gnutls_strerror(ret));
			/* XXX */
			failed_write(c);
		}
	} while (bufleft > 0);

	{
		GSeekable *sos = G_SEEKABLE(dos);
		g_assert(g_seekable_seek(sos, 0, G_SEEK_SET, NULL, NULL));
		g_assert(g_seekable_truncate(sos, 0, NULL, NULL));
	}
}

static void ekg_gnutls_handle_handshake_failure(GDataInputStream *s, GError *err, gpointer data) {
	struct ekg_gnutls_connection_starter *gcs = data;

	failed_async_connect(gcs->sockclient, err, gcs->parent);
	ekg_connection_remove(gcs->conn->connection);
	ekg_gnutls_free_connection(gcs->conn);
	ekg_gnutls_free_connection_starter(gcs);
}

static void ekg_gnutls_async_handshake(struct ekg_gnutls_connection_starter *gcs) {
	gint ret = gnutls_handshake(gcs->conn->session);

	switch (ret) {
		case GNUTLS_E_SUCCESS:
			{
				struct ekg_gnutls_connection *gc = gcs->conn;
				struct ekg_connection_starter *cs = gcs->parent;

				GInputStream *mi = g_memory_input_stream_new();
				GOutputStream *mo = g_memory_output_stream_new(
						NULL, 0, g_realloc, g_free);

					/* set streams */
				gc->instream = G_MEMORY_INPUT_STREAM(mi);
				gc->outstream = G_MEMORY_OUTPUT_STREAM(mo);

					/* switch handlers */
				gc->connection->callback = ekg_gnutls_handle_data;
				gc->connection->failure_callback = ekg_gnutls_handle_data_failure;
				gc->connection->priv_data = gc;
				gc->connection->flush_handler = ekg_gnutls_flush;

					/* this cleans up the socket, and cs */
				succeeded_async_connect(gcs->sockclient, gc->connection->conn,
						cs, mi, mo);
					/* and this cleans up gcs */
				ekg_gnutls_free_connection_starter(gcs);
			}
			break;
		case GNUTLS_E_AGAIN:
		case GNUTLS_E_INTERRUPTED:
			break;
		default:
			{
				GError *err = g_error_new_literal(EKG_GNUTLS_ERROR,
						ret, gnutls_strerror(ret));
				ekg_gnutls_handle_handshake_failure(NULL, err, gcs);
				g_error_free(err);
			}
	}
}

static void ekg_gnutls_handle_handshake_input(GDataInputStream *s, gpointer data) {
	struct ekg_gnutls_connection_starter *gcs = data;

	ekg_gnutls_async_handshake(gcs);
}

static void ekg_gnutls_new_session(
		GSocketClient *sockclient,
		GSocketConnection *sock,
		struct ekg_connection_starter *cs)
{
	gnutls_session_t s;
	gnutls_certificate_credentials_t cred;
	struct ekg_gnutls_connection *conn = g_slice_new(struct ekg_gnutls_connection);
	struct ekg_gnutls_connection_starter *gcs = g_slice_new(struct ekg_gnutls_connection_starter);

	g_assert(!gnutls_certificate_allocate_credentials(&cred));
	g_assert(!gnutls_init(&s, GNUTLS_CLIENT));
	g_assert(!gnutls_priority_set_direct(s, "PERFORMANCE", NULL)); /* XXX */
	g_assert(!gnutls_credentials_set(s, GNUTLS_CRD_CERTIFICATE, cred));

	gnutls_transport_set_pull_function(s, ekg_gnutls_pull);
	gnutls_transport_set_push_function(s, ekg_gnutls_push);
	gnutls_transport_set_ptr(s, conn);

	gcs->parent = cs;
	gcs->conn = conn;
	gcs->sockclient = sockclient;

	conn->session = s;
	conn->cred = cred;
	conn->connection_error = NULL;
	conn->connection = get_connection_by_outstream(
			ekg_connection_add(
				sock,
				g_io_stream_get_input_stream(G_IO_STREAM(sock)),
				g_io_stream_get_output_stream(G_IO_STREAM(sock)),
				ekg_gnutls_handle_handshake_input,
				ekg_gnutls_handle_handshake_failure,
				gcs)
			);
	g_assert(conn->connection);
	ekg_gnutls_async_handshake(gcs);
}

static void ekg_gnutls_log(gint level, const char *msg) {
	debug_ext(DEBUG_GGMISC, "[gnutls:%d] %s", level, msg);
}
#endif

void ekg_tls_init(void) {
#ifdef HAVE_LIBGNUTLS
	g_assert(!gnutls_global_init()); /* XXX: error handling */

	gnutls_global_set_log_function(ekg_gnutls_log);
	gnutls_global_set_log_level(3);
#endif
}

void ekg_tls_deinit(void) {
#ifdef HAVE_LIBGNUTLS
	gnutls_global_deinit();
#endif
}
