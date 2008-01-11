#define JABBER_HANDLER_ERROR(x)		static void x(session_t *s, xmlnode_t *n, const char *from, const char *id)

static char *jabber_iq_error_string(xmlnode_t *n) {			/* in n we have <error */
	const char *ecode;
	char *reason = NULL;

	if (!n) {
		debug_error("[JABBER] jabber_iq_error_string() IQ ERROR, but without <error?\n");
		return NULL;
	}

	/* XXX, przeczytac RFC, i zobaczyc jak prawdziwe klienty XMPP to robia */

	ecode = jabber_attr(n->atts, "code");

	if (n->data) {
		reason = jabber_unescape(n->data);
	} else {
		for (n = n->children; n; n = n->next) {
			if (n->data) {
				reason = jabber_unescape(n->data);
				break;
			}
		}
	}

	debug_error("[JABBER] jabber_iq_error_string: code=%s, [%s]\n", __(ecode), __(reason));

	return reason;
}

/* this sux, no idea howto pass formatname to jabber_handle_iq_error_generic() */

JABBER_HANDLER_ERROR(jabber_handle_iq_error_last) {
	char *error = jabber_iq_error_string(n);
	print("jabber_lastseen_error", session_name(s), from, error ? error : "ekg2 sux in parsing errors, for more info check debug");
	xfree(error);
}

JABBER_HANDLER_ERROR(jabber_handle_iq_error_version) {
	char *error = jabber_iq_error_string(n);
	print("jabber_version_error", session_name(s), from, error ? error : "ekg2 sux in parsing errors, for more info check debug");
	xfree(error);
}

JABBER_HANDLER_ERROR(jabber_handle_iq_error_disco_info) {
	char *error = jabber_iq_error_string(n);
	print("jabber_transinfo_error", session_name(s), from, error ? error : "ekg2 sux in parsing errors, for more info check debug");
	xfree(error);
}

JABBER_HANDLER_ERROR(jabber_handle_iq_error_disco) {
	int iscontrol = !xstrncmp(id, "control", 7);
	char *error = jabber_iq_error_string(n);

	print(iscontrol ? "jabber_remotecontrols_error" : "jabber_transport_error", session_name(s), from, error ? error : "ekg2 sux in parsing errors, for more info check debug");
	xfree(error);
}

JABBER_HANDLER_ERROR(jabber_handle_iq_error_privacy) {
	char *error = jabber_iq_error_string(n);
	print("jabber_privacy_error", session_name(s), from, error ? error : "ekg2 sux in parsing errors, for more info check debug");
	xfree(error);
}

JABBER_HANDLER_ERROR(jabber_handle_iq_error_private) {
	char *error = jabber_iq_error_string(n);
	print("jabber_private_list_error", session_name(s), from, error ? error : "ekg2 sux in parsing errors, for more info check debug");
	xfree(error);
}

JABBER_HANDLER_ERROR(jabber_handle_iq_error_vcard) {
	char *error = jabber_iq_error_string(n);
	print("jabber_userinfo_error", session_name(s), from, error ? error : "ekg2 sux in parsing errors, for more info check debug");
	xfree(error);
}

JABBER_HANDLER_ERROR(jabber_handle_iq_error_search) {
	char *error = jabber_iq_error_string(n);
	print("jabber_search_error", session_name(s), from, error ? error : "ekg2 sux in parsing errors, for more info check debug");
	xfree(error);
}

JABBER_HANDLER_ERROR(jabber_handle_iq_error_generic) {
	char *error = jabber_iq_error_string(n);
	debug_error("jabber_handle_iq_error_generic() %s\n", __(error));
	xfree(error);
}

static const struct jabber_iq_generic_handler jabber_iq_error_handlers[] = {
	{ "vCard",	"vcard-temp",					jabber_handle_iq_error_vcard },
	{ "query",	"jabber:iq:last",				jabber_handle_iq_error_last },
	{ NULL,		"jabber:iq:privacy",				jabber_handle_iq_error_privacy },
	{ NULL,		"jabber:iq:private",				jabber_handle_iq_error_private },
	{ NULL,		"jabber:iq:version",				jabber_handle_iq_error_version },
	{ NULL,		"jabber:iq:search",				jabber_handle_iq_error_search },
	{ NULL,		"http://jabber.org/protocol/disco#info",	jabber_handle_iq_error_disco_info },
	{ NULL,		"http://jabber.org/protocol/disco#items",	jabber_handle_iq_error_disco },
	{ "",		NULL,						NULL }
};

#if 0			/* ERROR handler for id: offer */
	char *uin = jabber_unescape(from);
	dcc_t *p = jabber_dcc_find(uin, id, NULL);

	if (p) {
		/* XXX, new theme it's for ip:port */
		print("dcc_error_refused", format_user(s, p->uid));
		dcc_close(p);
	} else {
		/* XXX, possible abuse attempt */
	}
	xfree(uin);
#endif

JABBER_HANDLER_ERROR(jabber_handle_iq_error_generic_old) {
	jabber_private_t *j = s->priv;

	xmlnode_t *e = xmlnode_find_child(n, "error");
	char *reason = (e) ? jabber_unescape(e->data) : NULL;

	if (!xstrncmp(id, "register", 8)) {
		print("register_failed", jabberfix(reason, "?"));
	} else if (!xstrcmp(id, "auth")) {
		j->parser = NULL; jabber_handle_disconnect(s, reason ? reason : _("Error connecting to Jabber server"), EKG_DISCONNECT_FAILURE);

	} else if (!xstrncmp(id, "passwd", 6)) {
		print("passwd_failed", jabberfix(reason, "?"));
		session_set(s, "__new_password", NULL);
	} else debug_error("[JABBER] GENERIC IQ ERROR: %s\n", __(reason));

	xfree(reason);
}

