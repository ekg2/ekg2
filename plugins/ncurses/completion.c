/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                2004 Piotr Kupisiewicz <deli@rzepaknet.us>
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

#define _BSD_SOURCE
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

#include "ekg2-config.h"

#ifndef HAVE_SCANDIR
#  include "compat/scandir.h"
#endif

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/events.h>
#include <ekg/metacontacts.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

#include "old.h"

/* nadpisujemy funkcjê xstrncasecmp() odpowiednikiem z obs³ug± polskich znaków */
#define xstrncasecmp(x...) xstrncasecmp_pl(x)

static char **completions = NULL;	/* lista dope³nieñ */
static char *last_line = NULL;
static char *last_line_without_complete = NULL;
static int last_pos = -1;
int continue_complete = 0;
int continue_complete_count = 0;
command_t *actual_completed_command;
session_t *session_in_line;

static void command_generator(const char *text, int len)
{
	const char *slash = (""), *dash = ("");
	list_t l;
	session_t *session = session_current;
	if (*text == ('/')) {
		slash = ("/");
		text++;
		len--;
	}

	if (*text == ('^')) {
		dash = ("^");
		text++;
		len--;
	}

	if (window_current->target)
		slash = ("/");

	for (l = commands; l; l = l->next) {
		command_t *c = l->data;
		char *without_sess_id = NULL;
		int plen = 0;
		if (session && session->uid)
			plen = (int)(xstrchr(session->uid, ':') - session->uid) + 1;

                if (session && !xstrncasecmp(c->name, session->uid, plen))
			without_sess_id = xstrchr(c->name, ':');

		if (!xstrncasecmp(text, c->name, len) && !array_item_contains(completions, c->name, 1))
			array_add_check(&completions, 
					saprintf(("%s%s%s"), slash, dash, c->name),
					1);
		else if (without_sess_id && !array_item_contains(completions, without_sess_id + 1, 1) && !xstrncasecmp(text, without_sess_id + 1, len))
			array_add_check(&completions, 
					saprintf(("%s%s%s"), slash, dash, without_sess_id + 1),
					1);
	}
}

static void events_generator(const char *text, int len)
{
	int i;
	for (i = 0; events_all && events_all[i]; i++)
		if (!xstrncasecmp(text, events_all[i], len))
			array_add_check(&completions, xstrdup(events_all[i]), 1);
}

static void ignorelevels_generator(const char *text, int len)
{
	int i;
	const char *tmp = NULL;
	char *pre = NULL;

	if ((tmp = xstrrchr(text, '|')) || (tmp = xstrrchr(text, ','))) {
		char *foo;

		pre = xstrdup(text);
		foo = xstrrchr(pre, *tmp);
		*(foo + 1) = 0;

		len -= tmp - text + 1;
		tmp = tmp + 1;
	} else
		tmp = text;

	for (i = 0; ignore_labels[i].name; i++)
		if (!xstrncasecmp(tmp, ignore_labels[i].name, len))
			array_add_check(&completions, ((tmp == text) ? xstrdup(ignore_labels[i].name) : saprintf("%s%s", pre, ignore_labels[i].name)), 1);
}

static void unknown_uin_generator(const char *text, int len)
{
	int i;
	
	for (i = 0; i < send_nicks_count; i++) {
		if (send_nicks[i] && xstrchr(send_nicks[i], ':') && xisdigit(xstrchr(send_nicks[i], ':')[1]) && !xstrncasecmp(text, send_nicks[i], len)) {
			array_add_check(&completions, xstrdup(send_nicks[i]), 1);
		}
	}
}

static void known_uin_generator(const char *text, int len)
{
	int done = 0;
	list_t l;
	session_t *s;
	char *tmp = NULL, *session_name = NULL;
	int tmp_len = 0;
	newconference_t *c; 

	if (!session_current)
		return;

	s  = session_current;

	tmp = xstrrchr(text, '/');
	if (tmp && tmp + 1) {
		tmp++;
		tmp_len = xstrlen(tmp);
		session_name = xstrndup(text, xstrlen(text) - tmp_len - 1);
		if (session_find(session_name))
			s = session_find(session_name);
	}

	for (l = s->userlist; l; l = l->next) {
		userlist_t *u = l->data;
		if (u->nickname && !xstrncasecmp(text, u->nickname, len)) {
			array_add_check(&completions, xstrdup(u->nickname), 1);
			done = 1;
		}
		
		if (u->nickname && tmp && !xstrncasecmp(tmp, u->nickname, tmp_len)) { 
                        array_add_check(&completions, saprintf(("%s/%s"), session_name, u->nickname), 1);
                        done = 1;
		}
	}

	for (l = s->userlist; l; l = l->next) {
		userlist_t *u = l->data;

		if (!done && !xstrncasecmp(text, u->uid, len)) {
			array_add_check(&completions, xstrdup(u->uid), 1);
		}
		if (!done && tmp && !xstrncasecmp(tmp, u->uid, tmp_len)) 
                       array_add_check(&completions, saprintf(("%s/%s"), session_name, u->uid), 1);
	}

	if (!window_current) 
		goto end;

	if ((c = newconference_find(window_current->session, window_current->target)))	l = c->participants;
	else										l = window_current->userlist;

        for (; l; l = l->next) {
                userlist_t *u = l->data;

                if (u->uid && !xstrncasecmp(text, u->uid, len)) {
                        array_add_check(&completions, xstrdup(u->uid), 1);
		}

                if (u->nickname && !xstrncasecmp(text, u->nickname, len)) {
			array_add_check(&completions, xstrdup(u->nickname), 1);
		}
        } 

end:
	if (session_name)
		xfree(session_name);
}

static void conference_generator(const char *text, int len)
{
        list_t l;

        for (l = conferences; l; l = l->next) {
                struct conference *c = l->data;

                if (!xstrncasecmp(text, c->name, len))
                        array_add_check(&completions, xstrdup(c->name), 1);
        }
}

static void plugin_generator(const char *text, int len)
{
        list_t l;

        for (l = plugins; l; l = l->next) {
                plugin_t *p = l->data;

                if (!xstrncasecmp(text, p->name, len)) {
                        array_add_check(&completions, xstrdup(p->name), 1);
		}
		if ((text[0] == '+' || text[0] == '-') && !xstrncasecmp(text + 1, p->name, len - 1)) {
			char *tmp = saprintf(("%c%s"), text[0], p->name);
			array_add_check(&completions, tmp, 1);
		}
        }
}

static void variable_generator(const char *text, int len)
{
	list_t l;
	for (l = variables; l; l = l->next) {
		variable_t *v = l->data;

		if (v->type == VAR_FOREIGN)
			continue;

		if (*text == '-') {
			if (!xstrncasecmp(text + 1, v->name, len - 1))
				array_add_check(&completions, 
				saprintf("-%s", v->name),
				1);
		} else {
			if (!xstrncasecmp(text, v->name, len)) {
				array_add_check(&completions, xstrdup(v->name), 1);
			}
		}
	}
}

static void ignored_uin_generator(const char *text, int len)
{
        session_t *s;
	list_t l;

        if (!session_current)
                return;

        s  = session_current;

	for (l = s->userlist; l; l = l->next) {
		userlist_t *u = l->data;

		if (!ignored_check(s, u->uid))
			continue;

		if (!u->nickname) {
			if (!xstrncasecmp(text, u->uid, len))
				array_add_check(&completions, xstrdup(u->uid), 1);
		} else {
			if (u->nickname && !xstrncasecmp(text, u->nickname, len))
				array_add_check(&completions, xstrdup(u->nickname), 1);
		}
	}
}

static void blocked_uin_generator(const char *text, int len)
{
        session_t *s;
	list_t l;

        if (!session_current)
                return;

        s  = session_current;

	for (l = s->userlist; l; l = l->next) {
		userlist_t *u = l->data;

		if (!ekg_group_member(u, "__blocked"))
			continue;

		if (!u->nickname) {
			if (!xstrncasecmp(text, u->uid, len))
				array_add_check(&completions, xstrdup(u->uid), 1);
		} else {
			if (u->nickname && !xstrncasecmp(text, u->nickname, len))
				array_add_check(&completions, xstrdup(u->nickname), 1);
		}
	}
}

static void empty_generator(const char *text, int len)
{

}

static void dir_generator(const char *text, int len)
{
	struct dirent **namelist = NULL;
	char *dname, *tmp;
	const char *fname;
	int count, i;

	/* `dname' zawiera nazwê katalogu z koñcz±cym znakiem `/', albo
	 * NULL, je¶li w dope³nianym tek¶cie nie ma ¶cie¿ki. */

	dname = xstrdup(text);

	if ((tmp = xstrrchr(dname, '/'))) {
		tmp++;
		*tmp = 0;
	} else {
		xfree(dname);
		dname = NULL;
	}

	/* `fname' zawiera nazwê szukanego pliku */

	fname = xstrrchr(text, '/');

	if (fname)
		fname++;
	else
		fname = text;

	/* zbierzmy listê plików w ¿±danym katalogu */

	count = scandir((dname) ? dname : ".", &namelist, NULL, alphasort);

	debug("dname=\"%s\", fname=\"%s\", count=%d\n", (dname) ? dname : "(null)", (fname) ? fname : "(null)", count);

	for (i = 0; i < count; i++) {
		char *name = namelist[i]->d_name, *tmp = saprintf("%s%s", (dname) ? dname : "", name);
		struct stat st;

		if (!stat(tmp, &st)) {
			if (!S_ISDIR(st.st_mode)) {
				xfree(tmp);
				xfree(namelist[i]);
				continue;
			}
		}

		xfree(tmp);

		if (!xstrcmp(name, ".")) {
			xfree(namelist[i]);
			continue;
		}

		/* je¶li mamy `..', sprawd¼ czy katalog sk³ada siê z
		 * `../../../' lub czego¶ takiego. */

		if (!xstrcmp(name, "..")) {
			const char *p;
			int omit = 0;

			for (p = dname; p && *p; p++) {
				if (*p != '.' && *p != '/') {
					omit = 1;
					break;
				}
			}

			if (omit) {
				xfree(namelist[i]);
				continue;
			}
		}

		if (!strncmp(name, fname, xstrlen(fname))) {
			name = saprintf("%s%s%s", (dname) ? dname : "", name, "/");
			array_add_check(&completions, name, 1);
		}

		xfree(namelist[i]);
        }

	xfree(dname);
	xfree(namelist);
}

static void file_generator(const char *text, int len)
{
	struct dirent **namelist = NULL;
	char *dname;
	char *tmp;
	const char *fname;
	int count, i;

	/* `dname' zawiera nazwê katalogu z koñcz±cym znakiem `/', albo
	 * NULL, je¶li w dope³nianym tek¶cie nie ma ¶cie¿ki. */

	dname = xstrdup(text);

	if ((tmp = xstrrchr(dname, '/'))) {
		tmp++;
		*tmp = 0;
	} else {
		xfree(dname);
		dname = NULL;
	}

	/* `fname' zawiera nazwê szukanego pliku */

	fname = xstrrchr(text, '/');

	if (fname)
		fname++;
	else
		fname = text;

again:
	/* zbierzmy listê plików w ¿±danym katalogu */

	count = scandir((dname) ? dname : ".", &namelist, NULL, alphasort);

	debug("dname=\"%s\", fname=\"%s\", count=%d\n", (dname) ? dname : "(null)", (fname) ? fname : "(null)", count);

	for (i = 0; i < count; i++) {
		char *name = namelist[i]->d_name, *tmp = saprintf("%s%s", (dname) ? dname : "", name);
		struct stat st;
		int isdir = 0;

		if (!stat(tmp, &st))
			isdir = S_ISDIR(st.st_mode);

		xfree(tmp);

		if (!xstrcmp(name, ".")) {
			xfree(namelist[i]);
			continue;
		}

		/* je¶li mamy `..', sprawd¼ czy katalog sk³ada siê z
		 * `../../../' lub czego¶ takiego. */

		if (!xstrcmp(name, "..")) {
			const char *p;
			int omit = 0;

			for (p = dname; p && *p; p++) {
				if (*p != '.' && *p != '/') {
					omit = 1;
					break;
				}
			}

			if (omit) {
				xfree(namelist[i]);
				continue;
			}
		}

		if (!strncmp(name, fname, xstrlen(fname))) {
			name = saprintf("%s%s%s", (dname) ? dname : "", name, (isdir) ? "/" : "");
			array_add_check(&completions, name, 1);
		}

		xfree(namelist[i]);
        }

	/* je¶li w dope³nieniach wyl±dowa³ tylko jeden wpis i jest katalogiem
	 * to wejd¼ do niego i szukaj jeszcze raz */

	if (array_count(completions) == 1 && xstrlen(completions[0]) > 0 && completions[0][xstrlen(completions[0]) - 1] == '/') {
		xfree(dname);
		dname = xstrdup(completions[0]);
		fname = "";
		xfree(namelist);
		namelist = NULL;
		array_free(completions);
		completions = NULL;

		goto again;
	}

	xfree(dname);
	xfree(namelist);
}

/*
 * theme_generator_adding ()
 *
 * function that helps theme_generator to add all of the paths
 *
 * dname - path
 * themes_only - only the .theme extension
 *
 */
static void theme_generator_adding(const char *text, int len, const char *dname, int themes_only)
{
	struct dirent **namelist = NULL;
	int count, i;

	count = scandir((dname) ? dname : ".", &namelist, NULL, alphasort);

	for(i = 0; i < count; i++) {
		struct stat st;
		char *name = namelist[i]->d_name, *tmp = saprintf("%s/%s", (dname) ? dname : "", name), *tmp2;

		if (!stat(tmp, &st)) {
			if (S_ISDIR(st.st_mode) && stat(saprintf("%s%s%s", tmp, "/", name), &st) == -1 && stat(saprintf("%s%s%s.theme", tmp, "/", name), &st) == -1) {
				xfree(namelist[i]);
				xfree(tmp);
				continue;
			}
		}

		xfree(tmp);

		if (!xstrcmp(name, ".") || !xstrcmp(name, "..")) {
			xfree(namelist[i]);
			continue;
		}

		tmp2 = xstrndup(name, xstrlen(name) - xstrlen(xstrstr(name, ".theme")));
		
		if (!xstrncmp(text, name, len) || (!xstrncmp(text, tmp2, len) && !themes_only) )
			array_add_check(&completions, tmp2, 1);
		else	xfree(tmp2);

		xfree(namelist[i]);
	}

	xfree(namelist);
}

static void theme_generator(const char *text, int len)
{

	theme_generator_adding(text, len, DATADIR "/themes", 0);
	theme_generator_adding(text, len, prepare_path("", 0), 1);
	theme_generator_adding(text, len, prepare_path("themes", 0), 0);
}

static void possibilities_generator(const char *text, int len)
{
	int i;
	command_t *c = actual_completed_command;

	if (!c)
		return;

	for (i = 0; c && c->possibilities && c->possibilities[i]; i++)
		if (!xstrncmp(text, c->possibilities[i], len)) {
			array_add_check(&completions, xstrdup(c->possibilities[i]), 1);
		}
}

static void window_generator(const char *text, int len)
{
	window_t *w;
	list_t l;

	for (l = windows; l; l=l->next)	{
		w = (window_t *)l->data;

		if (!w->target || xstrncmp(text, w->target, len))
			continue;

		array_add_check(&completions, xstrdup(w->target), 0);
	}
}

static void sessions_generator(const char *text, int len)
{
        list_t l;

        for (l = sessions; l; l = l->next) {
                session_t *v = l->data;
                if (*text == '-') {
                        if (!xstrncasecmp(text + 1, v->uid, len - 1))
                                array_add_check(&completions, saprintf("-%s", v->uid), 1);
			if (!xstrncasecmp(text + 1, v->alias, len - 1))
                                array_add_check(&completions, saprintf("-%s", v->alias), 1);
                } else {
                        if (!xstrncasecmp(text, v->uid, len))
                                array_add_check(&completions, xstrdup(v->uid), 1);
                        if (!xstrncasecmp(text, v->alias, len))
                                array_add_check(&completions, xstrdup(v->alias), 1);
                }
        }
}

static void metacontacts_generator(const char *text, int len)
{
        list_t l;

        for (l = metacontacts; l; l = l->next) {
		metacontact_t *m = l->data;
		
		if (!xstrncasecmp(text, m->name, len)) 
                	array_add_check(&completions, xstrdup(m->name), 1);
        }
}

static void sessions_var_generator(const char *text, int len)
{
        int i;
        plugin_t *p;

        if (!session_in_line)
                return;

        if (!(p = plugin_find_uid(session_in_line->uid)))
                return;


	for (i = 0; p->params[i]; i++) {
		if(*text == '-') {
                        if (!xstrncasecmp(text + 1, p->params[i]->key, len - 1))
                                array_add_check(&completions, saprintf(("-%s"), p->params[i]->key), 1);
                } else {
                        if (!xstrncasecmp(text, p->params[i]->key, len)) {
                                array_add_check(&completions, xstrdup(p->params[i]->key), 1);
			}
                }
        }
}

static void reason_generator(const char *text, int len)
{
	char *descr = session_current ? session_current->descr : NULL;
	if (descr && !xstrncasecmp(text, descr, len)) {
		/* not to good solution to avoid descr changing by complete */
		array_add_check(&completions, saprintf(("\001%s"), session_current->descr), 1);
	}
}


static struct {
	char ch;
	void (*generate)(const char *text, int len);
} generators[] = {
	{ 'u', known_uin_generator },
	{ 'C', conference_generator },
	{ 'U', unknown_uin_generator },
	{ 'c', command_generator },
	{ 'x', empty_generator },
	{ 'i', ignored_uin_generator },
	{ 'b', blocked_uin_generator },
	{ 'v', variable_generator },
	{ 'p', possibilities_generator },
	{ 'P', plugin_generator },
	{ 'w', window_generator },
	{ 'f', file_generator },
	{ 'e', events_generator },
        { 's', sessions_generator },
	{ 'S', sessions_var_generator },
	{ 'I', ignorelevels_generator },
	{ 'r', reason_generator },
	{ 't', theme_generator },
	{ 'o', dir_generator },
	{ 'm', metacontacts_generator }, 
	{ 0, NULL }
};

/*
 * ncurses_complete()
 *
 * funkcja obs³uguj±ca dope³nianie klawiszem tab.
 * Dzia³anie:
 * - Wprowadzona linia dzielona jest na wyrazy (uwzglêdniaj±c przecinki i znaki cudzyslowia)
 * - nastêpnie znaki separacji znajduj±ce siê miêdzy tymi wyrazami wrzucane s± do tablicy separators
 * - dalej sprawdzane jest za pomoc± zmiennej word_current (okre¶laj±cej aktualny wyraz bez uwzglêdnienia
 *   przecinków - po to, aby wiedzieæ czy w przypadku np funkcji /query ma byæ szukane dope³nienie
 * - zmienna word odpowiada za aktualny wyraz (*z* uwzglêdnieniem przecinków)
 * - words - tablica zawieraj± wszystkie wyrazy
 * - gdy jest to mo¿liwe szukane jest dope³nienie
 * - gdy dope³nieñ jest wiêcej ni¿ jedno (count > 1) wy¶wietlamy tylko "wspóln±" czê¶æ wszystkich dope³nieñ
 *   np ,,que'' w przypadku funkcji /query i /queue
 * - gdy dope³nienie jest tylko jedno wy¶wietlamy owo dope³nienie
 * - przy wy¶wietlaniu dope³nienia ca³a linijka konstruowana jest od nowa, poniewa¿ nie wiadomo w którym miejscu
 *   podany wyraz ma zostañ "wsadzony", st±d konieczna jest tablica separatorów, tablica wszystkich wyrazów itd ...
 * - przeskakiwanie miêdzy dope³nieniami po drugim TABie
 */
void ncurses_complete(int *line_start, int *line_index, char *line)
{
	char *start, **words, *separators;
	char *cmd;
	int i, count, word, j, words_count, word_current, open_quote;
	size_t linelen;

	/* 
	 * sprawdzamy czy mamy kontynuowaæ dope³nianie (przeskakiwaæ miêdzy dope³nianymi wyrazami 
	 * dzia³a to tylko gdy jeste¶my na koñcu linijki, gdy¿ nie ma sensu robiæ takiego przeskakiwania 
 	 * w ¶rodku linii - wtedy sama lista jest wystarczaj±ca 
 	 */
	if (xstrcmp(last_line, line) || last_pos != *line_index || *line_index != xstrlen(line)) {
		continue_complete = 0;
		continue_complete_count = 0;
	}
	
	/*
	 * je¶li uzbierano ju¿ co¶ to próbujemy wy¶wietliæ wszystkie mo¿liwo¶ci
	 */
	if (completions && !continue_complete) {
		int maxlen = 0, cols, rows;
		char *tmp;
		int complcount = array_count(completions);

		for (i = 0; completions[i]; i++) {
			size_t compllen = xstrlen(completions[i]);
			if (compllen + 2 > maxlen)
				maxlen = compllen + 2;
		}

		cols = (window_current->width - 6) / maxlen;
		if (cols == 0)
			cols = 1;

		rows = complcount / cols + 1;

		tmp = xmalloc((cols * maxlen + 2)*sizeof(char));

		for (i = 0; i < rows; i++) {
			int j;

			tmp[0] = 0;
			for (j = 0; j < cols; j++) {
				int cell = j * rows + i;

				if (cell < complcount) {
					int k;

					xstrcat(tmp, completions[cell]);

					for (k = xstrlen(completions[cell]); k < maxlen; k++)
						xstrcat(tmp, (" "));
				}
			}
			if (tmp[0])
				wcs_print("none", tmp);
		}
		
		/* w³±czamy nastêpny etap dope³nienia - przeskakiwanie miêdzy dope³nianymi wyrazami */
		continue_complete = 1;
		continue_complete_count = 0;
		last_line = xstrdup(line);
		last_pos = *line_index;
		xfree(last_line_without_complete);
		last_line_without_complete = xstrdup(line);
		xfree(tmp);

		return;
	}

	/* je¿eli przeskakujemy to trzeba wróciæ z lini± do poprzedniego stanu */
	if (continue_complete) {
		xstrcpy(line, last_line_without_complete);
	}
	linelen = xstrlen(line);

	/* zerujemy co mamy */
	start = xmalloc((linelen + 1)*sizeof(char));
	words = NULL;
	count = 0;

	/* podziel (uwzglêdniaj±c cudzys³owia)*/
	for (i = 0, j = 0, open_quote = 0; i < linelen; i++) {
		if(line[i] == '"') {
			for(j = 0,  i++; i < linelen && line[i] != '"'; i++, j++)
				start[j] = line[i];
			if (i == linelen)
				open_quote = 1;
		} else
			for(j = 0; i < linelen && !xisspace(line[i]) && line[i] != ','; j++, i++)
				start[j] = line[i];
		start[j] = '\0';
		/* "przewijamy" wiêksz± ilo¶æ spacji */
		for(i++; i < linelen && (xisspace(line[i]) || line[i] == ','); i++);
		i--;
		array_add(&words, xstrdup(start));
	}

	/* je¿eli ostatnie znaki to spacja, albo przecinek to trzeba dodaæ jeszcze pusty wyraz do words */
	if (linelen > 1 && (line[linelen - 1] == ' ' || line[linelen - 1] == ',') && !open_quote)
		array_add(&words, xstrdup(("")));

/*	 for(i = 0; i < array_count(words); i++)
		debug("words[i = %d] = \"%s\"\n", i, words[i]);     */

	/* inicjujemy pamiêc dla separators */
	if (words != NULL)
		separators = xmalloc(array_count(words) * sizeof(char) + 1);
	else
		separators = NULL;

	/* sprawd¼, gdzie jeste¶my (uwzgêdniaj±c cudzys³owia) i dodaj separatory*/
	for (word = 0, i = 0; i < linelen; i++, word++) {
		if(line[i] == '"')  {
			for(j = 0, i++; i < linelen && line[i] != '"'; j++, i++)
				start[j] = line[i];
		} else {
			for(j = 0; i < linelen && !xisspace(line[i]) && line[i] != ','; j++, i++)
				start[j] = line[i];
		}
		/* "przewijamy */
		for(i++; i < linelen && (xisspace(line[i]) || line[i] == ','); i++);
		/* ustawiamy znak koñca */
		start[j] = '\0';
		/* je¿eli to koniec linii, to koñczymy t± zabawê */
		if(i >= linelen)
	    		break;
		/* obni¿amy licznik o 1, ¿eby wszystko by³o okey, po "przewijaniu" */
		i--;
		/* hmm, jeste¶my ju¿ na wyrazie wskazywany przez kursor ? */
                if(i >= *line_index)
            		break;
	}

	/* dodajmy separatory - pewne rzeczy podobne do pêtli powy¿ej */
	for (i = 0, j = 0; i < linelen; i++, j++) {
		if(line[i] == '"')  {
			for(i++; i < linelen && line[i] != '"'; i++);
			if(i < linelen) 
				separators[j] = line[i + 1];
		} else {
			for(; i < linelen && !xisspace(line[i]) && line[i] != ','; i++);
			separators[j] = line[i];
		}

		for(i++; i < linelen && (xisspace(line[i]) || line[i] == ','); i++);
		i--;
	}

	if (separators)
		separators[j] = '\0'; // koniec ciagu
	
	/* aktualny wyraz bez uwzgledniania przecinkow */
	for (i = 0, words_count = 0, word_current = 0; i < linelen; i++, words_count++) {
		for(; i < linelen && !xisspace(line[i]); i++)
			if(line[i] == '"') 
				for(i++; i < linelen && line[i] != '"'; i++);
		for(i++; i < linelen && xisspace(line[i]); i++);
		if(i >= linelen) {
			word_current = words_count + 1;
			break;
		}
		i--;
		word_current++;
                /* hmm, jeste¶my ju¿ na wyrazie wskazywany przez kursor ? */
                if(i >= *line_index)
                        break;
	}
	words_count = array_count(words);
	if (linelen) {
		/* trzeba pododawaæ trochê do liczników w spefycicznych (patrz warunki) sytuacjach */
	        if (xisspace(line[linelen - 1]))
        	        word_current++;
		if ((xisspace(line[linelen - 1]) || line[linelen - 1] == ',') && word + 1 == words_count -1 )
			word++;
/*			unused?
		if (xisspace(line[linelen - 1]))
			words_count++;
*/
	}

/*
	debug("word = %d\n", word);
	debug("start = \"%s\"\n", start);
	debug("words_count = %d\n", words_count);
	debug("word_current: %d\n", word_current);
*/
/*
	 for(i = 0; i < xstrlen(separators); i++)
		debug("separators[i = %d] = \"%c\"\n", i, separators[i]);   */

	/* przeskakujemy na nastêpny wyraz - je¿eli konieczne i mo¿liwe */
	if (continue_complete && completions) {
		int cnt = continue_complete_count;

		count = array_count(completions);
		line[0] = '\0';			linelen = 0;
		if (continue_complete_count >= count - 1)
			continue_complete_count = 0;
		else
			continue_complete_count++;
		if (!completions[cnt]) /* nigdy nie powinno siê zdarzyæ, ale na wszelki ... */
			goto cleanup;
			
		for(i = 0; i < words_count; i++) {
			if(i == word) {
				if(xstrchr(completions[cnt],  ('\001'))) {
					if(completions[cnt][0] == '"')
						xstrncat(line, completions[cnt] + 2, xstrlen(completions[cnt]) - 2 - 1 );
					else
						xstrncat(line, completions[cnt] + 1, xstrlen(completions[cnt]) - 1);
				} else
			    		xstrcat(line, completions[cnt]);
				linelen = xstrlen(line);
				*line_index = linelen + 1;
			} else {
				if(xstrchr(words[i], (' '))) {
					char *tmp = 
						saprintf(("\"%s\""), words[i]);
					xstrcat(line, tmp);
					xfree(tmp);
				} else 
					xstrcat(line, words[i]);
			}
			if((i == words_count - 1 && line[xstrlen(line) - 1] != ' ' ))
				xstrcat(line, (" "));
			else if (line[xstrlen(line) - 1] != ' ') 
				xstrncat(line, separators + i, 1);
		}
		/* ustawiamy dane potrzebne do nastêpnego dope³nienia */
		xfree(last_line);
                last_line = xstrdup(line);
                last_pos = *line_index;
		goto cleanup;
	}
	cmd = saprintf(("/%s "), (config_tab_command) ? config_tab_command : "chat");
	/* nietypowe dope³nienie nicków przy rozmowach */
	if (!xstrcmp(line, ("")) || (!xstrncasecmp(line, cmd, xstrlen(cmd)) && word == 2 && send_nicks_count > 0) || (!xstrcasecmp(line, cmd) && send_nicks_count > 0)) {
		if (send_nicks_index >= send_nicks_count)
			send_nicks_index = 0;

		if (send_nicks_count) {
			char *nick = send_nicks[send_nicks_index++];
			snprintf(line, LINE_MAXLEN, (xstrchr(nick, ' ')) ? "%s\"%s\" " : "%s%s ", cmd, nick);
		} else
			snprintf(line, LINE_MAXLEN, "%s", cmd);
		*line_start = 0;
		*line_index = xstrlen(line);

                array_free(completions);
                array_free(words);
		xfree(start);
		xfree(separators);
		xfree(cmd);
		return;
	}
	xfree(cmd);

	/* pocz±tek nicka, komendy? */
	if (word == 0) {
		/* dj's fixes... */
		if (start[0] != '/' && window_current && window_current->target) {
	                known_uin_generator(start, xstrlen(start));
	                if (completions) {
	                        for (j = 0; completions[j]; j++) {
	                                string_t s;
	
	                                if (!xstrchr(completions[j], ('"')) && !xstrchr(completions[j], ('\\')) && !xstrchr(completions[j], (' '))) {
						s = string_init((""));
						string_append(s, completions[j]);
						if (config_completion_char && strlen(config_completion_char))
							string_append_c(s, *config_completion_char);
						else
							string_append_c(s, (':'));
						xfree(completions[j]);
						completions[j] = string_free(s, 0);
	                                        continue;
					}
	                                s = string_init(("\""));
	                                string_append(s, completions[j]);
	                                string_append_c(s, ('\"'));
					string_append_c(s, (':'));
	                                xfree(completions[j]);
	                                completions[j] = string_free(s, 0);
	                        }
	                }

		}
		if (!completions)
			command_generator(start, xstrlen(start));
	} else {
		char **params = NULL;
		int i;
		list_t l;
                char *cmd = (line[0] == '/') ? line + 1 : line;
		int len;

		for (len = 0; cmd[len] && !xisspace(cmd[len]); len++);

		/* first we look for some session complete */
		if (session_current) {
			session_t *session = session_current;
			int plen = (int)(xstrchr(session->uid, ':') - session->uid) + 1;

			for (l = commands; l; l = l->next) {
				command_t *c = l->data;

	                        if (xstrncasecmp(c->name, session->uid, plen))
	                                continue;

                	        if (!xstrncasecmp(c->name + plen, cmd, len)) {
	                                params = c->params;
	                                actual_completed_command = c;
	                                goto exact_match;
	                        }
			}
		}

        	for (l = commands; l; l = l->next) {
	                command_t *c = l->data;

	                if (!xstrncasecmp(c->name, cmd, len)) {
	                        params = c->params;
	                        actual_completed_command = c;
				break;
        	        }
        	}

exact_match: 
		/* for /set maybe we want to complete the file path */
		if (!xstrncmp(cmd, "set", 3) && words[1] && words[2] && word_current == 3) {
			variable_t *v;

			if ((v = variable_find(words[1]))) {
				switch (v->type) {
					case VAR_FILE:
						file_generator(words[word], xstrlen(words[word]));
						break;
					case VAR_THEME:
						theme_generator(words[word], xstrlen(words[word]));
						break;
					case VAR_DIR:
						dir_generator(words[word], xstrlen(words[word]));
						break;
					default:
						break;
				}
			}

		}

		if (word_current > array_count(params) + 1) 
			word_current = array_count(params) + 2;

		if (params && params[word_current - 2]) {
			int j;

			for (i = 0; generators[i].ch; i++) {
				for (j = 0; words[j]; j++) {
					if ((session_in_line = session_find(words[j])))
						break;
				}
				if (!session_in_line)
					session_in_line = session_current;
				for (j = 0; params[word_current - 2][j]; j++) {
					if (generators[i].ch == params[word_current - 2][j]) {
						generators[i].generate(words[word], xstrlen(words[word]));
					}
				}
			}		
		}
	
		if (completions) {
			for (j = 0; completions[j]; j++) {
				string_t s;
	
				if (!xstrchr(completions[j], ('"')) && !xstrchr(completions[j], ('\\')) && !xstrchr(completions[j], (' ')))
					continue;
				s = string_init(("\""));
				string_append(s, completions[j]);				
				string_append_c(s, ('\"'));
				xfree(completions[j]);
				completions[j] = string_free(s, 0);
			}
		}	 
	}
	count = array_count(completions);
	
	/* 
	 * je¶li jest tylko jedna mo¿lwio¶æ na dope³nienie to drukujemy co mamy,
	 * ewentualnie bierzemy czê¶æ wyrazów w cudzys³owia ...
	 * i uwa¿amy oczywi¶cie na \001 (patrz funkcje wy¿ej
	 */
	if (count == 1) {
		line[0] = '\0';
		for(i = 0; i < words_count; i++) {
			if(i == word) {
				if (xstrchr(completions[0],  '\001')) {
					if(completions[0][0] == '"')
						xstrncat(line, completions[0] + 2, xstrlen(completions[0]) - 2 - 1 );
					else
						xstrncat(line, completions[0] + 1, xstrlen(completions[0]) - 1);
				} else
			    		xstrcat(line, completions[0]);
				*line_index = xstrlen(line) + 1;
			} else {
				if (xstrchr(words[i], (' '))) {
					char *tmp = 
						saprintf(("\"%s\""), words[i]);
					xstrcat(line, tmp);
					xfree(tmp);
				} else
					xstrcat(line, words[i]);
			}
			if((i == words_count - 1 && line[xstrlen(line) - 1] != ' ' ))
				xstrcat(line, (" "));
			else if (line[xstrlen(line) - 1] != ' ')
                                xstrncat(line, separators + i, 1);
		}
		array_free(completions);
		completions = NULL;
	} else 

	/*
	 * gdy jest wiêcej mo¿liwo¶ci to robimy podobnie jak wy¿ej tyle, ¿e czasem
	 * trzeba u¿yæ cudzys³owia tylko z jednej storny, no i trzeba dope³niæ do pewnego miejsca
	 * w sumie proste rzeczy, ale jak widaæ jest trochê opcji ...
	 */
	if (count > 1) {
		int common = 0;
		int tmp = 0;
		int quotes = 0;
		char *s1  = completions[0];

                if (*s1 =='"') {
	                s1++;
			quotes = 1;
		}

	    	/* for(i = 0; completions[i]; i++)
                	debug("completions[i] = %s\n", completions[i]); */
		/*
		 * mo¿e nie za ³adne programowanie, ale skuteczne i w sumie jedyne w 100% spe³niaj±ce	
	 	 * wymagania dope³niania (uwzglêdnianie cudzyws³owiów itp...)
		 */
		for (i=1; ; i++, common++) {
			for (j=1; j < count; j++) {
				char *s2 = completions[j];

				if (*s2 == '"') {
					quotes = 1;
					s2++;
				}
				tmp = xstrncasecmp(s1, s2, i);
				/* debug("xstrncasecmp(\"%s\", \"%s\", %d) = %d\n", s1, s2, i, xstrncasecmp(s1, s2, i)); */
				if (tmp)
					break;
                        }
			if (tmp)
				break;
		}
	
		/* debug("common :%d\t\n", common); */

		if (xstrlen(line) + common < LINE_MAXLEN) {
			line[0] = '\0';
			for(i = 0; i < words_count; i++) {
				if (i == word) {
					if (quotes == 1 && completions[0][0] != '"') 
						xstrcat(line, ("\""));

					if (completions[0][0] == '"')
						common++;
						
					if (completions[0][common - 1] == '"')
						common--;

					xstrncat(line, completions[0], common);
					*line_index = xstrlen(line);
				} else {
					if (xstrchr(words[i], (' '))) {
						char *tmp = 
							saprintf(("\"%s\""), words[i]);
						xstrcat(line, tmp);
						xfree(tmp);
					} else
						xstrcat(line, words[i]);
				}

				if(separators[i]) 
					xstrncat(line, separators + i, 1);
			}
		}
	}

cleanup:
	array_free(words);
	xfree(start);
	xfree(separators);
	return;
}

void ncurses_complete_clear()
{
	array_free(completions);
	completions = NULL;
        continue_complete = 0;
        continue_complete_count = 0;
	xfree(last_line);
	last_line = NULL;
        xfree(last_line_without_complete);
	last_line_without_complete = NULL;
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
