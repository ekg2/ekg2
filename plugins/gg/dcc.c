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

struct gg_dcc *gg_dcc_socket = NULL;

COMMAND(gg_command_dcc)
{
	uin_t uin = atoi(session->uid + 3);
	gg_private_t *g = session_private_get(session);

	if (!session_check(session, 1, "gg")) {
		printq("invalid_session");
		return -1;
	}

	/* send, rsend */
	if (params[0] && (!xstrncasecmp(params[0], "se", 2) || !xstrncasecmp(params[0], "rse", 3))) {
		struct gg_dcc *gd = NULL;
		struct stat st;
		userlist_t *u;
		dcc_t *d;
		int fd;

		if (!params[1] || !params[2]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!(u = userlist_find(session, get_uid(session, params[1])))) {
			printq("user_not_found", params[1]);
			return -1;
		}

		if (!session_connected_get(session)) {
			printq("not_connected");
			return -1;
		}

		if (!xstrcmp(u->status, EKG_STATUS_NA)) {
			printq("dcc_user_not_avail", format_user(session, u->uid));
			return -1;
		}

		if (!u->ip) {
			printq("dcc_user_aint_dcc", format_user(session, u->uid));
			return -1;
		}

		if ((fd = open(params[2], O_RDONLY)) == -1) {
			printq("dcc_open_error", params[2], strerror(errno));
			return -1;
		}

		if (!stat(params[2], &st) && S_ISDIR(st.st_mode)) {
			printq("dcc_open_error", params[2], strerror(EISDIR));
			return -1;
		}

		close(fd);

		if (u->port < 10 || !xstrncasecmp(params[0], "rse", 3)) {
			/* nie mo¿emy siê z nim po³±czyæ, wiêc on spróbuje */
			gg_dcc_request(g->sess, atoi(u->uid + 3));
		} else {
			if (!(gd = gg_dcc_send_file(u->ip, u->port, uin, atoi(u->uid + 3)))) {
				printq("dcc_error", strerror(errno));
				return -1;
			}

			if (gg_dcc_fill_file_info(gd, params[2]) == -1) {
				printq("dcc_open_error", params[2], strerror(errno));
				gg_free_dcc(gd);
				return -1;
			}
		}
		
		d = dcc_add(u->uid, DCC_SEND, gd);
		dcc_filename_set(d, params[2]);

		if (gd)
			watch_add(&gg_plugin, gd->fd, gd->check, 0, gg_dcc_handler, gd);

		return 0;
	}

#if 0
	if (params[0][0] == 'v' || !xstrncasecmp(params[0], "rvo", 3)) {			/* voice, rvoice */
#ifdef HAVE_VOIP
		struct userlist *u = NULL;
		struct transfer *t, tt;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}
		
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

		/* sprawd¼, czy ju¿ nie wo³ano o rozmowê g³osow± */

#if 0
		for (l = transfers; l; l = l->next) {
			struct transfer *t = l->data;

			if (t->type == GG_SESSION_DCC_VOICE) {
				printq("dcc_voice_running");
				return 0;
			}
		}

		for (l = watches; l; l = l->next) {
			struct gg_session *s = l->data;

			if (s->type == GG_SESSION_DCC_VOICE) {
				printq("dcc_voice_running");
				return 0;
			}
		}
#endif
		/* je¶li nie by³o, to próbujemy sami zainicjowaæ */

		uin = get_uin(params[1]);

		if (!(u = userlist_find(uin, params[1]))) {
			printq("user_not_found", params[1]);
			return -1;
		}

		if (!sess || sess->state != GG_STATE_CONNECTED) {
			printq("not_connected");
			return -1;
		}

		if (!(GG_S_A(u->status) || GG_S_B(u->status)) && !(ignored_check(uin) & IGNORE_STATUS)) {
			printq("dcc_user_not_avail", format_user(u->uin));
			return -1;
		}

		if (!u->ip.s_addr) {
			printq("dcc_user_aint_dcc", format_user(u->uin));
			return -1;
		}

		memset(&tt, 0, sizeof(tt));
		tt.uin = uin;
		tt.id = transfer_id();
		tt.type = GG_SESSION_DCC_VOICE;
		tt.protocol = u->protocol;

		if (u->port < 10 || !xstrncasecmp(params[0], "rvo", 3)) {
			/* nie mo¿emy siê z nim po³±czyæ, wiêc on spróbuje */
			gg_dcc_request(sess, uin);
		} else {
			struct gg_dcc *d;
			
			if (!(d = gg_dcc_voice_chat(u->ip.s_addr, u->port, config_uin, uin))) {
				printq("dcc_error", strerror(errno));
				return -1;
			}

			list_add(&watches, d, 0);

			tt.dcc = d;
		}

		list_add(&transfers, &tt, sizeof(tt));
		voice_open();
#else
		printq("dcc_voice_unsupported");
#endif
		return -1;
	}
#endif
	
	/* get, resume */
	if (params[0] && (!xstrncasecmp(params[0], "g", 1) || !xstrncasecmp(params[0], "re", 2))) {
		dcc_t *d = NULL;
		struct gg_dcc *g;
		char *path;
		list_t l;
		
		for (l = dccs; l; l = l->next) {
			dcc_t *D = l->data;
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

		if (config_dcc_dir) 
		    	path = saprintf("%s/%s", config_dcc_dir, dcc_filename_get(d));
		else
		    	path = xstrdup(dcc_filename_get(d));
		
		if (params[0][0] == 'r') {
			g->file_fd = open(path, O_WRONLY);
			g->offset = lseek(g->file_fd, 0, SEEK_END);
		} else
			g->file_fd = open(path, O_WRONLY | O_CREAT, 0600);

		if (g->file_fd == -1) {
			printq("dcc_get_cant_create", path);
			gg_free_dcc(g);
			dcc_close(d);
			xfree(path);
			
			return -1;
		}
		
		xfree(path);
		
		printq("dcc_get_getting", format_user(session, dcc_uid_get(d)), dcc_filename_get(d));
		dcc_active_set(d, 1);
		
		watch_add(&gg_plugin, g->fd, g->check, 0, gg_dcc_handler, g);

		return 0;
	}

	return cmd_dcc(name, params, session, target, quiet);
}

void gg_dcc_close_handler(dcc_t *d)
{
	struct gg_dcc *g = dcc_private_get(d);

	if (!g)
		return;

	gg_dcc_free(g);
}

/*
 * gg_dcc_find()
 *
 * szuka dcc_t zawieraj±cy dan± struct gg_dcc.
 */
static dcc_t *gg_dcc_find(struct gg_dcc *d)
{
	list_t l;

	for (l = dccs; l; l = l->next) {
		dcc_t *D = l->data;

		if (d && dcc_private_get(D) == d)
			return D;
	}

	return NULL;
}

/*
 * gg_dcc_handler()
 *
 * obs³uga bezpo¶rednich po³±czeñ. w data jest przekazywana struct gg_dcc.
 */
void gg_dcc_handler(int type, int fd, int watch, void *data)
{
	struct gg_event *e;
	struct gg_dcc *d = data;
	int again = 1;
	list_t l;

	if (type != 0)
		return;

	if (!(e = gg_dcc_watch_fd(d))) {
		print("dcc_error", strerror(errno));

		if (d->type == GG_SESSION_DCC_SOCKET)
			gg_dcc_socket_close();

		return;
	}

	switch (e->type) {
		case GG_EVENT_DCC_NEW:
		{
			struct gg_dcc *d = e->event.dcc_new;
			int __port, __valid = 1;
			char *__host;

			debug("[gg] GG_EVENT_DCC_CLIENT_NEW\n");

			__host = inet_ntoa(*((struct in_addr*) &d->remote_addr));
			__port = d->remote_port;
			query_emit(NULL, "protocol-dcc-validate", &__host, &__port, &__valid, NULL);

			if (__valid)
				watch_add(&gg_plugin, d->fd, d->check, 0, gg_dcc_handler, d);
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
				return;
			}
			break;	
		}

		case GG_EVENT_DCC_CALLBACK:
		{
			int found = 0;
			char peer[16];
			list_t l;
			
			debug("[gg] GG_EVENT_DCC_CALLBACK\n");

			snprintf(peer, sizeof(peer), "gg:%d", d->peer_uin);
			
			for (l = dccs; l; l = l->next) {
				dcc_t *D = l->data;

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
			
				return;
			}
		}

		case GG_EVENT_DCC_NEED_FILE_INFO:
		{
			list_t l;

			debug("[gg] GG_EVENT_DCC_NEED_FILE_INFO\n");

			for (l = dccs; l; l = l->next) {
				dcc_t *D = l->data;

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
			char *path, *p;
			char uin[16];
			struct stat st;
			dcc_t *D;

			debug("[gg] GG_EVENT_DCC_NEED_FILE_ACK\n");
		        snprintf(uin, sizeof(uin), "gg:%d", d->uin);

			again = 0;

			if (!(D = gg_dcc_find(d))) {
				char peer[16];

				snprintf(peer, sizeof(peer), "gg:%d", d->peer_uin);
				D = dcc_add(peer, DCC_GET, d);
			}

			for (p = d->file_info.filename; *p; p++)
				if (*p < 32 || *p == '\\' || *p == '/')
					*p = '_';

			if (d->file_info.filename[0] == '.')
				d->file_info.filename[0] = '_';

			dcc_filename_set(D, d->file_info.filename);
			dcc_size_set(D, d->file_info.size);

			print("dcc_get_offer", format_user(session_find(uin), dcc_uid_get(D)), dcc_filename_get(D), itoa(d->file_info.size), itoa(dcc_id_get(D)));

			if (config_dcc_dir)
				path = saprintf("%s/%s", config_dcc_dir, dcc_filename_get(D));
			else
				path = xstrdup(dcc_filename_get(D));

			if (!stat(path, &st) && st.st_size < d->file_info.size)
				print("dcc_get_offer_resume", format_user(session_find(uin), dcc_uid_get(D)), dcc_filename_get(D), itoa(d->file_info.size), dcc_id_get(D));
			
			xfree(path);

			break;
		}
			
#if 0
		case GG_EVENT_DCC_NEED_VOICE_ACK:
			debug("[gg] GG_EVENT_DCC_NEED_VOICE_ACK\n");
#ifdef HAVE_VOIP
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
#else
			list_remove(&watches, d, 0);
			remove_transfer(d);
			gg_free_dcc(d);
#endif
			break;

		case GG_EVENT_DCC_VOICE_DATA:
			debug("[gg] GG_EVENT_DCC_VOICE_DATA\n");

#ifdef HAVE_VOIP
			voice_open();
			voice_play(e->event.dcc_voice_data.data, e->event.dcc_voice_data.length, 0);
#endif
			break;
#endif

		case GG_EVENT_DCC_DONE:
		{
			dcc_t *D;
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
					addr.s_addr = u->ip;
					port = u->port;
				}
				tmp = saprintf("%s (%s:%d)", xstrdup(format_user(session_find(uin), peer)), inet_ntoa(addr), port);
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

#ifdef HAVE_VOIP
			if (d->type == GG_SESSION_DCC_VOICE)
				voice_close();
#endif  /* HAVE_VOIP */

			dcc_close(gg_dcc_find(d));
			gg_free_dcc(d);
			d = NULL;

			break;
		}
	}

	/* uaktualnij statystyki */
	for (l = dccs; l; l = l->next) {
		dcc_t *D = l->data;

		if (dcc_private_get(D) != d)
			continue;

		if (!d)
			continue;

		if (d->state == GG_STATE_SENDING_FILE_HEADER || d->state == GG_STATE_READING_FILE_HEADER)
			dcc_active_set(D, 1);

		if (d->state == GG_STATE_SENDING_FILE || d->state == GG_STATE_GETTING_FILE)
			dcc_offset_set(D, d->offset);
	}

	if (d && d->type != GG_SESSION_DCC_SOCKET && again)
		watch_add(&gg_plugin, d->fd, d->check, 0, gg_dcc_handler, d);

	gg_event_free(e);
	
	return;
}

/*
 * gg_dcc_socket_open()
 */
int gg_dcc_socket_open()
{
	if (gg_dcc_socket)
		return 0;

	if (!(gg_dcc_socket = gg_dcc_socket_create(1, 0)))
		return -1;
	
	watch_add(&gg_plugin, gg_dcc_socket->fd, gg_dcc_socket->check, 1, gg_dcc_handler, gg_dcc_socket);

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

