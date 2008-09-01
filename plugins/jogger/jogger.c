
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

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/queries.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>
#include <ekg/vars.h>

#define JOGGER_DATE "2007-05-04"

static int jogger_theme_init(void);
int jogger_plugin_init(int prio);
static int jogger_plugin_destroy(void);

	/* messages.c */
void jogger_localize_texts();
void jogger_free_texts(int real_free);
QUERY(jogger_msghandler);
COMMAND(jogger_msg);
COMMAND(jogger_subscribe);

	/* drafts.c */
void jogger_localize_headers();
void jogger_free_headers(int real_free);
COMMAND(jogger_prepare);
COMMAND(jogger_publish);

	/* we need to be 'protocol' to establish sessions */
PLUGIN_DEFINE(jogger, PLUGIN_PROTOCOL, jogger_theme_init);

/**
 * jogger_session_find_uid() tries to find Jogger session connected with given session (@a s) and @a uid.
 *
 * @return	Session pointer or NULL if none match.
 */
session_t *jogger_session_find_uid(session_t *s, const char *uid) {
	session_t *js;

	for (js = sessions; js; js = js->next) {
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
	session_t *js;

	if (!s)
		return 0;

	for (js = sessions; js; js = js->next) {
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
		/* temporary jid: -> xmpp: hack */
	if (!xstrncasecmp(session_get(s, "used_session"), "jid:", 4)) {
		char *tmp = saprintf("xmpp:%s", session_get(s, "used_session") + 4);
		session_set(s, "used_session", tmp);
		xfree(tmp);
	}
	if (!xstrncasecmp(session_get(s, "used_uid"), "jid:", 4)) {
		char *tmp = saprintf("xmpp:%s", session_get(s, "used_uid") + 4);
		session_set(s, "used_uid", tmp);
		xfree(tmp);
	}
		/* /hack */
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

	{		/* update status */
		userlist_t *u	= userlist_find(js, tmpb);

		if (session_connected_get(s) != (u && (u->status > EKG_STATUS_NA))) {
			session_connected_set(s, (u && (u->status > EKG_STATUS_NA)));
			session_status_set(s, (u ? u->status : EKG_STATUS_NA));
		}
	}
}

	/* we need some dummy commands, e.g. /disconnect */
static COMMAND(jogger_null) {
	return 0;
}

static QUERY(jogger_print_version) {
	print("jogger_version", JOGGER_DATE);

	return 0;
}

static QUERY(jogger_newsession) {
	char *suid	= *(va_arg(ap, char **));
	session_t *js	= session_find(suid);

	if (!js || (js->plugin != &jogger_plugin))
		return 1;

	userlist_read(js);

	return 0;
}

static QUERY(jogger_postconfig) {
	session_t *js;
	void *p = ekg_convert_string_init("UTF-8", NULL, NULL);

	jogger_localize_texts(p);
	jogger_localize_headers(p);
	ekg_convert_string_destroy(p);

	for (js = sessions; js; js = js->next) {
		if ((js->plugin == &jogger_plugin) && !session_int_get(js, "userlist_keep"))
			userlist_free(js);
	}

	return 0;
}

static int jogger_theme_init(void) {
#ifndef NO_DEFAULT_THEME
	format_add("jogger_noentry", _("%> (%1) No thread with id %c%2%n found."), 1);
	format_add("jogger_subscribed", _("%> %|(%1) The thread %T%2%n has been subscribed."), 1);
	format_add("jogger_unsubscribed", _("%> %|(%1) The thread %T%2%n has been unsubscribed."), 1);
	format_add("jogger_subscription_denied", _("%! (%1) Subscription denied because of no permission."), 1);
	format_add("jogger_unsubscribed_earlier", _("%> (%1) The thread weren't subscribed."), 1);
	format_add("jogger_comment_added", _("%) %|(%1) Your comment was added to entry %c%2%n."), 1);
	format_add("jogger_modified", _("%> %|(%1) Subscribed entry has been modified: %c%2%n."), 1);
	format_add("jogger_published", _("%) %|(%1) Your new entry has been published as: %c%2%n."), 1);
	format_add("jogger_posting_denied", _("%! (%1) Comment posting denied because of no permission."), 1);
	format_add("jogger_version", _("%> %TJogger:%n match data %g%1%n."), 1);

	format_add("jogger_prepared", _("%) File %T%1%n is ready for submission."), 1);
	format_add("jogger_notprepared", _("%! No filename given and no entry prepared!"), 1);
	format_add("jogger_hashdiffers", _("%! %|File contents (checksum) differs from the time it was prepared. If you changed anything in the entry file, please run %Tprepare%n again. If you want to force submission, please use %Tpublish%n again."), 1);

	format_add("jogger_warning", _("%) %|During QA check of the entry, following warnings have been issued:"), 1);
	format_add("jogger_warning_brokenheader", _("%> %|* Header with broken syntax found at: %c%1%n"), 1);
	format_add("jogger_warning_wrong_key", _("%> %|* Header contains unknown/wrong key at: %c%1%n"), 1);
	format_add("jogger_warning_wrong_key_spaces", _("%> %|* Key in header mustn't be followed or preceeded by spaces at: %c%1%n"), 1);
	format_add("jogger_warning_deprecated_miniblog", _("%> %|* Key %Tminiblog%n is deprecated in favor of such category at: %c%1%n"), 1);
	format_add("jogger_warning_miniblog_techblog", _("%> %|* Miniblog entry will be posted to Techblog at: %c%1%n"), 1);
	format_add("jogger_warning_techblog_only", _("%> * Entries posted to Techblog should have also some normal category: %c%1%n"), 1);
	format_add("jogger_warning_malformed_url", _("%> %|* Malformed URL found at: %c%1%n"), 1);
	format_add("jogger_warning_spacesep", _("%> %|* Possibility of accidentially using space as a separator instead of commas: %c%1%n"), 1);
	format_add("jogger_warning_wrong_value", _("%> %|* Incorrect value found at: %c%1%n"), 1);
	format_add("jogger_warning_wrong_value_level", _("%> %|* Wrong %Tlevel%n found (level %Tnumber%n should be used), entry would be published on %Tzeroth%n level (not default) at: %c%1%n"), 1);
	format_add("jogger_warning_wrong_value_spaces", _("%> %|* Incorrent value found (try to remove leading&trailing spaces) at: %c%1%n"), 1);
	format_add("jogger_warning_wrong_value_empty", _("%> %|* Empty value found in header at: %c%1%n"), 1);
	format_add("jogger_warning_duplicated_header", _("%> %|* Duplicated header found at: %c%1%n"), 1);
	format_add("jogger_warning_mislocated_header", _("%> %|* Mislocated header (?) at: %c%1%n"), 1);
	format_add("jogger_warning_noexcerpt", _("%> %|* Entry text size exceeds 4096 bytes, but no <EXCERPT> tag has been found. It will be probably cut by Jogger near: ...%c%1%n..."), 1);
#endif
	return 0;
}

static plugins_params_t jogger_plugin_vars[] = {
	PLUGIN_VAR_ADD("entry_file",		VAR_STR, NULL, 0, NULL),
	PLUGIN_VAR_ADD("entry_hash",		VAR_STR, NULL, 0, NULL),
	PLUGIN_VAR_ADD("ignore_outgoing_entries",VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("log_formats",		VAR_STR, "simple,sqlite", 0, NULL),
	PLUGIN_VAR_ADD("newentry_open_query",	VAR_BOOL, "1", 0, NULL),
	PLUGIN_VAR_ADD("own_commentformat",	VAR_STR, NULL, 0, NULL),
	PLUGIN_VAR_ADD("own_commentformat_autodetect",	VAR_BOOL, "1", 0, NULL),
	PLUGIN_VAR_ADD("used_session",		VAR_STR, NULL, 0, jogger_usedchanged),
	PLUGIN_VAR_ADD("used_uid",		VAR_STR, NULL, 0, jogger_usedchanged),
	PLUGIN_VAR_ADD("userlist_keep",		VAR_BOOL, "0", 0, NULL),

	PLUGIN_VAR_END()
};

static const char *jogger_protocols[] = { "jogger:", NULL };

int jogger_plugin_init(int prio) {

	PLUGIN_CHECK_VER("jogger");

	jogger_plugin.params = jogger_plugin_vars;
	jogger_plugin.priv.protocol.protocols = jogger_protocols;
	/* statuses are not supported, we're just copying jabber session status */

	query_connect_id(&jogger_plugin, PLUGIN_PRINT_VERSION, jogger_print_version, NULL);
	query_connect_id(&jogger_plugin, PROTOCOL_VALIDATE_UID, jogger_validate_uid, NULL);
	query_connect_id(&jogger_plugin, PROTOCOL_STATUS, jogger_statuschanged, NULL);
	query_connect_id(&jogger_plugin, PROTOCOL_DISCONNECTED, jogger_statuscleanup, NULL);
	query_connect_id(&jogger_plugin, PROTOCOL_MESSAGE, jogger_msghandler, NULL);
	query_connect_id(&jogger_plugin, SESSION_ADDED, jogger_newsession, NULL);
	query_connect_id(&jogger_plugin, CONFIG_POSTINIT, jogger_postconfig, NULL);

#define JOGGER_CMDFLAGS SESSION_MUSTBELONG
#define JOGGER_CMDFLAGS_TARGET SESSION_MUSTBELONG|COMMAND_ENABLEREQPARAMS|COMMAND_PARAMASTARGET
	command_add(&jogger_plugin, "jogger:", "?", jogger_msg, JOGGER_CMDFLAGS, NULL);
	command_add(&jogger_plugin, "jogger:chat", "!uU !", jogger_msg, JOGGER_CMDFLAGS_TARGET, NULL);
	command_add(&jogger_plugin, "jogger:connect", NULL, jogger_null, JOGGER_CMDFLAGS, NULL);
	command_add(&jogger_plugin, "jogger:disconnect", NULL, jogger_null, JOGGER_CMDFLAGS, NULL);
	command_add(&jogger_plugin, "jogger:msg", "!uU !", jogger_msg, JOGGER_CMDFLAGS_TARGET, NULL);
	command_add(&jogger_plugin, "jogger:prepare", "?f", jogger_prepare, JOGGER_CMDFLAGS, NULL);
	command_add(&jogger_plugin, "jogger:publish", "?f", jogger_publish, JOGGER_CMDFLAGS, NULL);
	command_add(&jogger_plugin, "jogger:reconnect", NULL, jogger_null, JOGGER_CMDFLAGS, NULL);
	command_add(&jogger_plugin, "jogger:subscribe", "!uU", jogger_subscribe, JOGGER_CMDFLAGS_TARGET, NULL);
	command_add(&jogger_plugin, "jogger:unsubscribe", "!uU", jogger_subscribe, JOGGER_CMDFLAGS_TARGET, NULL);
#undef JOGGER_CMDFLAGS_TARGET
#undef JOGGER_CMDFLAGS

	jogger_free_texts(0); /* set NULLs */

	plugin_register(&jogger_plugin, prio);

	return 0;
}

static int jogger_plugin_destroy(void) {
	plugin_unregister(&jogger_plugin);
	
	jogger_free_texts(1);
	jogger_free_headers(1);

	return 0;
}
