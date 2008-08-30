/* $Id$ */

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

#include "ekg2-config.h"

#define _XOPEN_SOURCE 600
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "dynstuff.h"
#include "stuff.h"
#include "themes.h"
#include "xmalloc.h"
#include "windows.h"
#include "userlist.h"

#include "debug.h"
#include "dynstuff_inline.h"
#include "queries.h"

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
	static __DYNSTUFF_REMOVE_ITER,		/* formats_removei() */
	static __DYNSTUFF_DESTROY)		/* formats_destroy() */

/**
 * gim_hash()
 *
 */

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

/*
 * format_find()
 *
 * odnajduje warto¶æ danego formatu. je¶li nie znajdzie, zwraca pusty ci±g,
 * ¿eby nie musieæ uwa¿aæ na ¿adne null-references.
 *
 *  - name.
 */
const char *format_find(const char *name)
{
	struct format *fl;
	const char *tmp;
	int hash;

	if (!name)
		return "";

	if (config_speech_app && !xstrchr(name, ',')) {
		char *name2	= saprintf("%s,speech", name);
		const char *tmp = format_find(name2);

		xfree(name2);

		if (format_ok(tmp))
			return tmp;
	}

	if (config_theme && (tmp = xstrchr(config_theme, ',')) && !xstrchr(name, ',')) {
		char *name2	= saprintf("%s,%s", name, tmp + 1);
		const char *tmp = format_find(name2);

		xfree(name2);

		if (format_ok(tmp))
			return tmp;
	}

	hash = gim_hash(name);

	for (fl = formats[hash & 0xff]; fl; fl = fl->next) {
		struct format *f = fl;

		if (hash == f->name_hash && !xstrcmp(f->name, name))
			return f->value;
	}
	return "";
}

/*
 * format_ansi()
 *
 * zwraca sekwencjê ansi odpowiadaj±c± danemu kolorkowi z thememów ekg.
 */
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

	return ("");
}

/*
 * va_format_string()
 *
 * formatuje zgodnie z podanymi parametrami ci±g znaków.
 *
 *  - format - warto¶æ, nie nazwa formatu,
 *  - ap - argumenty.
 */
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
			else if (config_display_color) {
				const char *tmp = format_ansi(*p);
				string_append(buf, tmp);
			} 

			if (*p == '@') {
				char *str = (char*) args[*(p + 1) - '1'];

				if (str) {
					char *q = str + xstrlen(str) - 1;

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
				len = xstrlen(str);

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

/**
 * fstring_new()
 *
 * Change formatted ansi string (@a str) to Nowy-i-Lepszy (tm) [New-and-Better].
 *
 * @param str - string
 *
 * @sa format_string()	- Function to format strings.
 * @sa fstring_free()	- Function to free fstring_t.
 *
 * @return Allocated fstring_t.
 */

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
	res->str.b = tmpstr	= xmalloc((len + 1) * sizeof(char));
	res->attr		= xmalloc((len + 1) * sizeof(short));

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
/* 
	tmpstr[j] = (char) 0;
	res->attr[j] = 0;
 */

	return res;
}

/**
 * fstring_new_format()
 *
 *	char *tmp = format_string("format", .....);
 *	fstr = fstring_new(tmp);
 *	xfree(tmp);
 */

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

/**
 * fstring_free()
 *
 * Free memory allocated by @a str
 *
 * @todo XXX Think about freeing str->private
 *
 * @param str - fstring_t * to free.
 */
void fstring_free(fstring_t *str)
{
	if (!str)
		return;

	xfree(str->str.b);
	xfree(str->attr);
	xfree(str->private);
	xfree(str);
}

/*
 * format_string()
 *
 * j.w. tyle ¿e nie potrzeba dawaæ mu va_list, a wystarcz± zwyk³e parametry.
 *
 *  - format... - j.w.,
 */
char *format_string(const char *format, ...)
{
	va_list ap;
	char *tmp;

	va_start(ap, format);
	tmp = va_format_string(format, ap);
	va_end(ap);

	return tmp;
}

/**
 * print_window_c()
 *
 * Mind abbreviation ;) print_window_c() -> print_window_common()<br>
 * Common stuff for: print_window() and print_window_w()<br>
 * Change w->act if needed, format theme, make fstring() and send everything to window_print()
 *
 * @note	We only check if @a w == NULL, if you send here wrong window_t ptr, everything can happen. don't do it.
 *		Yeah, we can do here window_find_ptr() but it'll slow down code a lot.
 */

static void print_window_c(window_t *w, int activity, const char *theme, va_list ap) {
	char *stmp;

	/* w here shouldn't here be NULL. In case. */
	if (!w) {
		/* debug_fatal("print_window_common() w == NULL, theme: %s ap: 0x%x\n", w, theme, ap); */
		return;
	}

	/* Change w->act */
	if (w != window_current && !w->floating) {
		if (activity > w->act) {
			w->act = activity;
				/* emit UI_WINDOW_ACT_CHANGED only when w->act changed */
			query_emit_id(NULL, UI_WINDOW_ACT_CHANGED);
		}
	}

	stmp = va_format_string(format_find(theme), ap);
	{
		char *prompt = NULL;
		char *line;

		char *tmp = stmp;
		while ((line = split_line(&tmp))) {
			char *p;

			if ((p = xstrstr(line, "\033[00m"))) {
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
	}
	xfree(stmp);
}

/**
 * print_window_find()
 *
 * Find given window [based on @a target and @a session]
 * if not found, and separate set, create new one.
 *
 * Print given text in given window [@a target+ @a session]
 *
 * @todo	We have no policy for displaying messages by e.g. jabber resources.<br>
 *		For now we do: [only jabber]<br>
 *			- If @a target has '/' inside. We put there NUL char.<br>
 *			- After it we search for window with stripped '/'<br>
 *			- If founded, than done.<br>
 *		If not, we look for user in userlist.. And if not found, than we'll create new window with stripped '/' [only jabber]
 *		
 *
 * @param target	- target to look for.
 * @param session	- session to look for.
 * @param separate	- if essence of text is important to create new window
 */

static window_t *print_window_find(const char *target, session_t *session, int separate) {
	char *newtarget = NULL;
	const char *tmp;
	userlist_t *u;

	window_t *w;

	/* first of all, let's check if config_display_crap is unset and target is current window */
	if (!config_display_crap) {	/* it was with && (config_make_window & 3) */
		if (!target || !xstrcmp(target, "__current"))
			return window_status;
	}

	/* 1) let's check if we have such window as target... */

	/* if it's jabber and we've got '/' in target strip it. [XXX, window resources] */
	if ((!xstrncmp(target, "xmpp:", 5)) && (tmp = xstrchr(target, '/'))) {
		newtarget = xstrndup(target, tmp - target);
		w = window_find_s(session, newtarget);		/* and search for windows with stripped '/' */
		/* even if w == NULL here, we use newtarget to create window without resource */
		/* Yeah, we search for target on userlist, but u can be NULL also... */
		/* XXX, optimize and think about it */
	} else
		w = window_find_s(session, target);

	if (w) {
		xfree(newtarget);
		return w;
	}

	/* 2) if message is not important (not @a seperate) or we don't want create new windows at all [config_make_window & 3 == 0] 
	 *    than get __status window	*/

	if (!separate || (config_make_window & 3) == 0) {
		xfree(newtarget);
		return window_status;
	}

	/* if we have no window, let's find for it in userlist */
	u = userlist_find(session, target);

	/* if found, and we have nickname, than great! */
	if (u && u->nickname)
		target = u->nickname;			/* use nickname instead of target */
	else if (u && u->uid && ( /* don't use u->uid, if it has resource attached */
			xstrncmp(u->uid, "xmpp:", 5) || !xstrchr(u->uid, '/')))
		target = u->uid;			/* use uid instead of target. XXX here. think about jabber resources */
	else if (newtarget)
		target = newtarget;			/* use target with stripped '/' */
							/* XXX, window resources */

	/* 3) if we don't have window here, and if ((config_make_window & 3) == 1) [unused], than we should find empty window. */
	if ((config_make_window & 3) == 1) {
		window_t *wa;

		for (wa = windows; wa; wa = wa->next) {
			if (!wa->target && wa->id > 1) {
				w = wa;

				w->target = xstrdup(target);
				w->session = session;

				query_emit_id(NULL, UI_WINDOW_TARGET_CHANGED, &w);	/* XXX */
				break;
			}
			if (w)		/* wtf? */
				break;
		}
	}

	/* 4) if not found unused window, or ((config_make_window & 3) == 2) [always] than just create it */
	if (!w)
		w = window_new(target, session, 0);

	/* [FOR 3) and 4)] If we create window or we change target. notify user */

	print("window_id_query_started", itoa(w->id), target, session_name(session));
	print_window_w(w, EKG_WINACT_JUNK, "query_started", target, session_name(session));
	print_window_w(w, EKG_WINACT_JUNK, "query_started_window", target);

	xfree(newtarget);

	return w;
}

/**
 * print_window()
 *
 * Print given text in given window [@a target+ @a session]
 *
 * @todo	We have no policy for displaying messages by e.g. jabber resources.<br>
 *		For now we do: [only jabber]<br>
 *			- If @a target has '/' inside. We put there NUL char.<br>
 *			- After it we search for window with stripped '/'<br>
 *			- If founded, than done.<br>
 *		If not, we look for user in userlist.. And if not found, than we'll create new window with stripped '/' [only jabber]
 *		
 *
 * @param target	- target to look for.
 * @param session	- session to look for.
 * @param activity	- how important is text?
 * @param separate	- if essence of text is important to create new window
 * @param theme		- Name of format to format_string() with @a ... Text will be be built.
 * @param ...
 */

void print_window(const char *target, session_t *session, int activity, int separate, const char *theme, ...) {
	window_t *w;
	va_list ap;

	w = print_window_find(target, session, separate);

	va_start(ap, theme);
	print_window_c(w, activity, theme, ap);
	va_end(ap);
}

void print_info(const char *target, session_t *session, const char *theme, ...) {
	window_t *w;
	va_list ap;

	/* info configuration goes here... */
	w = print_window_find(target, session, 0);

	va_start(ap, theme);
	print_window_c(w, EKG_WINACT_JUNK, theme, ap);
	va_end(ap);
}

void print_warning(const char *target, session_t *session, const char *theme, ...) {
	window_t *w;
	va_list ap;

	/* warning configuration goes here... */
	w = print_window_find(target, session, 0);

	va_start(ap, theme);
	print_window_c(w, EKG_WINACT_JUNK, theme, ap);
	va_end(ap);
}

/**
 * print_window_w()
 *
 * Like print_window() but it takes window_t struct instead of target+session.
 *
 * @note	The same in print_window_c() we don't check if @a w is valid window ptr.
 *		Just be carefull. If you are not sure call:<br>
 *		<code>print_window_c(window_find_ptr(w), separate, theme, ...)</code>
 *		And eventually it will be displayed in (__status / or __current) window instead of good one.. But ekg2 won't crash.
 * 
 *		@param	w - window to display,<br>
 *			if NULL than __status or __current will be used. it depends on: config_default_status_window and config_display_crap variables.
 */

void print_window_w(window_t *w, int activity, const char *theme, ...) {
	va_list ap;

	if (!w) {	/* if no window passed, then get window based of */
		if (config_default_status_window || !config_display_crap)
			w = window_status;
		else	w = ((window_current != window_debug) ? window_current : window_status);	/* don't print in __debug window, print in __status one */
	}

	va_start(ap, theme);
	print_window_c(w, activity, theme, ap);
	va_end(ap);
}


/**
 * theme_cache_reset()
 *
 * Remove cached: @a prompt_cache, @a prompt2_cache, @a error_cache and @a timestamp_cache<br>
 * These values are used by va_format_string() to don't call format_find() on:<br>
 *	["prompt" "%>", "prompt2" "%)", "errror" "%!", "timestamp" "%#"]
 *
 */

void theme_cache_reset() {
	xfree(prompt_cache);
	xfree(prompt2_cache);
	xfree(error_cache);

	prompt_cache = prompt2_cache = error_cache = NULL;
	timestamp_cache = NULL;
}

/**
 * format_add()
 *
 * Add format with @a name and @a value. <br>
 * If @a replace set to 1, than if format with the same name exists. than format value will be replaced.
 *
 * @todo What about creating global variable: formats_unique_here.. and if set to 1, don't search if this format already exists? It should speedup theme_init() a little.
 *
 * @param name	- name of format
 * @param value - value of format
 * @param replace - if this format exists and is set to 1 than format value will be replaced with new one. else do nothing.
 */

void format_add(const char *name, const char *value, int replace) {
	struct format *fl;
	struct format *f;
	int hash;

	if (!name || !value)
		return;

	hash = gim_hash(name);

	if (hash == no_prompt_cache_hash && !xstrcmp(name, "no_prompt_cache")) {
		no_prompt_cache = 1;
		return;
	}

	for (fl = formats[hash & 0xff]; fl; fl = fl->next) {
		struct format *f = fl;

		if (hash == f->name_hash && !xstrcmp(name, f->name)) {
			if (replace) {
				xfree(f->value);
				f->value = xstrdup(value);
			}
			return;
		}
	}

	f = xmalloc(sizeof(struct format));
	f->name		= xstrdup(name);
	f->name_hash	= hash;
	f->value	= xstrdup(value);

	formats_add(&(formats[hash & 0xff]), f);
	return;
}

/**
 * format_remove()
 *
 * Remove format with given name.
 *
 * @todo We can speedup it a little, by passing to list_remove() ptr to previous item.. senseless?
 *
 * @param name
 *
 * @return	 0 if format was founded and removed from list<br>
 *		-1 else
 */

static int format_remove(const char *name) {
	struct format *fl;
	int hash;

	if (!name)
		return -1;

	hash = gim_hash(name);

	for (fl = formats[hash & 0xff]; fl; fl = fl->next) {
		struct format *f = fl;

		if (hash == f->name_hash && !xstrcmp(f->name, name)) {
			(void) formats_removei(&(formats[hash & 0xff]), f);
			return 0;
		}
	}

	return -1;
}

/*
 * theme_open() // funkcja wewnêtrzna
 *
 * próbuje otworzyæ plik, je¶li jeszcze nie jest otwarty.
 *
 *  - prevfd - deskryptor z poprzedniego wywo³ania,
 *  - prefix - ¶cie¿ka,
 *  - filename - nazwa pliku.
 */
static FILE *theme_open(const char *prefix, const char *filename)
{
	char buf[PATH_MAX];
	int save_errno;
	FILE *f;

	if (prefix)
		snprintf(buf, sizeof(buf), "%s/%s", prefix, filename);
	else
		snprintf(buf, sizeof(buf), "%s", filename);

	if ((f = fopen(buf, "r")))
		return f;

	if (prefix)
		snprintf(buf, sizeof(buf), "%s/%s.theme", prefix, filename);
	else
		snprintf(buf, sizeof(buf), "%s.theme", filename);

	save_errno = errno;

	if ((f = fopen(buf, "r")))
		return f;

	if (errno == ENOENT)
		errno = save_errno;

	return NULL;
}

/*
 * theme_read()
 *
 * wczytuje opis wygl±du z podanego pliku.
 *
 *  - filename - nazwa pliku z opisem,
 *  - replace - czy zastêpowaæ istniej±ce wpisy.
 *
 * zwraca 0 je¶li wszystko w porz±dku, -1 w przypadku b³êdu.
 */
int theme_read(const char *filename, int replace) {
	char *buf;
	FILE *f = NULL;

	if (!xstrlen(filename)) {
		/* XXX, DEFAULT_THEME <-> default.theme ? */
		filename = prepare_path("default.theme", 0);
		if (!filename || !(f = fopen(filename, "r")))
			return -1;
	} else {
		char *fn = xstrdup(filename), *tmp;

		if ((tmp = xstrchr(fn, ',')))
			*tmp = 0;

		errno = ENOENT;
		f = NULL;

		if (!xstrchr(fn, '/')) {
			if (!f) f = theme_open(prepare_path("", 0), fn);
			if (!f) f = theme_open(prepare_path("themes", 0), fn);
			if (!f) f = theme_open(DATADIR "/themes", fn);
		} else
			f = theme_open(NULL, fn);

		xfree(fn);

		if (!f)
			return -1;
	}
	if (!in_autoexec) {
		theme_free();
		theme_init();
	}
	/*	ui_event("theme_init"); */

	while ((buf = read_file(f, 0))) {
		char *value;

		if (buf[0] == '-')			format_remove(buf + 1);
		else if (buf[0] == '#')			;
		else if (!(value = xstrchr(buf, ' ')))	;
		else {
			char *p;
			*value++ = 0;

			for (p = value; *p; p++) {
				if (*p == '\\') {
					if (!*(p + 1))
						break;
					if (*(p + 1) == 'n')
						*p = '\n';
					memmove(p + 1, p + 2, xstrlen(p + 1));
				}
			}

			format_add(buf, value, replace);
		}
	}

	fclose(f);

	theme_cache_reset();

	return 0;
}

int theme_write(const char *filename) {
	FILE *f;
	int i;

	if (!filename)
		return -1;

	if (!(f = fopen(filename, "w")))
		return -1;

	for (i = 0; i < 0x100; i++) {
		struct format *ff;

		for (ff = formats[i]; ff; ff = ff->next) {
			char *escaped;
			
			escaped = escape(ff->value);
			fprintf(f, "%s %s\n", ff->name, escaped);
			xfree(escaped);
		}
	}
	fclose(f);

	return 0;
}

/*
 * theme_free()
 *
 * usuwa formatki z pamiêci.
 */
void theme_free() {
	int i;

	for (i = 0; i < 0x100; i++)
		formats_destroy(&(formats[i]));

	no_prompt_cache = 0;

	theme_cache_reset();
}

void theme_plugins_init() {
	plugin_t *p;

	for (p = plugins; p; p = p->next) {
		if (p->theme_init)
			p->theme_init();
	}
}

static const char *theme_init_contact_helper(const char *theme, char color) {
	static char tmp[30];
	int i;

	for (i = 0; theme[i]; i++)
		tmp[i] = (theme[i] == '$') ? color : theme[i];

	tmp[i] = '\0';
	return tmp;
}

static void theme_init_contact_status(const char *status, char color, int want_quick_list) {
	char tmp[100];

#define FORMAT_ADD(format, value, replace) sprintf(tmp, format, status); format_add(tmp, value, replace)
#define FORMAT_ADDF(format, fvalue, replace) FORMAT_ADD(format, theme_init_contact_helper(fvalue, color), replace);
	FORMAT_ADD("contacts_%s_header", "", 1);
/* standard */
	FORMAT_ADDF("contacts_%s", 			" %$%1%n", 1);
	FORMAT_ADDF("contacts_%s_descr",		"%Ki%$%1%n", 1);
	FORMAT_ADDF("contacts_%s_descr_full",		"%Ki%$%1%n %2", 1);
/* blink */
	FORMAT_ADDF("contacts_%s_blink",        	" %$%i%1%n", 1);
	FORMAT_ADDF("contacts_%s_descr_blink",		"%K%ii%$%i%1%n", 1);
	FORMAT_ADDF("contacts_%s_descr_full_blink",	"%K%ii%$%i%1%n %2", 1);
/* typing */
	FORMAT_ADDF("contacts_%s_typing", 		"%W*%$%1%n", 1);
	FORMAT_ADDF("contacts_%s_descr_typing",		"%W*%$%1%n", 1);
	FORMAT_ADDF("contacts_%s_descr_full_typing",	"%W*%$%1%n %2", 1);
/* typing+blink */
	FORMAT_ADDF("contacts_%s_blink_typing", 	"%W%i*%$%i%1%n", 1);
	FORMAT_ADDF("contacts_%s_descr_blink_typing", 	"%W%i*%$%i%1%n", 1);
	FORMAT_ADDF("contacts_%s_descr_full_blink_typing", "%W%i*%$%i%1%n %2", 1);

	FORMAT_ADD("contacts_%s_footer", "", 1);

	if (want_quick_list) {		/* my macros are evil, so don't remove brackets! */
		FORMAT_ADDF("quick_list_%s",		" %$%1%n", 1);
	}

#undef FORMAT_ADD
#undef FORMAT_ADDF
}

/*
 * theme_init()
 *
 * ustawia domy¶lne warto¶ci formatek.
 */
void theme_init()
{
	theme_cache_reset();
	no_prompt_cache_hash = gim_hash("no_prompt_cache");
#ifndef NO_DEFAULT_THEME
	/* wykorzystywane w innych formatach */
	format_add("prompt", "%K:%g:%G:%n", 1);
	format_add("prompt,speech", " ", 1);
	format_add("prompt2", "%K:%c:%C:%n", 1);
	format_add("prompt2,speech", " ", 1);
	format_add("error", "%K:%r:%R:%n", 1);
	format_add("error,speech", "b³±d!", 1);
	format_add("timestamp", "%T", 1);
	format_add("timestamp,speech", " ", 1);

	/* prompty i statusy dla ui-ncurses */
	format_add("ncurses_prompt_none", "", 1);
	format_add("ncurses_prompt_query", "[%1] ", 1);
	format_add("statusbar", " %c(%w%{time}%c)%w %c(%w%{?session %{?away %G}%{?avail %Y}%{?chat %W}%{?dnd %K}%{?xa %g}%{?gone %R}"
			"%{?invisible %C}%{?notavail %r}%{session}}%{?!session ---}%c) %{?window (%wwin%c/%w%{?typing %C}%{window}}"
			"%{?query %c:%W%{query}}%{?debug %c(%Cdebug}%c)%w%{?activity  %c(%wact%c/%W}%{activity}%{?activity %c)%w}"
			"%{?mail  %c(%wmail%c/%w}%{mail}%{?mail %c)}%{?more  %c(%Gmore%c)}", 1);
	format_add("header", " %{?query %c(%{?query_away %w}%{?query_avail %W}%{?query_invisible %K}%{?query_notavail %k}"
			"%{?query_chat %W}%{?query_dnd %K}%{query_xa %g}%{?query_gone %R}%{?query_unknown %M}%{?query_error %m}%{?query_blocking %m}"
			"%{query}%{?query_descr %c/%w%{query_descr}}%c) %{?query_ip (%wip%c/%w%{query_ip}%c)} %{irctopic}}"
			"%{?!query %c(%wekg2%c/%w%{version}%c) (%w%{url}%c)}", 1);
	format_add("statusbar_act_important", "%Y", 1);
	format_add("statusbar_act_important2us", "%W", 1);
	format_add("statusbar_act", "%K", 1);
	format_add("statusbar_act_typing", "%c", 1);
	format_add("statusbar_act_important_typing", "%C", 1);
	format_add("statusbar_act_important2us_typing", "%C", 1);
	format_add("statusbar_timestamp", "%H:%M", 1);

	/* ui-password-input */
	format_add("password_input", _("Please input password:"), 1);
	format_add("password_repeat", _("Please repeat password:"), 1);
	format_add("password_empty", _("%! No password entered"), 1);
	format_add("password_nomatch", _("%! Entered passwords do not match"), 1);
	format_add("password_nosupport", _("%! %|UI-plugin doesn't seem to support password input, please use command-line input."), 1);

	format_add("session_password_input", _("Password for %1:"), 1);

	/* dla funkcji format_user() */
	format_add("known_user", "%T%1%n/%2", 1);
	format_add("known_user,speech", "%1", 1);
	format_add("unknown_user", "%T%1%n", 1);

	/* czêsto wykorzystywane, ró¿ne, przydatne itd. */
	format_add("none", "%1\n", 1);
	format_add("generic", "%> %1\n", 1);
	format_add("generic_bold", "%> %T%1%n\n", 1);
	format_add("generic2", "%) %1\n", 1);
	format_add("generic2_bold", "%) %T%1%n\n", 1);
	format_add("generic_error", "%! %1\n", 1);
	/* debug */
	format_add("debug",	"%n%1\n", 1);
	format_add("fdebug",	"%B%1\n", 1);
	format_add("iodebug",	"%y%1\n", 1);
	format_add("iorecvdebug", "%Y%1\n", 1);
	format_add("edebug",	"%R%1\n", 1);
	format_add("wdebug",	"%W%1\n", 1);
	format_add("warndebug",	"%r%1\n", 1);
	format_add("okdebug",	"%G%1\n", 1);

	format_add("value_none", _("(none)"), 1);
	format_add("not_enough_params", _("%! Too few parameters. Try %Thelp %1%n\n"), 1);
	format_add("invalid_params", _("%! Invalid parameters. Try %Thelp %1%n\n"), 1);
	format_add("var_not_set", _("%! Required variable %T%2%n by %T%1%n is unset\n"), 1);
	format_add("invalid_uid", _("%! Invalid user id\n"), 1);
	format_add("invalid_session", _("%! Invalid session\n"), 1);
	format_add("invalid_nick", _("%! Invalid username\n"), 1);
	format_add("user_not_found", _("%! User %T%1%n not found\n"), 1);
	format_add("not_implemented", _("%! This function isn't ready yet\n"), 1);
	format_add("unknown_command", _("%! Unknown command: %T%1%n\n"), 1);
	format_add("welcome", _("%> %Tekg2-%1%n (%ge%Gk%gg %Gr%ge%Gl%go%Ga%gd%Ge%gd%n)\n%> Software licensed on GPL v2 terms\n\n"), 1);
	format_add("welcome,speech", _("welcome in e k g 2."), 1);
	format_add("ekg_version", _("%) %Tekg2-%1%n (compiled %2)\n"), 1);
	format_add("secure", _("%Y(encrypted)%n"), 1);
	format_add("day_changed", _("%) Day changed to: %W%1"), 1);

	/* add, del */
	format_add("user_added", _("%> (%2) Added %T%1%n to roster\n"), 1);
	format_add("user_deleted", _("%) (%2) Removed %T%1%n from roster\n"), 1);
	format_add("user_cleared_list", _("%) (%1) Roster cleared\n"), 1);
	format_add("user_exists", _("%! (%2) %T%1%n already in roster\n"), 1);
	format_add("user_exists_other", _("%! (%3) %T%1%n already in roster as %2\n"), 1);

	/* zmiany stanu */
	format_add("away", _("%> (%1) Status changed to %Gaway%n\n"), 1);
	format_add("away_descr", _("%> (%3) Status changed to %Gaway%n: %T%1%n%2\n"), 1);
	format_add("back", _("%> (%1) Status changed to %Yavailable%n\n"), 1);
	format_add("back_descr", _("%> (%3) Status changed to %Yavailable%n: %T%1%n%2%n\n"), 1);
	format_add("invisible", _("%> (%1) Status changed to %cinvisible%n\n"), 1);
	format_add("invisible_descr", _("%> (%3) Status changed to %cinvisible%n: %T%1%n%2\n"), 1);
	format_add("dnd", _("%> (%1) Status changed to %Bdo not disturb%n\n"), 1);
	format_add("dnd_descr", _("%> (%3) Status changed to %Bdo not disturb%n: %T%1%n%2\n"), 1);
	format_add("gone", _("%> (%1) Status changed to %Rgone%n\n"), 1);
	format_add("gone_descr", _("%> (%3) Status changed to %Rgone%n: %T%1%n%2%n%n\n"), 1);
	format_add("ffc", _("%> (%1) Status changed to %Wfree for chat%n\n"), 1);
	format_add("ffc_descr", _("%> (%3) Status changed to %Wfree for chat%n: %T%1%n%2%n\n"), 1);
	format_add("xa", _("%> (%1) Status changed to %gextended away%n\n"), 1);
	format_add("xa_descr", _("%> (%3) Status changed to %gextended away%n: %T%1%n%2%n%n\n"), 1);
	format_add("private_mode_is_on", _("% (%1) Friends only mode is on\n"), 1);
	format_add("private_mode_is_off", _("%> (%1) Friends only mode is off\n"), 1);
	format_add("private_mode_on", _("%) (%1) Turned on ,,friends only'' mode\n"), 1);
	format_add("private_mode_off", _("%> (%1) Turned off ,,friends only'' mode\n"), 1);
	format_add("private_mode_invalid", _("%! Invalid value'\n"), 1);
	format_add("descr_too_long", _("%! Description longer than maximum %T%1%n characters\nDescr: %B%2%b%3%n\n"), 1);

	format_add("auto_away", _("%> (%1) Auto %Gaway%n\n"), 1);
	format_add("auto_away_descr", _("%> (%3) Auto %Gaway%n: %T%1%n%2%n\n"), 1);
	format_add("auto_xa", _("%> (%1) Auto %gextended away%n\n"), 1);
	format_add("auto_xa_descr", _("%> (%3) Auto %gextended away%n: %T%1%n%2%n\n"), 1);
	format_add("auto_back", _("%> (%1) Auto back%n\n"), 1);
	format_add("auto_back_descr", _("%> (%3) Auto back: %T%1%n%2%n\n"), 1);

	/* pomoc */
	format_add("help", "%> %T%1%n %2 - %3\n", 1);
	format_add("help_no_params", "%> %T%1%n - %2\n", 1);
	format_add("help_more", "%) %|%1\n", 1);
	format_add("help_alias", _("%) %T%1%n is an alias and don't have description\n"), 1);
	format_add("help_footer", _("\n%> %|%Thelp <command>%n will show more details about command. Prepending %T^%n to command name will hide it's result. Instead of <uid/alias> one can use %T$%n, which means current query user.\n\n"), 1);
	format_add("help_quick", _("%> %|Before using consult the brochure. File %Tdocs/ULOTKA.en%n is a short guide on included documentation. If you don't have it, you can visit %Thttp://www.ekg2.org/%n\n"), 1);
	format_add("help_set_file_not_found", _("%! Can't find variables descriptions (incomplete installation)\n"), 1);
	format_add("help_set_file_not_found_plugin", _("%! Can't find variables descriptions for %T%1%n plugin (incomplete installation)\n"), 1);
	format_add("help_set_var_not_found", _("%! Cant find description of %T%1%n variable\n"), 1);
	format_add("help_set_header", _("%> %T%1%n (%2, default value: %3)\n%>\n"), 1);
	format_add("help_set_body", "%> %|%1\n", 1);
	format_add("help_set_footer", "", 1);
	format_add("help_command_body", "%> %|%1\n", 1);
	format_add("help_command_file_not_found", _("%! Can't find commands descriptions (incomplete installation)\n"), 1);
	format_add("help_command_file_not_found_plugin", _("%! Can't find commands descriptions for %T%1%n plugin (incomplete installation)\n"), 1);
	format_add("help_command_not_found", _("%! Can't find command description: %T%1%n\n"), 1);
	format_add("help_script", _("%) %T%1%n is an script command and don't have description. Try /%1 help\n"), 1);
	format_add("help_session_body", "%> %|%1\n", 1);
	format_add("help_session_file_not_found", _("%! Can't find variables descriptions for %T%1%n plugin (incomplete installation)\n"), 1);
	format_add("help_session_var_not_found", _("%! Cant find description of %T%1%n variable\n"), 1);
	format_add("help_session_header", _("%> %1->%T%2%n (%3, default value: %4)\n%>\n"), 1);
	format_add("help_session_footer", "", 1);

	/* ignore, unignore, block, unblock */
	format_add("ignored_added", _("%> Ignoring %T%1%n\n"), 1);
	format_add("ignored_modified", _("%> Modified ignore level of %T%1%n\n"), 1);
	format_add("ignored_deleted", _("%) Unignored %1\n"), 1);
	format_add("ignored_deleted_all", _("%) Ignore list cleared up\n"), 1);
	format_add("ignored_exist", _("%! %1 already beeing ignored\n"), 1);
	format_add("ignored_list", "%> %1 %2\n", 1);
	format_add("ignored_list_empty", _("%! Ignore list ist empty\n"), 1);
	format_add("error_not_ignored", _("%! %1 is not beeing ignored\n"), 1);
	format_add("blocked_added", _("%> Blocking %T%1%n\n"), 1);
	format_add("blocked_deleted", _("%) Unblocking %1\n"), 1);
	format_add("blocked_deleted_all", _("%) Block list cleared up\n"), 1);
	format_add("blocked_exist", _("%! %1 already beeing blocked\n"), 1);
	format_add("blocked_list", "%> %1\n", 1);
	format_add("blocked_list_empty", _("%! Block list is empty\n"), 1);
	format_add("error_not_blocked", _("%! %1 is not beeing blocked\n"), 1);

	/* contact list */
	format_add("list_empty", _("%! Roster is empty\n"), 1);
	format_add("list_avail", _("%> %1 %Y(available)%n %b%3:%4%n\n"), 1);
	format_add("list_avail_descr", _("%> %1 %Y(available: %n%5%Y)%n %b%3:%4%n\n"), 1);
	format_add("list_away", _("%> %1 %G(away)%n %b%3:%4%n\n"), 1);
	format_add("list_away_descr", _("%> %1 %G(away: %n%5%G)%n %b%3:%4%n\n"), 1);
	format_add("list_dnd", _("%> %1 %B(do not disturb)%n %b%3:%4%n\n"), 1);
	format_add("list_dnd_descr", _("%> %1 %B(do not disturb:%n %5%G)%n %b%3:%4%n\n"), 1);
	format_add("list_chat", _("%> %1 %W(free for chat)%n %b%3:%4%n\n"), 1);
	format_add("list_chat_descr", _("%> %1 %W(free for chat%n: %5%W)%n %b%3:%4%n\n"), 1);
	format_add("list_error", _("%> %1 %m(error) %b%3:%4%n\n"), 1);
	format_add("list_error", _("%> %1 %m(error%n: %5%m)%n %b%3:%4%n\n"), 1);
	format_add("list_xa", _("%> %1 %g(extended away)%n %b%3:%4%n\n"), 1);
	format_add("list_xa_descr", _("%> %1 %g(extended away: %n%5%g)%n %b%3:%4%n\n"), 1);
	format_add("list_gone", _("%> %1 %R(gone)%n %b%3:%4%n\n"), 1);
	format_add("list_gone_descr", _("%> %1 %R(gone: %n%5%g)%n %b%3:%4%n\n"), 1);
	format_add("list_notavail", _("%> %1 %r(offline)%n\n"), 1);
	format_add("list_notavail_descr", _("%> %1 %r(offline: %n%5%r)%n\n"), 1);
	format_add("list_invisible", _("%> %1 %c(invisible)%n %b%3:%4%n\n"), 1);
	format_add("list_invisible_descr", _("%> %1 %c(invisible: %n%5%c)%n %b%3:%4%n\n"), 1);
	format_add("list_blocking", _("%> %1 %m(blocking)%n\n"), 1);
	format_add("list_unknown", "%> %1\n", 1);
	format_add("modify_offline", _("%> %1 will not see your status\n"), 1);
	format_add("modify_online", _("%> %1 will see your status\n"), 1);
	format_add("modify_done", _("%> Modified item in roster\n"), 1);

	/* lista kontaktów z boku ekranu */
	format_add("contacts_header", "", 1);
	format_add("contacts_header_group", "%K %1%n", 1);
	format_add("contacts_metacontacts_header", "", 1);

	theme_init_contact_status("avail", 'Y', 1);
	theme_init_contact_status("away", 'G', 1);
	theme_init_contact_status("dnd", 'B', 1);
	theme_init_contact_status("chat", 'W', 1);
	theme_init_contact_status("error", 'm', 0);
	theme_init_contact_status("xa", 'g', 1);
	theme_init_contact_status("gone", 'R', 1);
	theme_init_contact_status("notavail", 'r', 0);
	theme_init_contact_status("invisible", 'c', 1);

	format_add("contacts_blocking_header", "", 1);
	format_add("contacts_blocking", " %m%1%n", 1);
	format_add("contacts_blocking_footer", "", 1);

	theme_init_contact_status("unknown", 'M', 0);

	format_add("contacts_footer", "", 1);
	format_add("contacts_footer_group", "", 1);
	format_add("contacts_metacontacts_footer", "", 1);
	format_add("contacts_vertical_line_char", "|", 1);
	format_add("contacts_horizontal_line_char", "-", 1);

	/* we are saying goodbye and we are saving configuration */
	format_add("quit", _("%> Bye\n"), 1);
	format_add("quit_descr", _("%> Bye: %T%1%n%2\n"), 1);
	format_add("config_changed", _("Save new configuration ? (t-yes/n-no) "), 1);
	format_add("config_must_reconnect", _("%) You must reconnect for the changes to take effect\n"), 2);
	format_add("quit_keep_reason", _("You've set keep_reason to save status.\nDo you want to save current description to file (it will be restored upon next EKG exec)? (t-yes/n-no) "), 1);
	format_add("saved", _("%> Configuration saved\n"), 1);
	format_add("error_saving", _("%! There was some error during save\n"), 1);

	/* incoming messages */
	format_add("message", "%g.-- %n%1 %c%2%n%6%n%g--- -- -%n\n%g|%n %|%3%n\n%|%g`----- ---- --- -- -%n\n", 1);
	format_add("message_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("message_timestamp_today", "(%H:%M) ", 1);
	format_add("message_timestamp_now", "", 1);
	format_add("message,speech", _("message from %1: %3."), 1);

	format_add("empty", "%3\n", 1);

	format_add("conference", "%g.-- %n%1 %c%2%n%6%n%g--- -- -%n\n%g|%n %|%3%n\n%|%g`----- ---- --- -- -%n\n", 1);
	format_add("conference_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("conference_timestamp_today", "(%H:%M) ", 1);
	format_add("conference_timestamp_now", "", 1);
	format_add("confrence,speech", _("message from %1: %3."), 1);

	format_add("chat", "%c.-- %n%1 %c%2%n%6%n%c--- -- -%n\n%c|%n %|%3%n\n%|%c`----- ---- --- -- -%n\n", 1);
	format_add("chat_me", "%) %|%C%4%n %3\n", 1);
	format_add("chat_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("chat_timestamp_today", "(%H:%M) ", 1);
	format_add("chat_timestamp_now", "", 1);
	format_add("chat,speech", _("message from %1: %3."), 1);

	format_add("sent", "%b.-- %n%1 %c%2%n%6%n%b--- -- -%n\n%b|%n %|%3%n\n%|%b`----- ---- --- -- -%n\n", 1);
	format_add("sent_me", "%> %|%G%4%n %3\n", 1);
	format_add("sent_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("sent_timestamp_today", "(%H:%M) ", 1);
	format_add("sent_timestamp_now", "", 1);
	format_add("sent,speech", "", 1);

		/* XXX: change default colors for these two, but dunno to what
		 * they should be darker than standard ones to emphasize that
		 * these are jest old messages loaded from the db, not received */
	format_add("log", "%c.-- %n%1 %c%2%n%6%n%c--- -- -%n\n%c|%n %|%3%n\n%|%c`----- ---- --- -- -%n\n", 1);
	format_add("log_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("log_timestamp_today", "(%H:%M) ", 1);
	format_add("log_timestamp_now", "", 1);
	format_add("log,speech", _("message from %1: %3."), 1);

	format_add("sent_log", "%b.-- %n%1 %c%2%n%6%n%b--- -- -%n\n%b|%n %|%3%n\n%|%b`----- ---- --- -- -%n\n", 1);
	format_add("sent_log_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("sent_log_timestamp_today", "(%H:%M) ", 1);
	format_add("sent_log_timestamp_now", "", 1);
	format_add("sent_log,speech", "", 1);

	format_add("system", _("%m.-- %TSystem message%m --- -- -%n\n%m|%n %|%3%n\n%|%m`----- ---- --- -- -%n\n"), 1);
	format_add("system,speech", _("system message: %3."), 1);

	/* acks of messages */
	format_add("ack_queued", _("%> Message to %1 will be delivered later\n"), 1);
	format_add("ack_delivered", _("%> Message to %1 delivered\n"), 1);
	format_add("ack_unknown", _("%> Not clear what happened to message to %1\n"), 1);
	format_add("ack_tempfail", _("%! %|Message to %1 encountered temporary delivery failure (e.g. message queue full). Please try again later.\n"), 1);
	format_add("ack_filtered", _("%! %|Message to %1 encountered permament delivery failure (e.g. forbidden content). Before retrying, try to fix the problem yourself (e.g. ask second side to add us to userlist).\n"), 1);
	format_add("message_too_long", _("%! Message was too long and got shortened\n"), 1);

	/* people are changing their statuses */
	format_add("status_avail", _("%> (%3) %1 is %Yavailable%n\n"), 1);
	format_add("status_avail_descr", _("%> (%3) %1 is %Yavailable%n: %T%4%n\n"), 1);
	format_add("status_away", _("%> (%3) %1 is %Gaway%n\n"), 1);
	format_add("status_away_descr", _("%> (%3) %1 is %Gaway%n: %T%4%n\n"), 1);
	format_add("status_notavail", _("%> (%3) %1 is %roffline%n\n"), 1);
	format_add("status_notavail_descr", _("%> (%3) %1 is %roffline%n: %T%4%n\n"), 1);
	format_add("status_invisible", _("%> (%3) %1 is %cinvisible%n\n"), 1);
	format_add("status_invisible_descr", _("%> (%3) %1 is %cinvisible%n: %T%4%n\n"), 1);
	format_add("status_xa", _("%> (%3) %1 is %gextended away%n\n"), 1);
	format_add("status_xa_descr", _("%> (%3) %1 is %gextended away%n: %T%4%n\n"), 1);
	format_add("status_gone", _("%> (%3) %1 is %Rgone%n\n"), 1);
	format_add("status_gone_descr", _("%> (%3) %1 is %Rgone%n: %T%4%n\n"), 1);
	format_add("status_dnd", _("%> (%3) %1 %Bdo not disturb%n\n"), 1);
	format_add("status_dnd_descr", _("%> (%3) %1 %Bdo not disturb%n: %T%4%n\n"), 1);
	format_add("status_error", _("%> (%3) %1 %merror fetching status%n\n"), 1);
	format_add("status_error_descr", _("%> (%3) %1 %merror fetching status%n: %T%4%n\n"), 1);
	format_add("status_chat", _("%> (%3) %1 is %Wfree for chat%n\n"), 1);
	format_add("status_chat_descr", _("%> (%3) %1 is %Wfree for chat%n: %T%4%n\n"), 1);
	format_add("status_unknown", _("%> (%3) %1 is %Munknown%n\n"), 1);
	format_add("status_unknown_descr", _("%> (%3) %1 is %Munknown%n: %T%4%n\n"), 1);

	/* connection with server */
	format_add("connecting", _("%> (%1) Connecting to server %n\n"), 1);
	format_add("conn_failed", _("%! (%2) Connection failure: %1%n\n"), 1);
	format_add("conn_failed_resolving", _("Server not found"), 1);
	format_add("conn_failed_connecting", _("Can't connect to server"), 1);
	format_add("conn_failed_invalid", _("Invalid server response"), 1);
	format_add("conn_failed_disconnected", _("Server disconnected"), 1);
	format_add("conn_failed_password", _("Invalid password"), 1);
	format_add("conn_failed_404", _("HTTP server error"), 1);
	format_add("conn_failed_tls", _("Error negotiating TLS"), 1);
	format_add("conn_failed_memory", _("No memory"), 1);
	format_add("conn_stopped", _("%! (%1) Connection interrupted %n\n"), 1);
	format_add("conn_timeout", _("%! (%1) Connection timed out%n\n"), 1);
	format_add("connected", _("%> (%1) Connected%n\n"), 1);
	format_add("connected_descr", _("%> (%2) Connected: %T%1%n\n"), 1);
	format_add("disconnected", _("%> (%1) Disconnected%n\n"), 1);
	format_add("disconnected_descr", _("%> (%2) Disconnected: %T%1%n\n"), 1);
	format_add("already_connected", _("%! (%1) Already connected. Use %Treconnect%n to reconnect%n\n"), 1);
	format_add("during_connect", _("%! (%1) Connecting in progress. Use %Tdisconnect%n to abort%n\n"), 1);
	format_add("conn_broken", _("%! (%1) Connection broken: %2%n\n"), 1);
	format_add("conn_disconnected", _("%! (%1) Server disconnected%n\n"), 1);
	format_add("not_connected", _("%! (%1) Not connected.%n\n"), 1);
	format_add("not_connected_msg_queued", _("%! (%1) Not connected. Message will be delivered when connected.%n\n"), 1);
	format_add("wrong_id", _("%! (%1) Wrong session id.%n\n"), 1);
	format_add("inet_addr_failed", _("%! (%1) Invalid \"server\".%n\n"), 1);
	format_add("invalid_local_ip", _("%! (%1) Invalid local address. I'm clearing %Tlocal_ip%n session variable\n"), 1);
	format_add("auto_reconnect_removed", _("%! (%1) EKG2 won't try to connect anymore - use /connect.%n\n"), 1);

	/* obs³uga motywów */
	format_add("theme_loaded", "%> Loaded theme %T%1%n\n", 1);
	format_add("theme_default", "%> Default theme selected\n", 1);
	format_add("error_loading_theme", "%! Error loading theme: %1\n", 1);

	/* zmienne, konfiguracja */
	format_add("variable", "%> %1 = %2\n", 1);
	format_add("variable_not_found", _("%! Unknown variable: %T%1%n\n"), 1);
	format_add("variable_invalid", _("%! Invalid session variable value\n"), 1);
	format_add("no_config", _("%! Incomplete configuration. Use:\n%!   %Tsession -a <gg:gg-number/xmpp:jabber-id>%n\n%!   %Tsession password <password>%n\n%!   %Tsave%n\n%! And then:\n%!	 %Tconnect%n\n%! If you don't have uid, use:\n%!   %Tregister <e-mail> <password>%n\n\n%> %|Query windows will be created automatically. To switch windows press %TAlt-number%n or %TEsc%n and then number. To start conversation use %Tquery%n. To add someone to roster use %Tadd%n. All key shortcuts are described in %TREADME%n. There is also %Thelp%n command. Remember about prefixes before UID, for example %Tgg:<no>%n. \n\n"), 2);
	format_add("no_config,speech", _("incomplete configuration. enter session -a, and then gg: gg-number, or xmpp: jabber id, then session password and your password. enter save to save. enter connect to connect. if you dont have UID enter register, space, e-mail and password. Query windows will be created automatically. To switch windows press Alt and window number or Escape and then number. To start conversation use query command. To add someone to roster use add command. All key shortcuts are described in README file. There is also help command."), 1);
	format_add("no_config_gg_not_loaded", _("%! Incomplete configuration. Use:\n%!	 %T/plugin +gg%n - to load gg plugin\n%!   %Tsession -a <gg:gg-number/xmpp:jabber-id>%n\n%!   %Tsession password <password>%n\n%!   %Tsave%n\n%! And then:\n%!	 %Tconnect%n\n%! If you don't have uid, use:\n%!   %Tregister <e-mail> <password>%n\n\n%> %|Query windows will be created automatically. To switch windows press %TAlt-number%n or %TEsc%n and then number. To start conversation use %Tquery%n. To add someone to roster use %Tadd%n. All key shortcuts are described in %TREADME%n. There is also %Thelp%n command. Remember about prefixes before UID, for example %Tgg:<no>%n. \n\n"), 2);
	format_add("no_config_no_libgadu", _("%! Incomplete configuration. %TBIG FAT WARNING:%n\n%!    %Tgg plugin has not been compiled, probably there is no libgadu library in the system\n%! Use:\n%!   %Tsession -a <xmpp:jabber-id>%n\n%!   %Tsession password <password>%n\n%!	%Tsave%n\n%! And then:\n%!   %Tconnect%n\n%! If you don't have uid, use:\n%!   %Tregister%n\n\n%> %|Query windows will be created automatically. To switch windows press %TAlt-number%n or %TEsc%n and then number. To start conversation use %Tquery%n. To add someone to roster use %Tadd%n. All key shortcuts are described in %TREADME%n. There is also %Thelp%n command. Remember about prefixes before UID, for example %Txmpp:<jid>%n. \n\n"), 2);
	format_add("error_reading_config", _("%! Error reading configuration file: %1\n"), 1);
	format_add("config_read_success", _("%> Configuratin read correctly.%n\n"), 1);
	format_add("config_line_incorrect", _("%! Invalid line '%T%1%n', skipping\n"), 1);
	format_add("autosaved", _("%> Automatically saved settings\n"), 1);

	/* config_upgrade() */
	format_add("config_upgrade_begin", _("%) EKG2 upgrade detected. In the meantime, following changes were made:\n"), 1);
	format_add("config_upgrade_important",	"%) %W%2) %y*%n %1\n", 1);
	format_add("config_upgrade_major",	"%) %W%2) %Y*%n %1\n", 1);
	format_add("config_upgrade_minor",	"%) %W%2) %c*%n %1\n", 1);
	format_add("config_upgrade_end", _("%) To make configuration upgrade permament, please save your configuration: %c/save%n\n"), 1);

	/* rejestracja nowego numeru */
	format_add("register", _("%> Registration successful. Your number: %T%1%n\n"), 1);
	format_add("register_change_passwd", _("%> Your password for account %T%1%n is '%T%2%n'. Change it as soon as possible, using command /passwd <newpassword>"), 1);
	format_add("register_failed", _("%! Error during registration: %1\n"), 1);
	format_add("register_pending", _("%! Registration in progress\n"), 1);
	format_add("register_timeout", _("%! Registration timed out\n"), 1);
	format_add("registered_today", _("%! Already registered. Do not abuse\n"), 1);

	/* deleting user's account from public catalog */
	format_add("unregister", _("%> Account removed\n"), 1);
	format_add("unregister_timeout", _("%! Account removal timed out\n"), 1);
	format_add("unregister_bad_uin", _("%! Unknown uin: %T%1%n\n"), 1);
	format_add("unregister_failed", _("%! Error while deleting account: %1\n"), 1);

	/* password remind */
	format_add("remind", _("%> Password sent\n"), 1);
	format_add("remind_failed", _("%! Error while sending password: %1\n"), 1);
	format_add("remind_timeout", _("%! Password sending timed out\n"), 1);

	/* password change */
	format_add("passwd", _("%> Password changed\n"), 1);
	format_add("passwd_failed", _("%! Error while changing password: %1\n"), 1);
	format_add("passwd_timeout", _("%! Password changing timed out\n"), 1);
	format_add("passwd_possible_abuse", "%> (%1) Password reply send by wrong uid: %2, if this is good server uid please report this to developers and manually "
						"change your session password using /session password", 1);
	format_add("passwd_abuse", "%! (%1) Somebody want to clear our password (%2)", 1);

	/* changing information in public catalog */
	format_add("change", _("%> Informations in public directory chenged\n"), 1);
	format_add("change_failed", _("%! Error while changing informations in public directory\n"), 1);

	/* users finding */
	format_add("search_failed", _("%! Error while search: %1\n"), 1);
	format_add("search_not_found", _("%! Not found\n"), 1);
	format_add("search_no_last", _("%! Last search returned no result\n"), 1);
	format_add("search_no_last_nickname", _("%! No nickname in last search\n"), 1);
	format_add("search_stopped", _("%> Search stopped\n"), 1);

	/* 1 uin, 2 name, 3 nick, 4 city, 5 born, 6 gender, 7 active */
	format_add("search_results_multi_avail", "%Y<>%n", 1);
	format_add("search_results_multi_away", "%G<>%n", 1);
	format_add("search_results_multi_invisible", "%c<>%n", 1);
	format_add("search_results_multi_notavail", "  ", 1);
	format_add("search_results_multi_unknown", " -", 1);
	/*	format_add("search_results_multi_female", "k", 1); */
	/*	format_add("search_results_multi_male", "m", 1); */
	format_add("search_results_multi", "%7 %[-8]1 %K|%n %[12]3 %K|%n %[12]2 %K|%n %[4]5 %K|%n %[12]4\n", 1);

	format_add("search_results_single_avail", _("%Y(available)%N"), 1);
	format_add("search_results_single_away", _("%G(away)%n"), 1);
	format_add("search_results_single_notavail", _("%r(offline)%n"), 1);
	format_add("search_results_single_invisible", _("%c(invisible)%n"), 1);
	format_add("search_results_single_unknown", "%T-%n", 1);
	format_add("search_results_single", _("%) Nickname:  %T%3%n\n%) Number: %T%1%n %7\n%) Name: %T%2%n\n%) City: %T%4%n\n%) Birth year: %T%5%n\n"), 1);

	/* exec */
	format_add("process", "%> %(-5)1 %2\n", 1);
	format_add("no_processes", _("%! There are no running procesees\n"), 1);
	format_add("process_exit", _("%> Proces %1 (%2) exited with %3 status\n"), 1);
	format_add("exec", "%1\n",1);	/* lines are ended by \n */
	format_add("exec_error", _("%! Error running process : %1\n"), 1);
	format_add("exec_prompt", "$ %1\n", 1);

	/* detailed info about user */
	format_add("user_info_header", "%K.--%n %T%1%n/%2 %K--- -- -%n\n", 1);
	format_add("user_info_nickname", _("%K| %nNickname: %T%1%n\n"), 1);
	format_add("user_info_status", _("%K| %nStatus: %T%1%n\n"), 1);
	format_add("user_info_status_time_format", "%Y-%m-%d %H:%M", 1);
	format_add("user_info_status_time", _("%K| %nCurrent status since: %T%1%n\n"), 1);
	format_add("user_info_block", _("%K| %nBlocked\n"), 1);
	format_add("user_info_offline", _("%K| %nCan't see our status\n"), 1);
	format_add("user_info_name", _("%K| %nName: %T%1 %2%n\n"), 1);
	format_add("user_info_mobile", _("%K| %nTelephone: %T%1%n\n"), 1);
	format_add("user_info_ip", _("%K| %nAddress: %T%1%n\n"), 1);
	format_add("user_info_last_ip", _("%K| %nLast address: %T%1%n\n"), 1);
	format_add("user_info_groups", _("%K| %nGroups: %T%1%n\n"), 1);
	format_add("user_info_never_seen", _("%K| %nNever seen\n"), 1);
	format_add("user_info_last_seen", _("%K| %nLast seen: %T%1%n\n"), 1);
	format_add("user_info_last_seen_time", "%Y-%m-%d %H:%M", 1);
	format_add("user_info_last_status", _("%K| %nLast status: %T%1%n\n"), 1);
	format_add("user_info_footer", "%K`----- ---- --- -- -%n\n", 1);

	format_add("user_info_avail", _("%Yavailable%n"), 1);
	format_add("user_info_avail_descr", _("%Yavailable%n %K(%n%2%K)%n"), 1);
	format_add("user_info_away", _("%Gaway%n"), 1);
	format_add("user_info_away_descr", _("%Gaway%n %K(%n%2%K)%n"), 1);
	format_add("user_info_notavail", _("%roffline%n"), 1);
	format_add("user_info_notavail_descr", _("%roffline%n %K(%n%2%K)%n"), 1);
	format_add("user_info_invisible", _("%cinvisible%n"), 1);
	format_add("user_info_invisible_descr", _("%cinvisible%n %K(%n%2%K)%n"), 1);
	format_add("user_info_dnd", _("%Bdo not disturb%n"), 1);
	format_add("user_info_dnd_descr", _("%Bdo not disturb%n %K(%n%2%K)%n"), 1);
	format_add("user_info_chat", _("%Wfree for chat%n"), 1);
	format_add("user_info_chat_descr", _("%Wfree for chat%n %K(%n%2%K)%n"), 1);
	format_add("user_info_error", _("%m error%n"), 1);
	format_add("user_info_error_descr", _("%merror%n %K(%n%2%K)%n"), 1);
	format_add("user_info_xa", _("%gextended away%n"), 1);
	format_add("user_info_xa_descr", _("%gextended away%n %K(%n%2%K)%n"), 1);
	format_add("user_info_gone", _("%Rgone%n"), 1);
	format_add("user_info_gone_descr", _("%Rgone%n %K(%n%2%K)%n"), 1);
	format_add("user_info_blocking", _("%mblocking%n"), 1);
	format_add("user_info_blocking_descr", _("%mblocking%n %K(%n%2%K)%n"), 1);
	format_add("user_info_unknown", _("%Munknown%n"), 1);

	format_add("resource_info_status", _("%K| %nResource: %W%1%n Status: %T%2 Prio: %g%3%n"), 1);

	/* grupy */
	format_add("group_members", _("%> %|Group %T%1%n: %2\n"), 1);
	format_add("group_member_already", _("%! %1 already in group %T%2%n\n"), 1);
	format_add("group_member_not_yet", _("%! %1 not in group %T%2%n\n"), 1);
	format_add("group_empty", _("%! Group %T%1%n is empty\n"), 1);

	/* status */
	format_add("show_status_profile", _("%) Profile: %T%1%n\n"), 1);
	format_add("show_status_uid", "%) UID: %T%1%n\n", 1);
	format_add("show_status_uid_nick", "%) UID: %T%1%n (%T%2%n)\n", 1);
	format_add("show_status_status", _("%) Current status: %T%1%2%n\n"), 1);
	format_add("show_status_status_simple", _("%) Current status: %T%1%n\n"), 1);
	format_add("show_status_server", _("%) Current server: %T%1%n:%T%2%n\n"), 1);
	format_add("show_status_server_tls", _("%) Current server: %T%1%n:%T%2%Y (connection encrypted)%n\n"), 1);
	format_add("show_status_connecting", _("%) Connecting ..."), 1);
	format_add("show_status_avail", _("%Yavailable%n"), 1);
	format_add("show_status_avail_descr", _("%Yavailable%n (%T%1%n%2)"), 1);
	format_add("show_status_away", _("%Gaway%n"), 1);
	format_add("show_status_away_descr", _("%Gaway%n (%T%1%n%2)"), 1);
	format_add("show_status_invisible", _("%cinvisible%n"), 1);
	format_add("show_status_invisible_descr", _("%cinvisible%n (%T%1%n%2)"), 1);
	format_add("show_status_xa", _("%gextended away%n"), 1);
	format_add("show_status_xa_descr", _("%gextended away%n (%T%1%n%2)"), 1);
	format_add("show_status_gone", _("%Rgone%n"), 1);
	format_add("show_status_gone_descr", _("%Rgone%n (%T%1%n%2)"), 1);
	format_add("show_status_dnd", _("%cdo not disturb%n"), 1);
	format_add("show_status_dnd_descr", _("%cdo not disturb%n (%T%1%n%2)"), 1);
	format_add("show_status_chat", _("%Wfree for chat%n"), 1);
	format_add("show_status_chat_descr", _("%Wfree for chat%n (%T%1%n%2)"), 1);
	format_add("show_status_notavail", _("%roffline%n"), 1);
	format_add("show_status_private_on", _(", for friends only"), 1);
	format_add("show_status_private_off", "", 1);
	format_add("show_status_connected_since", _("%) Connected since: %T%1%n\n"), 1);
	format_add("show_status_disconnected_since", _("%) Disconnected since: %T%1%n\n"), 1);
	format_add("show_status_last_conn_event", "%Y-%m-%d %H:%M", 1);
	format_add("show_status_last_conn_event_today", "%H:%M", 1);
	format_add("show_status_ekg_started_since", _("%) Program started: %T%1%n\n"), 1);
	format_add("show_status_ekg_started", "%Y-%m-%d %H:%M", 1);
	format_add("show_status_ekg_started_today", "%H:%M", 1);
	format_add("show_status_msg_queue", _("%) Messages queued for delivery: %T%1%n\n"), 1);

	/* aliasy */
	format_add("aliases_list_empty", _("%! No aliases\n"), 1);
	format_add("aliases_list", "%> %T%1%n: %2\n", 1);
	format_add("aliases_list_next", "%> %3	%2\n", 1);
	format_add("aliases_add", _("%> Created alias %T%1%n\n"), 1);
	format_add("aliases_append", _("%> Added to alias %T%1%n\n"), 1);
	format_add("aliases_del", _("%) Removed alias %T%1%n\n"), 1);
	format_add("aliases_del_all", _("%) Removed all aliases\n"), 1);
	format_add("aliases_exist", _("%! Alias %T%1%n already exists\n"), 1);
	format_add("aliases_noexist", _("%! Alias %T%1%n doesn't exist\n"), 1);
	format_add("aliases_command", _("%! %T%1%n is internal command\n"), 1);
	format_add("aliases_not_enough_params", _("%! Alias %T%1%n requires more parameters\n"), 1);

	/* dcc connections */
	format_add("dcc_attack", _("%! To many direct connections, last from %1\n"), 1);
	format_add("dcc_limit", _("%! %|Direct connections count over limit, so they got disabled. To enable them use %Tset dcc 1% and reconnect. Limit is controlled by %Tdcc_limit%n variable.\n"), 1);
	format_add("dcc_create_error", _("%! Can't turn on direct connections: %1\n"), 1);
	format_add("dcc_error_network", _("%! Error transmitting with %1\n"), 1);
	format_add("dcc_error_refused", _("%! Connection to %1 refused\n"), 1);
	format_add("dcc_error_unknown", _("%! Uknown direct connection error\n"), 1);
	format_add("dcc_error_handshake", _("%! Can't connect with %1\n"), 1);
	format_add("dcc_user_aint_dcc", _("%! %1 doesn't have direct connections enabled\n"), 1);
	format_add("dcc_timeout", _("%! Direct connection to %1 timed out\n"), 1);
	format_add("dcc_not_supported", _("%! Operation %T%1%n isn't supported yet\n"), 1);
	format_add("dcc_open_error", _("%! Can't open %T%1%n: %2\n"), 1);
	format_add("dcc_show_pending_header", _("%> Pending connections:\n"), 1);
	format_add("dcc_show_pending_send", _("%) #%1, %2, sending %T%3%n\n"), 1);
	format_add("dcc_show_pending_get", _("%) #%1, %2, receiving %T%3%n\n"), 1);
	format_add("dcc_show_pending_voice", _("%) #%1, %2, chat\n"), 1);
	format_add("dcc_show_active_header", _("%> Active connections:\n"), 1);
	format_add("dcc_show_active_send", _("%) #%1, %2, sending %T%3%n, %T%4b%n z %T%5b%n (%6%%)\n"), 1);
	format_add("dcc_show_active_get", _("%) #%1, %2, receiving %T%3%n, %T%4b%n z %T%5b%n (%6%%)\n"), 1);
	format_add("dcc_show_active_voice", _("%) #%1, %2, chat\n"), 1);
	format_add("dcc_show_empty", _("%! No direct connections\n"), 1);
	format_add("dcc_receiving_already", _("%! File %T%1%n from %2 is being received\n"), 1);

	format_add("dcc_done_get", _("%> Finished receiving file %T%2%n from %1\n"), 1);
	format_add("dcc_done_send", _("%> Finished sending file %T%2%n to %1\n"), 1);
	format_add("dcc_close", _("%) Connection with %1 closed\n"), 1);

	format_add("dcc_voice_offer", _("%) %1 wants to chat\n%) Use  %Tdcc voice #%2%n to start chat or %Tdcc close #%2%n to refuse\n"), 1);
	format_add("dcc_voice_running", _("%! Only one simultanous voice chat possible\n"), 1);
	format_add("dcc_voice_unsupported", _("%! Voice chat not compiled in. See %Tdocs/voip.txt%n\n"), 1);
	format_add("dcc_get_offer", _("%) %1 sends %T%2%n (size %T%3b%n)\n%) Use %Tdcc get #%4%n to receive or %Tdcc close #%4%n to refuse\n"), 1);
	format_add("dcc_get_offer_resume", _("%) File exist, you can resume with %Tdcc resume #%4%n\n"), 1);
	format_add("dcc_get_getting", _("%) Started receiving %T%2%n from %1\n"), 1);
	format_add("dcc_get_cant_create", _("%! Can't open file %T%1%n\n"), 1);
	format_add("dcc_not_found", _("%! Connection not found: %T%1%n\n"), 1);
	format_add("dcc_invalid_ip", _("%! Invalid IP address\n"), 1);
	format_add("dcc_user_notavail", _("%! %1 has to available to connect\n"), 1);

	/* query */
	format_add("query_started", _("%) (%2) Query with %T%1%n started\n"), 1);
	format_add("query_started_window", _("%) Press %TAlt-G%n to ignore, %TAlt-K%n to close window\n"), 1);
	format_add("query_finished", _("%) (%2) Finished query with %T%1%n\n"), 1);
	format_add("query_exist", _("%! (%3) Query with %T%1%n already in window no %T%2%n\n"), 1);

	/* zdarzenia */
	format_add("events_list_empty", _("%! No events\n"), 1);
	format_add("events_list_header", "", 1);
	format_add("events_list", "%> %5 on %1 %3 %4 - prio %2\n", 1);
	format_add("events_add", _("%> Added event %T%1%n\n"), 1);
	format_add("events_del", _("%) Removed event %T%1%n\n"), 1);
	format_add("events_del_all", _("%) Removed all events\n"), 1);
	format_add("events_exist", _("%! Event %T%1%n exist for %2\n"), 1);
	format_add("events_del_noexist", _("%! Event %T%1%n do not exist\n"), 1);

	/* contact list from the server */
	format_add("userlist_put_ok", _("%> (%1) Roster exported\n"), 1);
	format_add("userlist_put_error", _("%! (%1) Error exporting roster\n"), 1);
	format_add("userlist_get_ok", _("%> (%1) Roster imported\n"), 1);
	format_add("userlist_get_error", _("%! (%1) Error importing roster\n"), 1);
	format_add("userlist_clear_ok", _("%) (%1) Removed roster from server\n"), 1);
	format_add("userlist_clear_error", _("%! (%1) Error removing roster from server\n"), 1);

	/* szybka lista kontaktów pod F2 */
	format_add("quick_list", "%)%1\n", 1);
	format_add("quick_list,speech", _("roster:"), 1);
	format_add("quick_list_avail,speech", _("%1 is available"), 1);
	format_add("quick_list_away,speech", _("%1 is away"), 1);
	format_add("quick_list_chat,speech", _("%1 is free for chat"), 1);
	format_add("quick_list_xa,speech", _("%1 is extended away"), 1);
	format_add("quick_list_gone,speech", _("%1 is gone"), 1);
	format_add("quick_list_dnd,speech", _("%1 has 'do not disturb' status"), 1);

	/* window */
	format_add("window_add", _("%) New window created\n"), 1);
	format_add("window_noexist", _("%! Choosen window do not exist\n"), 1);
	format_add("window_doesnt_exist", _("%! Window %T%1%n does not exist\n"), 1);
	format_add("window_no_windows", _("%! Can't close last window\n"), 1);
	format_add("window_del", _("%) Window closed\n"), 1);
	format_add("windows_max", _("%! Window limit exhausted\n"), 1);
	format_add("window_list_query", _("%) %1: query with %T%2%n\n"), 1);
	format_add("window_list_nothing", _("%) %1 no query\n"), 1);
	format_add("window_list_floating", _("%) %1: floating %4x%5 in %2,%3 %T%6%n\n"), 1);
	format_add("window_id_query_started", _("%) (%3) Query with %T%2%n started in %T%1%n\n"), 1);
	format_add("window_kill_status", _("%! Can't close status window!\n"), 1);
	format_add("window_cannot_move_status", _("%! Can't move status window!\n"), 1);
	format_add("window_invalid_move", _("%! Window %T%1%n can't be moved\n"), 1);

	format_add("cant_kill_irc_window", _("Can't kill window. Use /window kill"), 1);

	format_add("file_doesnt_exist", _("%! Can't open file %T%1%n\n"), 1);

	/* bind */
	format_add("bind_seq_incorrect", _("%! Sequence %T%1%n is invalid\n"), 1);
	format_add("bind_seq_add", _("%> Sequence %T%1%n added\n"), 1);
	format_add("bind_seq_remove", _("%) Sequence %T%1%n removed\n"), 1);
	format_add("bind_seq_list", "%> %1: %T%2%n\n", 1);
	format_add("bind_seq_exist", _("%! Sequence %T%1%n is already bound\n"), 1);
	format_add("bind_seq_list_empty", _("%! No bound actions\n"), 1);
	format_add("bind_doesnt_exist", _("%! Can't find sequence %T%1%n\n"), 1);
	format_add("bind_press_key", _("%! Press key(s) which should be bound\n"), 1);
	format_add("bind_added", _("%> Binding added\n"), 1);

	/* at */
	format_add("at_list", "%> %1, %2, %3 %K(%4)%n %5\n", 1);
	format_add("at_added", _("%> Created plan %T%1%n\n"), 1);
	format_add("at_deleted", _("%) Removed plan %T%1%n\n"), 1);
	format_add("at_deleted_all", _("%) Removed user's plans\n"), 1);
	format_add("at_exist", _("%! Plan %T%1%n already exists\n"), 1);
	format_add("at_noexist", _("%! Plan %T%1%n do not exists\n"), 1);
	format_add("at_empty", _("%! No plans\n"), 1);
	format_add("at_timestamp", "%d-%m-%Y %H:%M", 1);
	format_add("at_back_to_past", _("%! If time travels were possible...\n"), 1);

	/* timer */
	format_add("timer_list", "%> %1, %2s, %3 %K(%4)%n %T%5%n\n", 1);
	format_add("timer_added", _("%> Created timer %T%1%n\n"), 1);
	format_add("timer_deleted", _("%) Removed timer  %T%1%n\n"), 1);
	format_add("timer_deleted_all", _("%) Removed user's timers\n"), 1);
	format_add("timer_exist", _("%! Timer %T%1%n already exists\n"), 1);
	format_add("timer_noexist", _("%! Timer %T%1%n does not exists\n"), 1);
	format_add("timer_empty", _("%! No timers\n"), 1);

	/* last */
	format_add("last_list_in", "%) %Y <<%n [%1] %2 %3\n", 1);
	format_add("last_list_out", "%) %G >>%n [%1] %2 %3\n", 1);
	format_add("last_list_status", "%) %G *%n [%1] %2 is %3\n", 1);
	format_add("last_list_status_descr", "%) %G *%n [%1] %2 is %3: %4\n", 1);
	format_add("last_list_empty", _("%! No messages logged\n"), 1);
	format_add("last_list_empty_status", _("%! No change status logged\n"), 1);
	format_add("last_list_empty_nick", _("%! No messages from %T%1%n logged\n"), 1);
	format_add("last_list_empty_nick_status", _("%! No change status by %T%1%n logged\n"), 1);
	format_add("last_list_timestamp", "%d-%m-%Y %H:%M", 1);
	format_add("last_list_timestamp_today", "%H:%M", 1);
	format_add("last_clear_uin", _("%) Messages from %T%1%n cleared\n"), 1);
	format_add("last_clear_uin_status", _("%) Statuses from %T%1%n cleared\n"), 1);
	format_add("last_clear", _("%) All messages cleared\n"), 1);
	format_add("last_clear_status", _("%) All statuses cleared\n"), 1);
	format_add("last_begin_uin", _("%) Lastlog from %T%1%n begins\n"), 1);
	format_add("last_begin_uin_status", _("%) Lastlog status from %T%1%n begins\n"), 1);
	format_add("last_begin", _("%) Lastlog begin\n"), 1);
	format_add("last_begin_status", _("%) Lastlog status begin\n"), 1);
	format_add("last_end", _("%) Lastlog end\n"), 1);
	format_add("last_end_status", _("%) Lastlog status end\n"), 1);


	/* lastlog */
	format_add("lastlog_title",	_("%) %gLastlog [%B%2%n%g] from window: %W%T%1%n"), 1);
	format_add("lastlog_title_cur", _("%) %gLastlog [%B%2%n%g] from window: %W%T%1 (*)%n"), 1);

	/* queue */
	format_add("queue_list_timestamp", "%d-%m-%Y %H:%M", 1);
	format_add("queue_list_message", "%) %G >>%n [%1] %2 %3\n", 1);
	format_add("queue_clear", _("%) Message queue cleared\n"), 1);
	format_add("queue_clear_uid", _("%) Message queue for %T%1%n cleared\n"), 1);
	format_add("queue_wrong_use", _("%! Command works only when disconected\n"), 1);
	format_add("queue_empty", _("%! Messaged queue is empty\n"), 1);
	format_add("queue_empty_uid", _("%! No messages to %T%1%n in queue\n"), 1);
	format_add("queue_flush", _("%> (%1) Sent messages from queue\n"), 1);

	/* conference */
	format_add("conferences_list_empty", _("%! No conference\n"), 1);
	format_add("conferences_list", "%> %T%1%n: %2\n", 1);
	format_add("conferences_list_ignored", _("%> %T%1%n: %2 (%yignored%n)\n"), 1);
	format_add("conferences_add", _("%> Created conference %T%1%n\n"), 1);
	format_add("conferences_not_added", _("%! Conference not created %T%1%n\n"), 1);
	format_add("conferences_del", _("%) Removed conference %T%1%n\n"), 1);
	format_add("conferences_del_all", _("%) Removed all conferences\n"), 1);
	format_add("conferences_exist", _("%! Conference %T%1%n already exists\n"), 1);
	format_add("conferences_noexist", _("%! Conference %T%1%n do not exists\n"), 1);
	format_add("conferences_name_error", _("%! Conference name should start with %T#%n\n"), 1);
	format_add("conferences_rename", _("%> Conference renamed: %T%1%n --> %T%2%n\n"), 1);
	format_add("conferences_ignore", _("%> Konference %T%1%n will be ignored\n"), 1);
	format_add("conferences_unignore", _("%> Conference %T%1%n won't be ignored\n"), 1);
	format_add("conferences_joined", _("%> Joined %1 to conference %T%2%n\n"), 1);
	format_add("conferences_already_joined", _("%> %1 already in conference %T%2%n\n"), 1);

	/* shared by http */
	format_add("http_failed_resolving", _("Server not found"), 1);
	format_add("http_failed_connecting", _("Can not connect ro server"), 1);
	format_add("http_failed_reading", _("Server disconnected"), 1);
	format_add("http_failed_writing", _("Server disconnected"), 1);
	format_add("http_failed_memory", _("No memory"), 1);

	format_add("session_name", "%B%1%n", 1);
	format_add("session_variable", "%> %T%1->%2 = %R%3%n\n", 1); /* uid, var, new_value*/
	format_add("session_variable_removed", _("%> Removed  %T%1->%2%n\n"), 1); /* uid, var */
	format_add("session_variable_doesnt_exist", _("%! Unknown variable: %T%1->%2%n\n"), 1); /* uid, var */
	format_add("session_list", "%> %T%1%n %3\n", 1); /* uid, uid, %{user_info_*} */
	format_add("session_list_alias", "%> %T%2%n/%1 %3\n", 1); /* uid, alias, %{user_info_*} */
	format_add("session_list_empty", _("%! Session list is empty\n"), 1);
	format_add("session_info_header", "%) %T%1%n %3\n", 1); /* uid, uid, %{user_info_*} */
	format_add("session_info_header_alias", "%) %T%2%n/%1 %3\n", 1); /* uid, alias, %{user_info_*} */
	format_add("session_info_param", "%)	%1 = %T%2%n\n", 1); /* key, value */
	format_add("session_info_footer", "", 1); /* uid */
	format_add("session_exists", _("%! Session %T%1%n already exists\n"), 1); /* uid */
	format_add("session_doesnt_exist", _("%! Sesion %T%1%n does not exist\n"), 1); /* uid */
	format_add("session_added", _("%> Created session %T%1%n\n"), 1); /* uid */
	format_add("session_removed", _("%> Removed session %T%1%n\n"), 1); /* uid */
	format_add("session_format", "%T%1%n", 1);
	format_add("session_format_alias", "%T%1%n/%2", 1);
	format_add("session_cannot_change", _("%! Can't change session in query window%n\n"), 1);
	format_add("session_password_changed", _("%> %|(%1) Looks like you're changing password in connected session. This does only set password on the client-side. If you want you change your account password, please use dedicated function (e.g. /passwd)."), 1);
	format_add("session_locked", _("%! %|Session %T%1%n is currently locked. If there aren't any other copy of EKG2 using it, please call: %c/session --unlock%n to unlock it.\n"), 1);
	format_add("session_not_locked", _("%! Session %T%1%n is not locked"), 1);

	format_add("metacontact_list", "%> %T%1%n", 1);
	format_add("metacontact_list_empty", "%! Metacontact list is empty\n", 1);
	format_add("metacontact_exists", "%! Metacontact %T%1%n already exists\n", 1);
	format_add("metacontact_added", "%> Metacontact %T%1%n added\n", 1);
	format_add("metacontact_removed", "%> Metacontact %T%1%n removed\n", 1);
	format_add("metacontact_doesnt_exist", "%! Metacontact %T%1%n doesn't exist\n", 1);
	format_add("metacontact_added_item", "%> Added %T%1/%2%n to metacontact %T%3%n\n", 1);
	format_add("metacontact_removed_item", "%> Removed %T%1/%2%n from metacontact %T%3%n\n", 1);
	format_add("metacontact_item_list_header", "", 1);
	format_add("metacontact_item_list", "%> %T%1/%2 (%3)%n - prio %T%4%n\n", 1);
	format_add("metacontact_item_list_empty", "%! Metacontact is empty\n", 1);
	format_add("metacontact_item_list_footer", "", 1);
	format_add("metacontact_item_doesnt_exist", "%! Contact %T%1/%2%n doesn't exiet\n", 1);
	format_add("metacontact_info_header", "%K.--%n Metacontact %T%1%n %K--- -- -%n\n", 1);
	format_add("metacontact_info_status", "%K| %nStatus: %T%1%n\n", 1);
	format_add("metacontact_info_footer", "%K`----- ---- --- -- -%n\n", 1);

	format_add("metacontact_info_avail", _("%Yavailable%n"), 1);
	format_add("metacontact_info_avail_descr", _("%Yavailable%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_away", _("%Gaway%n"), 1);
	format_add("metacontact_info_away_descr", _("%Gaway%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_notavail", _("%roffline%n"), 1);
	format_add("metacontact_info_notavail_descr", _("%roffline%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_invisible", _("%cinvisible%n"), 1);
	format_add("metacontact_info_invisible_descr", _("%cinvisible%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_dnd", _("%Bdo not disturb%n"), 1);
	format_add("metacontact_info_dnd_descr", _("%Bdo not disturb%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_chat", _("%Wfree for chat%n"), 1);
	format_add("metacontact_info_chat_descr", _("%Wfree for chat%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_error", _("%merror%n"), 1);
	format_add("metacontact_info_error_descr", _("%merror%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_xa", _("%gextended away%n"), 1);
	format_add("metacontact_info_xa_descr", _("%gextended away%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_gone", _("%Rgone%n"), 1);
	format_add("metacontact_info_gone_descr", _("%Rgone%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_blocking", _("%mblocking%n"), 1);
	format_add("metacontact_info_blocking_descr", _("%mblocking%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_unknown", _("%Munknown%n"), 1);

	format_add("plugin_already_loaded", _("%! Plugin %T%1%n already loaded%n.\n"), 1);
	format_add("plugin_doesnt_exist", _("%! Plugin %T%1%n can not be found%n\n"), 1);
	format_add("plugin_incorrect", _("%! Plugin %T%1%n is not correct EKG2 plugin%n\n"), 1);
	format_add("plugin_not_initialized", _("%! Plugin %T%1%n not initialized correctly, check debug window%n\n"), 1);
	format_add("plugin_unload_ui", _("%! Plugin %T%1%n is an UI plugin and can't be unloaded%n\n"), 1);
	format_add("plugin_loaded", _("%> Plugin %T%1%n loaded%n\n"), 1);
	format_add("plugin_unloaded", _("%> Plugin %T%1%n unloaded%n\n"), 1);
	format_add("plugin_list", _("%> %T%1%n - %2%n\n"), 1);
	format_add("plugin_prio_set", _("%> Plugin %T%1%n prio has been changed to %T%2%n\n"), 1);
	format_add("plugin_default", _("%> Plugins prio setted to default\n"), 1);

	format_add("script_autorun_succ",	_("%> Script %W%1%n successful %G%2%n autorun dir"), 1);		/* XXX sciezka by sie przydala */
	format_add("script_autorun_fail",	_("%! Script %W%1%n failed %R%2%n autorun dir %r(%3)"), 1);
	format_add("script_autorun_unkn",	_("%! Error adding/removing script %W%1%n from autorundir %r(%3)"), 1);
	format_add("script_loaded",		_("%) Script %W%1%n %g(%2)%n %Gloaded %b(%3)"), 1);
	format_add("script_incorrect",		_("%! Script %W%1%n %g(%2)%n %rNOT LOADED%n %R[incorrect %3 script or you've got syntax errors]"), 1);
	format_add("script_incorrect2",		_("%! Script %W%1%n %g(%2)%n %rNOT LOADED%n %R[script has no handler or error in getting handlers]"), 1);
	format_add("script_removed",		_("%) Script %W%1%n %g(%2)%n %Rremoved %b(%3)"), 1);
	format_add("script_need_name",		_("%! No filename given\n"), 1);
	format_add("script_not_found",		_("%! Can't find script %W%1"), 1);
	format_add("script_wrong_location",	_("%! Script have to be in %g%1%n (don't add path)"), 1);
	format_add("script_error",		_("%! %rScript error: %|%1"), 1);

	format_add("script_autorun_list", "%) Script %1 -> %2\n", 1);
	format_add("script_eval_error", _("%! Error running code\n"), 1);
	format_add("script_list", _("%> %1 (%2, %3)\n"), 1);
	format_add("script_list_empty", _("%! No scripts loaded\n"), 1);
	format_add("script_generic", "%> [script,%2] (%1) %3\n", 1);
	format_add("script_varlist", _("%> %1 = %2 (%3)\n"), 1);
	format_add("script_varlist_empty", _("%! No script vars!\n"), 1);

	format_add("directory_cant_create",	_("%! Can't create directory: %1 (%2)"), 1);

	/* charset stuff */
	format_add("console_charset_using",	_("%) EKG2 detected that your console works under: %W%1%n Please verify and eventually change %Gconsole_charset%n variable"), 1);
	format_add("console_charset_bad",	_("%! EKG2 detected that your console works under: %W%1%n, but in %Gconsole_charset%n variable you've got: %W%2%n Please verify."), 1);
	format_add("iconv_fail",		_("%! iconv_open() fail to initialize charset conversion between %W%1%n and %W%2%n. Check %Gconsole_charset%n variable, if it won't help inform ekg2 dev team and/or upgrade iconv"), 1);
	format_add("iconv_list",		_("%) %g%[-10]1%n %c<==> %g%[-10]2%n %b(%nIn use: %W%3, %4%b)"), 1);
	format_add("iconv_list_bad",		_("%! %R%[-10]1%n %r<==> %R%[-10]2%n %b[%rINIT ERROR: %5%b] %b(%nIn use: %W%3, %4%b) "), 1);

#ifdef WITH_ASPELL
	/* aspell */
	format_add("aspell_init", "%> Please wait while initiating spellcheck...", 1);
	format_add("aspell_init_success", "%> Spellcheck initiated.", 1);
	format_add("aspell_init_error", "%! Spellcheck error: %T%1%", 1);
#endif 
	/* jogger-like I/O */
	format_add("io_cantopen", _("%! %|Unable to open file: %T%1%n (%c%2%n)!"), 1);
	format_add("io_nonfile", _("%! %|Given path doesn't appear to be regular file: %T%1%n!"), 1);
	format_add("io_cantread", _("%! %|Unable to read file: %T%1%n (%c%2%n)!"), 1);
	format_add("io_truncated", _("%! %|WARNING: EOF before reaching filesize (%c%2%n vs. %c%3%n). File %T%1%n probably truncated (somehow)!"), 1);
	format_add("io_expanded", _("%! %|WARNING: EOF after reaching filesize (%c%2%n vs. %c%3%n). File %T%1%n probably got expanded!"), 1);
	format_add("io_emptyfile", _("%! File %T%1%n is empty!"), 1);
	format_add("io_toobig", _("%! Size of file %T%1%n exceeds maximum allowed length (at least %c%2%n vs. %c%3%n)!"), 1);
	format_add("io_binaryfile", _("%! %|WARNING: The file %T%1%n probably contains NULs (is binary), so it can't be properly handled. It will be read until first encountered NUL, i.e. to offset %c%2%n (vs. filesize of %c%3%n)!"), 1);

	/* dns stuff */
	format_add("dns2_resolved",	_("%) DNS2 RESOLVED %G%1: %W%2%n"), 1);

	theme_plugins_init();
#endif	/* !NO_DEFAULT_THEME */
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
