/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl
 *                2004 Piotr Kupisiewicz <deletek@ekg2.org>
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/themes.h>
#include <ekg/stuff.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include <libgadu.h>

#include "gg.h"
#include "misc.h"

list_t gg_reminds = NULL;
list_t gg_registers = NULL;
list_t gg_unregisters = NULL;
list_t gg_userlists = NULL;

int gg_register_done = 0;
char *gg_register_password = NULL;
char *gg_register_email = NULL;
int gg_userlist_put_config;

static WATCHER(gg_handle_register)	/* tymczasowy */
{
	struct gg_http *h = data;
	struct gg_pubdir *p;
	session_t *s;
	char *tmp;

	if (type == 2) {
		debug("[gg] gg_handle_register() timeout\n");
		print("register_timeout");
		goto fail;
	}

	if (type)
		return -1;

	if (!h) {
		debug("[gg] gg_handle_register() called with NULL data\n");
		return -1;
	}

	if (gg_pubdir_watch_fd(h) || h->state == GG_STATE_ERROR) {
		print("register_failed", gg_http_error_string(h->error));
		goto fail;
	}

	if (h->state != GG_STATE_DONE) {
		watch_t *w;
		if (watch == h->check && h->fd == fd) { 
			if ((w = watch_find(&gg_plugin, fd, watch))) 
				watch_timeout_set(w, h->timeout);
			else debug("[gg] watches managment went to hell?\n");
			return 0;
		}

		w = watch_add(&gg_plugin, h->fd, h->check, gg_handle_register, h);
		watch_timeout_set(w, h->timeout);
		return -1;
	}

	if (!(p = h->data) || !p->success) {
		print("register_failed", gg_http_error_string(0));
		goto fail;
	}

	print("register", itoa(p->uin));
	gg_register_done = 1;

	tmp = saprintf("gg:%d", p->uin);
	s = session_add(tmp);
	xfree(tmp);
	session_set(s, "password", gg_register_password);	xfree(gg_register_password);	gg_register_password = NULL;
	session_set(s, "email", gg_register_email);		xfree(gg_register_email);	gg_register_email = NULL;

	window_session_set(window_current, s);

fail:
	list_remove(&gg_registers, h, 0);
	gg_free_pubdir(h);
	return -1;
}

COMMAND(gg_command_register)
{
	struct gg_http *h;
	char *passwd;
	watch_t *w;

	if (gg_register_done) {
		wcs_printq("registered_today");
		return -1;
	}
	
	if (!params[0] || !params[1] || !params[2]) {
		wcs_printq("not_enough_params", name);
		return -1;
	}
	
	if (gg_registers) {
		wcs_printq("register_pending");
		return -1;
	}

        if (!last_tokenid) {
	        wcs_printq("gg_token_missing");
                return -1;
        }

	passwd = gg_locale_to_cp(xstrdup(params[1]));
	
	if (!(h = gg_register3(params[0], passwd, last_tokenid, params[2], 1))) {
		xfree(passwd);
		printq("register_failed", strerror(errno));
		return -1;
	}

	xfree(passwd);

	w = watch_add(&gg_plugin, h->fd, h->check, gg_handle_register, h); 
	watch_timeout_set(w, h->timeout);

	list_add(&gg_registers, h, 0);

	gg_register_email = xstrdup(params[0]);
	gg_register_password = xstrdup(params[1]);

	return 0;
}
/* zwalnianie w type == 1 */
static WATCHER(gg_handle_unregister)	/* tymczasowy */
{
	struct gg_http *h = data;
	struct gg_pubdir *s;

	if (type == 2) {
		debug("[gg] gg_handle_unregister() timeout\n");
		print("unregister_timeout");
		goto fail;
	}

	if (type)
		return 0;

	if (!h) {
		debug("[gg] gg_handle_unregister() called with NULL data\n");
		return -1;
	}

	if (gg_pubdir_watch_fd(h) || h->state == GG_STATE_ERROR) {
		print("unregister_failed", gg_http_error_string(h->error));
		goto fail;
	}

	if (h->state != GG_STATE_DONE) {
		watch_t *w = watch_add(&gg_plugin, h->fd, h->check, gg_handle_unregister, h);
		watch_timeout_set(w, h->timeout);

		return -1;
	}

	if (!(s = h->data) || !s->success) {
		print("unregister_failed", gg_http_error_string(0));
		goto fail;
	}

	print("unregister", itoa(s->uin));

fail:
	list_remove(&gg_unregisters, h, 0);
	gg_free_pubdir(h);
	return -1;
}

COMMAND(gg_command_unregister)
{
	struct gg_http *h;
	watch_t *w;
	uin_t uin;
	char *passwd;

        if (!last_tokenid) {
                wcs_printq("token_missing");
                return -1;
        }

	if (!xstrncasecmp(params[0], "gg:", 3))
		uin = atoi(params[0] + 3);
	else
		uin = atoi(params[0]);

	if (uin < 0) {
		printq("unregister_bad_uin", params[0]);
		return -1;
	}
	passwd = gg_locale_to_cp(xstrdup(params[1]));

	if (!(h = gg_unregister3(uin, passwd, last_tokenid, params[2], 1))) {
		printq("unregister_failed", strerror(errno));
		xfree(passwd);
		return -1;
	}
	xfree(passwd);
	w = watch_add(&gg_plugin, h->fd, h->check, gg_handle_unregister, h); 
	watch_timeout_set(w, h->timeout);

	list_add(&gg_unregisters, h, 0);

	return 0;
}

static WATCHER(gg_handle_passwd)	/* tymczasowy */
{
	struct gg_http *h = data;
	struct gg_pubdir *p = NULL;
	list_t l;

	if (type == 2) {
		debug("[gg] gg_handle_passwd() timeout\n");
		print("passwd_timeout");
		goto fail;
	}

	if (type)
		return 0;

	if (!h) {
		debug("[gg] gg_handle_passwd() called with NULL data\n");
		return -1;
	}

	if (gg_pubdir_watch_fd(h) || h->state == GG_STATE_ERROR) {
		print("passwd_failed", gg_http_error_string(h->error));
		goto fail;
	}

	if (h->state != GG_STATE_DONE) {
		watch_t *w = watch_add(&gg_plugin, h->fd, h->check, gg_handle_passwd, h);
		watch_timeout_set(w, h->timeout);
		
		return -1;
	}

	if (!(p = h->data) || !p->success) {
		print("passwd_failed", gg_http_error_string(0));
		goto fail;
	}

	print("passwd");

fail:
	for (l = sessions; l; l = l->next) {
		session_t *s = l->data;
		gg_private_t *g = session_private_get(s);
		list_t m;

		if (xstrncasecmp(s->uid, "gg:", 3))
			continue;
		
		for (m = g->passwds; m; ) {
			struct gg_http *sh = m->data;

			m = m->next;

			if (sh != h)
				continue;

			if (p && p->success) {
				char *new_passwd = (char *) session_get(s, "new_password");
				session_set(s, "password", new_passwd);
				xfree(new_passwd);
			}
			session_set(s, "new_password", NULL);

			list_remove(&g->passwds, h, 0);
			gg_free_pubdir(h);
		}
	}
	return -1;
}

COMMAND(gg_command_passwd)
{
	gg_private_t *g = session_private_get(session);
	struct gg_http *h;
	watch_t *w;

	char *oldpasswd = gg_locale_to_cp(xstrdup(session_get(session, "password")));
	char *newpasswd = gg_locale_to_cp(xstrdup(params[0]));

#ifdef HAVE_GG_CHANGE_PASSWD4 /* gg_change_passwd4 since ~ LIBGADU 20030930 */
	const char *config_email = session_get(session, "email");

	if (!last_tokenid) {
		printq("gg_token_missing");
		return -1;
	}
	if (!params[1]) {
		printq("not_enough_params", name);
		return -1;
	}

	if (!config_email) {
		printq("var_not_set", name, "/session email");
		return -1;
	}

	if (!(h = gg_change_passwd4(atoi(session->uid + 3), config_email, (oldpasswd) ? oldpasswd : "", newpasswd, last_tokenid, params[1], 1)))
#else 
	if (!(h = gg_change_passwd3(atoi(session->uid + 3), (oldpasswd) ? oldpasswd : "", newpasswd, "", 1)))
#endif 
	{
		xfree(newpasswd);
		xfree(oldpasswd);
		printq("passwd_failed", strerror(errno));
		return -1;
	}

	session_set(session, "new_password", params[0]);

	w = watch_add(&gg_plugin, h->fd, h->check, gg_handle_passwd, h); 
	watch_timeout_set(w, h->timeout);

	list_add(&g->passwds, h, 0);

	/* memleaks ? ... mh, if gg_change_passwd3 fails... we have freeing it.. why here was not? it's ok or not? libgadu/we frees it ?*/
#if 0
	xfree(newpasswd);
	xfree(oldpasswd);
#endif

	return 0;
}

static WATCHER(gg_handle_remind)	/* tymczasowy */
{
	struct gg_http *h = data;
	struct gg_pubdir *s;

	if (type == 2) {
		debug("[gg] gg_handle_remind() timeout\n");
		print("remind_timeout");
		goto fail;
	}

	if (type)
		return 0;

	if (!h) {
		debug("[gg] gg_handle_remind() called with NULL data\n");
		return -1;
	}

	if (gg_pubdir_watch_fd(h) || h->state == GG_STATE_ERROR) {
		print("remind_failed", gg_http_error_string(h->error));
		goto fail;
	}

	if (h->state != GG_STATE_DONE) {
		watch_t *w = watch_add(&gg_plugin, h->fd, h->check, gg_handle_remind, h);
		watch_timeout_set(w, h->timeout);

		return -1;
	}

	if (!(s = h->data) || !s->success) {
		print("remind_failed", gg_http_error_string(0));
		goto fail;
	}

	print("remind");

fail:
	list_remove(&gg_reminds, h, 0);
	gg_free_pubdir(h);
	return -1;
}

COMMAND(gg_command_remind)
{
	gg_private_t *g = session_private_get(session);
	struct gg_http *h;
	watch_t *w;
	uin_t uin = 0;

	if (params[0])
		uin = atoi(params[0]);
	else {
		if (!uin && (!session || !g || xstrncasecmp(session_uid_get(session), "gg:", 3))) {
			if (!params[0])
				wcs_printq("invalid_session");
			return -1;
		}

		uin = atoi(session_uid_get(session) + 3);
	}

	if (!uin) {
		wcs_printq("invalid_uid");
		return -1;
	}
	
	if (!(h = gg_remind_passwd(uin, 1))) {
		printq("remind_failed", strerror(errno));
		return -1;
	}

	w = watch_add(&gg_plugin, h->fd, h->check, gg_handle_remind, h); 
	watch_timeout_set(w, h->timeout);

	list_add(&gg_reminds, h, 0);

	return 0;
}

COMMAND(gg_command_list)
{
/* moze mi ktos qrwa powiedziec po co bylo to passwd ? tylko byly memleaki i jakies goto.... */
	gg_private_t *g = session_private_get(session);
	/* list --get */
	if (params[0] && match_arg(params[0], 'g', ("get"), 2)) {
                if (gg_userlist_request(g->sess, GG_USERLIST_GET, NULL) == -1) {
                        printq("userlist_get_error", strerror(errno));
			return -1;
	        }
		return 0;
	}

	/* list --clear */
	if (params[0] && match_arg(params[0], 'c', ("clear"), 2)) {
                if (gg_userlist_request(g->sess, GG_USERLIST_PUT, NULL) == -1) {
                        printq("userlist_clear_error", strerror(errno));
                        return -1;
                }
		gg_userlist_put_config = 2;
		return 0;
	}
	
	/* list --put */
	if (params[0] && (match_arg(params[0], 'p', ("put"), 2))) {
		char *contacts = userlist_dump(session);
		char *cpcontacts;

		cpcontacts = gg_locale_to_cp(contacts);

                if (gg_userlist_request(g->sess, GG_USERLIST_PUT, cpcontacts) == -1) {
                        printq("userlist_put_error", strerror(errno));
                        xfree(cpcontacts);
                        return -1;
                }
		gg_userlist_put_config = 0;
		xfree(cpcontacts);
		return 0;
	}
	return cmd_list(name, params, session, target, quiet);
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
