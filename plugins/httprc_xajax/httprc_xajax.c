/*
 *  (C) Copyright 2006+ Jakub 'darkjames' Zawadzki <darkjames@darkjames.ath.cx>
 *  			Michal 'GiM' Spadlinski <gim at skrzynka dot pl>
 *                      Michal 'peres' Gorny
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

#include <stdio.h>

#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <time.h>

#include <stdarg.h>
#include <stdlib.h>

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/log.h>
#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include <plugins/ncurses/ecurses.h>

/* string that you're typing in browser's window:
 * e.g: your.server.with.ekg2.com, localhost, 127.0.0.1
 */
#define LOCALHOST "localhost"
#define HTTPRCXAJAX_DEFPORT "8080"

/* PLUGIN_LOG NOT PLUGIN_UI, since it depends on ncurses !!! */
PLUGIN_DEFINE(httprc_xajax, PLUGIN_UI, NULL);

typedef struct {
	void *window;
	char *prompt;
	int prompt_len;
	int margin_left, margin_right, margin_top, margin_bottom;
	fstring_t **backlog;
	int backlog_size;
	int redraw;
	int start;
	int lines_count;
	void **lines;
	int overflow;
	int (*handle_redraw)(window_t *w);
	void (*handle_mouse)(int x, int y, int state);
} ncurses_window_t;

typedef struct {
	char *cookie;
	int collector_active;
	string_t collected;
} client_t;

list_t clients = NULL;

client_t *find_client_by_cookie(list_t clients, char *cookie)
{
	client_t *p;
	list_t a;
	for (a=clients; a; a=a->next)
	{
		p = (client_t *)a->data;
		if (!xstrcmp(p->cookie, cookie))
			break;
	}
	if (a)
		return p;
	return NULL;
}

char *generate_cookie(void)
{
	return saprintf("%x%d%d", rand()*rand(), (int)time(NULL), rand());
}

inline char *wcs_to_normal_http(const CHAR_T *str) {
	if (!str) return NULL;
#if USE_UNICODE
	if (config_use_unicode) {
		int len		= wcstombs(NULL, (wchar_t *) str,0);
		char *tmp 	= xmalloc(len+1);
		int ret;

		ret = wcstombs(tmp, (wchar_t *) str, len);
		return tmp;
	} else
#endif
		return (char *) str;
}

char *escape_single_quote(char *p, int inuni)
{
	string_t s = string_init(NULL);
	int l=xstrlen(p), r;
#if USE_UNICODE
	if (inuni)
	{
		mbtowc(NULL, NULL, 0);	/* reset */
		while (l>0)
		{
			if ((r = mbtowc(NULL, p, l)) == -1)
				string_append_c(s, '?'), r=1;
			else if (r == 1)
			{
				if (*p == '\'')
					string_append(s, "\\'");
				else
					string_append_c(s, *p);
			} else
				string_append_n(s, p, r);
			l -= r;
			p += r;
		}
	} else
#endif
	{
		while (l>0)
		{
			if (*p == '\'')
				string_append(s, "\\'");
			else
				string_append_c(s, *p);
			l --;
			p ++;
		}
	}
	return string_free(s, 0);
}

char *http_fstring(int winid, char *parent, char *str, short *attr, int inuni)
{
	string_t asc = string_init(NULL);
	int i, last, lastbeg, tempchar, len, att;
	char *normal;
	char *tmp;
	char *colortbl[10] = { "grey", "red", "green", "yellow", "blue", "purple", "turquoise", "white" };

#define ISBOLD		(att & FSTR_BOLD)
#define ISBLINK		(att & FSTR_BLINK) 
#define ISUNDERLINE	(att & FSTR_UNDERLINE)
#define ISREVERSE	(att & FSTR_REVERSE)
#define ISNORMAL	(att & FSTR_NORMAL)
#define ISONLYNORMAL	((att & FSTR_NORMAL) && !(att & (FSTR_BOLD|FSTR_BLINK|FSTR_REVERSE|FSTR_UNDERLINE)))
#define FORE 		(att & FSTR_FOREMASK)
#define BACK 		((att & FSTR_BACKMASK)>>3)
#define ADDJS(x) 	string_append(asc, x)
#define ADDJSf(x...) 	string_append_format(asc, x)

	lastbeg = 0;
	last = attr[0];
	/* I do not create nested tags like:
	 * <strong><span>...</span>  ... <span> ... </span> </strong>
	 * since this would be quite senseless
	 */
	len = strlen(str);
#if USE_UNICODE
	if (config_use_unicode && inuni)
		len = wcslen(str);
#endif
	for (i = 1; i <= len; i++)
	{
		if (attr[i] == last)
			continue;
	
		if (inuni) {
			tempchar = __S(str, i);
			__SREP(str, i, 0);
		} else {
			tempchar = str[i];
			str[i] = 0;
		}
		att = attr[lastbeg];
		if (inuni)
			normal = wcs_to_normal_http(__SPTR(str, lastbeg));
		else
			normal = str+lastbeg;
		if (ISONLYNORMAL)
		{
			ADDJSf("%s.appendChild(document.createTextNode('%s'));\n",parent,(tmp = escape_single_quote(normal,inuni)));
		} else {
			if (ISBOLD || ISUNDERLINE || ISBLINK)
				ADDJS("em = document.createElement('em'); em.setAttribute('class', '");
			/* DO NOT REMOVE THOSE SPACES AT THE END!!! */
			if (ISBOLD) 		ADDJS("bold ");
			if (ISUNDERLINE) 	ADDJS("underline ");
			if (ISBLINK) 		ADDJS("blink ");
			if (ISBOLD || ISUNDERLINE || ISBLINK)
				ADDJS("');");
			ADDJS("sp = document.createElement('span');");
			
			if (!ISNORMAL)
				ADDJSf("sp.setAttribute('class', '%s');", colortbl[FORE]);
			ADDJSf("sp.appendChild(document.createTextNode('%s'));\n",(tmp = escape_single_quote(normal,inuni)));
			if (ISBOLD)
			{
				ADDJS("em.appendChild(sp);");
				ADDJSf("%s.appendChild(em);", parent);
			} else 
				ADDJSf("%s.appendChild(sp);", parent);
		}
		if (normal != str+lastbeg)
			xfree(normal);
		xfree(tmp);

		ADDJS("\n");
		if (inuni)
			__SREP(str, i, tempchar);
		else
			str[i]=tempchar;
		lastbeg = i;
		last = attr[i];
	}
	if (!len)
	{
		ADDJSf("%s.appendChild(document.createTextNode('\\u00a0'));\n", parent);
	}

	return string_free(asc, 0);
}


QUERY(httprc_xajax_def_action)
{
	list_t a;
	client_t *p;
	static int xxxid=0;
	int nobr = 0;

	int gname=0, gw=0, gline=0;
	char *name = NULL, *tmp = NULL;
	window_t *w = NULL;
	fstring_t *line = NULL;

	for (a=clients; a; a=a->next)
	{
		p = (client_t *)a->data;
		if (!(p->collector_active))
			continue;
		if (!xstrcmp((char *)data, "ui-window-print"))
		{
			if (!gw)
				w = *(va_arg(ap, window_t **)),gw=1;
			if (w->id != 0)
			{
				if (!gline) {
					char *fstringed;
					ncurses_window_t *n = w->private;
					line = *(va_arg(ap, fstring_t **));
					gline=1;
					fstringed = http_fstring(w->id, "ch", line->str, line->attr, 0);
					tmp = saprintf("glst=gwins[%d][2].length;\n"
							"ch = document.createElement('li');\n"
							"ch.setAttribute('id', 'lin'+glst);\n"
							"%s\n"
							"ch.className='info'+(glst%2);\n"
							"gwins[%d][2][glst]=ch;\n"
							"if (current_window != %d) { xajax.$('wi'+%d).className='act'; }\n"
							"else { window_content_add_line(%d); }\n",
							w->id, fstringed, w->id,
							w->id, w->id,
							w->id);
					xfree(fstringed);
				}
				FILE *fp=fopen("dupa", "a"); fprintf(fp, "%s:%s\n", p->cookie, line->str); fflush(fp); fclose(fp);
				/* it'd be easier if we'd got iface for creating xml files... */

				string_append(p->collected, "<cmd n=\"ap\" t=\"LOG\" p=\"innerHTML\"><![CDATA[");
				string_append(p->collected, itoa(nobr));
				string_append(p->collected, (char *)data);
				string_append(p->collected, " = ");
				string_append(p->collected, itoa(w->id));
				string_append(p->collected, " = ");
				string_append(p->collected, line->str);
				string_append(p->collected, "]]></cmd>");
				string_append(p->collected, "<cmd n=\"js\"><![CDATA[");
				string_append(p->collected, tmp);
				string_append(p->collected, "]]></cmd>");
			} else {
				nobr = 1;
			}
		} else if (!xstrcmp((char *)data, "ui-window-new")) {
			if (!gw) {
				w = *(va_arg(ap, window_t **));
				gw=1;
			}
			
				/* create gwins structure */
				if (w == window_current)
					tmp = saprintf("gwins[%d] = new Array(2, \"%s\", new Array());\n ", w->id, window_target(w));
				else if (w->act)
					tmp = saprintf("gwins[%d] = new Array(1, \"%s\", new Array());\n ", w->id, window_target(w));
				else
					tmp = saprintf("gwins[%d] = new Array(0, \"%s\", new Array());\n ", w->id, window_target(w));
				string_append(p->collected, "<cmd n=\"js\"><![CDATA[");
				string_append(p->collected, tmp);
				/* add call to update_windows_list [in ekg.js] */
				string_append(p->collected, "update_windows_list();\n");
				string_append(p->collected, "]]></cmd>");


			
			string_append(p->collected, "<cmd n=\"ap\" t=\"LOG\" p=\"innerHTML\"><![CDATA[");
			string_append(p->collected, (char *)data);
			string_append(p->collected, " = ID: ");
			string_append(p->collected, itoa(w->id));
			string_append(p->collected, " = targ:");
			string_append(p->collected, w->target?w->target:"empty_target");
			string_append(p->collected, " = sess:");
			string_append(p->collected, w->session?(w->session->uid?w->session->uid:"empty_sessionuid"):"empty_session");
			string_append(p->collected, "]]></cmd>");
		} else if (!xstrcmp((char *)data, "ui-window-kill")) {
			if (!gw) {
				w = *(va_arg(ap, window_t **));
				gw=1;
			}
			
				/* kill gwins struct */
				tmp = saprintf("gwins[%d] = void 0;\n", w->id);
				string_append(p->collected, "<cmd n=\"js\"><![CDATA[");
				string_append(p->collected, tmp);
				/* add call to update_windows_list [in ekg.js] */
				string_append(p->collected, "update_windows_list();\n");
				xfree(tmp);
				tmp = saprintf("update_window_content(%d);\n",window_current->id);
				string_append(p->collected, tmp);
				string_append(p->collected, "]]></cmd>");
				
			string_append(p->collected, "<cmd n=\"ap\" t=\"LOG\" p=\"innerHTML\"><![CDATA[");
			string_append(p->collected, (char *)data);
			string_append(p->collected, " = current: ");
			string_append(p->collected, itoa(window_current->id));
			string_append(p->collected, " = ID: ");
			string_append(p->collected, itoa(w->id));
			string_append(p->collected, " = targ:");
			string_append(p->collected, window_target(w));
			string_append(p->collected, " = sess:");
			string_append(p->collected, w->session?(w->session->uid?w->session->uid:"empty_sessionuid"):"empty_session");
			string_append(p->collected, "]]></cmd>");

		} else if (!xstrcmp((char *)data, "variable-changed")) {
			if (!gname) {
				name = *(va_arg(ap, char**));
				gname=1;
			}
			string_append(p->collected, "<cmd n=\"ap\" t=\"LOG\" p=\"innerHTML\"><![CDATA[");
			string_append(p->collected, (char *)data);
			string_append(p->collected, " = ");
			string_append(p->collected, name);
			string_append(p->collected, "]]></cmd>");
		} else {
			debug("oth: %08X\n", data);
			string_append(p->collected, "<cmd n=\"ap\" t=\"LOG\" p=\"innerHTML\"><![CDATA[");
			string_append(p->collected, (char *)data);
			string_append(p->collected, "]]></cmd>");
		}
		if (!nobr)
		{
			string_append(p->collected, "<cmd n=\"ce\" t=\"LOG\" p=\"l");
			string_append(p->collected, itoa(xxxid++));
			string_append(p->collected, "\"><![CDATA[br]]></cmd>");
		}
	}
	xfree(tmp);
	return 0;
}

const char *http_timestamp(time_t t) {
	static char buf[2][100];
	struct tm *tm = localtime(&t);
	static int i = 0;

	const char *format = format_find("timestamp");

	if (!format)
		return itoa(t);

	i = i % 2;
	if (!strftime(buf[i], sizeof(buf[0]), format, tm) && xstrlen(format)>0)
		xstrcpy(buf[i], "TOOLONG");
	return buf[i++];
}

#define WATCH_FIND(w, fd) \
	w = watch_find(&httprc_xajax_plugin, fd, WATCH_WRITE);\
	if (!w) w = watch_add_line(&httprc_xajax_plugin, fd, WATCH_WRITE_LINE, http_watch_send, NULL); \


WATCHER_LINE(http_watch_send)  {
	if (type) return 0;
	/* XXX, sprawdzic czy user chce gzipa. jesli tak wysylamy gzipniete. */
	/* XXX, sprawdzic czy user leci po https */
	/* XXX, sprawdzic, czy user leci w kulki! */
	return write(fd, watch, xstrlen(watch));
}

typedef enum {
	HTTP_METHOD_UNKNOWN = -1,
	HTTP_METHOD_OPTIONS = 0,
	HTTP_METHOD_GET,
	HTTP_METHOD_HEAD,
	HTTP_METHOD_POST,
	HTTP_METHOD_PUT,
	HTTP_METHOD_DELETE,
	HTTP_METHOD_TRACE,
	HTTP_METHOD_CONNECT,
} http_method_t;

WATCHER(http_watch_read) {
	char rbuf[4096];
	char *buf;
	int len, send_cookie_again = 1;
	client_t *client = NULL;
	watch_t *send_watch = NULL;

	http_method_t method = HTTP_METHOD_UNKNOWN;
	int ver = -1;	/* 0 - HTTP/1.0 1 - HTTP/1.1 */
	char *req = NULL;

	char *line;
	int clen = 0;

	if (type) {
		close(fd);
		return 0;
	}

	len = read(fd, &rbuf[0], sizeof(rbuf)-1);
	rbuf[len] = 0;

	buf = &rbuf[0];

	if ((line = split_line(&buf))) {
		/* Request-Line   = Method SP Request-URI SP HTTP-Version CRLF */
		char *httpver = NULL;
		char *hline = NULL;

		if (!xstrncmp(line, "OPTIONS ", 8))	method = HTTP_METHOD_OPTIONS;
		else if (!xstrncmp(line, "TRACE ", 6))	method = HTTP_METHOD_TRACE;
		else if (!xstrncmp(line, "CONNECT ", 8))method = HTTP_METHOD_CONNECT;

		else if (!xstrncmp(line, "GET ", 4))	method = HTTP_METHOD_GET;
		else if (!xstrncmp(line, "HEAD ", 5))	method = HTTP_METHOD_HEAD;
		else if (!xstrncmp(line, "POST ", 5))	method = HTTP_METHOD_POST;

		else if (!xstrncmp(line, "PUT ", 4))	method = HTTP_METHOD_PUT;
		else if (!xstrncmp(line, "DELETE ", 7))	method = HTTP_METHOD_DELETE;
		/* HTTP/1.0 definiuje jeszcze LINK i UNLINK */

		else {
			/* pozostale traktujemy jako nie zaimplementowane... */
			return 0;
		}
		/* przechodzimy do nastepnego pola */
		if (!(line = xstrchr(line, ' ')))
			return 0;

		line++;
		req = line;			/* request */

		if ((line = xstrchr(line, ' '))) {
			*line = 0;
			line++;
		} else {
			return 0;
		}

		httpver = line;

		/* method, req, httpver w buf naglowki... */
		if (!xstrcmp(httpver, "HTTP/1.1"))	ver = 1;
		else if (!xstrcmp(httpver, "HTTP/1.0")) ver = 0;
		else {
			/* zly protokol, papa. */
			return 0;
		}
		if (ver == 0 && (method == HTTP_METHOD_OPTIONS || method == HTTP_METHOD_TRACE || method == HTTP_METHOD_CONNECT)) {
			/* probowano zarzadzac czegos czego nie ma w aktualnym protokole, papa */

			return 0;
		}
		debug(":: %d %s %d\n", method, req, ver);
		/* headers */
		while ((hline = split_line(&buf))) {
			if (!xstrcmp(hline, "\r")) {
				if (xstrlen(buf))
					buf++;
				break; /* \r\n headers ends */
			}
			debug("XXX, %s\n", hline);
			if (!xstrncmp(hline, "Cookie: httprc=", 15) && xstrlen(hline)>17)
			{
				char *cookie=xstrdup(hline+15);
				debug("Cookie found: %s\n", cookie);
				if ((client = find_client_by_cookie(clients, cookie))) {
					send_cookie_again = 0;
					client->collector_active = 1;
				}
				xfree(cookie);
			}
		}
		if (send_cookie_again)
		{
			client = (client_t *)xcalloc(1, sizeof(client_t));
			client->collector_active = 1;
			client->collected = string_init("");
			client->cookie = generate_cookie();
			list_add(&clients, client, 0);
			debug("Adding client %s!\n", client->cookie);
		}
		/* reszta, e.g POST */
		while ((hline = split_line(&buf))) {
			debug("XXXX, %s\n", hline);
		}
	} else {
		/* failed ? */
		return 0;
	}

	debug ("%d %08x\n", send_cookie_again, client);
	WATCH_FIND(send_watch, fd);

	if (!send_watch) {
		debug_error("[%s:%d] NOT SEND_WATCH @ fd: %d!\n", __FILE__, __LINE__, fd);
		return -1;
	}

	if (send_watch->buf)
		clen = send_watch->buf->len;

#define httprc_write(watch, args...) 	string_append_format(watch->buf, args)
#define httprc_write2(watch, str)	string_append_n(watch->buf, str, -1)
#define httprc_write3(watch, str, len)	string_append_raw(watch->buf, str, len)

#define HTTP_HEADER(ver, scode, eheaders) \
	httprc_write(send_watch,				\
		"%s %d %s\r\n"						/* statusline: $PROTOCOL $SCODE $RESPONSE */\
		"Server: ekg2-CVS-httprc_xajax plugin\r\n"		/* server info */	\
		"%s\r\n",							/* headers */		\
		ver == 0 ? "HTTP/1.0" : ver == 1 ? "HTTP/1.1" : "",	/* PROTOCOL */		\
		scode, 							/* Status code */	\
			/* some typical responses */		\
			scode == 100 ? "Continue" :		\
			scode == 101 ? "Switching Protocols" :	\
			scode == 200 ? "OK" :			\
			scode == 302 ? "Found" :		\
			"",					\
		eheaders ? eheaders : "\r\n"			\
		); clen = (send_watch->buf ? send_watch->buf->len : 0) - clen;

	if (method == HTTP_METHOD_GET) {
		string_t htheader = string_init("");
		if (!xstrcmp(req, "/ekg2.js") || !xstrcmp(req, "/ekg2.css") || !xstrcmp(req, "/xajax.js")) {
			FILE *f = NULL;
			char *mime;

			if (!xstrcmp(req, "/xajax.js"))		{ f = fopen(DATADIR"/plugins/httprc_xajax/xajax_0.2.4.js", "r");	mime = "text/javascript"; }
			else if (!xstrcmp(req, "/ekg2.js"))	{ f = fopen(DATADIR"/plugins/httprc_xajax/ekg2.js", "r");		mime = "text/javascript"; }
			else 					{ f = fopen(DATADIR"/plugins/httprc_xajax/ekg2.css", "r");		mime = "text/css"; }

			string_append(htheader, "Connection: Keep-Alive\r\n"
					"Keep-Alive: timeout=5, max=100\r\n"
					"Content-Type: ");
			string_append(htheader, mime);
			string_append(htheader, "\r\n");
			HTTP_HEADER(ver, 200, htheader->str);
			string_free(htheader, 1);

			if (f) {
				char buf[4096];
				int len;

				while ((!feof(f))) {
					len = fread(&buf[0], 1, sizeof(buf), f);
					httprc_write3(send_watch, buf, len);	
				}
				fclose(f);
			} else {
				debug_error("[%s:%d] File couldn't be open req: %s\n", __FILE__, __LINE__, req);
			}
		} else {
			char *temp;
			int i, j;
			list_t l;

			window_t *w = window_current; 
			ncurses_window_t *n;

			/* if user is making a refresh, we must clear collected events
			 * whether he has a cookie or not
			 */
			client->collected = string_init("");
			if (send_cookie_again) {
				string_append(htheader, "Set-Cookie: httprc=");
				string_append(htheader, client->cookie);
				string_append(htheader, "; path=/\r\n");
			}
			string_append(htheader, "Cache-Control: no-store, no-cache, must-revalidate, post-check=0, pre-check=0\r\n"
					"Pragma: no-cache\r\n"
					"Connection: Keep-Alive\r\n"
					"Keep-Alive: timeout=5, max=100\r\n"
					"Content-Type: text/html\r\n");
			HTTP_HEADER(ver, 200, htheader->str);
			string_free(htheader, 1);

			/* naglowek HTML */
			httprc_write(send_watch,
					"<?xml version=\"1.0\" encoding=\"%s\"?>\n"
					"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"
					"<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
					"\t<head>\n"
					"\t\t<meta http-equiv=\"Content-Type\" content=\"text/html; charset=%s\" />\n"
					"\t\t<title>EKG2 :: Remote Control</title>\n"
					"\t\t<link rel=\"stylesheet\" href=\"ekg2.css\" type=\"text/css\" />\n"
					"\t\t<script type=\"text/javascript\">\n"
					""
					"var xajaxRequestUri=\"http://"LOCALHOST":"HTTPRCXAJAX_DEFPORT"/xajax/\";\n"
					"var xajaxDebug=false;\n"
					"var xajaxStatusMessages=false;\n"
					"var xajaxWaitCursor=true;\n"
					"var xajaxDefinedGet=0;\n"
					"var xajaxDefinedPost=1;\n"
					"var xajaxLoaded=false;\n"
					"function xajax_events(){return xajax.call(\"events\", arguments, 1); }"
					""
					"</script>\n"
					"\t\t<script type=\"text/javascript\" src=\"xajax.js\"> </script>\n"
					"\t\t<script type=\"text/javascript\">\n", config_console_charset, config_console_charset);

			
			htheader = string_init("gwins = new Array();\n");
			string_append (htheader, "current_window = ");
			string_append (htheader, itoa(window_current->id));
			string_append (htheader, ";\n");

			for (l = windows; l; l = l->next) {
				window_t *w = l->data;
				char *tempdata;
				if (w == window_current)
					string_append_format(htheader, "gwins[%d] = new Array(2, \"%s\", new Array());\n ", w->id, window_target(w));
				else if (w->act)
					string_append_format(htheader, "gwins[%d] = new Array(1, \"%s\", new Array());\n ", w->id, window_target(w));
				else
					string_append_format(htheader, "gwins[%d] = new Array(0, \"%s\", new Array());\n ", w->id, window_target(w));

				/* we don't want debug window... */
				if (w->id == 0)
					continue;

				n = w->private;
				string_append(htheader, "i=0;\n");
				temp = saprintf("gwins[%d][2][i++] = ch;\n", w->id);
				for (j=0, i = n->backlog_size-1; i >= 0; i--) {
					/* really, really stupid... */
					string_append(htheader, "ch = document.createElement('li');\n"
							"ch.setAttribute('id', 'lin'+i);\n");
					tempdata = http_fstring(w->id, "ch", n->backlog[i]->str, n->backlog[i]->attr, 1);
					string_append(htheader, tempdata);
					if (j^=1)
						string_append(htheader, "ch.className='info1';");
					else
						string_append(htheader, "ch.className='info2';");
					/* add object to gwins table */
					string_append(htheader, temp);
					xfree(tempdata);
				}
				xfree(temp);
			}
			httprc_write2(send_watch, htheader->str);
			string_free(htheader, 1);

			httprc_write(send_watch,	
					"\t\t</script>"
					"\t\t<script type=\"text/javascript\" src=\"ekg2.js\"> </script>\n"
					"\t\t<script type=\"text/javascript\">\n"
					"window.setTimeout(function () {\n"
					"\tif (!xajaxLoaded) {\n"
					"\t\talert('Error: the xajax Javascript file could not be included.');\n\t}\n}, 6000);\n"
					"function eventsinbackground() {\n"
					"\txajax_events();"
					"\twindow.setTimeout('eventsinbackground()', 3000);\n"
					"}\n"
					"\t\t</script>\n"
					"\t</head>\n"
					"\t<body>\n"
					"\t\t<div id=\"left\">\n"
					"\t\t\t<ul id=\"windows_list\">\n");

			httprc_write2(send_watch, "\t\t\t</ul>\n"
					"\t\t\t<ul id=\"window_content\">\n");

			httprc_write2(send_watch, 
					"\t\t\t</ul>\n"
					"\t\t</div>\n"
					"\t\t<div id=\"right\">\n"
					"\t\t\t<dl>\n");
			/* USERLISTA */
			if (session_current) {
				httprc_write(send_watch, "\t\t\t\t<dt>Aktualna sesja: %s</dt>\n",
						session_current->alias ? session_current->alias : session_current->uid);
				if (session_current->userlist)
					httprc_write2(send_watch, "\t\t\t\t<dd><ul>\n");
				for (l = session_current->userlist; l; l = l->next) {
					userlist_t *u = l->data;
					httprc_write(send_watch, "\t\t\t\t\t<li class=\"%s\"><a href=\"#\">%s</a></li>\n", u->status, u->nickname ? u->nickname : u->uid);
				}
				if (session_current->userlist)
					httprc_write2(send_watch, "\t\t\t\t</ul></dd>\n");
			}
			/* KOMENDY */
			httprc_write(send_watch, 
					"\t\t\t</dl>\n"
					"\t\t</div>\n\n"
					"\t\t<div id=\"input\">\n"
					"\t\t\t<form action=\"#\" method=\"post\">\n"
					"\t\t\t\t<input type=\"text\" name=\"cmd\" value=\"\" />\n"
					"\t\t\t</form>\n"
					"\t\t\t<a href=\"#\" onclick=\"alert('xxx'); xajax_events(); alert('yyy'); return false;\">AJAX!</a>\n"
					"\t\t</div>\n"
					"\t\t<div id=\"LOG\">\n"
					"\t\t</div>\n"
					"\t\t<div id=\"LOG2\" style=\"background-color:#666;\">\n"
					"\t\t</div>\n"
					"\t\t<div id=\"LOG3\" style=\"background-color:#444;\">\n"
					"\t\t</div>\n"
					"\t</body>\n"
					"\t<script>\n"
					"update_windows_list();\n"
					"update_window_content(%d);\n"
					"document.onload=update_windows_list();\n"
					"document.onload=update_window_content(%d);\n"
					"window.setTimeout('eventsinbackground()', 3000);\n"
					"</script>\n"
					"</html>", w->id, w->id);
		}
	} else if (method == HTTP_METHOD_POST && !xstrcmp(req, "/xajax/")) {
		HTTP_HEADER(ver, 200, "Content-Type: text/html\r\n");
		httprc_write(send_watch, "<?xml version=\"1.0\" encoding=\"%s\"?>\n"
			"<xjx>%s</xjx>",
			config_console_charset, client->collected->str);
//		FILE *fp=fopen("dupa", "a"); fprintf(fp, "sending data:%s\n", client->collected->str); fflush(fp); fclose(fp);
		string_free(client->collected, 1);
		client->collected = string_init("");
	} else {
		HTTP_HEADER(ver, 200, "Content-Type: text/html\r\n");
	}

/* commit data */
	if (send_watch->buf) {
		char *tmp = saprintf("Content-length: %d\r\n", send_watch->buf->len - clen);
		string_insert(send_watch->buf, clen - 2, tmp);
		xfree(tmp);
	}

	watch_handle_write(send_watch);
	return -1;
}

WATCHER(http_watch_accept) {
	struct sockaddr sa;
	int cfd;
	unsigned int salen = sizeof(sa);

	if (type) {
		close(fd);
		return 0;
	}

	if ((cfd = accept(fd, &sa, &salen)) == -1) {
		debug("[httprc-xajax] accept() failed: %s\n", strerror(errno));
		return -1;
	}
	debug("[httprc-xajax] accept() succ\n");
	watch_add_line(&httprc_xajax_plugin, cfd, WATCH_READ, http_watch_read, NULL);
	return 0;
}

int httprc_xajax_plugin_init(int prio) {
	int one = 1;
	int fd;
	struct sockaddr_in sin;

	sin.sin_family = AF_INET;
	sin.sin_port = htons(atoi(HTTPRCXAJAX_DEFPORT));
	sin.sin_addr.s_addr = INADDR_ANY;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		debug("[httprc-xajax] socket() failed: %s\n", strerror(errno));
		return -1;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1)
		debug("[httprc-xajax] setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
	
	if (bind(fd, (struct sockaddr*) &sin, sizeof(sin))) {
		debug("[httprc-xajax] bind() failed: %s\n", strerror(errno));
		return -1;
	}

	if (listen(fd, 10)) {
		debug("[httprc-xajax] listen() failed: %s\n", strerror(errno));
		return -1;
	}

	watch_add(&httprc_xajax_plugin, fd, WATCH_READ, http_watch_accept, NULL);

/*	variable_add(&rc_plugin, TEXT("remote_control"), VAR_STR, 1, &rc_paths, rc_paths_changed, NULL, NULL); */

	plugin_register(&httprc_xajax_plugin, prio);

	plugin_var_add(&httprc_xajax_plugin, "port", VAR_INT, HTTPRCXAJAX_DEFPORT, 0, NULL);

//	query_connect(&httprc_xajax_plugin, ("set-vars-default"), httprc_xajax_def_action, NULL);
//	query_connect(&httprc_xajax_plugin, ("ui-beep"), httprc_xajax_def_action, NULL);
//	query_connect(&httprc_xajax_plugin, ("ui-is-initialized"), httprc_xajax_def_action, NULL);
	query_connect(&httprc_xajax_plugin, ("ui-window-switch"),	httprc_xajax_def_action, "ui-window-switch");
	query_connect(&httprc_xajax_plugin, ("ui-window-print"),	httprc_xajax_def_action, "ui-window-print");
	query_connect(&httprc_xajax_plugin, ("ui-window-new"),		httprc_xajax_def_action, "ui-window-new");
	query_connect(&httprc_xajax_plugin, ("ui-window-kill"),		httprc_xajax_def_action, "ui-window-kill");
	query_connect(&httprc_xajax_plugin, ("ui-window-target-changed"), httprc_xajax_def_action, "ui-target-changed");
	/* We're not touching this one, since this would cause
	 * A LOT of unneeded traffic!
	query_connect(&httprc_xajax_plugin, ("ui-window-act-changed"),	httprc_xajax_def_action, "ui-window-act-changed");
	 */
	query_connect(&httprc_xajax_plugin, ("ui-window-refresh"),	httprc_xajax_def_action, "ui-window-refresh");
	query_connect(&httprc_xajax_plugin, ("ui-window-clear"),	httprc_xajax_def_action, "ui-window-clear");
	query_connect(&httprc_xajax_plugin, ("session-added"),		httprc_xajax_def_action, "session-added");
	query_connect(&httprc_xajax_plugin, ("session-removed"),	httprc_xajax_def_action, "session-removed");
	query_connect(&httprc_xajax_plugin, ("session-changed"),	httprc_xajax_def_action, "session-changed");
	query_connect(&httprc_xajax_plugin, ("userlist-changed"),	httprc_xajax_def_action, "userlist-changed");
	query_connect(&httprc_xajax_plugin, ("userlist-added"),		httprc_xajax_def_action, "userlist-added");
	query_connect(&httprc_xajax_plugin, ("userlist-removed"),	httprc_xajax_def_action, "userlist-removed");
	query_connect(&httprc_xajax_plugin, ("userlist-renamed"),	httprc_xajax_def_action, "userlist-renamed");
	query_connect(&httprc_xajax_plugin, ("binding-set"),		httprc_xajax_def_action, "binding-set");
	query_connect(&httprc_xajax_plugin, ("binding-command"),	httprc_xajax_def_action, "binding-command");
	query_connect(&httprc_xajax_plugin, ("binding-default"),	httprc_xajax_def_action, "binding-default");
	query_connect(&httprc_xajax_plugin, ("variable-changed"),	httprc_xajax_def_action, "variable-changed");
	query_connect(&httprc_xajax_plugin, ("conference-renamed"),	httprc_xajax_def_action, "conference-renamed");

	query_connect(&httprc_xajax_plugin, ("metacontact-added"),	httprc_xajax_def_action, "metacontact-added");
	query_connect(&httprc_xajax_plugin, ("metacontact-removed"),	httprc_xajax_def_action, "metacontact-removed");
	query_connect(&httprc_xajax_plugin, ("metacontact-item-added"),	httprc_xajax_def_action, "metacontact-item-added");
	query_connect(&httprc_xajax_plugin, ("metacontact-item-removed"), httprc_xajax_def_action, "metacontact-item-removed");
	query_connect(&httprc_xajax_plugin, ("config-postinit"),	httprc_xajax_def_action, "config-postinit");

	return 0;
}

static int httprc_xajax_plugin_destroy() {
	plugin_unregister(&httprc_xajax_plugin);
	return 0;
}
