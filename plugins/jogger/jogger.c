
/*
 *  (C) Copyright 2007	Michał Górny & EKG2 authors
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

#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/queries.h>
#include <ekg/sessions.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>
#include <ekg/vars.h>

static int jogger_theme_init(void);
int jogger_plugin_init(int prio);
static int jogger_plugin_destroy(void);

	/* messages.c */
QUERY(jogger_msghandler);

	/* we need to be 'protocol' to establish sessions */
PLUGIN_DEFINE(jogger, PLUGIN_PROTOCOL, jogger_theme_init);

/**
 * jogger_session_find_uid() tries to find Jogger session connected with given session (@a s) and @a uid.
 *
 * @return	Session pointer or NULL if none match.
 */
session_t *jogger_session_find_uid(session_t *s, const char *uid) {
	list_t l;

	for (l = sessions; l; l = l->next) {
		session_t *js = l->data;

		if (js->plugin == &jogger_plugin) {
			const char *jsw	= session_get(js, "used_session");

			if (jsw && (!xstrcasecmp(jsw, session_uid_get(s)) || !xstrcasecmp(jsw, session_alias_get(s)))) {
				const char *jsu		= session_get(js, "used_uid");
				const char *nickname	= get_nickname(s, uid);

				if (!xstrcasecmp(uid, jsu) || (nickname && !xstrcasecmp(nickname, jsu)))
						/* yeah, that's it! */
					return js;
			}
		}
	}

	return NULL;
}

static QUERY(jogger_validate_uid) {
	const char *uid		= *(va_arg(ap, const char **));
	int *valid		= va_arg(ap, int *);

	if (uid && !xstrncmp(uid, "jogger:", 7)) {
		(*valid)++;
		return -1;
	}

	return 0;
}

static QUERY(jogger_statuschanged) {
	const char *suid	= *(va_arg(ap, const char **));
	const char *uid		= *(va_arg(ap, const char **));
	const int status	= *(va_arg(ap, const int *));

	session_t *s		= session_find(suid);
	session_t *js;

	if (!s || !uid || !status)
		return 0;

	if ((js = jogger_session_find_uid(s, uid))) {
		session_connected_set(js, (status > EKG_STATUS_NA));
		session_status_set(js, status);
	}

	return 0;
}

static QUERY(jogger_statuscleanup) {
	const char *suid	= *(va_arg(ap, const char **));

	session_t *s		= session_find(suid);
	list_t l;

	if (!s)
		return 0;

	for (l = sessions; l; l = l->next) {
		session_t *js = l->data;

		if (js->plugin == &jogger_plugin) {
			const char *jsw	= session_get(js, "used_session");

			if (session_connected_get(js) && jsw
					&& (!xstrcasecmp(jsw, session_uid_get(s)) || !xstrcasecmp(jsw, session_alias_get(s)))) {
				session_connected_set(js, 0);
				session_status_set(js, EKG_STATUS_NA);
			}
		}
	}

	return 0;
}

static void jogger_usedchanged(session_t *s, const char *varname) {
	const char *tmp, *tmpb;
		/* first check if session is correct */
	session_t *js = session_find((tmp = session_get(s, "used_session")));

	if (!js) {
#if 0
		print("jogger_wrong_session", tmp);
		session_connected_set(s, 0);
		session_status_set(s, EKG_STATUS_ERROR);
#endif
		return;
	}

	if (xstrcmp((tmpb = session_uid_get(js)), tmp)) /* replace alias with UID */
		session_set(s, "used_session", tmpb);

		/* then check the UID */
	if (!(tmpb = get_uid(js, (tmp = session_get(s, "used_uid"))))) {
#if 0
		print("jogger_wrong_uid", tmp);
		session_connected_set(s, 0);
		session_status_set(s, EKG_STATUS_ERROR);
#endif
		return;
	}

	if (xstrcmp(tmpb, tmp)) /* replace nickname with UID */
		session_set(s, "used_uid", tmpb);

		/* update status */
#if 0 /* XXX: startup problems */
	{
		userlist_t *u	= userlist_find(js, tmpb);

		session_connected_set(s, (u->status > EKG_STATUS_NA));
		session_status_set(s, u->status);
	}
#endif
}

static int jogger_theme_init(void) {
#ifndef NO_DEFAULT_THEME
#endif
	return 0;
}

static plugins_params_t jogger_plugin_vars[] = {
	PLUGIN_VAR_ADD("own_commentformat",	0, VAR_STR, NULL, 0, NULL),
	PLUGIN_VAR_ADD("used_session",		0, VAR_STR, NULL, 0, jogger_usedchanged),
	PLUGIN_VAR_ADD("used_uid",		0, VAR_STR, NULL, 0, jogger_usedchanged),

	PLUGIN_VAR_END()
};

int jogger_plugin_init(int prio) {
		/* XXX: force prio? or only warn? */
	jogger_plugin.params = jogger_plugin_vars;
	
	query_connect_id(&jogger_plugin, PROTOCOL_VALIDATE_UID, jogger_validate_uid, NULL);
	query_connect_id(&jogger_plugin, PROTOCOL_STATUS, jogger_statuschanged, NULL);
	query_connect_id(&jogger_plugin, PROTOCOL_DISCONNECTED, jogger_statuscleanup, NULL);

	query_connect_id(&jogger_plugin, PROTOCOL_MESSAGE, jogger_msghandler, NULL);

	plugin_register(&jogger_plugin, prio);

	return 0;
}

static int jogger_plugin_destroy(void) {
	plugin_unregister(&jogger_plugin);

	return 0;
}
