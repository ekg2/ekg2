#if 0

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include <ekg/themes.h>	/* ? */

#include <ekg/queries.h>

#endif

/* TODO:
 *  - uzywac konwerterow.
 *  - uzywac uint16_t, uint32_t
 */

static int polchat_mode_to_ekg_mode(unsigned short status) {
#if 0
	int kolorek = (status >> 4);
#endif

	if (status & 0x0002) return EKG_STATUS_AVAIL;	/* OP */
	if (status & 0x0001) return EKG_STATUS_AWAY;	/* priv? */
	return EKG_STATUS_XA;				/* normal */
	

/*
   if ((status & 0x00ff8c) != 0x0000)
   	debug_error("Unknown status data: %.4x\n", status);
 */
}

static int hex_to_dec(unsigned char ch1, unsigned char ch2) {
	int res = 0;

	if (xisdigit(ch1))	res = (ch1 - '0') << 4;
	else			res = ((tolower(ch1)-'a')+10) << 4;

	if (xisdigit(ch2))	res |= ch2 - '0';
	else			res |= ((tolower(ch2)-'a')+10);

	return res;
}

static char *html_to_ekg2(char *tekst) {
	string_t str;

	int bold = 0;
	int underline = 0;
	char color = '\0';

	debug_white("html_to_ekg2() %s\n", tekst);

	str = string_init(NULL);

	while (*tekst) {
		if (*tekst == '<') {
			int reset = 0;
			char *btekst = tekst;

			while (*tekst && *tekst != '>') 
				tekst++;

			if (*tekst == '\0')
				break;

			tekst++;

			if (btekst[1] == '/') {
				if (!xstrncmp("</u>", btekst, tekst-btekst))	underline = 0;
				if (!xstrncmp("</b>", btekst, tekst-btekst)) 	bold = 0;
				if (!xstrncmp("</font>", btekst, tekst-btekst))	color = 0;

				string_append(str, "%n");	reset = 1;
			}

			if ((reset && underline) || (!underline && !xstrncmp("<u>", btekst, tekst-btekst))) {
				underline = 1;
				string_append(str, "%U");
			}

			if (!reset && !xstrncmp("<font ", btekst, 6)) {
#define ishex(x) ((x >= '0' && x <= '9')  || (x >= 'A' && x <= 'F') || (x >= 'a' && x <= 'f'))
				char *fnt_color = xstrstr(btekst, " color=");
				char new_color = color;

				if (fnt_color && fnt_color < tekst) {
					if (fnt_color[7] == '#' && 
						ishex(fnt_color[8]) && ishex(fnt_color[9]) && 
						ishex(fnt_color[10]) && ishex(fnt_color[11]) && 
						ishex(fnt_color[12]) && ishex(fnt_color[13])) 
					{
						new_color = color_map(
								hex_to_dec(fnt_color[8], fnt_color[9]), 
								hex_to_dec(fnt_color[10], fnt_color[11]), 
								hex_to_dec(fnt_color[12], fnt_color[13]));
					} else {
						if (!xstrncasecmp(&(fnt_color[7]), "red", 3)) new_color = 'r';
						else
							debug_error("NOT IMPLEMENTED COLOR=\n");


					}


					if (new_color != color) {
						string_append_c(str, '%');
						string_append_c(str, bold ? toupper(new_color) : new_color);
						color = new_color;
					}
				}
#undef ishex
			} else if (reset && color) {
				string_append_c(str, '%');
				string_append_c(str, bold ? toupper(color) : color);
				continue;
			}

			if ((reset && bold) || (!bold && !xstrncmp("<b>", btekst, tekst-btekst))) {
				bold = 1;
				if (!color) string_append(str, "%T");
				else {
					string_append_c(str, '%');
					string_append_c(str, toupper(color));
				}
			}
			continue;

		} else if (*tekst == '&') {		/* eskejpniete */
			char *btekst = tekst;

			/* here we do trick: 
			 * 	We go forward until ';' or NUL is found
			 * 	When we found ';' 
			 *
			 * 	We match strings with ';' so we convert only valid html-escaped tags
			 */

			while (*tekst && *tekst != ';') 
				tekst++;

			if (*tekst)
				tekst++;

			if (0);
			else if (!xstrncmp("&amp;", btekst, tekst-btekst))	string_append_c(str, '&');
			else if (!xstrncmp("&lt;", btekst, tekst-btekst))	string_append_c(str, '<');
			else if (!xstrncmp("&gt;", btekst, tekst-btekst))	string_append_c(str, '>');
			else if (!xstrncmp("&quot;", btekst, tekst-btekst))	string_append_c(str, '\"');
			/* ... */
			else {
				char *tmp = xstrndup(btekst, tekst - btekst);

				debug_error("html_to_ekg2() invalid/unknown escaped-tag [%s]\n", tmp);
				string_append(str, tmp);

				xfree(tmp);
			}
			
			continue;
		}

		if (*tekst == '%' || *tekst == '\\') 
			string_append_c(str, '\\');

		string_append_c(str, *tekst);
		tekst++;
	}
	return string_free(str, 0);
}

#define POLCHAT_HANDLER(x) static int x(session_t *s, int nheaders, int nstrings, unsigned short *headers, char **strings)

POLCHAT_HANDLER(polchat_echo_request) {
	if (nheaders == 1 && !nstrings)	{
		polchat_sendpkt(s, 0x00, NULL);
		return 0;
	} 
	
	return 1;
}

POLCHAT_HANDLER(polchat_msg) {
	polchat_private_t *j = s->priv;

	if (nheaders == 1 && nstrings == 1) {
		char *tmp = html_to_ekg2(strings[0]);
		char *tmp2= format_string(tmp);

/* XXX, wysylac przez PROTOCOL-MESSAGE */
		print_window(j->room, s, EKG_WINACT_IMPORTANT, 1, "none", tmp2);

		xfree(tmp2);
		xfree(tmp);

		return 0;
	}
	return 1;
}

POLCHAT_HANDLER(polchat_privmsg) {
	if (nheaders == 1 && nstrings == 2) {			/* wiadomosc ktora ktos nam wyslal */
		char *tmp = html_to_ekg2(strings[0]);
		char *tmp2= format_string(tmp);

		char *uid = polchat_uid(strings[1]);

/* XXX, wysylac przez PROTOCOL-MESSAGE */
		print_window(uid, s, EKG_WINACT_IMPORTANT, 1, "none", tmp2);

		xfree(uid);

		xfree(tmp2);
		xfree(tmp);

		return 0;
	} else if (nheaders == 1 && nstrings == 3) {		/* wiadomosc ktora wyslalismy */
		char *tmp = html_to_ekg2(strings[0]);
		char *tmp2= format_string(tmp);

		char *uid = polchat_uid(strings[2]);

/* XXX, wysylac przez PROTOCOL-MESSAGE */
		print_window(uid, s, EKG_WINACT_IMPORTANT, 1, "none", tmp2);

		/* mozemy sprawdzic czy strings[1] == my */

		xfree(uid);

		xfree(tmp2);
		xfree(tmp);

		return 0;
	}
	
	return 1;
}

POLCHAT_HANDLER(polchat_roomconfig) {
	if (nheaders == 1 && nstrings == 2) {
#if 0
		if ((ptr = strstr(ppart->strings[0], "color_user=")))
		{
			ptr += 11;
			sscanf(ptr, "#%x", &tmp);
			colourt[0] = transformrgb((tmp >> 16) & 0x00FF, (tmp >> 8) & 0x00FF, tmp & 0x00FF);
		}
		if ((ptr = strstr(ppart->strings[0], "color_op=")))
		{
			ptr += 9;
			sscanf(ptr, "#%x", &tmp);
			colourop = transformrgb((tmp >> 16) & 0x00FF, (tmp >> 8) & 0x00FF, tmp & 0x00FF);
		}
		if ((ptr = strstr(ppart->strings[0], "color_guest=")))
		{
			ptr += 12;
			tmp = sscanf(ptr, "#%x #%x #%x #%x #%x #%x #%x", &tempt[0],
					&tempt[1], &tempt[2], &tempt[3], &tempt[4], &tempt[5],
					&tempt[6]);
			for (i = 0; i <tmp; i++)
			{
				colourt[i + 1] = transformrgb((tempt[i] >> 16) & 0x00FF, (tempt[i] >> 8) & 0x00FF, tempt[i] & 0x00FF);
			}
		}
#endif
		return 0;
	}
	return 1;
}

POLCHAT_HANDLER(polchat_clientconfig) {
	if (nheaders == 1 && nstrings == 1)
		return 0;
	return 1;
}

POLCHAT_HANDLER(polchat_join) {
	if (nheaders == 2 && nstrings == 1) {
		userlist_t *u;
		char *uid;

		uid = polchat_uid(strings[0]);

		/* XXX, userlist_find() */

		if ((u = userlist_add(s, uid, strings[0]))) {
			u->status = polchat_mode_to_ekg_mode(headers[1]);

			query_emit_id(NULL, USERLIST_REFRESH);
		}

		xfree(uid);

		return 0;
	}
	return 1;
}

POLCHAT_HANDLER(polchat_part) {
	if (nheaders == 1 && nstrings == 1) {
		userlist_remove(s, userlist_find(s, strings[0]));

		query_emit_id(NULL, USERLIST_REFRESH);

		return 0;
	}
	return 1;
}

POLCHAT_HANDLER(polchat_roominfo) {
	if (nheaders == 2 && nstrings == 2) {
		debug_function("polchat_roominfo() HEADER0_ROOMINFO: NAME: %s DESC: %s\n", strings[0], strings[1]);
		/* XXX, update j-> & use in ncurses header like irc-topic */
#if 0
		xfree(roomname);
		roomname = xstrdup(strings[0]);
		xfree(roomdesc);
		roomdesc = xstrdup(strings[1]);
#endif
		return 0;
	}
	return 1;
}

POLCHAT_HANDLER(polchat_welcomemsg) {
	polchat_private_t *j = s->priv;

	if (nheaders == 1 && nstrings == 1) {
		window_t *w;

		if (s->connecting) {
			/* new-status */
			s->status = EKG_STATUS_AVAIL;
			/* connected */
			protocol_connected_emit(s);
		}

		if (j->room) {
			debug_error("polchat_welcomemsg() but j->room: %s [newone: %s]\n", j->room, __(j->newroom));
			xfree(j->room);
		}

		j->room = j->newroom;
		j->newroom = NULL;

		if (!(w = window_find_s(s, j->room)))
			w = window_new(j->room, s, 0);

		
		{
			char *tmp = html_to_ekg2(strings[0]);
			char *tmp2= format_string(tmp);

			print_window_w(w, EKG_WINACT_MSG, "none", tmp2);

			xfree(tmp2);
			xfree(tmp);
		}

		return 0;
	}

	return 1;
}

POLCHAT_HANDLER(polchat_goodbyemsg) {
	polchat_private_t *j = s->priv;

	if (nheaders == 1 && nstrings == 1) {
		userlist_free(s);

		{
			char *tmp = html_to_ekg2(strings[0]);
			char *tmp2= format_string(tmp);

			print_window(j->room, s, EKG_WINACT_IMPORTANT, 1, "none", tmp2);

			xfree(tmp2);
			xfree(tmp);
		}

		if (!j->room)
			debug_error("polchat_goodbyemsg() but j->room == NULL\n");

		xfree(j->room);
		j->room = NULL;

		return 0;
	}

	return 1;
}

POLCHAT_HANDLER(polchat_nicklist) {
	if (nheaders >= 5 && headers[1] == 0x0001 && headers[2] == 0x0001 && headers[3] == 0x0000 && headers[4] == 0x0000) {
		int i;

		for (i = 0; i < nstrings; i++) {
			userlist_t *u;
			char *uid;

			debug_function("polchat_processpkt() HEADER0_NICKLIST: %s\n", strings[i]);

			uid = polchat_uid(strings[i]);

			/* XXX, userlist_find() */

			if ((u = userlist_add(s, uid, strings[i]))) {
				
				if (nheaders >= 1 + (5 + 2 * i)) {
					u->status = polchat_mode_to_ekg_mode(headers[5 + 2 * i]);
				} else {
					debug_error("polchat_nicklist() ERROR: %d vs %d\n", 5 + 2 * i, nheaders);
					u->status = EKG_STATUS_ERROR;
				}

				/* XXX, atrybuty lokalne w headers[5 + 2 * i + 1] */
			}

			xfree(uid);
		}

		query_emit_id(NULL, USERLIST_REFRESH);
		return 0;
	}
	return 1;
}

POLCHAT_HANDLER(polchat_errormsg) {
	if (nheaders == 1 && nstrings == 1) {
		polchat_handle_disconnect(s, strings[0], EKG_DISCONNECT_FAILURE);
		return 0;
	}
	return 1;
}

POLCHAT_HANDLER(polchat_wejoin) {
	polchat_private_t *j = s->priv;

	if (nheaders == 2 && nstrings == 1) {
		if (headers[1] != 0x0004) return 1;

		if (xstrcmp(j->nick, strings[0])) {
			if (j->nick[0] == '~') return 1;

			/* fix nick, if we've got at beg '~' <-- anonymous */

			if (strings[0][0] != '~' || xstrcmp(j->nick, &(strings[0][1]))) {
				debug_error("polchat_wejoin() j->nick [%s] vs str0 [%s]\n", j->nick, strings[0]);
				return 1;
			}

			xfree(j->nick);
			j->nick = xstrdup(strings[0]);
		}

/*		print_window(j->room, s, EKG_WINACT_IMPORTANT, 1, "polchat_joined_you", session_name(s), j->nick, j->room + 8); */

		return 0;
	}
	return 1;
}

POLCHAT_HANDLER(polchat_nick_update) {
	if (nheaders == 2 && nstrings == 1) {
		userlist_t *u;

		if (!(u = userlist_find(s, strings[0]))) {
			debug_error("polchat_nick_update() UNKNOWN NICK\n");
			return 1;
		}
		
		u->status = polchat_mode_to_ekg_mode(headers[1]);
		query_emit_id(NULL, USERLIST_REFRESH);

		return 0;
	}
	return 1;
}

struct {
	char *name;
	unsigned short id;
	int (*handler)(session_t *, int nheaders, int nstrings, unsigned short *headers, char **strings);

} polchat_handlers[] = {
	{ "HEADER0_ECHOREQUEST",	0x0001, polchat_echo_request },
	{ "HEADER0_MSG",		0x0262, polchat_msg },
	{ "HEADER0_PRIVMSG",		0x0263, polchat_privmsg },
	{ "HEADER0_CLIENTCONFIG",	0x0266, polchat_clientconfig },
	{ "HEADER0_JOIN",		0x0267, polchat_join },
	{ "HEADER0_PART",		0x0268, polchat_part },
	{ "HEADER0_ROOMINFO",		0x0271, polchat_roominfo },
	{ "HEADER0_ROOMCONFIG",		0x0272, polchat_roomconfig },
	{ "HEADER0_WELCOMEMSG",		0x0276, polchat_welcomemsg },
	{ "HEADER0_GOODBYEMSG",		0x0277, polchat_goodbyemsg },
	{ "HEADER0_NICKLIST",		0x026b, polchat_nicklist },
	{ "HEADER0_ERRORMSG",		0xffff, polchat_errormsg },

	{ "HEADER0_WEJOIN",		0x026a,	polchat_wejoin },
	{ "HEADER0_NICKUPDATE",		0x0269, polchat_nick_update },
	{ NULL, 0, NULL }
};

void polchat_processpkt(session_t *s, unsigned short nheaders, unsigned short nstrings, unsigned char *data, size_t len) {
	unsigned short *headers = NULL;
	char **strings = NULL;
	int unk;
	int pok = 0;
	int i;

	debug("polchat_processpkt() nheaders: %d nstrings: %d len: %d\n", nheaders, nstrings, len);

	if (!len) 
		return;

	headers = xcalloc(nheaders, sizeof(unsigned short));
	strings	= xcalloc(nstrings+1, sizeof(char *));

/* x naglowkow po 2 bajty kazdy (short) BE */
	for (i = 0; i < nheaders; i++) {
		if (len < 2) goto invalid_packet; len -= 2;

		headers[i] = data[0] << 8 | data[1];
		data += 2;
	}

/* x stringow w &data[2] data[0..1]  -> rozmiar, stringi NUL terminated */
	for (i = 0; i < nstrings; i++) {
		unsigned short strlen;
		
		if (len < 2) goto invalid_packet; len -= 2;
		
		strlen = (data[0] << 8 | data[1]);

		if (len < strlen+1) goto invalid_packet; len -= (strlen+1);

		strings[i] = ekg_utf8_to_locale(xstrndup((char*) &data[2], strlen));
		data += (2 + strlen + 1);
	}

	if (len) 
		debug_error("polchat_processpkt() headers && string parsed but len left: %d\n", len);

	pok = 1;
	unk = 1;

	if (nheaders) {
		int i;

		for (i = 0; polchat_handlers[i].name; i++) {
			if (polchat_handlers[i].id == headers[0]) {
				debug("polchat_processpkt() %s [0x%.4x, %d] \n", polchat_handlers[i].name, polchat_handlers[i].id, polchat_handlers[i].id);

				unk = polchat_handlers[i].handler(s, nheaders, nstrings, headers, strings);
				break;
			}
		}
	}

	if (unk) {
		int i;
		debug_error("polchat_processpkt() XXX nheaders: %d nstrings: %d\n\t", nheaders, nstrings);
		for (i = 0; i < nheaders; i++) 
			debug_error("headers[%d]: %.4x [%d]\n", i, headers[i], headers[i]);

		debug_error("\n");
		for (i = 0; i < nstrings; i++)
			debug_error("\tstrings[%d]: %s\n", i, strings[i]);

		debug_error("\n");
	}

invalid_packet: 
	if (!pok)
		debug_error("polchat_processpkt() invalid len packet!! exploit warning?\n");

	xfree(headers);
	array_free(strings);
}

// vim:syn=c
