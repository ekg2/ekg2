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
				"<feature var=\"jabber:iq:last\"/>"						/* jabber_handle_iq_get_last() */
				"<feature var=\"jabber:iq:version\"/>"						/* jabber_handle_iq_get_version() */
				"<feature var=\"urn:xmpp:ping\"/>"						/* jabber_handle_iq_ping() */
				"<feature var=\"http://jabber.org/protocol/chatstates\"/>"
				"<feature var=\"google:mail:notify\"/>"
			"</query></iq>", from, id);
#if 0
				"<feature var=\"http://jabber.org/protocol/commands\"/>"
				"<feature var=\"http://jabber.org/protocol/bytestreams\"/>"
				"<feature var=\"http://jabber.org/protocol/si\"/>"
				"<feature var=\"http://jabber.org/protocol/si/profile/file-transfer\"/>"
#endif

}

/**
 * jabber_handle_iq_get_last()
 *
 * <b>XEP-0012: Last Activity</b> (http://www.xmpp.org/extensions/xep-0012.html) <i>[1.2 2007-02-15]</i> (<b><i>iq:type='get' iq::query:xmlns='jabber:iq:last'</i></b>)<br>
 *
 * Send reply about our last activity.<br>
 *
 * @todo From XEP-0012:
 * 	 - 8. A client MUST provide a way for a human user to disable sending of Last Activity responses from the client's full JID (<node@domain.tld/resource>).
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
 * <b>XEP-0092: Software Version</b> (http://www.xmpp.org/extensions/xep-0092.html) <i>[1.1 2007-02-15]</i> (<b><i>iq:type='get' iq::query:xmlns='jabber:iq:version'</i></b>)<br>
 *
 * Send info about our program and system<br>
 *
 * @note 
 * 	<b>PRIVACY WARNING:</b> It'll send potential useful information like: what version of kernel you use.<br>
 * 	If you don't want to send this information set session variables:<br>
 * 		- <i>ver_client_name</i> - If you want spoof program name. [Although I think it's good to send info about ekg2. Because it's good program.]<br>
 * 		- <i>ver_client_version</i> - If you want to spoof program version.<br>
 * 		- <i>ver_os</i> - The most useful, to spoof OS name, version and stuff.<br>
 *
 * @todo From XEP-0092:
 * 	 - 5. an application MUST provide a way for a human user or administrator to disable sharing of information about the operating system.
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

/**
 * jabber_handle_iq_ping()
 *
 * <b>XEP-0199: XMPP Ping</b> (http://www.xmpp.org/extensions/xep-0199.html) <i>[1.0 2007-06-12]</i> (<b><i>iq:type='get' iq::ping:xmlns='urn:xmpp:ping'</i></b>)<br>
 *
 * @note From XEP-0199:
 * 	 - 6. If a connected resource receives a ping request but it does not want to reveal its network availability to the sender for any reason 
 * 	 (e.g., because the sender is not authorized to know the connected resource's availability),
 * 	 then it too MUST reply with a <service-unavailable/> error.
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

// vim:syn=c
