/* $Id: themes.c 4590 2008-09-01 19:00:56Z wiechu $ */

/*
 *  (C) Copyright 2001-2006 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Leszek Krupiñski <leafnode@wafel.com>
 *			    Adam Mikuta <adamm@ekg2.org>
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

#include "ekg2-remote-config.h"

#define _XOPEN_SOURCE 600
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "stuff.h"
#include "themes.h"
#include "xmalloc.h"
#include "windows.h"
#include "userlist.h"

#include "debug.h"
#include "dynstuff_inline.h"
#include "queries.h"

static void theme_cache_reset();

static char *prompt_cache = NULL, *prompt2_cache = NULL, *error_cache = NULL;
static const char *timestamp_cache = NULL;

static int no_prompt_cache = 0;
static int no_prompt_cache_hash = 0x139dcbd6;	/* hash value of "no_prompt_cache" */

struct format {
	struct format *next;
	char *name;
	int name_hash;
	char *value;
};

static struct format* formats[0x100];

static LIST_FREE_ITEM(list_format_free, struct format *) {
	xfree(data->value);
	xfree(data->name);
}

DYNSTUFF_LIST_DECLARE(formats, struct format, list_format_free,
	static __DYNSTUFF_ADD_BEGINNING,	/* formats_add() */
	__DYNSTUFF_NOREMOVE,
	static __DYNSTUFF_DESTROY)		/* formats_destroy() */

static int gim_hash(const char *name) {
#define ROL(x) (((x>>25)&0x7f)|((x<<7)&0xffffff80))
	int hash = 0;

	for (; *name; name++) {
		hash ^= *name;
		hash = ROL(hash);
	}

	return hash;
#undef ROL
}

const char *format_find(const char *name)
{
	struct format *fl;
	int hash;

	if (!name)
		return "";

	hash = gim_hash(name);

	for (fl = formats[hash & 0xff]; fl; fl = fl->next) {
		struct format *f = fl;

		if (hash == f->name_hash && !strcmp(f->name, name))
			return f->value;
	}
	return "";
}

static const char *format_ansi(char ch) {
	if (ch == 'k')
		return ("\033[2;30m");
	if (ch == 'K')
		return ("\033[1;30m");
	if (ch == 'l')
		return ("\033[40m");
	if (ch == 'r')
		return ("\033[2;31m");
	if (ch == 'R')
		return ("\033[1;31m");
	if (ch == 's')
		return ("\033[41m");
	if (ch == 'g')
		return ("\033[2;32m");
	if (ch == 'G')
		return ("\033[1;32m");
	if (ch == 'h')
		return ("\033[42m");
	if (ch == 'y')
		return ("\033[2;33m");
	if (ch == 'Y')
		return ("\033[1;33m");
	if (ch == 'z')
		return ("\033[43m");
	if (ch == 'b')
		return ("\033[2;34m");
	if (ch == 'B')
		return ("\033[1;34m");
	if (ch == 'e')
		return ("\033[44m");
	if (ch == 'm' || ch == 'p')
		return ("\033[2;35m");
	if (ch == 'M' || ch == 'P')
		return ("\033[1;35m");
	if (ch == 'q')
		return ("\033[45m");
	if (ch == 'c')
		return ("\033[2;36m");
	if (ch == 'C')
		return ("\033[1;36m");
	if (ch == 'd')
		return ("\033[46m");
	if (ch == 'w')
		return ("\033[2;37m");
	if (ch == 'W')
		return ("\033[1;37m");
	if (ch == 'x')
		return ("\033[47m");
	if (ch == 'n')			/* clear all attributes */
		return ("\033[0m");
	if (ch == 'T')			/* bold */
		return ("\033[1m");
	if (ch == 'N')			/* clears all attr exc for bkgd */
		return ("\033[2m");
	if (ch == 'U')			/* underline */
		return ("\033[4m");
	if (ch == 'i')			/* blink */
		return ("\033[5m");
	if (ch == 'V')			/* reverse */
		return ("\033[7m");
	if (ch == 'A')
		return ("\033(0");
	if (ch == 'a')
		return ("\033(B");

	return NULL;
}

static void format_ansi_append(string_t str, char ch) {
	string_append(str, format_ansi(ch));
}

static char *va_format_string(const char *format, va_list ap) {
	string_t buf = string_init(NULL);
	static int dont_resolve = 0;
	const char *p;
	char *args[9] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	int i, argc = 0;

	/* liczymy ilo¶æ argumentów */
	for (p = format; *p; p++) {
		if (*p == '\\' && p[1] == '%') {
			p++;
			continue;
		}

		if (*p != '%')
			continue;

		p++;

		if (!*p)
			break;

		if (*p == '@') {
			p++;

			if (!*p)
				break;

			if ((*p - '0') > argc)
				argc = *p - '0';

		} else if (*p == '(' || *p == '[') {
			if (*p == '(') {
				while (*p && *p != ')')
					p++;
			} else {
				while (*p && *p != ']')
					p++;
			}

			if (*p)
				p++;

			if (!*p)
				break;

			if ((*p - '0') > argc)
				argc = *p - '0';
		} else {
			if (*p >= '1' && *p <= '9' && (*p - '0') > argc)
				argc = *p - '0';
		}
	}

	for (i = 0; i < argc; i++)
		args[i] = va_arg(ap, char *);

	if (!dont_resolve) {
		dont_resolve = 1;
		if (no_prompt_cache) {
			/* zawsze czytaj */
			timestamp_cache	= format_find("timestamp");
			prompt_cache	= format_string(format_find("prompt"));
			prompt2_cache	= format_string(format_find("prompt2"));
			error_cache	= format_string(format_find("error"));
		} else {
			/* tylko je¶li nie s± keszowanie */
			if (!timestamp_cache)	timestamp_cache = format_find("timestamp");
			if (!prompt_cache)	prompt_cache	= format_string(format_find("prompt"));
			if (!prompt2_cache)	prompt2_cache	= format_string(format_find("prompt2"));
			if (!error_cache)	error_cache	= format_string(format_find("error"));
		}
		dont_resolve = 0;
	}

	p = format;

	while (*p) {
		if (*p == '\\' && (p[1] == '%' || p[1] == '\\')) {
			string_append_c(buf, p[1]);
			p += 2;
			continue;
		} else if (*p == '%') {
			int fill_before = 0;
			int fill_after	= 0;
			int fill_soft	= 1;
			int fill_length = 0;
			char fill_char	= ' ';
			int center	= 0;

			p++;
			if (!*p)
				break;
			/* This is conditional formatee, it looks like:
			 * %{NcdefSTUV}X
			 * N - is a parameter number, first letter of this parameter will be checked against 'cdef' letters
			 *   if N[0] == 'c', %S formatee is used
			 *   if N[0] == 'd', %T formatee is used
			 *   if N[0] == 'e', %U formatee is used
			 *   if N[0] == 'f', %V formatee is used
			 */
			if (*p == '{') {		/* how does it works ? i czemu tego nie ma w docs/themes.txt ? */
							/* bo to pisa³ GiM, a on jak wiadomo super komentuje kod :> */
				int hm		= 0;
				char *str;
				char *cnt;

				p++;

				if (*p == '}')				{  p++; p++; continue; }	/* dj: why p++; p++; ? who wrote it? */
													/* G->dj: It's on purpose, format looks like:
													 * ${...}X
													 */
				else if (!(*p >= '0' && *p <= '9'))	{	p++; continue; }	/* not number, skip it this formatee */
				else					str = args[*p - '1'];		/* if number, get str from args table */

				p++;
				/* there must be even, point cnt to the half of string
				 * [according to example above p points to "cdef.." cnt points to "STUV", hm=4]
				 */
				cnt = (char *)p;
				while (*cnt && *cnt!='}') { cnt++; hm++; }
				hm>>=1; cnt=(char *)(p+hm);
				/* debug(">>> [HM:%d]", hm); */
				for (; hm>0; hm--) {
					if (*p == *str) break;
					p++; cnt++;
				}
				/* debug(" [%c%c][%d] [%s] ", *p, *cnt, hm, p); */
				/* hm == 0, means N-th parameter doesn't fit to any of letter specified,
				 * so we skip this formatee, but first we must fix 'p' to point to proper place
				 */
				if (!hm) { 
					p = *cnt ? *(cnt+1) ? (cnt+2) : (cnt+1) : cnt;		/* + point 'p' = cnt+2 if it exist
												 *   ((cnt+2) is after end of formatee
												 *    (there's X after enclosing '}')
												 * + or point to '\0' */
					continue; 
				}
				/* N-th param matched a letter, so point 'p' to that free X at the end,
				 * and correct it with proper formatee letter, and go-on with theme code :)
				 * Now you should understand why there's that 'X' at the end :>
				 */
				p=(cnt+hm+1);
				*((char *)p)=*cnt;
			}
			if (*p == '%')
				string_append_c(buf, '%');
			else if (*p == '>')
				string_append(buf, prompt_cache);
			else if (*p == ')')
				string_append(buf, prompt2_cache);
			else if (*p == '!')
				string_append(buf, error_cache);
			else if (*p == '|')
				string_append(buf, "\033[00m"); /* g³upie, wiem */
			else if (*p == ']')
				string_append(buf, "\033[000m");	/* jeszcze g³upsze */
			else if (*p == '#')
				string_append(buf, timestamp(timestamp_cache));
			else if (config_display_color)
				format_ansi_append(buf, *p);

			if (*p == '@') {
				char *str = (char*) args[*(p + 1) - '1'];

				if (str) {
					char *q = str + strlen(str) - 1;

					while (q >= str && (isspace(*q) || ispunct(*q)))
						q--;

					if (*q == 'a')
						string_append(buf, "a");
					else
						string_append(buf, "y");
				} else
					string_append(buf, "y"); /* display_notify&4, I think male form would be fine for UIDs */
				p += 2;
				continue;
			}

			if (*p == '[' || *p == '(') {
				char *q;

				fill_soft = (*p == '(');
				p++;

				if (*p == '^') {
					center = 1;
					p++;
				}
				/* fill_char = ' '; */		/* zadeklarowane wczesniej */

				if (*p == '.') {
					fill_char = '0';
					p++;
				} else if (*p == ',') {
					fill_char = '.';
					p++;
				} else if (*p == '_') {
					fill_char = '_';
					p++;
				}

				fill_length = strtol(p, &q, 0);
				p = q;
				if (fill_length > 0)
					fill_after = 1;
				else {
					fill_length = -fill_length;
					fill_before = 1;
				}
				p++;
			}

			if (*p >= '1' && *p <= '9') {
				char *str = (char *) args[*p - '1'];
				int i, len;

				if (!str)
					str = "";
				len = strlen(str);

				if (fill_length) {
					if (len >= fill_length) {
						if (!fill_soft)
							len = fill_length;
						fill_length = 0;
					} else
						fill_length -= len;
				}

				if (center) {
					fill_before = fill_after = 1;
					center = fill_length & 1;
					fill_length /= 2;
				}

				if (fill_before)
					for (i = 0; i < fill_length+center; i++)
						string_append_c(buf, fill_char);

				string_append_n(buf, str, len);

				if (fill_after)
					for (i = 0; i < fill_length; i++)
						string_append_c(buf, fill_char);
			}
		} else
			string_append_c(buf, *p);
		p++;
	}

	if (!dont_resolve && no_prompt_cache)
		theme_cache_reset();

	if (!config_display_pl_chars)
		iso_to_ascii((unsigned char *) buf->str);

	return string_free(buf, 0);
}

fstring_t *fstring_new(const char *str) {
#define NPAR 16			/* ECMA-48 CSI have got max 16 params (NPAR) defined in <linux/console_struct.h> */
	fstring_t *res;
	char *tmpstr;
	short attr = FSTR_NORMAL;
	int i, j, len = 0, isbold = 0;

	for (i = 0; str[i]; i++) {
		if (str[i] == 27) {
			int parcount = 0;
			if (str[i + 1] != '[')
				continue;
			i += 2;		/* skip begining */
			while ((str[i] == ';' && parcount++ < NPAR-1) || (str[i] >= '0' && str[i] <= '9'))	/* find max NPAR-1 seq */
				i++;
			if (str[i] == 'm') i++;		/* skip 'm' */
			i--;

			continue;
		}

		if (str[i] == 9) {
			len += (8 - (len % 8));
			continue;
		}

		if (str[i] == 13)
			continue;

		len++;
	}

	res			= xmalloc(sizeof(fstring_t));
	res->str.b = tmpstr	= xmalloc2((len + 1) * sizeof(char));
	res->attr		= xmalloc2((len + 1) * sizeof(short));

	res->margin_left = -1;
/*
	res->prompt_len = 0;
	res->prompt_empty = 0;
 */

	for (i = 0, j = 0; str[i]; i++) {
		if ((str[i] == 27) && (str[i+1] == '(')) {
			i += 2;
			if (str[i] == '0') {
				attr |= FSTR_ALTCHARSET;
				continue;
			} else if (str[i] == 'B') {
				attr &= ~FSTR_ALTCHARSET;
				continue;
			}
		} else if (str[i] == 27) {		/* ESC- */
			unsigned short par[NPAR]	= { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; 
			unsigned short parlen[NPAR]	= { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; 

			int npar	= 0; 

			if (str[i + 1] != ('['))
				continue;
			i += 2;

			/* parse ECMA-48 CSI here & build data */
			while (1) {	/* idea based from kernel sources */
				char c = str[i++];

				if (c == ';' && npar < NPAR -1) {	/* next param */
					npar++;
					continue;
				} else if (c >= '0' && c <= '9') {	/* code */
					par[npar] *= 10;			/* multiply current */
					par[npar] += c - '0';		/* add current */
					parlen[npar]++;			/* some old code must know sequence length... stupid */
					continue;
				} else {				/* params done */
					break;
				}
			}
			i--;

			if (str[i] == 'm') {	/* parse sequence to internal attr we only parse seq like \033[...m */
				int k;
				for (k=0; k <= npar; k++) {
					unsigned short cur = par[k];
					switch (cur) {
						case 0:				/* RESET */
							attr = FSTR_NORMAL;
							isbold = 0;
							if (parlen[k] >= 2)
								res->prompt_len = j;

							if (parlen[k] == 3)
								res->prompt_empty = 1;
							break;
						case 1:				/* BOLD */
							if (k == npar && !isbold)		/* if (*p == ('m') && !isbold) */
								attr ^= FSTR_BOLD;
							else {					/* if (*p == (';')) */
								attr |= FSTR_BOLD;
								isbold = 1;
							}
							break;
						case 2:
							attr &= (FSTR_BACKMASK);
							isbold = 0;
							break;
						case 4:	attr ^= FSTR_UNDERLINE;	break;	/* UNDERLINE */
						case 5: attr ^= FSTR_BLINK;	break;	/* BLINK */
						case 7: attr ^= FSTR_REVERSE;	break;	/* REVERSE */
					}

					if (cur >= 30 && cur <= 37) {
						attr &= ~(FSTR_NORMAL+FSTR_FOREMASK);
						attr |= (cur - 30);
					}

					if (cur >= 40 && cur <= 47) {
						attr &= ~(FSTR_NORMAL+FSTR_BACKMASK);
						attr |= (cur - 40) << 3;
					}
				}
			}
//			else debug("Invalid/unsupported by ekg2 ECMA-48 CSI seq? (npar: %d)\n", npar);	/* sequence not ended with m */
			continue;
		}

		if (str[i] == 13)
			continue;

		if (str[i] == ('/') && str[i + 1] == ('|')) {
			if (i == 0 || str[i - 1] != ('/')) {
				res->margin_left = j;
				i++;
				continue;
			}
			continue;
		}

		if (str[i] == 9) {
			int k = 0, l = 8 - (j % 8);

			for (k = 0; k < l; j++, k++) {
				tmpstr[j] = (' ');
				res->attr[j] = attr;
			}

			continue;
		}

		tmpstr[j] = str[i];
		res->attr[j] = attr;
		j++;
	}

	tmpstr[j] = (char) 0;
	res->attr[j] = 0;

	return res;
}

fstring_t *fstring_new_format(const char *format, ...) {
	fstring_t *fstr;
	va_list ap;
	char *tmp;

	va_start(ap, format);
	tmp = va_format_string(format, ap);

	fstr = fstring_new(tmp);

	xfree(tmp);
	return fstr;
}

void fstring_free(fstring_t *str)
{
	if (!str)
		return;

	xfree(str->str.b);
	xfree(str->attr);
	xfree(str->private);
	xfree(str);
}

char *format_string(const char *format, ...)
{
	va_list ap;
	char *tmp;

	va_start(ap, format);
	tmp = va_format_string(format, ap);
	va_end(ap);

	return tmp;
}

static char *split_line(char **ptr) {
	char *foo, *res;

	if (!ptr || !(res = *ptr) || !res[0])
		return NULL;

	if (!(foo = strchr(res, '\n')))
		*ptr += strlen(res);
	else {
		size_t reslen;
		*ptr = foo + 1;
		*foo = 0;

		reslen = strlen(res);
		if (res[reslen - 1] == '\r')
			res[reslen - 1] = 0;
	}

	return res;
}

static void window_print(window_t *w, fstring_t *line) {
	if (!line->ts)
		line->ts = time(NULL);
	query_emit_id(NULL, UI_WINDOW_PRINT, &w, &line);	/* XXX */
}

void print_window_w(window_t *w, int activity, const char *theme, ...) {
	char *stmp;
	va_list ap;

	char *prompt = NULL;
	char *line;

	char *tmp;
	
	if (!w) {	/* if no window passed, then get window based of */
		if (config_default_status_window || !config_display_crap)
			w = window_status;
		else	w = ((window_current != window_debug) ? window_current : window_status);	/* don't print in __debug window, print in __status one */

		if (!w)
			return;
	}

	va_start(ap, theme);

	/* Change w->act */
	if (w != window_current && !w->floating) {
		if (activity > w->act) {
			w->act = activity;
				/* emit UI_WINDOW_ACT_CHANGED only when w->act changed */
			query_emit_id(NULL, UI_WINDOW_ACT_CHANGED);
		}
	}

	stmp = va_format_string(format_find(theme), ap);
	tmp = stmp;

	while ((line = split_line(&tmp))) {
		char *p;

		if ((p = strstr(line, "\033[00m"))) {
			xfree(prompt);
			if (p != line)
				prompt = xstrndup(line, (int) (p - line));
			else
				prompt = NULL;
			line = p;
		}

		if (prompt) {
			char *tmp = saprintf("%s%s", prompt, line);
			window_print(w, fstring_new(tmp));
			xfree(tmp);
		} else
			window_print(w, fstring_new(line));
	}
	xfree(prompt);
	xfree(stmp);

	va_end(ap);
}

static fstring_t *remote_format_string(const char *str, time_t ts) {
	fstring_t *res;
	short attr = FSTR_NORMAL;
	int i, j, len = 0, isbold = 0;

	for (i = 0; str[i]; ) {
		if (str[i] == '\\' && (str[i+1] == '%' || str[i+1] == '\\')) {
			i += 2;
			len++;
			continue;
		}
		
		if (str[i] == '%') {
			if (str[i+1] == '\0')
				break;

			i += 2;
			continue;
		}

		i++;
		len++;
	}

	res		= xmalloc2(sizeof(fstring_t));
	res->str.b	= xmalloc2((len + 1) * sizeof(char));
	res->attr	= xmalloc2((len + 1) * sizeof(short));
	res->ts		= ts;
	res->private	= NULL;
	res->prompt_len = 0;
	res->margin_left= -1;
	res->prompt_empty= 0;

	for (i = 0, j = 0; str[i]; ) {
		if (str[i] == '\\' && (str[i+1] == '%' || str[i+1] == '\\')) {
			res->str.b[j] = str[i+1];
			res->attr[j] = attr;

			i += 2;
			j += 1;
			continue;
		}

		if (str[i] == '%') {
			if (str[i+1] == '\0')
				break;

			if (config_display_color) switch (str[i+1]) 
			{
				#define FORE_COMMON(x) \
					attr &= (FSTR_BACKMASK);		\
					attr &= ~(FSTR_NORMAL+FSTR_FOREMASK);	\
					attr |= x;				\
					isbold = 0;				\
					break;

				#define FORE_COMMON_BOLD(x) \
					attr &= ~(FSTR_NORMAL+FSTR_FOREMASK);	\
					attr |= x;				\
					attr |= FSTR_BOLD;			\
					isbold = 1;				\
					break;

				#define BACK_COMMON(x) \
					attr &= ~(FSTR_NORMAL+FSTR_BACKMASK);	\
					attr |= x << 3;				\
					break;

				case 'k': FORE_COMMON(0);
				case 'K': FORE_COMMON_BOLD(0);
				case 'l': BACK_COMMON(0);

				case 'r': FORE_COMMON(1);
				case 'R': FORE_COMMON_BOLD(1);
				case 's': BACK_COMMON(1);

				case 'g': FORE_COMMON(2);
				case 'G': FORE_COMMON_BOLD(2);
				case 'h': BACK_COMMON(2);

				case 'y': FORE_COMMON(3)
				case 'Y': FORE_COMMON_BOLD(3)
				case 'z': BACK_COMMON(3)

				case 'b': FORE_COMMON(4)
				case 'B': FORE_COMMON_BOLD(4)
				case 'e': BACK_COMMON(4)

				case 'm': case 'p': FORE_COMMON(5)
				case 'M': case 'P': FORE_COMMON_BOLD(5)
				case 'q': BACK_COMMON(5)

				case 'c': FORE_COMMON(6)
				case 'C': FORE_COMMON_BOLD(6)
				case 'd': BACK_COMMON(6)

				case 'w': FORE_COMMON(7)
				case 'W': FORE_COMMON_BOLD(7)
				case 'x': BACK_COMMON(7)

				case 'n':
					  attr = FSTR_NORMAL;
					  isbold = 0;
					  break;

				case 'T':
					if (!isbold)
						attr ^= FSTR_BOLD;
					break;

				case 'N':
					attr &= (FSTR_BACKMASK);
					isbold = 0;
					break;

				case 'U': attr ^= FSTR_UNDERLINE; break;
				case 'i': attr ^= FSTR_BLINK; break;
				case 'V': attr ^= FSTR_REVERSE; break;
				case 'A': attr |= FSTR_ALTCHARSET; break;
				case 'a': attr &= ~FSTR_ALTCHARSET; break;

				default:
					debug_error("remote_format_string() %%%c [%d]\n", str[i], str[i]);

		#undef FORE_COMMON
		#undef FORE_COMMON_BOLD
		#undef BACK_COMMON
			}
			i += 2;
			continue;
		}

		res->str.b[j] = str[i];
		res->attr[j] = attr;

		i++;
		j++;
	}

	res->str.b[j] = (char) '\0';
	res->attr[j] = 0;

	if (!config_display_pl_chars)
		iso_to_ascii((unsigned char *) res->str.b);

	return res;
}

EXPORTNOT void remote_print_window(int id, time_t ts, char *data) {
	window_t *w;
	
	if ((w = window_exist(id))) {
		fstring_t *fstr;

		fstr = remote_format_string(data, ts);
		window_print(w, fstr);
	} else {
		/* popsute ! */

	}
}

static void theme_cache_reset() {
	xfree(prompt_cache);
	xfree(prompt2_cache);
	xfree(error_cache);

	prompt_cache = prompt2_cache = error_cache = NULL;
	timestamp_cache = NULL;
}

static void format_add_c(const char *name, const char *value, int remote) {
	struct format *fl;
	struct format *f;
	int hash;

	if (!name || !value)
		return;

	hash = gim_hash(name);

	if (hash == no_prompt_cache_hash && !strcmp(name, "no_prompt_cache")) {
		no_prompt_cache = 1;
		return;
	}

	if (remote == 0) {
		for (fl = formats[hash & 0xff]; fl; fl = fl->next) {
			struct format *f = fl;

			if (hash == f->name_hash && !strcmp(name, f->name)) {
				if (0 /* replace */) {
					xfree(f->value);
					f->value = xstrdup(value);
				}
				return;
			}
		}
	}

	f = xmalloc(sizeof(struct format));
	f->name		= xstrdup(name);
	f->name_hash	= hash;
	f->value	= xstrdup(value);

	formats_add(&(formats[hash & 0xff]), f);
}

void format_add(const char *name, const char *value, int replace) {
	format_add_c(name, value, 0);
}

EXPORTNOT void remote_format_add(const char *name, const char *value) {
	format_add_c(name, value, 1);
}

EXPORTNOT void theme_free() {
	int i;

	for (i = 0; i < 0x100; i++)
		formats_destroy(&(formats[i]));

	no_prompt_cache = 0;

	theme_cache_reset();
}

EXPORTNOT void theme_init() { 
	theme_cache_reset();
	no_prompt_cache_hash = gim_hash("no_prompt_cache");

	/* XXX, inny kolorek? */
	format_add("remote_debug",	"ekg2-remote: %n%1\n", 1);
	format_add("remote_fdebug",	"ekg2-remote: %B%1\n", 1);
	format_add("remote_iodebug",	"ekg2-remote: %y%1\n", 1);
	format_add("remote_iorecvdebug", "ekg2-remote: %Y%1\n", 1);
	format_add("remote_edebug",	"ekg2-remote: %R%1\n", 1);
	format_add("remote_wdebug",	"ekg2-remote: %W%1\n", 1);
	format_add("remote_warndebug",	"ekg2-remote: %r%1\n", 1);
	format_add("remote_okdebug",	"ekg2-remote: %G%1\n", 1);

	format_add("remote_welcome", 	_("%> %|%Tekg2-remote-0.4%n (%ge%Gk%gg %Gr%ge%Gl%go%Ga%gd%Ge%gd%n)\n"
						"Connected to: %T%1%n\n%rNOTE: All commands except /quit will be executed remotely!\n"
						"If you want to shutdown remote ekg2 type %W/exit [reason]%n"), 1);

	format_add("remote_console_charset_using", _("%) %Tekg2-remote%n detected that your console works under: %W%1%n Please verify and run ekg2-remote -c your-encoding if needed"), 1);
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
