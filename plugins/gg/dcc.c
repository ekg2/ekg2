/* $Id$ */

#include "ekg2-config.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ekg/audio.h>
#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/protocol.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include <libgadu.h>

#include "dcc.h"
#include "gg.h"

int audiofds[2] = { -1, -1};

AUDIO_DEFINE(gg_dcc);

struct gg_dcc *gg_dcc_socket = NULL;

static int dcc_limit_time = 0;	/* time from the first connection */
static int dcc_limit_count = 0; /* how many connections from the last time */

/* vars */
int gg_config_audio;
int gg_config_dcc;
int gg_config_dcc_port;
char *gg_config_dcc_ip;
char *gg_config_dcc_limit;

typedef struct {
	dcc_t *dcc;
	int len;
} gg_audio_private_t;

/* some audio stuff */
AUDIO_CONTROL(gg_dcc_audio_control) {
	va_list ap;
	if (type == AUDIO_CONTROL_INIT) {
		va_start(ap, aio);

		va_end(ap);
		return (void *) 1;
	} else if ((type == AUDIO_CONTROL_SET && !aio) || (type == AUDIO_CONTROL_GET && aio)) {
		gg_audio_private_t *priv;
		char *attr;
		int dccid = -1;
		int dccfd = -1;
		int len = GG_DCC_VOICE_FRAME_LENGTH_505;

		if (type == AUDIO_CONTROL_GET)	priv = aio->priv_data;
		else				priv= xmalloc(sizeof(gg_audio_private_t));

		va_start(ap, aio);

		while ((attr = va_arg(ap, char *))) {
			if (type == AUDIO_CONTROL_GET) {
				char **val = va_arg(ap, char **);
				debug("[gg_dcc_audio_control AUDIO_CONTROL_GET] attr: %s value: 0x%x\n", attr, val);

				if (!xstrcmp(attr, "format"))		*val = xstrdup("gsm");
				else					*val = NULL;
			} else {
				char *val = va_arg(ap, char *);
				debug("[gg_dcc_audio_control AUDIO_CONTROL_SET] attr: %s value: %s\n", attr, val);

				if (!xstrcmp(attr, "dccid"))		dccid = atoi(val);
				else if (!xstrcmp(attr, "fd"))		dccfd = atoi(val);
				else if (!xstrcmp(attr, "len"))		len = atoi(val);
			}
		}
		va_end(ap);
		{ 
			dcc_t *d;
			for (d = dccs; d; d = d->next) {
				if (d->id == dccid) {
					priv->dcc = d;
					break;
				}
			}
		}
		if (!priv->dcc) { xfree(priv); return NULL; }
		priv->len = len;

		aio		= xmalloc(sizeof(audio_io_t));
		aio->a		= &gg_dcc_audio;
		aio->priv_data	= priv;
		aio->fd		= dccfd;
	} else if (type == AUDIO_CONTROL_HELP) {
		return NULL;
	}
	return aio;
} 

WATCHER_AUDIO(gg_dcc_audio_read) {
	char sbuf[GG_DCC_VOICE_FRAME_LENGTH_505];
	int len;

	if (type) return -1;
	
	len = read(fd, sbuf, sizeof(buf));
	sbuf[len] = 0;

	if (len > 0) {
		if (len == GG_DCC_VOICE_FRAME_LENGTH_505)
			string_append_raw(buf, sbuf+1, len-1);
		else	string_append_raw(buf, sbuf, len);
	}

	debug("gg_dcc_audio_read() %d\n", len);
	return len;
}

WATCHER_AUDIO(gg_dcc_audio_write) {
	gg_audio_private_t *priv = data;
	int len = -1; 
	int rlen;

	char output[GG_DCC_VOICE_FRAME_LENGTH_505];

//	debug("gg_dcc_audio_write type: %d\n", type);
	if (type) return 0;

	if (!dccs || !priv->dcc) {
		debug("gg_dcc_audio_write DCC NOT FOUND\n");
		return -1;
	}

	if (!priv->dcc->active) return buf->len;

	if (priv->len == GG_DCC_VOICE_FRAME_LENGTH_505) rlen = GG_DCC_VOICE_FRAME_LENGTH_505 - 1;
	else						rlen = priv->len;

	if (buf->len < rlen) return 0;

	if (priv->len == GG_DCC_VOICE_FRAME_LENGTH_505) {
		output[0] = 0;
		memcpy(&(output[1]), buf->str, rlen);
	} else	memcpy(&(output[0]), buf->str, rlen);

	if (!(gg_dcc_voice_send(priv->dcc->priv, output, priv->len)))
		len = rlen;

	return len;
}

/* 
 * gg_changed_dcc()
 *
 * called when some dcc_* variables are changed
 */

/* khem, var jest gg:*.... byl kod ktory nie dzialal... tak?  */
void gg_changed_dcc(const char *var)
{
	if (!xstrcmp(var, ("gg:dcc"))) {
		if (!gg_config_dcc) {
			gg_dcc_socket_close();
			gg_dcc_ip = 0;
			gg_dcc_port = 0;
		}
	
		if (gg_config_dcc) {
			if (gg_dcc_socket_open(gg_config_dcc_port) == -1)
				print("dcc_create_error", strerror(errno));
		}
	} else if (!xstrcmp(var, ("gg:dcc_ip"))) {
		if (gg_config_dcc_ip) {
			if (!xstrcasecmp(gg_config_dcc_ip, "auto")) {
				gg_dcc_ip = inet_addr("255.255.255.255");
			} else {
				if (inet_addr(gg_config_dcc_ip) != INADDR_NONE) 
					gg_dcc_ip = inet_addr(gg_config_dcc_ip);
				else {
					print("dcc_invalid_ip");
					gg_config_dcc_ip = NULL;
					gg_dcc_ip = 0;
				}
			}
		} else
			gg_dcc_ip = 0;
	} else if (!xstrcmp(var, ("gg:dcc_port"))) {
		if (gg_config_dcc && gg_config_dcc_port) {
			gg_dcc_socket_close();
			gg_dcc_ip = 0;
			gg_dcc_port = 0;

			if (gg_dcc_socket_open(gg_config_dcc_port) == -1)
				print("dcc_create_error", strerror(errno));
	
		}
	} else if (!xstrcmp(var, ("gg:audio"))) {
		if (gg_config_audio && (!audio_find("oss") || !codec_find("gsm"))) {
			gg_config_audio = 0;
			debug("[gg_config_audio] failed to set gg:audio to 1 cause not found oss audio or gsm codec...\n");
			return;
		} else if (gg_config_audio)	gg_dcc_audio_init();
		else				gg_dcc_audio_close();
	}

	if (!in_autoexec)
		print("config_must_reconnect");
}

#ifdef HAVE_GG_DCC7

static TIMER(gg_dcc_handler_timeout) {
	struct gg_common *gd = (struct gg_common *) data;

	if (type) {
		return 0;
	}

	debug("gg_dcc_handler_timeout() type: %d state: %d\n", gd->type, gd->state);

	if (gd->type == GG_SESSION_DCC7_GET) {
		if (gd->fd != -1) {
			debug("gg_dcc_handler_timeout() gd->fd: %d\n", gd->fd);
			/* great! */


			watch_add(&gg_plugin, gd->fd, gd->check, gg_dcc7_handler, gd);	/* add watch */
			return -1;							/* destroy timer */
		}


	} else {
		if (gd->state == GG_STATE_REQUESTING_ID) {	/* timeoutnelo na zadaniu id */
			/* here destroy dcc */
			return -1;
		}

		if (gd->state == GG_STATE_WAITING_FOR_ACCEPT)	/* timeoutnelo na oczekiwnaiu na uzytkownika */
			return 0;

	}

	return 0;
}

#endif

COMMAND(gg_command_dcc)
{
	uin_t uin = atoi(session->uid + 3);
	gg_private_t *g = session_private_get(session);

	/* send, rsend */
	if (params[0] && (!xstrncasecmp(params[0], "se", 2) || !xstrncasecmp(params[0], "rse", 3))) {
		struct stat st;
		userlist_t *u;
		gg_userlist_private_t *up;
		dcc_t *d;
		int fd;
		int __ip, __port;
		const char *fn;

		void *dccdata = NULL;

		if (!params[1] || !params[2]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!(fn = prepare_path_user(params[2]))) {
			printq("generic_error", "path too long"); /* XXX? */
			return -1;
		}

		if (!(u = userlist_find(session, get_uid(session, params[1])))) {
			printq("user_not_found", params[1]);
			return -1;
		}
		up = gg_userlist_priv_get(u);

		__ip = user_private_item_get_int(u, "ip");
		__port = user_private_item_get_int(u, "port");

		if (!session_connected_get(session)) {
			printq("not_connected", session_name(session));
			return -1;
		}

		if (u->status == EKG_STATUS_NA) {
			printq("dcc_user_not_avail", format_user(session, u->uid));
			return -1;
		}

		if (!__ip) {
			printq("dcc_user_aint_dcc", format_user(session, u->uid));
			return -1;
		}
		
		if (!stat(fn, &st) && !S_ISREG(st.st_mode)) {
			printq("io_nonfile", params[2]);
			return -1;
		}

		if ((fd = open(fn, O_RDONLY|O_NONBLOCK)) == -1) {
			if (errno == ENXIO)
				printq("io_nonfile", params[2]);
			else
				printq("io_cantopen", params[2], strerror(errno));
			return -1;
		}

		close(fd);

#ifdef HAVE_GG_DCC7
		if (up->protocol >= 0x2a) {
			struct gg_dcc7 *gd;

			if (!(gd = gg_dcc7_send_file(g->sess, atoi(u->uid + 3), fn, fn, NULL))) {
				printq("dcc_error", strerror(errno));
				return -1;
			}
			dccdata = gd;
		} else 
#endif
		{
			if (__port < 10 || !xstrncasecmp(params[0], "rse", 3)) {
				/* nie mo¿emy siê z nim po³±czyæ, wiêc on spróbuje */
				gg_dcc_request(g->sess, atoi(u->uid + 3));
			} else {
				struct gg_dcc *gd;

				if (!(gd = gg_dcc_send_file(__ip, __port, uin, atoi(u->uid + 3)))) {
					printq("dcc_error", strerror(errno));
					return -1;
				}

				if (gg_dcc_fill_file_info(gd, fn) == -1) {
					printq("dcc_open_error", params[2], strerror(errno));
					gg_free_dcc(gd);
					return -1;
				}

				dccdata = gd;
			}
		}
		
		d = dcc_add(session, u->uid, DCC_SEND, dccdata);
		dcc_filename_set(d, fn);
		dcc_size_set(d, st.st_size);

		if (dccdata) {
			struct gg_common *gd = (struct gg_common *) dccdata;

#ifdef HAVE_GG_DCC7
			if (gd->fd != -1)
				watch_add(&gg_plugin, gd->fd, gd->check, gg_dcc_handler, gd);
			else {
				debug("[GG,DCC,SEND] Adding timer [timeout: %d] instead of watch (w->fd: %d) (w->check: %d) (gd: %p)\n", gd->timeout,
					gd->fd, gd->check, gd);
				timer_add(&gg_plugin, NULL, gd->timeout, 1, gg_dcc_handler_timeout, gd);
			}
#endif
			watch_add(&gg_plugin, gd->fd, gd->check, gg_dcc_handler, gd);
		}

		return 0;
	}

	if (params[0] && (params[0][0] == 'v' || !xstrncasecmp(params[0], "rvo", 3))) {			/* voice, rvoice */
		struct gg_dcc *gd = NULL;
		dcc_t *d;
		userlist_t *u;
		gg_userlist_private_t *up;
		int __ip, __port;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!gg_config_audio) {
			printq("dcc_voice_unsupported");
			return -1;
		}

		if (!(u = userlist_find(session, get_uid(session, params[1])))) {
			printq("user_not_found", params[1]);
			return -1;
		}
		up = gg_userlist_priv_get(u);

		if (!session_connected_get(session)) {
			printq("not_connected", session_name(session));
			return -1;
		}

		if (u->status == EKG_STATUS_NA) {
			printq("dcc_user_not_avail", format_user(session, u->uid));
			return -1;
		}

		__ip = user_private_item_get_int(u, "ip");

		if (!__ip) {
			printq("dcc_user_aint_dcc", format_user(session, u->uid));
			return -1;
		}
		
		__port = user_private_item_get_int(u, "port");

#if 0
		list_t l;
		struct transfer *t, tt;
		/* sprawdzamy najpierw przychodz±ce po³±czenia */
		
		for (t = NULL, l = transfers; l; l = l->next) {
			struct transfer *f = l->data;
			struct userlist *u;
			
			f = l->data;

			if (!f->dcc || !f->dcc->incoming || f->type != GG_SESSION_DCC_VOICE)
				continue;
			
			if (params[1][0] == '#' && atoi(params[1] + 1) == f->id) {
				t = f;
				break;
			}

			if (t && (u = userlist_find(t->uin, NULL))) {
				if (!xstrcasecmp(params[1], itoa(u->uin)) || (u->display && !xstrcasecmp(params[1], u->display))) {
					t = f;
					break;
				}
			}
		}

		if (t) {
			if ((u = userlist_find(t->uin, NULL)))
				t->protocol = u->protocol;

			list_add(&watches, t->dcc, 0);
			voice_open();
			return 0;
		}
#endif
		if (audiofds[0] != -1 || audiofds[1] != -1) {
			printq("dcc_voice_running");
			return 0;
		}

		if (__port < 10 || !xstrncasecmp(params[0], "rvo", 3)) {
			/* nie mo¿emy siê z nim po³±czyæ, wiêc on spróbuje */
			gg_dcc_request(g->sess, atoi(u->uid + 3));
		} else {
			if (!(gd = gg_dcc_voice_chat(__ip, __port, uin, atoi(u->uid + 3)))) {
				printq("dcc_error", strerror(errno));
				return -1;
			}
		}
		if (!(d = dcc_add(session, u->uid, DCC_VOICE, gd))) return -1;

		if (gd)
			watch_add(&gg_plugin, gd->fd, gd->check, gg_dcc_handler, gd);

		pipe(audiofds);
		stream_create("Gygy audio OUTPUT",
				__AINIT_F("oss", AUDIO_READ, "freq", "8000", "sample", "16", "channels", "1"),
				__CINIT_F("gsm", "with-ms", "1"),
				__AINIT_F("gg_dcc", AUDIO_WRITE, "dccuid", u->uid, 
					"len", (up->protocol >= 0x1b) ? itoa(GG_DCC_VOICE_FRAME_LENGTH_505) : itoa(GG_DCC_VOICE_FRAME_LENGTH),
					"dccid", itoa(d->id) /*, "fd", itoa(audiofds[1]) */ ));
		stream_create("Gygy audio INPUT",
				__AINIT_F("gg_dcc", AUDIO_READ, "dccid", itoa(d->id), "uid", u->uid, "fd", itoa(audiofds[0])), 
				__CINIT_F("gsm", "with-ms", "1"),
				__AINIT_F("oss", AUDIO_WRITE, "freq", "8000", "sample", "16", "channels", "1"));
		return 0;
	}
	
	/* get, resume */
	if (params[0] && (!xstrncasecmp(params[0], "g", 1) || !xstrncasecmp(params[0], "re", 2))) {
		dcc_t *d = NULL, *D;
		struct gg_common *g;
		char *path;

		int fd;
		unsigned int offset = 0;
		
		for (D = dccs; D; D = D->next) {
			userlist_t *u;

			if (!dcc_private_get(D) || !dcc_filename_get(D) || dcc_type_get(D) != DCC_GET)
				continue;
			
			if (!params[1]) {
				if (dcc_active_get(D))
					continue;
				d = D;
				break;
			}

			if (params[1][0] == '#' && xstrlen(params[1]) > 1 && atoi(params[1] + 1) == dcc_id_get(D)) {
				d = D;
				break;
			}

			if ((u = userlist_find(session, dcc_uid_get(D)))) {
				if (!xstrcasecmp(params[1], u->uid) || (u->nickname && !xstrcasecmp(params[1], u->nickname))) {
					d = D;
					break;
				}
			}
		}

		if (!d || !(g = dcc_private_get(d))) {
			printq("dcc_not_found", (params[1]) ? params[1] : "");
			return -1;
		}

		if (dcc_active_get(d)) {
			printq("dcc_receiving_already", dcc_filename_get(d), format_user(session, dcc_uid_get(d)));
			return -1;
		}

		if (xstrncmp(d->uid, "gg:", 3)) {
			debug_error("%s:%d /dcc command, incorrect `%s`!\n", __FILE__, __LINE__, __(d->uid));
			printq("generic_error", "Use /dcc on correct session, sorry");
			return -1;
		}

		if (config_dcc_dir) 
			path = saprintf("%s/%s", config_dcc_dir, dcc_filename_get(d));
		else
			path = xstrdup(dcc_filename_get(d));

		if (params[0][0] == 'r') {
			fd = open(path, O_WRONLY);
			if (fd != -1)
				offset = lseek(fd, 0, SEEK_END);
		} else
			fd = open(path, O_WRONLY | O_CREAT, 0600);

		if (fd == -1) {
			printq("dcc_get_cant_create", path);
#ifdef HAVE_GG_DCC7
	/* XXX, use damn close_handler! and call only dcc_close() */
			if (g->type == GG_SESSION_DCC7_GET)	gg_dcc7_free((struct gg_dcc7 *) g);
			else
#endif
				gg_free_dcc((struct gg_dcc *) g);

			dcc_close(d);
			xfree(path);
			
			return -1;
		}
		
		xfree(path);
		
		printq("dcc_get_getting", format_user(session, dcc_uid_get(d)), dcc_filename_get(d));
		dcc_active_set(d, 1);

#ifdef HAVE_GG_DCC7
		if (g->type == GG_SESSION_DCC7_GET) {
			struct gg_dcc7 *gd = (struct gg_dcc7 *) g;

			gd->file_fd = fd;
			gd->offset = offset;

				/* hack, active waiting */
			timer_add(&gg_plugin, NULL, 1 /* gd->timeout */, 1, gg_dcc_handler_timeout, gd);

			gg_dcc7_accept(gd, offset);		/* accept */
		} else 
#endif
		{
			struct gg_dcc *gd = (struct gg_dcc *) g;

			gd->file_fd = fd;
			gd->offset = offset;
			watch_add(&gg_plugin, gd->fd, gd->check, gg_dcc_handler, gd);
		}

		return 0;
	}
	return cmd_dcc(name, params, session, target, quiet);
}

#ifdef HAVE_GG_DCC7

static void gg_dcc7_close_handler(dcc_t *d)
{
	struct gg_dcc7 *g;

	if (!d || !(g = d->priv))
		return;
	
	if (d->type == DCC_VOICE) {
		close(audiofds[0]);
		close(audiofds[1]);
		audiofds[0] = audiofds[1] = -1;
	}
	gg_dcc7_free(g);
}

#endif

/*
 * gg_dcc_find()
 *
 * szuka dcc_t zawieraj±cy dan± struct gg_dcc.
 */
dcc_t *gg_dcc_find(void *d)
{
	dcc_t *D;

	for (D = dccs; D; D = D->next) {
		if (D && D->priv == d)
			return D;
	}

	return NULL;
}

#ifdef HAVE_GG_DCC7

WATCHER(gg_dcc7_handler)
{
	dcc_t *D;
	struct gg_event *e;
	struct gg_dcc7 *d = data;
	int again = 1;

	if (type)
		return 0;

	D = gg_dcc_find(d);

	if (!(e = gg_dcc7_watch_fd(d))) {
		print("dcc_error", strerror(errno));
		if (d->type != GG_SESSION_DCC7_SOCKET) {
			
			if (!D) {
				gg_dcc7_free(d);
			} else {
				gg_dcc7_close_handler(D);
				dcc_close(D);
			}

			return -1;
		}
	}

	switch (e->type) {
		case GG_EVENT_DCC7_DONE:
			debug_function("## GG_EVENT_DCC7_DONE\n");
			again = 0;

			if (!D) {
				gg_dcc7_free(d);
				break;
			}

			print((D->type == DCC_SEND) ? "dcc_done_send" : "dcc_done_get", format_user(D->session, D->uid), D->filename);
			
			gg_dcc7_close_handler(D);
			dcc_close(D);
			break;
			
		case GG_EVENT_DCC7_ERROR:
		{
			struct in_addr addr;
			unsigned short port = d->remote_port;
			char *tmp;

			again = 0;

			if (!D) {
				gg_dcc7_free(d);
				break;
			}

			addr.s_addr = d->remote_addr;

#if 0
			if (d->peer_uin) {
				struct userlist *u = userlist_find(D->session, D->uid);
				if (!addr.s_addr && u) {
					addr.s_addr = u->ip.s_addr;
					port = u->port;
				}
				tmp = saprintf("%s (%s:%d)", format_user(D->session, D->uid), inet_ntoa(addr), port);
			} else 
				tmp = saprintf("%s:%d", inet_ntoa(addr), port);
#endif
			tmp = saprintf("%s (%s:%d)", format_user(D->session, D->uid), inet_ntoa(addr), port);

			switch (e->event.dcc7_error) {
				case GG_ERROR_DCC7_HANDSHAKE:
					print("dcc_error_handshake", tmp);
					break;
				case GG_ERROR_DCC7_NET:
					print("dcc_error_network", tmp);
					break;
				case GG_ERROR_DCC7_REFUSED:
					print("dcc_error_refused", tmp);
					break;
				default:
					print("dcc_error_unknown", tmp);
			}

			xfree(tmp);

			gg_dcc7_close_handler(D);
			dcc_close(D);

			break;
		}
	}

	if (d /* && d->type != GG_SESSION_DCC_SOCKET */ && again) {
		if (d->fd == fd && d->check == watch) return 0;
		watch_add(&gg_plugin, d->fd, d->check, gg_dcc7_handler, d);

		/* XXX, timeouty */
	}

	gg_event_free(e);
	
	return -1;
}

#endif

/*
 * gg_dcc_handler()
 *
 * obs³uga bezpo¶rednich po³±czeñ. w data jest przekazywana struct gg_dcc.
 */
WATCHER(gg_dcc_handler)	/* tymczasowy */
{
	struct gg_event *e;
	struct gg_dcc *d = data;
	int again = 1;
	dcc_t *D;

	if (type != 0)
		return 0;

	if (!(e = gg_dcc_watch_fd(d))) {
		print("dcc_error", strerror(errno));

		if (d->type == GG_SESSION_DCC_SOCKET)
			gg_dcc_socket_close();

		return -1;
	}

	switch (e->type) {
		case GG_EVENT_DCC_NEW:
		{
			struct gg_dcc *d = e->event.dcc_new;
			int __port, __valid = 1;
			char *__host;

			debug("[gg] GG_EVENT_DCC_CLIENT_NEW\n");

			if (gg_config_dcc_limit) {
				int c, t = 60;
				char *tmp;

				if ((tmp = xstrchr(gg_config_dcc_limit, '/')))
					t = atoi(tmp + 1);

				c = atoi(gg_config_dcc_limit);

				if (time(NULL) - dcc_limit_time > t) {
					dcc_limit_time = time(NULL);
					dcc_limit_count = 0;
				}

				dcc_limit_count++;

				if (dcc_limit_count > c) {
					print("dcc_limit");
					gg_config_dcc = 0;
					gg_changed_dcc(("dcc"));

					dcc_limit_time = 0;
					dcc_limit_count = 0;

					gg_dcc_free(e->event.dcc_new);
					e->event.dcc_new = NULL;
					break;
				}
			}

			__host = inet_ntoa(*((struct in_addr*) &d->remote_addr));
			__port = d->remote_port;
			query_emit(NULL, ("protocol-dcc-validate"), &__host, &__port, &__valid, NULL);

			if (__valid)
				watch_add(&gg_plugin, d->fd, d->check, gg_dcc_handler, d);
			else
				gg_dcc_free(d);

			e->event.dcc_new = NULL;
			
			break;
		}

		case GG_EVENT_DCC_CLIENT_ACCEPT:
		{
			char peer[16], uin[16];
			session_t *s; 

			debug("[gg] GG_EVENT_DCC_CLIENT_ACCEPT\n");

			snprintf(peer, sizeof(peer), "gg:%d", d->peer_uin);
			snprintf(uin, sizeof(uin), "gg:%d", d->uin);
			
			s = session_find(uin);

			if (!s || !userlist_find(s, peer) || (ignored_check(s, peer) & IGNORE_DCC)) {
				debug("[gg] unauthorized client (uin=%ld), closing connection\n", d->peer_uin);
				gg_free_dcc(d);
				gg_event_free(e);
				return -1;
			}
			break;	
		}

		case GG_EVENT_DCC_CALLBACK:
		{
			int found = 0;
			char peer[16];
			
			debug("[gg] GG_EVENT_DCC_CALLBACK\n");

			snprintf(peer, sizeof(peer), "gg:%d", d->peer_uin);
			
			for (D = dccs; D; D = D->next) {
				debug("[gg] dcc id=%d, uid=%d, type=%d\n", dcc_id_get(D), dcc_uid_get(D), dcc_type_get(D));

				if (!xstrcmp(dcc_uid_get(D), peer) && !dcc_private_get(D)) {
					debug("[gg] found transfer, uid=%d, type=%d\n", dcc_uid_get(D), dcc_type_get(D));

					dcc_private_set(D, d);
					gg_dcc_set_type(d, (dcc_type_get(D) == DCC_SEND) ? GG_SESSION_DCC_SEND : GG_SESSION_DCC_VOICE);
					found = 1;
					
					break;
				}
			}
			
			if (!found) {
				debug("[gg] connection from %d not found\n", d->peer_uin);
				gg_dcc_free(d);
				gg_event_free(e);	
				return -1;
			}

			break;	/* w ekg jest break... byl bug? */
		}

		case GG_EVENT_DCC_NEED_FILE_INFO:
		{
			debug("[gg] GG_EVENT_DCC_NEED_FILE_INFO\n");

			for (D = dccs; D; D = D->next) {
				if (dcc_private_get(D) != d)
					continue;

				if (gg_dcc_fill_file_info(d, dcc_filename_get(D)) == -1) {
					debug("[gg] gg_dcc_fill_file_info() failed (%s)\n", strerror(errno));
					print("dcc_open_error", dcc_filename_get(D));
					dcc_close(D);
					gg_free_dcc(d);
					
					break;
				}

				break;
			}

			break;
		}
			
		case GG_EVENT_DCC_NEED_FILE_ACK:
		{
			char *path;
			unsigned char *p;
			char uin[16];
			struct stat st;

			debug("[gg] GG_EVENT_DCC_NEED_FILE_ACK\n");
			snprintf(uin, sizeof(uin), "gg:%d", d->uin);

			again = 0;

			if (!(D = gg_dcc_find(d))) {
				char peer[16];

				snprintf(peer, sizeof(peer), "gg:%d", d->peer_uin);
				D = dcc_add(NULL, peer, DCC_GET, d);		/* XXX, fix it. */
			}

			for (p = d->file_info.filename; *p; p++)
				if (*p < 32 || *p == '\\' || *p == '/')
					*p = '_';

			if (d->file_info.filename[0] == '.')
				d->file_info.filename[0] = '_';

			dcc_filename_set(D, (const char *) d->file_info.filename);
			dcc_size_set(D, d->file_info.size);

			print("dcc_get_offer", format_user(session_find(uin), dcc_uid_get(D)), dcc_filename_get(D), itoa(d->file_info.size), itoa(dcc_id_get(D)));

			if (config_dcc_dir)
				path = saprintf("%s/%s", config_dcc_dir, dcc_filename_get(D));
			else
				path = xstrdup(dcc_filename_get(D));

			if (!stat(path, &st) && st.st_size < d->file_info.size)
				print("dcc_get_offer_resume", format_user(session_find(uin), dcc_uid_get(D)), dcc_filename_get(D), itoa(d->file_info.size), itoa(dcc_id_get(D)));
			
			xfree(path);

			break;
		}
			
		case GG_EVENT_DCC_NEED_VOICE_ACK:
			debug("[gg] GG_EVENT_DCC_NEED_VOICE_ACK\n");
#if 0
			if (gg_config_audio && 0) {
				/* ¿eby nie sprawdza³o, póki luser nie odpowie */
				list_remove(&watches, d, 0);

				if (!(t = find_transfer(d))) {
					tt.uin = d->peer_uin;
					tt.type = GG_SESSION_DCC_VOICE;
					tt.filename = NULL;
					tt.dcc = d;
					tt.id = transfer_id();
					if (!(t = list_add(&transfers, &tt, sizeof(tt)))) {
						gg_free_dcc(d);
						break;
					}
				}
			
				t->type = GG_SESSION_DCC_VOICE;

				print("dcc_voice_offer", format_user(t->uin), itoa(t->id));
			} else { 
				list_remove(&watches, d, 0);
				remove_transfer(d);
				gg_free_dcc(d);
			}
#endif
			break;

		case GG_EVENT_DCC_VOICE_DATA:
			debug("[gg] GG_EVENT_DCC_VOICE_DATA\n");
			if (gg_config_audio && audiofds[1] != -1) {
				write(audiofds[1], e->event.dcc_voice_data.data, e->event.dcc_voice_data.length);
			}
			break;
		case GG_EVENT_DCC_DONE:
		{
			char uin[16];

			debug("[gg] GG_EVENT_DCC_DONE\n");
			snprintf(uin, sizeof(uin), "gg:%d", d->uin);

			if (!(D = gg_dcc_find(d))) {
				gg_free_dcc(d);
				d = NULL;
				break;
			}

			print((dcc_type_get(D) == DCC_SEND) ? "dcc_done_send" : "dcc_done_get", format_user(session_find(uin), dcc_uid_get(D)), dcc_filename_get(D));
			
			dcc_close(D);
			gg_free_dcc(d);
			d = NULL;

			break;
		}
			
		case GG_EVENT_DCC_ERROR:
		{
			struct in_addr addr;
			unsigned short port = d->remote_port;
			char *tmp;
			char uin[16];
			snprintf(uin, sizeof(uin), "gg:%d", d->uin);

			addr.s_addr = d->remote_addr;

			if (d->peer_uin) {
				char peer[16];
				userlist_t *u;
				
				snprintf(peer, sizeof(peer), "gg:%d", d->peer_uin);
				u = userlist_find(session_find(uin), peer);

				if (!addr.s_addr && u) {
					addr.s_addr = user_private_item_get_int(u, "ip");
					port = user_private_item_get_int(u, "port");
				}
				tmp = saprintf("%s (%s:%d)", format_user(session_find(uin), peer), inet_ntoa(addr), port);
			} else 
				tmp = saprintf("%s:%d", inet_ntoa(addr), port);
			
			switch (e->event.dcc_error) {
				case GG_ERROR_DCC_HANDSHAKE:
					print("dcc_error_handshake", tmp);
					break;
				case GG_ERROR_DCC_NET:
					print("dcc_error_network", tmp);
					break;
				case GG_ERROR_DCC_REFUSED:
					print("dcc_error_refused", tmp);
					break;
				default:
					print("dcc_error_unknown", tmp);
			}

			xfree(tmp);
			if (d->type == GG_SESSION_DCC_VOICE) {
				close(audiofds[1]);
				close(audiofds[0]);

				audiofds[0] = -1;
				audiofds[1] = -1;

			}
			dcc_close(gg_dcc_find(d));
			gg_free_dcc(d);
			d = NULL;

			break;
		}
	}

	/* uaktualnij statystyki */
	for (D = dccs; D; D = D->next) {
		if (dcc_private_get(D) != d)
			continue;

		if (!d)
			continue;

		if (d->state == GG_STATE_SENDING_FILE_HEADER || d->state == GG_STATE_READING_FILE_HEADER)
			dcc_active_set(D, 1);
		if (d->state == GG_STATE_READING_VOICE_HEADER)
			dcc_active_set(D, 1);

		if (d->state == GG_STATE_SENDING_FILE || d->state == GG_STATE_GETTING_FILE)
			dcc_offset_set(D, d->offset);
	}

	if (d && d->type != GG_SESSION_DCC_SOCKET && again) {
		if (d->fd == fd && d->check == watch) return 0;
		watch_add(&gg_plugin, d->fd, d->check, gg_dcc_handler, d);
	}

	gg_event_free(e);
	
	return -1;
}

static WATCHER(gg_dcc_handler_open) {	/* wrapper */
	gg_dcc_handler(type, fd, watch, data);
	return 0;
}

/*
 * gg_dcc_socket_open()
 */
int gg_dcc_socket_open(int port)
{
	if (gg_dcc_socket)
		return 0;

	if (!(gg_dcc_socket = gg_dcc_socket_create(1, port)))
		return -1;
	
	watch_add(&gg_plugin, gg_dcc_socket->fd, gg_dcc_socket->check, gg_dcc_handler_open, gg_dcc_socket);

	return 0;
}

/*
 * gg_dcc_socket_close()
 *
 * zamyka nas³uchuj±cy port dcc.
 */
void gg_dcc_socket_close()
{
	if (!gg_dcc_socket)
		return;

	watch_remove(&gg_plugin, gg_dcc_socket->fd, gg_dcc_socket->check);

	gg_free_dcc(gg_dcc_socket);
	gg_dcc_socket = NULL;
}

void gg_dcc_audio_init() {
	if (gg_config_audio) {
		if (audiofds[0] != -1) close(audiofds[0]);
		if (audiofds[1] != -1) close(audiofds[1]);
		audiofds[0] = -1;
		audiofds[1] = -1;
		audio_register(&gg_dcc_audio);
	}
}

void gg_dcc_audio_close() {
	if (!gg_config_audio) {
		if (audiofds[0] != -1) close(audiofds[0]);
		if (audiofds[1] != -1) close(audiofds[1]);
		audiofds[0] = -1;
		audiofds[1] = -1;
		audio_unregister(&gg_dcc_audio);
	}
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
