#define JABBER_HANDLER_GET_REPLY(x) 	static void x(session_t *s, xmlnode_t *n, const char *from, const char *id)

/**
 * jabber_handle_iq_get_disco()
 *
 * Handler for IQ GET QUERY xmlns="http://jabber.org/protocol/disco#items"<br>
 * Send some info about what ekg2 can do/know with given node [node= in n->atts]<br>
 * XXX info about it in XEP/RFC
 *
 * @todo 	We send here only info about node: http://jabber.org/protocol/commands
 * 		Be more XEP/RFC compilant... return error if node not known, return smth
 * 		what we can do at all. etc. etc.
 */

JABBER_HANDLER_GET_REPLY(jabber_handle_iq_get_disco) {
	jabber_private_t *j = s->priv;

	if (!xstrcmp(jabber_attr(n->atts, "node"), "http://jabber.org/protocol/commands")) {	/* jesli node commandowe */
		/* XXX, check if $uid can do it */
		watch_write(j->send_watch, 
				"<iq to=\"%s\" type=\"result\" id=\"%s\">"
				"<query xmlns=\"http://jabber.org/protocol/disco#items\" node=\"http://jabber.org/protocol/commands\">"
				"<item jid=\"%s/%s\" name=\"Set Status\" node=\"http://jabber.org/protocol/rc#set-status\"/>"
				"<item jid=\"%s/%s\" name=\"Forward Messages\" node=\"http://jabber.org/protocol/rc#forward\"/>"
				"<item jid=\"%s/%s\" name=\"Set Options\" node=\"http://jabber.org/protocol/rc#set-options\"/>"
				"<item jid=\"%s/%s\" name=\"Set ALL ekg2 Options\" node=\"http://ekg2.org/jabber/rc#ekg-set-all-options\"/>"
				"<item jid=\"%s/%s\" name=\"Manage ekg2 plugins\" node=\"http://ekg2.org/jabber/rc#ekg-manage-plugins\"/>"
				"<item jid=\"%s/%s\" name=\"Manage ekg2 plugins\" node=\"http://ekg2.org/jabber/rc#ekg-manage-sessions\"/>"
				"<item jid=\"%s/%s\" name=\"Execute ANY command in ekg2\" node=\"http://ekg2.org/jabber/rc#ekg-command-execute\"/>"
				"</query></iq>", from, id, 
				s->uid+5, j->resource, s->uid+5, j->resource, 
				s->uid+5, j->resource, s->uid+5, j->resource,
				s->uid+5, j->resource, s->uid+5, j->resource,
				s->uid+5, j->resource);
		return;
	}
	/* XXX, tutaj jakies ogolne informacje co umie ekg2 */
}

/**
 * jabber_handle_iq_get_disco_info()
 *
 * Handler for IQ GET QUERY xmlns="http://jabber.org/protocol/disco#info"<br>
 * XXX
 *
 */

JABBER_HANDLER_GET_REPLY(jabber_handle_iq_get_disco_info) {
	jabber_private_t *j = s->priv;

	watch_write(j->send_watch, "<iq to=\"%s\" type=\"result\" id=\"%s\">"
			"<query xmlns=\"http://jabber.org/protocol/disco#info\">"
			"<feature var=\"http://jabber.org/protocol/commands\"/>"
			"<feature var=\"http://jabber.org/protocol/bytestreams\"/>"
			"<feature var=\"http://jabber.org/protocol/si\"/>"
			"<feature var=\"http://jabber.org/protocol/si/profile/file-transfer\"/>"
			"<feature var=\"http://jabber.org/protocol/chatstates\"/>"
			"</query></iq>", from, id);


}

/**
 * jabber_handle_iq_get_last()
 *
 * Handler for IQ GET QUERY xmlns="jabber:iq:last"<br>
 * Send reply about our last activity.<br>
 * XXX info about it from XEP/RFC
 */

JABBER_HANDLER_GET_REPLY(jabber_handle_iq_get_last) {
	jabber_private_t *j = s->priv;

	watch_write(j->send_watch, 
			"<iq to=\"%s\" type=\"result\" id=\"%s\">"
			"<query xmlns=\"jabber:iq:last\" seconds=\"%d\">"
			"</query></iq>", from, id, (time(NULL)-s->activity));
}

/**
 * jabber_handle_iq_get_version()
 *
 * Handler for IQ GET QUERY xmlns="jabber:iq:version"<br>
 * Send info about our program and system<br>
 *
 * <b>PRIVACY WARNING:</b> It'll send potential useful information like: what version of kernel you use.<br>
 * If you don't want to send this information set session variables:<br>
 * 	- <i>ver_client_name</i> - If you want spoof program name. [Although I think it's good to send info about ekg2. Because it's good program.]<br>
 * 	- <i>ver_client_version</i> - If you want to spoof program version.<br>
 * 	- <i>ver_os</i> - The most useful, to spoof OS name, version and stuff.<br>
 */

JABBER_HANDLER_GET_REPLY(jabber_handle_iq_get_version) {
	jabber_private_t *j = s->priv;

	const char *ver_os;
	const char *tmp;

	char *escaped_client_name	= jabber_escape(jabberfix((tmp = session_get(s, "ver_client_name")), DEFAULT_CLIENT_NAME));
	char *escaped_client_version	= jabber_escape(jabberfix((tmp = session_get(s, "ver_client_version")), VERSION));
	char *osversion;

	if (!(ver_os = session_get(s, "ver_os"))) {
		struct utsname buf;

		if (uname(&buf) != -1) {
			char *osver = saprintf("%s %s %s", buf.sysname, buf.release, buf.machine);
			osversion = jabber_escape(osver);
			xfree(osver);
		} else {
			osversion = xstrdup(("unknown")); /* uname failed and not ver_os session variable */
		}
	} else {
		osversion = jabber_escape(ver_os);	/* ver_os session variable */
	}

	watch_write(j->send_watch, "<iq to=\"%s\" type=\"result\" id=\"%s\">" 
			"<query xmlns=\"jabber:iq:version\">"
			"<name>%s</name>"
			"<version>%s</version>"
			"<os>%s</os></query></iq>", 
			from, id, 
			escaped_client_name, escaped_client_version, osversion);

	xfree(escaped_client_name);
	xfree(escaped_client_version);
	xfree(osversion);
}

/*
 * jabber_handle_iq_ping()
 *
 * XEP-0199
 */

JABBER_HANDLER_GET_REPLY(jabber_handle_iq_ping) {
	jabber_private_t *j = s->priv;

	watch_write(j->send_watch, "<iq to=\"%s\" id=\"%s\" type=\"result\"/>\n",
			from, id);
}

static const struct jabber_iq_generic_handler jabber_iq_get_handlers[] = {
	{ "query",		"jabber:iq:last",				jabber_handle_iq_get_last },
	{ NULL,			"jabber:iq:version",				jabber_handle_iq_get_version },
	{ NULL,			"http://jabber.org/protocol/disco#items",	jabber_handle_iq_get_disco },
	{ NULL,			"http://jabber.org/protocol/disco#info",	jabber_handle_iq_get_disco_info },

	{ "ping",		"urn:xmpp:ping",				jabber_handle_iq_ping },

	{ "",			NULL,						NULL }
};

