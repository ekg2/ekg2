#define JABBER_HANDLER_RESULT(x) 	static void x(session_t *s, xmlnode_t *n, const char *from, const char *id)
#define JABBER_HANDLER_SET(x)	 	static void x(session_t *s, xmlnode_t *n, const char *from, const char *id)

JABBER_HANDLER_RESULT(jabber_handle_iq_result_disco) {
	xmlnode_t *item = xmlnode_find_child(n, "item");
	char *uid = jabber_unescape(from);

	int iscontrol = !xstrncmp(id, "control", 7);

	if (item) {
		int i = 1;
		print(iscontrol ? "jabber_remotecontrols_list_begin" : "jabber_transport_list_begin", session_name(s), uid);
		for (; item; item = item->next, i++) {
			char *sjid  = jabber_unescape(jabber_attr(item->atts, "jid"));
			char *sdesc = jabber_unescape(jabber_attr(item->atts, "name"));
			char *snode = jabber_unescape(jabber_attr(item->atts, "node"));

			if (!iscontrol)	print(snode ? "jabber_transport_list_item_node" : "jabber_transport_list_item", 
					session_name(s), uid, sjid, snode, sdesc, itoa(i));
			else		print("jabber_remotecontrols_list_item", 
					session_name(s), uid, sjid, snode, sdesc, itoa(i));
			xfree(sdesc);
			xfree(sjid);
			xfree(snode);
		}
		print(iscontrol ? "jabber_remotecontrols_list_end"	: "jabber_transport_list_end", session_name(s), uid);
	} else	print(iscontrol ? "jabber_remotecontrols_list_nolist" : "jabber_transport_list_nolist", session_name(s), uid);
	xfree(uid);
}

JABBER_HANDLER_RESULT(jabber_handle_iq_result_disco_info) {
	const char *wezel = jabber_attr(n->atts, "node");
	char *uid = jabber_unescape(from);
	xmlnode_t *node;

	print((wezel) ? "jabber_transinfo_begin_node" : "jabber_transinfo_begin", session_name(s), uid, wezel);

	for (node = n->children; node; node = node->next) {
		if (!xstrcmp(node->name, "identity")) {
			char *name	= jabber_unescape(jabber_attr(node->atts, "name"));	/* nazwa */
//			char *cat	= jabber_attr(node->atts, "category");			/* server, gateway, directory */
//			char *type	= jabber_attr(node->atts, "type");			/* typ: im */

			if (name) /* jesli nie ma nazwy don't display it. */
				print("jabber_transinfo_identify" /* _server, _gateway... ? */, session_name(s), uid, name);

			xfree(name);
		} else if (!xstrcmp(node->name, "feature")) {
			char *var = jabber_attr(node->atts, "var");
			char *tvar = NULL; /* translated */
			int user_command = 0;

			/* dj, jakas glupota... ale ma ktos pomysl zeby to inaczej zrobic?... jeszcze istnieje pytanie czy w ogole jest sens to robic.. */
			if (!xstrcmp(var, "http://jabber.org/protocol/disco#info")) 		tvar = "/xmpp:transpinfo";
			else if (!xstrcmp(var, "http://jabber.org/protocol/disco#items")) 	tvar = "/xmpp:transports";
			else if (!xstrcmp(var, "http://jabber.org/protocol/disco"))		tvar = "/xmpp:transpinfo && /xmpp:transports";
			else if (!xstrcmp(var, "http://jabber.org/protocol/muc"))		tvar = "/xmpp:mucpart && /xmpp:mucjoin";
			else if (!xstrcmp(var, "http://jabber.org/protocol/stats"))		tvar = "/xmpp:stats";
			else if (!xstrcmp(var, "jabber:iq:register"))		    		tvar = "/xmpp:register";
			else if (!xstrcmp(var, "jabber:iq:search"))				tvar = "/xmpp:search";

			else if (!xstrcmp(var, "http://jabber.org/protocol/bytestreams")) { user_command = 1; tvar = "/xmpp:dcc (PROT: BYTESTREAMS)"; }
			else if (!xstrcmp(var, "http://jabber.org/protocol/si/profile/file-transfer")) { user_command = 1; tvar = "/xmpp:dcc"; }
			else if (!xstrcmp(var, "http://jabber.org/protocol/commands")) { user_command = 1; tvar = "/xmpp:control"; } 
			else if (!xstrcmp(var, "jabber:iq:version"))	{ user_command = 1;	tvar = "/xmpp:ver"; }
			else if (!xstrcmp(var, "jabber:iq:last"))	{ user_command = 1;	tvar = "/xmpp:lastseen"; }
			else if (!xstrcmp(var, "vcard-temp"))		{ user_command = 1;	tvar = "/xmpp:change && /xmpp:userinfo"; }
			else if (!xstrcmp(var, "message"))		{ user_command = 1;	tvar = "/xmpp:msg"; }

			else if (!xstrcmp(var, "http://jabber.org/protocol/vacation"))	{ user_command = 2;	tvar = "/xmpp:vacation"; }
			else if (!xstrcmp(var, "presence-invisible"))	{ user_command = 2;	tvar = "/invisible"; } /* we ought use jabber:iq:privacy */
			else if (!xstrcmp(var, "jabber:iq:privacy"))	{ user_command = 2;	tvar = "/xmpp:privacy"; }
			else if (!xstrcmp(var, "jabber:iq:private"))	{ user_command = 2;	tvar = "/xmpp:private && /xmpp:config && /xmpp:bookmark"; }

			if (tvar)	print(	user_command == 2 ? "jabber_transinfo_comm_not" : 
					user_command == 1 ? "jabber_transinfo_comm_use" : "jabber_transinfo_comm_ser", 

					session_name(s), uid, tvar, var);
			else		print("jabber_transinfo_feature", session_name(s), uid, var, var);
		} else if (!xstrcmp(node->name, "x") && !xstrcmp(node->xmlns, "jabber:x:data") && !xstrcmp(jabber_attr(node->atts, "type"), "result")) {
			jabber_handle_xmldata_result(s, node->children, uid);
		}
	}
	print("jabber_transinfo_end", session_name(s), uid);
	xfree(uid);
}

/**
 * jabber_handle_iq_result_last()
 *
 * Handler for IQ RESULT QUERY xmlns="jabber:iq:last"<br>
 * Display info about server uptime/client idle/client logout time.
 *
 * @todo It's ugly written.
 */

JABBER_HANDLER_RESULT(jabber_handle_iq_result_last) {
	const char *last = jabber_attr(n->atts, "seconds");
	char buff[21];
	char *from_str;
	int seconds;

	seconds = atoi(last);

	if ((seconds>=0) && (seconds < 999*24*60*60-1))
		/* days, hours, minutes, seconds... */
		snprintf(buff, sizeof(buff), _("%03dd %02dh %02dm %02ds"),seconds / 86400, \
				(seconds / 3600) % 24, (seconds / 60) % 60, seconds % 60);
	else
		strcpy(buff, _("very long"));

	from_str = (from) ? jabber_unescape(from) : NULL;

	print(
		(xstrchr(from_str, '/') ? "jabber_lastseen_idle" :	/* if we have resource than display -> user online+idle		*/
		xstrchr(from_str, '@') ? "jabber_lastseen_response" :	/* if we have '@' in xmpp: than display ->  user logged out	*/
					"jabber_lastseen_uptime"),	/* otherwise we have server -> server up for: 			*/
			jabberfix(from_str, "unknown"), buff);
	xfree(from_str);
}

/**
 * jabber_handle_iq_result_version()
 *
 * Handler for IQ RESULT QUERY xmlns="jabber:iq:version"<br>
 * Display info about smb program version and system<br>
 *
 */

JABBER_HANDLER_RESULT(jabber_handle_iq_result_version) {
	xmlnode_t *name = xmlnode_find_child(n, "name");
	xmlnode_t *version = xmlnode_find_child(n, "version");
	xmlnode_t *os = xmlnode_find_child(n, "os");

	char *from_str	= (from) ?	jabber_unescape(from) : NULL;
	char *name_str	= (name) ?	jabber_unescape(name->data) : NULL;
	char *version_str = (version) ? jabber_unescape(version->data) : NULL;
	char *os_str	= (os) ?	jabber_unescape(os->data) : NULL;

	print("jabber_version_response",
			jabberfix(from_str, "unknown"), jabberfix(name_str, "unknown"), 
			jabberfix(version_str, "unknown"), jabberfix(os_str, "unknown"));
	xfree(os_str);
	xfree(version_str);
	xfree(name_str);
	xfree(from_str);
}

JABBER_HANDLER_RESULT(jabber_handle_iq_result_privacy) {
	jabber_private_t *j = s->priv;

	xmlnode_t *active 	= xmlnode_find_child(n, "active");
	xmlnode_t *def		= xmlnode_find_child(n, "default");
	char *activename   	= active ? jabber_attr(active->atts, "name") : NULL;
	char *defaultname 	= def ? jabber_attr(def->atts, "name") : NULL;

	xmlnode_t *node;
	int i = 0;

	for (node = n->children; node; node = node->next) {
		if (!xstrcmp(node->name, "list")) {
			if (node->children) {	/* Display all details */
				list_t *lista = NULL;
				xmlnode_t *temp;
				i = -1;

				if (session_int_get(s, "auto_privacylist_sync") != 0) {
					const char *list = session_get(s, "privacy_list");
					if (!list) list = "ekg2";

					if (!xstrcmp(list, jabber_attr(node->atts, "name"))) {
						jabber_privacy_free(j);
						lista = &(j->privacy);
					}
				}

				print("jabber_privacy_item_header", session_name(s), j->server, jabber_attr(node->atts, "name"));

				for (temp=node->children; temp; temp = temp->next) {	
					if (!xstrcmp(temp->name, "item")) {
						/* (we should sort it by order! XXX) (server send like it was set) */
						/* a lot TODO */
						xmlnode_t *iq		= xmlnode_find_child(temp, "iq");
						xmlnode_t *message	= xmlnode_find_child(temp, "message");
						xmlnode_t *presencein	= xmlnode_find_child(temp, "presence-in");
						xmlnode_t *presenceout	= xmlnode_find_child(temp, "presence-out");

						char *value = jabber_attr(temp->atts, "value");
						char *type  = jabber_attr(temp->atts, "type");
						char *action = jabber_attr(temp->atts, "action");
						char *jid, *formated;
						char *formatedx;

						if (!xstrcmp(type, "jid")) 		jid = saprintf("xmpp:%s", value);
						else if (!xstrcmp(type, "group")) 	jid = saprintf("@%s", value);
						else					jid = xstrdup(value);

						formated = format_string(
								format_find(!xstrcmp(action,"allow") ? "jabber_privacy_item_allow" : "jabber_privacy_item_deny"), jid);

						formatedx = format_string(
								format_find(!xstrcmp(action,"allow") ? "jabber_privacy_item_allow" : "jabber_privacy_item_deny"), "*");

						xfree(formatedx);
						formatedx = xstrdup("x");

						if (lista) {
							jabber_iq_privacy_t *item = xmalloc(sizeof(jabber_iq_privacy_t));

							item->type  = xstrdup(type);
							item->value = xstrdup(value);
							item->allow = !xstrcmp(action,"allow");

							if (iq)		item->items |= PRIVACY_LIST_IQ;
							if (message)	item->items |= PRIVACY_LIST_MESSAGE;
							if (presencein)	item->items |= PRIVACY_LIST_PRESENCE_IN;
							if (presenceout)item->items |= PRIVACY_LIST_PRESENCE_OUT;
							item->order = atoi(jabber_attr(temp->atts, "order"));

							LIST_ADD_SORTED(lista, item, 0, jabber_privacy_add_compare);
						}

						print("jabber_privacy_item", session_name(s), j->server, 
								jabber_attr(temp->atts, "order"), formated, 
								message		? formatedx : " ",
								presencein	? formatedx : " ", 
								presenceout	? formatedx : " ", 
								iq		? formatedx : " ");

						xfree(formated);
						xfree(formatedx);
						xfree(jid);
					}
				}
				{	/* just help... */
					char *allowed = format_string(format_find("jabber_privacy_item_allow"), "xmpp:allowed - JID ALLOWED");
					char *denied  = format_string(format_find("jabber_privacy_item_deny"), "@denied - GROUP DENIED");
					print("jabber_privacy_item_footer", session_name(s), j->server, allowed, denied);
					xfree(allowed); xfree(denied);
				}
			} else { 		/* Display only name */
				int dn = 0;
				/* rekurencja, ask for this item (?) */
				if (!i) print("jabber_privacy_list_begin", session_name(s), j->server);
				if (i != -1) i++;
				if (active && !xstrcmp(jabber_attr(node->atts, "name"), activename)) {
					print("jabber_privacy_list_item_act", session_name(s), j->server, itoa(i), activename); dn++; }
				if (def && !xstrcmp(jabber_attr(node->atts, "name"), defaultname)) {
					print("jabber_privacy_list_item_def", session_name(s), j->server, itoa(i), defaultname); dn++; }
				if (!dn) print("jabber_privacy_list_item", session_name(s), j->server, itoa(i), jabber_attr(node->atts, "name")); 
			}
		}
	}
	if (i > 0)  print("jabber_privacy_list_end", session_name(s), j->server);
	if (i == 0) print("jabber_privacy_list_noitem", session_name(s), j->server);
}

JABBER_HANDLER_RESULT(jabber_handle_iq_result_private) {
	jabber_private_t *j = s->priv;
	xmlnode_t *node;

	for (node = n->children; node; node = node->next) {
		char *lname	= jabber_unescape(node->name);
		const char *ns	= node->xmlns;
		xmlnode_t *child;

		int config_display = 1;
		int bookmark_display = 1;
		int quiet = 0;

		if (!xstrcmp(node->name, "ekg2")) {
			if (!xstrcmp(ns, "ekg2:prefs") && !xstrncmp(id, "config", 6)) 
				config_display = 0;	/* if it's /xmpp:config --get (not --display) we don't want to display it */ 
			/* XXX, other */
		}
		if (!xstrcmp(node->name, "storage")) {
			if (!xstrcmp(ns, "storage:bookmarks") && !xstrncmp(id, "config", 6))
				bookmark_display = 0;	/* if it's /xmpp:bookmark --get (not --display) we don't want to display it (/xmpp:bookmark --get performed @ connect) */
		}

		if (!config_display || !bookmark_display) quiet = 1;

		if (node->children)	printq("jabber_private_list_header", session_name(s), lname, ns);
		if (!xstrcmp(node->name, "ekg2") && !xstrcmp(ns, "ekg2:prefs")) { 	/* our private struct, containing `full` configuration of ekg2 */
			for (child = node->children; child; child = child->next) {
				char *cname	= jabber_unescape(child->name);
				char *cvalue	= jabber_unescape(child->data);
				if (!xstrcmp(child->name, "plugin") && !xstrcmp(child->xmlns, "ekg2:plugin")) {
					xmlnode_t *temp;
					printq("jabber_private_list_plugin", session_name(s), lname, ns, jabber_attr(child->atts, "name"), jabber_attr(child->atts, "prio"));
					for (temp = child->children; temp; temp = temp->next) {
						char *snname = jabber_unescape(temp->name);
						char *svalue = jabber_unescape(temp->data);
						printq("jabber_private_list_subitem", session_name(s), lname, ns, snname, svalue);
						xfree(snname);
						xfree(svalue);
					}
				} else if (!xstrcmp(child->name, "session") && !xstrcmp(child->xmlns, "ekg2:session")) {
					xmlnode_t *temp;
					printq("jabber_private_list_session", session_name(s), lname, ns, jabber_attr(child->atts, "uid"));
					for (temp = child->children; temp; temp = temp->next) {
						char *snname = jabber_unescape(temp->name);
						char *svalue = jabber_unescape(temp->data);
						printq("jabber_private_list_subitem", session_name(s), lname, ns, snname, svalue);
						xfree(snname);
						xfree(svalue);
					}
				} else	printq("jabber_private_list_item", session_name(s), lname, ns, cname, cvalue);
				xfree(cname); xfree(cvalue);
			}
		} else if (!xstrcmp(node->name, "storage") && !xstrcmp(ns, "storage:bookmarks")) { /* JEP-0048: Bookmark Storage */
			/* destroy previously-saved list */
			jabber_bookmarks_free(j);

			/* create new-one */
			for (child = node->children; child; child = child->next) {
				jabber_bookmark_t *book = xmalloc(sizeof(jabber_bookmark_t));

				debug_function("[JABBER:IQ:PRIVATE BOOKMARK item=%s\n", child->name);
				if (!xstrcmp(child->name, "conference")) {
					xmlnode_t *temp;

					book->type	= JABBER_BOOKMARK_CONFERENCE;
					book->private.conf		= xmalloc(sizeof(jabber_bookmark_conference_t));
					book->private.conf->name	= jabber_unescape(jabber_attr(child->atts, "name"));
					book->private.conf->jid		= jabber_unescape(jabber_attr(child->atts, "jid"));
					book->private.conf->autojoin	= !xstrcmp(jabber_attr(child->atts, "autojoin"), "true");

					book->private.conf->nick	= jabber_unescape( (temp = xmlnode_find_child(child, "nick")) ? temp->data : NULL);
					book->private.conf->pass        = jabber_unescape( (temp = xmlnode_find_child(child, "password")) ? temp->data : NULL);

					printq("jabber_bookmark_conf", session_name(s), book->private.conf->name, book->private.conf->jid,
							book->private.conf->autojoin ? "X" : " ", book->private.conf->nick, book->private.conf->pass);

				} else if (!xstrcmp(child->name, "url")) {
					book->type	= JABBER_BOOKMARK_URL;
					book->private.url	= xmalloc(sizeof(jabber_bookmark_url_t));
					book->private.url->name	= jabber_unescape(jabber_attr(child->atts, "name"));
					book->private.url->url	= jabber_unescape(jabber_attr(child->atts, "url"));

					printq("jabber_bookmark_url", session_name(s), book->private.url->name, book->private.url->url);

				} else { debug_error("[JABBER:IQ:PRIVATE:BOOKMARK UNKNOWNITEM=%s\n", child->name); xfree(book); book = NULL; }

				if (book) list_add(&j->bookmarks, book, 0);
			}
		} else {
			/* DISPLAY IT ? w jakim formacie?
			 * + CHILD item=value item2=value2
			 *  - SUBITEM .....
			 *  - SUBITEM........
			 *  + SUBCHILD ......
			 *   - SUBITEM
			 * ? XXX
			 */
		}
		if (node->children)	printq("jabber_private_list_footer", session_name(s), lname, ns);
		else 			printq("jabber_private_list_empty", session_name(s), lname, ns);
		xfree(lname);
	}
}

JABBER_HANDLER_RESULT(jabber_handle_gmail_result_mailbox) {
	jabber_private_t *j = s->priv;

	char *mailcount = jabber_attr(n->atts, "total-matched");
	int tid_set = 0;
	xmlnode_t *child;
	xfree(j->last_gmail_result_time);
	j->last_gmail_result_time = xstrdup(jabber_attr(n->atts, "result-time"));

	print("gmail_count", session_name(s), mailcount);

	/* http://code.google.com/apis/talk/jep_extensions/gmail.html */
	for (child = n->children; child; child = child->next) {
		if (!xstrcmp(child->name, "mail-thread-info")) {
			if (!tid_set)
			{
				xfree(j->last_gmail_tid);
				j->last_gmail_tid = xstrdup(jabber_attr(child->atts, "tid"));
			}
			tid_set = 1;
			xmlnode_t *subchild;
			string_t from = string_init(NULL);

			char *amessages = jabber_attr(child->atts, "messages");		/* messages count in thread */
			char *subject = NULL;
			int firstsender = 1;

			for (subchild = child->children; subchild; subchild = subchild->next) {
				if (0) {
				} else if (!xstrcmp(subchild->name, "subject")) {
					if (xstrcmp(subchild->data, "")) {
						xfree(subject);
						subject = jabber_unescape(subchild->data);
					}

				} else if (!xstrcmp(subchild->name, "senders")) {
					xmlnode_t *senders;

					for (senders = subchild->children; senders; senders = senders->next) {
						char *aname = jabber_unescape(jabber_attr(senders->atts, "name"));
						char *amail = jabber_attr(senders->atts, "address");

						if (!firstsender)
							string_append(from, ", ");

						if (aname) {
							char *tmp = saprintf("%s <%s>", aname, amail);
							string_append(from, tmp);
							xfree(tmp);
						} else {
							string_append(from, amail);
						}

						firstsender = 0;
						xfree(aname);
					}
				} else if (!xstrcmp(subchild->name, "labels")) {	/* <labels>        | 
											   A tag that contains a pipe ('|') delimited list of labels applied to this thread. */
				} else if (!xstrcmp(subchild->name, "snippet")) {	/* <snippet>       | 
											   A snippet from the body of the email. This must be HTML-encoded. */
				} else debug_error("[jabber] google:mail:notify/mail-thread-info wtf: %s\n", __(subchild->name));
			}

			print((amessages && atoi(amessages) > 1) ? "gmail_thread" : "gmail_mail", 
					session_name(s), from->str, jabberfix(subject, "(no subject)"), amessages);

			string_free(from, 1);
			xfree(subject);
		} else debug_error("[jabber, iq] google:mail:notify wtf: %s\n", __(child->name));
	}
	if (mailcount && atoi(mailcount)) /* we don't want to beep or send events if no new mail is available */
		newmail_common(s);
}

JABBER_HANDLER_RESULT(jabber_handle_iq_result_search) {
	jabber_private_t *j = s->priv;

	xmlnode_t *node;
	int rescount = 0;
	char *uid = jabber_unescape(from);
	int formdone = 0;

	for (node = n->children; node; node = node->next) {
		if (!xstrcmp(node->name, "item")) rescount++;
	}
	if (rescount > 1) print("jabber_search_begin", session_name(s), uid);

	for (node = n->children; node; node = node->next) {
		if (!xstrcmp(node->name, "item")) {
			xmlnode_t *tmp;
			char *jid 	= jabber_attr(node->atts, "jid");
			char *nickname	= tlenjabber_unescape( (tmp = xmlnode_find_child(node, "nick"))  ? tmp->data : NULL);
			char *fn	= tlenjabber_unescape( (tmp = xmlnode_find_child(node, "first")) ? tmp->data : NULL);
			char *lastname	= tlenjabber_unescape( (tmp = xmlnode_find_child(node, "last"))  ? tmp->data : NULL);
			char *email	= tlenjabber_unescape( (tmp = xmlnode_find_child(node, "email")) ? tmp->data : NULL);

			/* idea about displaink user in depend of number of users founded gathered from gg plugin */
			print(rescount > 1 ? "jabber_search_items" : "jabber_search_item", 
					session_name(s), uid, jid, nickname, fn, lastname, email);
			xfree(nickname);
			xfree(fn);
			xfree(lastname);
			xfree(email);
		} else {
			xmlnode_t *reg;
			if (rescount == 0) rescount = -1;
			if (formdone) continue;

			for (reg = n->children; reg; reg = reg->next) {
				if (!xstrcmp(reg->name, "x") && !xstrcmp("jabber:x:data", reg->xmlns)) {
					if (!xstrcmp(jabber_attr(reg->atts, "type"), "form")) {
						formdone = 1;
						jabber_handle_xmldata_form(s, uid, "search", reg->children, "--jabber_x_data");
						break;
					} else if (!xstrcmp(jabber_attr(reg->atts, "type"), "result")) {
						formdone = 1;
						jabber_handle_xmldata_result(s, reg->children, uid);
						break;
					}
				}
			}

			if (!formdone) {
				/* XXX */
			}
		}
	}
	if (rescount > 1) print("jabber_search_end", session_name(s), uid);
	if (rescount == 0) print("search_not_found"); /* not found */
	xfree(uid);
}

JABBER_HANDLER_RESULT(jabber_handle_result_pubsub) {
	xmlnode_t *p;

	for (p = n->children; p; p = p->next) {
		if (!xstrcmp(p->name, "items")) {
			const char *nodename = jabber_attr(p->atts, "node");
			xmlnode_t *node;

			debug_error("XXX %s\n", __(nodename));

			for (node = p->children; node; node = node->next) {
				if (!xstrcmp(node->name, "item")) {
					const char *itemid = jabber_attr(node->atts, "id");
					debug_error("XXX XXX %s\n", __(itemid));

					/* XXX HERE, node->children... is entry. */
				} else debug_error("[%s:%d] wtf? %s\n", __FILE__, __LINE__, __(node->name));
			} 
		} else debug_error("[%s:%d] wtf? %s\n", __FILE__, __LINE__, __(p->name));
	}
}


/*******************************************************************************************************/
/* these need cleanup SET/ RESULT */

JABBER_HANDLER_RESULT(jabber_handle_iq_roster) {
	int roster_retrieved = (session_int_get(s, "__roster_retrieved") == 1);

	jabber_private_t *j = s->priv;

	xmlnode_t *item = xmlnode_find_child(n, "item");

	for (; item ; item = item->next) {
		const char *jid = jabber_attr(item->atts, "jid");
		userlist_t *u;
		char *uid;

		if (j->istlen) {
			/* do it like tlen does with roster - always @tlen.pl */
			char *atsign	= xstrchr(jid, '@');

			if (atsign)
				*atsign	= 0;
			uid = saprintf("tlen:%s@tlen.pl", jid);
		} else
			uid = saprintf("xmpp:%s", jid);

		/* jeśli element rostera ma subscription = remove to tak naprawde użytkownik jest usuwany;
		   w przeciwnym wypadku - nalezy go dopisać do userlisty; dodatkowo, jesli uzytkownika
		   mamy już w liscie, to przyszla do nas zmiana rostera; usunmy wiec najpierw, a potem
		   sprawdzmy, czy warto dodawac :) */

		if (roster_retrieved && (u = userlist_find(s, uid)))
			userlist_remove(s, u);

		if (!xstrncmp(jabber_attr(item->atts, "subscription"), "remove", 6)) {
			/* nic nie robimy, bo juz usuniete */
		} else {
			char *nickname = tlenjabber_unescape(jabber_attr(item->atts, "name"));

			const char *authval;
			xmlnode_t *group;

			u = userlist_add(s, uid, nickname); 

			if ((authval = jabber_attr(item->atts, "subscription"))) {
				jabber_userlist_private_t *up = jabber_userlist_priv_get(u);

				if (up)
					for (up->authtype = EKG_JABBER_AUTH_BOTH; (up->authtype > EKG_JABBER_AUTH_NONE) && xstrcasecmp(authval, jabber_authtypes[up->authtype]); (up->authtype)--);

				if (!up || !(up->authtype & EKG_JABBER_AUTH_TO)) {
					if (u && u->status == EKG_STATUS_NA)
						u->status = EKG_STATUS_UNKNOWN;
				} else {
					if (u && u->status == EKG_STATUS_UNKNOWN)
						u->status = EKG_STATUS_NA;
				}
			}

			for (group = xmlnode_find_child(item, "group"); group ; group = group->next ) {
				char *gname = jabber_unescape(group->data);
				ekg_group_add(u, gname);
				xfree(gname);
			}

			if (roster_retrieved) {
				command_exec_format(NULL, s, 1, ("/auth --probe %s"), uid);
			}
			xfree(nickname); 
		}
		xfree(uid);
	}; /* for */

	{		/* nickname generator */
		list_t l;

		for (l = s->userlist; l;) {
			userlist_t *u = l->data;

			if (u && !u->nickname) {
				char *myuid	= xstrdup(u->uid);
				char *userpart	= xstrdup(u->uid);
				char *tmp;
				const char **cp;

				const char *possibilities[] = {
					userpart+5,	/* user-part of UID */
					myuid+5,	/* JID without resource */
					u->uid+5,	/* JID with resource */
					myuid,		/* UID without resource */
					u->uid,		/* UID with resource */
					NULL };

				if ((tmp = xstrchr(userpart, '@')))	*tmp	= 0;
				if ((tmp = xstrchr(myuid, '/')))	*tmp	= 0;

				for (cp = possibilities; *cp; cp++) {
					list_t m;

					for (m = s->userlist; m; m = m->next) {
						userlist_t *w = m->data;

						if (w && w->nickname && !xstrcasecmp(w->nickname, *cp))
							break;
					}

					if (!m)
						break;
				}

				if (*cp) {
					u->nickname = xstrdup(*cp);
					userlist_replace(s, u);		/* resort */

					/* sorting changes order,
					 * so we need to start from beginning
					 * sorry */
					l = s->userlist;

					xfree(userpart);
					xfree(myuid);
					continue;
				} else
					debug_error("[jabber] can't find any free nickname for UID %s.. that's kinda bitch!\n", u->uid);

				xfree(userpart);
				xfree(myuid);
			}

			l = l->next;
		}
	}

	session_int_set(s, "__roster_retrieved", 1);
}

JABBER_HANDLER_RESULT(jabber_handle_iq_result_register) {
	xmlnode_t *reg;
	char *from_str = (from) ? jabber_unescape(from) : xstrdup(_("unknown"));
	int done = 0;

	for (reg = n->children; reg; reg = reg->next) {
		if (!xstrcmp(reg->name, "x") && !xstrcmp("jabber:x:data", reg->xmlns) && 
				( !xstrcmp("form", jabber_attr(reg->atts, "type")) || !jabber_attr(reg->atts, "type")))
		{
			done = 1;
			jabber_handle_xmldata_form(s, from_str, "register", reg->children, "--jabber_x_data");
			break;
		}
	}
	if (!done && !n->children) { 
		/* XXX */
		done = 1;
	}
	if (!done) {
		xmlnode_t *instr = xmlnode_find_child(n, "instructions");
		print("jabber_form_title", session_name(s), from_str, from_str);

		if (instr && instr->data) {
			char *instr_str = jabber_unescape(instr->data);
			print("jabber_form_instructions", session_name(s), from_str, instr_str);
			xfree(instr_str);
		}
		print("jabber_form_command", session_name(s), from_str, "register", "");

		for (reg = n->children; reg; reg = reg->next) {
			char *jname, *jdata;
			if (!xstrcmp(reg->name, "instructions")) continue;

			jname = jabber_unescape(reg->name);
			if (!xstrcmp(jname, "password"))
				jdata = xstrdup("(...)");
			else
				jdata = jabber_unescape(reg->data);
			print("jabber_registration_item", session_name(s), from_str, jname, jdata);
			xfree(jname);
			xfree(jdata);
		}
		print("jabber_form_end", session_name(s), from_str, "register");
	}
	xfree(from_str);
}

JABBER_HANDLER_RESULT(jabber_handle_vcard) {
	xmlnode_t *fullname = xmlnode_find_child(n, "FN");
	xmlnode_t *nickname = xmlnode_find_child(n, "NICKNAME");
	xmlnode_t *birthday = xmlnode_find_child(n, "BDAY");
	xmlnode_t *adr  = xmlnode_find_child(n, "ADR");
	xmlnode_t *city = xmlnode_find_child(adr, "LOCALITY");
	xmlnode_t *desc = xmlnode_find_child(n, "DESC");

	char *from_str     = (from)	? jabber_unescape(from) : NULL;
	char *fullname_str = (fullname) ? jabber_unescape(fullname->data) : NULL;
	char *nickname_str = (nickname) ? jabber_unescape(nickname->data) : NULL;
	char *bday_str     = (birthday) ? jabber_unescape(birthday->data) : NULL;
	char *city_str     = (city)	? jabber_unescape(city->data) : NULL;
	char *desc_str     = (desc)	? jabber_unescape(desc->data) : NULL;

	print("jabber_userinfo_response", 
			jabberfix(from_str, _("unknown")), 	jabberfix(fullname_str, _("unknown")),
			jabberfix(nickname_str, _("unknown")),	jabberfix(bday_str, _("unknown")),
			jabberfix(city_str, _("unknown")),	jabberfix(desc_str, _("unknown")));
	xfree(desc_str);
	xfree(city_str);
	xfree(bday_str);
	xfree(nickname_str);
	xfree(fullname_str);
	xfree(from_str);
}



JABBER_HANDLER_RESULT(jabber_handle_iq_result_vacation) {
	xmlnode_t *temp;

	char *message	= jabber_unescape( (temp = xmlnode_find_child(n, "message")) ? temp->data : NULL);
	char *begin	= (temp = xmlnode_find_child(n, "start")) && temp->data ? temp->data : _("begin");
	char *end	= (temp = xmlnode_find_child(n, "end")) && temp->data  ? temp->data : _("never");

	print("jabber_vacation", session_name(s), message, begin, end);

	xfree(message);
}

JABBER_HANDLER_RESULT(jabber_handle_iq_muc_owner) {
	xmlnode_t *node;
	int formdone = 0;
	char *uid = jabber_unescape(from);

	for (node = n->children; node; node = node->next) {
		if (!xstrcmp(node->name, "x") && !xstrcmp("jabber:x:data", node->xmlns)) {
			if (!xstrcmp(jabber_attr(node->atts, "type"), "form")) {
				formdone = 1;
				jabber_handle_xmldata_form(s, uid, "admin", node->children, NULL);
				break;
			} 
		}
	}
//	if (!formdone) ;	// XXX
	xfree(uid);
}

JABBER_HANDLER_RESULT(jabber_handle_iq_muc_admin) {
	xmlnode_t *node;
	int count = 0;

	for (node = n->children; node; node = node->next) {
		if (!xstrcmp(node->name, "item")) {
			char *jid		= jabber_attr(node->atts, "jid");
//			char *aff		= jabber_attr(node->atts, "affiliation");
			xmlnode_t *reason	= xmlnode_find_child(node, "reason");
			char *rsn		= reason ? jabber_unescape(reason->data) : NULL;

			print("jabber_muc_banlist", session_name(s), from, jid, rsn ? rsn : "", itoa(++count));
			xfree(rsn);
		}
	}
}

JABBER_HANDLER_RESULT(jabber_handle_iq_result_new_mail) {
	jabber_private_t *j = s->priv;

	watch_write(j->send_watch, "<iq type=\"get\" id=\"gmail%d\"><query xmlns=\"google:mail:notify\"/></iq>", j->id++);
}

JABBER_HANDLER_SET(jabber_handle_iq_set_new_mail) {
	jabber_private_t *j = s->priv;

	print("gmail_new_mail", session_name(s));
	watch_write(j->send_watch, "<iq type='result' id='%s'/>", jabber_attr(n->atts, "id"));
	if (j->last_gmail_result_time && j->last_gmail_tid)
		watch_write(j->send_watch, "<iq type=\"get\" id=\"gmail%d\"><query xmlns=\"google:mail:notify\" newer-than-time=\"%s\" newer-than-tid=\"%s\" /></iq>", j->id++, 
			j->last_gmail_result_time, j->last_gmail_tid);
	else
		watch_write(j->send_watch, "<iq type=\"get\" id=\"gmail%d\"><query xmlns=\"google:mail:notify\"/></iq>", j->id++);
}

JABBER_HANDLER_SET(jabber_handle_si_set) {
	xmlnode_t *p;

	if (((p = xmlnode_find_child(n, "file")))) {  /* JEP-0096: File Transfer */
		dcc_t *D;
		char *uin = jabber_unescape(from);
		char *uid;
		char *filename	= jabber_unescape(jabber_attr(p->atts, "name"));
		char *size 	= jabber_attr(p->atts, "size");
#if 0
		xmlnode_t *range; /* unused? */
#endif
		jabber_dcc_t *jdcc;

		uid = saprintf("xmpp:%s", uin);

		jdcc = xmalloc(sizeof(jabber_dcc_t));
		jdcc->session	= s;
		jdcc->req 	= xstrdup(id);
		jdcc->sid	= jabber_unescape(jabber_attr(n->atts, "id"));
		jdcc->sfd	= -1;

		D = dcc_add(s, uid, DCC_GET, NULL);
		dcc_filename_set(D, filename);
		dcc_size_set(D, atoi(size));
		dcc_private_set(D, jdcc);
		dcc_close_handler_set(D, jabber_dcc_close_handler);
/* XXX, result
		if ((range = xmlnode_find_child(p, "range"))) {
			char *off = jabber_attr(range->atts, "offset");
			char *len = jabber_attr(range->atts, "length");
			if (off) dcc_offset_set(D, atoi(off));
			if (len) dcc_size_set(D, atoi(len));
		}
*/
		print("dcc_get_offer", format_user(s, uid), filename, size, itoa(dcc_id_get(D))); 

		xfree(uin);
		xfree(uid);
		xfree(filename);
	}


}

JABBER_HANDLER_RESULT(jabber_handle_si_result) {
	jabber_private_t *j = s->priv;

	char *uin = jabber_unescape(from);
	dcc_t *d;

	if ((d = jabber_dcc_find(uin, id, NULL))) {
		xmlnode_t *node;
		jabber_dcc_t *p = d->priv;
		char *stream_method = NULL;

		for (node = n->children; node; node = node->next) {
			if (!xstrcmp(node->name, "feature") && !xstrcmp(node->xmlns, "http://jabber.org/protocol/feature-neg")) {
				xmlnode_t *subnode;
				for (subnode = node->children; subnode; subnode = subnode->next) {
					if (!xstrcmp(subnode->name, "x") && !xstrcmp(subnode->xmlns, "jabber:x:data") && 
							!xstrcmp(jabber_attr(subnode->atts, "type"), "submit")) {
						/* var stream-method == http://jabber.org/protocol/bytestreams */
						jabber_handle_xmldata_submit(s, subnode->children, NULL, 0, "stream-method", &stream_method, NULL);
					}
				}
			}
		}
		if (!xstrcmp(stream_method, "http://jabber.org/protocol/bytestreams")) 	p->protocol = JABBER_DCC_PROTOCOL_BYTESTREAMS; 
		else debug_error("[JABBER] JEP-0095: ERROR, stream_method XYZ error: %s\n", stream_method);
		xfree(stream_method);

		if (p->protocol == JABBER_DCC_PROTOCOL_BYTESTREAMS) {
			struct jabber_streamhost_item streamhost;
			jabber_dcc_bytestream_t *b;
			list_t l;

			b = p->private.bytestream = xmalloc(sizeof(jabber_dcc_bytestream_t));
			b->validate = JABBER_DCC_PROTOCOL_BYTESTREAMS;

			if (jabber_dcc_ip && jabber_dcc) {
				/* basic streamhost, our ip, default port, our jid. check if we enable it. XXX*/
				streamhost.jid	= saprintf("%s/%s", s->uid+5, j->resource);
				streamhost.ip	= xstrdup(jabber_dcc_ip);
				streamhost.port	= jabber_dcc_port;
				list_add(&(b->streamlist), &streamhost, sizeof(struct jabber_streamhost_item));
			}

			/* 	... other, proxy, etc, etc..
				streamhost.ip = ....
				streamhost.port = ....
				list_add(...);
				*/

			xfree(p->req);
			p->req = xstrdup(itoa(j->id++));

			watch_write(j->send_watch, "<iq type=\"set\" to=\"%s\" id=\"%s\">"
					"<query xmlns=\"http://jabber.org/protocol/bytestreams\" mode=\"tcp\" sid=\"%s\">", 
					d->uid+5, p->req, p->sid);

			for (l = b->streamlist; l; l = l->next) {
				struct jabber_streamhost_item *item = l->data;
				watch_write(j->send_watch, "<streamhost port=\"%d\" host=\"%s\" jid=\"%s\"/>", item->port, item->ip, item->jid);
			}
			watch_write(j->send_watch, "<fast xmlns=\"http://affinix.com/jabber/stream\"/></query></iq>");

		}
	} else /* XXX */;
}

JABBER_HANDLER_RESULT(jabber_handle_bind) {
	jabber_private_t *j = s->priv;
	
	if (session_int_get(s, "__session_need_start") == 1) {
		watch_write(j->send_watch, 
				"<iq type=\"set\" id=\"auth\"><session xmlns=\"urn:ietf:params:xml:ns:xmpp-session\"/></iq>",
				j->id++);

		session_int_set(s, "__session_need_start", 0);

	} else debug_error("jabber_handle_bind() but not __session_need_start\n");

}

JABBER_HANDLER_RESULT(jabber_handle_iq_result_generic) {
	debug_error("jabber_handle_iq_result_generic()\n");
}

static const struct jabber_iq_generic_handler jabber_iq_result_handlers[] = {
	{ "vCard",	"vcard-temp",					jabber_handle_vcard },

	{ "pubsub",	"http://jabber.org/protocol/pubsub#event",	jabber_handle_result_pubsub },			/* not done */
	{ "mailbox",	"google:mail:notify",				jabber_handle_gmail_result_mailbox },		/* not done */
	{ "new-mail",	"google:mail:notify",				jabber_handle_iq_result_new_mail },		/* not done */

	{ "si",		NULL,						jabber_handle_si_result },			/* not done */

	{ "query",	"jabber:iq:last",				jabber_handle_iq_result_last },
	{ NULL,		"jabber:iq:privacy",				jabber_handle_iq_result_privacy },		/* zaczete */
	{ NULL,		"jabber:iq:private",				jabber_handle_iq_result_private },
	{ NULL,		"jabber:iq:register",				jabber_handle_iq_result_register },		/* not done */
	{ NULL,		"jabber:iq:roster",				jabber_handle_iq_roster },			/* not done */
	{ NULL,		"jabber:iq:search",				jabber_handle_iq_result_search },
	{ NULL,		"jabber:iq:version",				jabber_handle_iq_result_version },
	{ NULL,		"http://jabber.org/protocol/disco#info",	jabber_handle_iq_result_disco_info },
	{ NULL,		"http://jabber.org/protocol/disco#items",	jabber_handle_iq_result_disco },
	{ NULL,		"http://jabber.org/protocol/muc#admin",		jabber_handle_iq_muc_admin },			/* bez ERROR */
	{ NULL,		"http://jabber.org/protocol/muc#owner",		jabber_handle_iq_muc_owner },			/* bez ERROR */
	{ NULL,		"http://jabber.org/protocol/vacation",		jabber_handle_iq_result_vacation },		/* done, but not checked, without ERROR */

	{ "bind",	"urn:ietf:params:xml:ns:xmpp-bind",		jabber_handle_bind },				/* not done */

	{ "",		NULL,						NULL }
};

/* XXX: temporary hack: roster przychodzi jako typ 'set' (przy dodawaniu), jak
 *      i typ "result" (przy zażądaniu rostera od serwera) */

/* niektore hacki zdecydowanie za dlugo... */

static const struct jabber_iq_generic_handler jabber_iq_set_handlers[] = {
	{ "vCard",	"vcard-temp",					jabber_handle_vcard },

	{ "new-mail",	"google:mail:notify",				jabber_handle_iq_set_new_mail },

	{ "si",		NULL,						jabber_handle_si_set },

	{ "query",	"jabber:iq:privacy",				jabber_handle_iq_result_privacy },		/* XXX: przeniesc kod ktory przychodzi jako set do innej funkcji */
	{ NULL,		"jabber:iq:roster",				jabber_handle_iq_roster },
	{ "",		NULL,						NULL }
};

