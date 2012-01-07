#if 0

#include "ekg2.h"

#endif

/* TODO:
 *  - uzywac konwerterow.
 *  - uzywac uint16_t, uint32_t
 */

static int polchat_mode_to_ekg_mode(unsigned short status) {
	if (status & 0x0002) return EKG_STATUS_FFC;	/* OP */
	if (status & 0x0001) return EKG_STATUS_AWAY;	/* priv? */
	return EKG_STATUS_XA;				/* normal */


/*
   if ((status & 0x00ff8c) != 0x0000)
   	debug_error("Unknown status data: %.4x\n", status);
 */
}

static void polchat_user_set_status(session_t *s, char *roomuid, char *uid, int mode) {
	window_t *w;
	userlist_t *u;

	if ((w=window_find_s(s, roomuid))) {
		if ((u = userlist_find_u(&(w->userlist), uid))) {
			u->status = polchat_mode_to_ekg_mode(mode);
		}
	}
}

static userlist_t *polchat_user_add(session_t *s, char *nick, char *room, int mode) {
	window_t *w;
	char *uid, *room_uid;
	userlist_t *u = NULL;

	debug_function("polchat_user_add() mode:0x%x, room:%s, nick:%s\n", mode, room, nick);

	if (!(nick && room))
		return NULL;

	room_uid = polchat_uid(room);
	w = window_find_s(s, room_uid);
	uid = polchat_uid(nick);

	if (w && !(u = userlist_find_u(&(w->userlist), uid))) {
		u = userlist_add_u(&(w->userlist), uid, nick);
		polchat_user_set_status(s, room_uid, uid, mode);
	}

	xfree(uid);
	xfree(room_uid);

	return u;
}

static int hex_to_dec(unsigned char ch1, unsigned char ch2) {
	int res = 0;

	if (xisdigit(ch1))	res = (ch1 - '0') << 4;
	else			res = ((tolower(ch1)-'a')+10) << 4;

	if (xisdigit(ch2))	res |= ch2 - '0';
	else			res |= ((tolower(ch2)-'a')+10);

	return res;
}

static char *html_to_ekg2(char *tekst, char *target) {
	string_t str;

	int bold = 0;
	int underline = 0;
	char color = '\0';

	debug_white("html_to_ekg2(%s) %s\n", target, tekst);

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
						char *c = fnt_color+7;
						if (*c == '"') c++;
						if	(!xstrncasecmp(c, "red", 3)) new_color = 'r';
						else if (!xstrncasecmp(c, "orange", 6)) new_color = 'y';
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

static char *polchat_unformat_string(char *text) {
	char c;
	string_t str = string_init(NULL);
	int esc = 0, format = 0;

	for (; (c=*text); text++) {
		if (esc) {
			string_append_c(str, c);
			esc = 0;
			continue;
		}
		if (format) {
			format = 0;
			continue;
		}
		if (c=='\\') {
			esc = 1;
			continue;
		}
		if (c=='%') {
			format = 1;
			continue;
		}
		string_append_c(str, c);
	}
	return string_free(str, 0);
}

static int polchat_display(session_t *s, char *target, char *html, int type) {
	char *clear, format, *text;
	polchat_private_t *j;

	if (!s || !(j = s->priv))
		return 1;

	text = html_to_ekg2(html, target);
	format = format_string(text);
	clear = polchat_unformat_string(text);

	if (target && *target) {
		char *dest = polchat_uid(target);

		if (!xstrncmp(clear, "**", 2)) {
			print_window(dest, s, EKG_WINACT_JUNK, 1, "none", format);
		} else {
			int class = EKG_MSGCLASS_CHAT | EKG_NO_THEMEBIT;
			int beep  = EKG_NO_BEEP;

			switch (type)
			{
				case 1: /* priv msg */
					beep = EKG_TRY_BEEP;
					break;
				case 2: /* our msg */
					class |= EKG_MSGCLASS_NOT2US;
					break;
				default:
					if (xstrcasestr(clear, j->nick)) {
						beep = EKG_TRY_BEEP;
					} else {
						class |= EKG_MSGCLASS_NOT2US;
					}
					break;
			}

			protocol_message_emit(s, dest, NULL, format, NULL, time(NULL), class, NULL, beep, 1);

		}

		xfree(dest);
	} else {
		print_window("__status", s, EKG_WINACT_JUNK, 1, "none", format);
	}

	xfree(clear);
	xfree(format);
	xfree(text);
	return 0;
}

#define POLCHAT_HANDLER(x) static int x(session_t *s, int nheaders, int nstrings, unsigned short *headers, char **strings)

POLCHAT_HANDLER(polchat_echo_request) {
	if (nheaders == 1 && !nstrings)	{
		return polchat_sendpkt(s, 0x00, NULL);
	}

	return 1;
}

POLCHAT_HANDLER(polchat_msg) {
	if (nheaders == 1 && nstrings > 1) {
		return polchat_display(s, strings[1], strings[0], 0);
	}
	return 1;
}

POLCHAT_HANDLER(polchat_privmsg) {
	if (nheaders == 1 && nstrings == 2) {			/* wiadomosc ktora ktos nam wyslal */
		return polchat_display(s, strings[1], strings[0], 1);
	} else if (nheaders == 1 && nstrings == 3) {		/* wiadomosc ktora wyslalismy */
		return polchat_display(s, strings[1], strings[0], 2);
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
	if (nheaders >= 2 && nstrings >= 2) {
		if (polchat_user_add(s, strings[0], strings[1], headers[1])) {
			query_emit(NULL, "userlist-refresh");
		}
		return 0;
	}
	return 1;
}

POLCHAT_HANDLER(polchat_part) {
	userlist_t *u;
	window_t *w;
	char *room;

	if (!(nheaders == 1 && nstrings >= 2))
		return 1;

	room = polchat_uid(strings[1]);
	if ((w = window_find_s(s, room))) {
		if ((u = userlist_find_u(&(w->userlist), strings[0])))
			userlist_remove_u(&(w->userlist), u);
	}
	xfree(room);

	query_emit(NULL, "userlist-refresh");

	return 0;
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
	if (nheaders == 1 && nstrings >= 2) {
		window_t *w;
		char *room = polchat_uid(strings[1]);

		if (s->connecting) {
			/* new-status */
			s->status = EKG_STATUS_AVAIL;
			/* connected */
			protocol_connected_emit(s);
		}

		if (!(w = window_find_s(s, room)))
			w = window_new(room, s, 0);

		if (w)
			window_switch(w->id);

		xfree(room);

		return polchat_display(s, strings[1], strings[0], 0);

	}

	return 1;
}

POLCHAT_HANDLER(polchat_goodbyemsg) {
	if (nheaders == 1 && nstrings >= 2) {
		char *room = polchat_uid(strings[1]);
		window_t *w = window_find_s(s, room);

		if (w && w->userlist) {
			userlists_destroy(&(w->userlist));
			query_emit(NULL, "userlist-refresh");
		}

		xfree(room);

		return polchat_display(s, strings[1], strings[0], 0);

	}

	return 1;
}

POLCHAT_HANDLER(polchat_nicklist) {
	if (nheaders >= 5) { //&& headers[1] == 0x0001 && headers[2] == 0x0001 && headers[3] == 0x0001 && headers[4] == 0x0000) {
		int i;
		char *room = strings[0];

		for (i = 1; i < nstrings; i+=2) {

			if (nheaders > i+4) {
				polchat_user_add(s, strings[i], room, headers[i+4]);
			} else {
				debug_error("polchat_nicklist() ERROR: %d vs %d\n", 4 + i, nheaders);
				polchat_user_add(s, strings[i], room, 0);
			}

		}

		query_emit(NULL, "userlist-refresh");
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

		return 0;
	}
	return 1;
}

POLCHAT_HANDLER(polchat_nick_update) {
	char *uid;
	char *room;

	if (!(nheaders == 2 && nstrings == 2))
		return 1;

	uid = polchat_uid(strings[0]);
	room = polchat_uid(strings[1]);

	polchat_user_set_status(s, room, uid, headers[1]);

	xfree(uid);
	xfree(room);

	query_emit(NULL, "userlist-refresh");

	return 0;
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

	debug_function("polchat_processpkt() nheaders: %d nstrings: %d len: %d\n", nheaders, nstrings, len);

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

		strings[i] = ekg_utf8_to_core(xstrndup((char*) &data[2], strlen));
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
		debug_error("polchat_processpkt() XXX nheaders: %d nstrings: %d\n", nheaders, nstrings);
		for (i = 0; i < MAX(nheaders,nstrings); i++) {
			if (i<nheaders)
				debug_error("headers[%d]: %.4x [%d]", i, headers[i], headers[i]);
			if (i<nstrings)
				debug_error("\tstrings[%d]: %s", i, strings[i]);
			debug_error("\n");
		}
		debug("\n");
	}

invalid_packet:
	if (!pok)
		debug_error("polchat_processpkt() invalid len packet!! exploit warning?\n");

	xfree(headers);
	g_strfreev(strings);
}

// vim:syn=c
