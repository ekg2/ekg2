/* $Id$ */

#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <dirent.h>
#ifndef HAVE_SCANDIR
#  include "compat/scandir.h"
#endif

#include <ekg/dynstuff.h>
#include <ekg/xmalloc.h>
#include <ekg/commands.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>

#include "old.h"

/* nadpisujemy funkcjê strncasecmp() odpowiednikiem z obs³ug± polskich znaków */
#define strncasecmp(x...) strncasecmp_pl(x)


static char **completions = NULL;	/* lista dope³nieñ */

/* 
 * zamienia podany znak na ma³y je¶li to mo¿liwe 
 * obs³uguje polskie znaki
 */
static int tolower_pl(const unsigned char c) {
	switch(c) {
		case 161: // ¡
			return 177; 
		case 198: // Æ
			return 230;
		case 202: // Ê
			return 234;
		case 163: // £
			return 179;
		case 209: // Ñ
			return 241;
		case 211: // Ó
			return 243;
		case 166: // ¦
			return 182;
		case 175: // ¯
			return 191;
		case 172: // ¬
			return 188;
		default: //reszta
			return tolower(c);
	}
}

/* 
 * porównuje dwa ci±gi o okre¶lonej przez n d³ugo¶ci
 * dzia³a analogicznie do strncasecmp()
 * obs³uguje polskie znaki
 */

int strncasecmp_pl(const char * cs,const char * ct,size_t count)
{
        register signed char __res = 0;

        while (count) {
                if ((__res = tolower_pl(*cs) - tolower_pl(*ct++)) != 0 || !*cs++)
                        break;
                count--;
        }

        return __res;
}


/* 
 * zamienia wszystkie znaki ci±gu na ma³e
 * zwraca ci±g po zmianach
 */
static char *str_tolower(const char *text) {
	int i;
	char *tmp;

	tmp = xmalloc(strlen(text) + 1);
	
        for(i=0; i <= strlen(text); i++)
		tmp[i] = tolower_pl(text[i]);
	tmp[i] = '\0';
	return tmp;
}

static void dcc_generator(const char *text, int len)
{
	const char *words[] = { "close", "get", "send", "list", "rsend", "rvoice", "voice", NULL };
	int i;

	for (i = 0; words[i]; i++)
		if (!strncasecmp(text, words[i], len))
			array_add(&completions, xstrdup(words[i]));
}

static void command_generator(const char *text, int len)
{
	const char *slash = "", *dash = "";
	list_t l;

	if (*text == '/') {
		slash = "/";
		text++;
		len--;
	}

	if (*text == '^') {
		dash = "^";
		text++;
		len--;
	}

	if (window_current->target)
		slash = "/";
			
	for (l = commands; l; l = l->next) {
		command_t *c = l->data;

		if (!strncasecmp(text, c->name, len))
			array_add(&completions, saprintf("%s%s%s", slash, dash, c->name));
	}
}

static void events_generator(const char *text, int len)
{
#if 0
	int i;
	const char *tmp = NULL;
	char *pre = NULL;

	if ((tmp = strrchr(text, '|')) || (tmp = strrchr(text, ','))) {
		char *foo;

		pre = xstrdup(text);
		foo = strrchr(pre, *tmp);
		*(foo + 1) = 0;

		len -= tmp - text + 1;
		tmp = tmp + 1;
	} else
		tmp = text;

	for (i = 0; event_labels[i].name; i++)
		if (!strncasecmp(tmp, event_labels[i].name, len))
			array_add(&completions, ((tmp == text) ? xstrdup(event_labels[i].name) : saprintf("%s%s", pre, event_labels[i].name)));
#endif
}

static void ignorelevels_generator(const char *text, int len)
{
	int i;
	const char *tmp = NULL;
	char *pre = NULL;

	if ((tmp = strrchr(text, '|')) || (tmp = strrchr(text, ','))) {
		char *foo;

		pre = xstrdup(text);
		foo = strrchr(pre, *tmp);
		*(foo + 1) = 0;

		len -= tmp - text + 1;
		tmp = tmp + 1;
	} else
		tmp = text;

	for (i = 0; ignore_labels[i].name; i++)
		if (!strncasecmp(tmp, ignore_labels[i].name, len))
			array_add(&completions, ((tmp == text) ? xstrdup(ignore_labels[i].name) : saprintf("%s%s", pre, ignore_labels[i].name)));
}

static void unknown_uin_generator(const char *text, int len)
{
	int i;

	for (i = 0; i < send_nicks_count; i++) {
		if (send_nicks[i] && xisdigit(send_nicks[i][0]) && !strncasecmp(text, send_nicks[i], len))
			if (!array_contains(completions, send_nicks[i], 0))
				array_add(&completions, xstrdup(send_nicks[i]));
	}
}

static void known_uin_generator(const char *text, int len)
{
	list_t l, sl;
	int done = 0;

	for (sl = sessions; sl; sl = sl->next) {	
		session_t *s = sl->data;
		for (l = s->userlist; l; l = l->next) {
			userlist_t *u = l->data;

			if (u->nickname && !strncasecmp(text, u->nickname, len)) {
				array_add(&completions, xstrdup(u->nickname));
				done = 1;
			}
		}

		for (l = s->userlist; l; l = l->next) {
			userlist_t *u = l->data;

			if (!done && !strncasecmp(text, u->uid, len))
				array_add(&completions, xstrdup(u->uid));
		}
	}
	for (l = conferences; l; l = l->next) {
		struct conference *c = l->data;

		if (!strncasecmp(text, c->name, len))
			array_add(&completions, xstrdup(c->name));
	}

	unknown_uin_generator(text, len);
}

static void variable_generator(const char *text, int len)
{
	list_t l;

	for (l = variables; l; l = l->next) {
		variable_t *v = l->data;

		if (v->type == VAR_FOREIGN)
			continue;

		if (*text == '-') {
			if (!strncasecmp(text + 1, v->name, len - 1))
				array_add(&completions, saprintf("-%s", v->name));
		} else {
			if (!strncasecmp(text, v->name, len))
				array_add(&completions, xstrdup(v->name));
		}
	}
}

static void ignored_uin_generator(const char *text, int len)
{
	list_t l, sl;

	for (sl = sessions; sl; sl = sl -> next) {
		session_t *s = sl->data;
		for (l = s->userlist; l; l = l->next) {
			userlist_t *u = l->data;

			if (!ignored_check(s, u->uid))
				continue;

			if (!u->nickname) {
				if (!strncasecmp(text, u->uid, len))
					array_add(&completions, xstrdup(u->uid));
			} else {
				if (u->nickname && !strncasecmp(text, u->nickname, len))
					array_add(&completions, xstrdup(u->nickname));
			}
		}
	}
}

static void blocked_uin_generator(const char *text, int len)
{
	list_t l, sl;

        for (sl = sessions; sl; sl = sl -> next) {
                session_t *s = sl->data;

		for (l = s->userlist; l; l = l->next) {
			userlist_t *u = l->data;

		if (!group_member(u, "__blocked"))
			continue;

		if (!u->nickname) {
			if (!strncasecmp(text, u->uid, len))
				array_add(&completions, xstrdup(u->uid));
		} else {
			if (u->nickname && !strncasecmp(text, u->nickname, len))
				array_add(&completions, xstrdup(u->nickname));
			}
		}
	}
}

static void empty_generator(const char *text, int len)
{

}

void file_generator(const char *text, int len)
{
	struct dirent **namelist = NULL;
	char *dname, *tmp;
	const char *fname;
	int count, i;

	/* `dname' zawiera nazwê katalogu z koñcz±cym znakiem `/', albo
	 * NULL, je¶li w dope³nianym tek¶cie nie ma ¶cie¿ki. */

	dname = xstrdup(text);

	if ((tmp = strrchr(dname, '/'))) {
		tmp++;
		*tmp = 0;
	} else
		dname = NULL;

	/* `fname' zawiera nazwê szukanego pliku */

	fname = strrchr(text, '/');

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

		if (!strcmp(name, ".")) {
			xfree(namelist[i]);
			continue;
		}

		/* je¶li mamy `..', sprawd¼ czy katalog sk³ada siê z
		 * `../../../' lub czego¶ takiego. */
		
		if (!strcmp(name, "..")) {
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
		
		if (!strncmp(name, fname, strlen(fname))) {
			name = saprintf("%s%s%s", (dname) ? dname : "", name, (isdir) ? "/" : "");
			array_add(&completions, name);
		}

		xfree(namelist[i]);
        }

	/* je¶li w dope³nieniach wyl±dowa³ tylko jeden wpis i jest katalogiem
	 * to wejd¼ do niego i szukaj jeszcze raz */

	if (array_count(completions) == 1 && strlen(completions[0]) > 0 && completions[0][strlen(completions[0]) - 1] == '/') {
		xfree(dname);
		dname = xstrdup(completions[0]);
		fname = "";
		array_free(completions);
		completions = NULL;

		goto again;
	}

	xfree(dname);
	xfree(namelist);
}

static void python_generator(const char *text, int len)
{
	const char *words[] = { "load", "unload", "run", "exec", "list", NULL };
	int i;

	for (i = 0; words[i]; i++)
		if (!strncasecmp(text, words[i], len))
			array_add(&completions, xstrdup(words[i]));
}

static void window_generator(const char *text, int len)
{
	const char *words[] = { "new", "kill", "move", "next", "resize", "prev", "switch", "clear", "refresh", "list", "active", "last", NULL };
	int i;

	for (i = 0; words[i]; i++)
		if (!strncasecmp(text, words[i], len))
			array_add(&completions, xstrdup(words[i]));
}

static struct {
	char ch;
	void (*generate)(const char *text, int len);
} generators[] = {
	{ 'u', known_uin_generator },
	{ 'U', unknown_uin_generator },
	{ 'c', command_generator },
	{ 's', empty_generator },
	{ 'i', ignored_uin_generator },
	{ 'b', blocked_uin_generator },
	{ 'v', variable_generator },
	{ 'd', dcc_generator },
	{ 'p', python_generator },
	{ 'w', window_generator },
	{ 'f', file_generator },
	{ 'e', events_generator },
	{ 'I', ignorelevels_generator },
	{ 0, NULL }
};

/*
 * ncurses_complete()
 *
 * funkcja obs³uguj±ca dope³nianie klawiszem tab.
 */
void ncurses_complete(int *line_start, int *line_index, char *line)
{
	char *start, *cmd, **words, *separators;
	int i, count, word, j;

	/* je¶li linia jest pusta: return */
	if (!strcmp(line, "")) 
		return;
	    
	start = xmalloc(strlen(line) + 1);
	
	/* je¶li uzbierano ju¿ co¶ */
	if (completions) {
		int maxlen = 0, cols, rows;
		char *tmp;

		for (i = 0; completions[i]; i++)
			if (strlen(completions[i]) + 2 > maxlen)
				maxlen = strlen(completions[i]) + 2;

		cols = (window_current->width - 6) / maxlen;
		if (cols == 0)
			cols = 1;

		rows = array_count(completions) / cols + 1;

		tmp = xmalloc(cols * maxlen + 2);

		for (i = 0; i < rows; i++) {
			int j;

			strcpy(tmp, "");

			for (j = 0; j < cols; j++) {
				int cell = j * rows + i;

				if (cell < array_count(completions)) {
					int k;

					strcat(tmp, completions[cell]); 

					for (k = 0; k < maxlen - strlen(completions[cell]); k++)
						strcat(tmp, " ");
				}
			}

			if (strcmp(tmp, "")) {
				print("none", tmp);
			}
		}

		xfree(tmp);
		xfree(start);
		return;
	}

	/* podziel */
	words = array_make(line, " ,\t", 0, 1, 1);

	if (strlen(line) > 1 && line[strlen(line) - 1] == ' ')
		array_add(&words, xstrdup(""));
		

	separators = xmalloc(array_count(words));
		
	/* sprawd¼, gdzie jeste¶my */
	for (word = 0, i = 0; i < strlen(line); i++, word++) {
		for(j = 0; i < strlen(line) && !xisspace(line[i]) && line[i] != ','; j++, i++) 
				start[j] = line[i];
		for(i++; i < strlen(line) && (xisspace(line[i]) || line[i] == ','); i++);
		start[j] = '\0';
		if(i >= strlen(line))
	    		break;
		i--;
                if(i >= *line_index)
            		break;

	}

	/* dodajemy separatory */
	for(i = 0, j = 0; i < strlen(line); i++, j++)  {
		for(; i < strlen(line) && !xisspace(line[i]) && line[i] != ','; i++) ;
			separators[j] = line[i];
		for(i++; i < strlen(line) && (xisspace(line[i]) || line[i] == ','); i++);
		i--;
	}

/*	debug("word = %d\n", word);
	debug("start = \"%s\"\n", start);   */
	
	/* nietypowe dope³nienie nicków przy rozmowach */
	cmd = saprintf("/%s ", (config_tab_command) ? config_tab_command : "chat");

	if (!strcmp(line, "") || (!strncasecmp(line, cmd, strlen(cmd)) && word == 2 && send_nicks_count > 0) || (!strcasecmp(line, cmd) && send_nicks_count > 0)) {
		if (send_nicks_index >= send_nicks_count)
			send_nicks_index = 0;

		if (send_nicks_count) {
			char *nick = send_nicks[send_nicks_index++];

			snprintf(line, LINE_MAXLEN, (strchr(nick, ' ')) ? "%s\"%s\" " : "%s%s ", cmd, nick);
		} else
			snprintf(line, LINE_MAXLEN, "%s", cmd);
		*line_start = 0;
		*line_index = strlen(line);

		xfree(cmd);
		array_free(completions);
		array_free(words);
		completions = NULL;
		xfree(start);
		xfree(separators);
		return;
	}

	xfree(cmd);

	/* pocz±tek komendy? */
	if (word == 0)
		command_generator(start, strlen(start));
	else {
		char *params = NULL;
		int abbrs = 0, i;
		list_t l;
		char **blocks;

		for (l = commands; l; l = l->next) {
			command_t *c = l->data;
			int len = strlen(c->name);
			char *cmd = (line[0] == '/') ? line + 1 : line;

			if (!strncasecmp(cmd, c->name, len) && xisspace(cmd[len])) {
				params = c->params;
				abbrs = 1;
				break;
			}

			for (len = 0; cmd[len] && cmd[len] != ' '; len++);

			if (!strncasecmp(cmd, c->name, len)) {
				params = c->params;
				abbrs++;
			} else
				if (params && abbrs == 1)
					break;
		}
		

		blocks = array_make(line, " \t", 0, 1, 1);

		if (strlen(line) > 1 && line[strlen(line) - 1] == ' ')
			array_add(&blocks, xstrdup(""));

		if ((params && abbrs == 1 && word < strlen(params) + 1 )|| (strchr(params, 'u') && array_count(blocks) == strlen(strchr(params, 'u')))) {
			
			for (i = 0; generators[i].ch; i++) {
			
				if (generators[i].ch == params[word - 1] || strchr(params, 'u')) {
					int j;

					
					generators[i].generate(words[word], strlen(words[word]));

					for (j = 0; completions && completions[j]; j++) {
						string_t s;
						const char *p;

						if (!strchr(completions[j], '"') && !strchr(completions[j], '\\') && !strchr(completions[j], ' '))
							continue;
						
						s = string_init("\"");

						for (p = completions[j]; *p; p++) {
							if (!strchr("\"\\", *p))
								string_append_c(s, *p);
							else {
								string_append_c(s, '\\');
								string_append_c(s, *p);
							}
						}
						string_append_c(s, '\"');

						xfree(completions[j]);
						completions[j] = string_free(s, 0);
					}
					break;
				}
			} 
		}
		
		array_free(blocks);
	}

	count = array_count(completions);

	/* je¶li jest tylko jedno dope³enienie */
	if (count == 1) {

		line[0] = '\0';		
		for(i = 0; i < array_count(words); i++) {
			if(i == word) {
				strcat(line, completions[0]);
				*line_index = strlen(line) + 1;
			}
			else
				strcat(line, words[i]);
			if(i == array_count(words) - 1 && line[strlen(line) - 1] != ' ')
				strcat(line, " ");
			else if (line[strlen(line) - 1] != ' ') {
				char tmp[2];
				tmp[0] = separators[i];
				tmp[1] = '\0';
				strcat(line, tmp);
			}
		}
		array_free(completions);
		completions = NULL;
	}

	/* je¶li jest ich wiêcej */
	if (count > 1) {
		int common = 0;

		for(i=1, j = 0;i < 10; i++, common++) { 
			for(j=1; j < count; j++) {
				if(strncasecmp(completions[0], completions[j], i) < 0)
					break;
			}
			if(j < count && strncasecmp(completions[0], completions[j], i) < 0)
				break;

		}
		
		if (strlen(line) + common < LINE_MAXLEN) {
		
			line[0] = '\0';		
			for(i = 0; i < array_count(words); i++) {
				if(i == word) {
					strncat(line, str_tolower(completions[0]), common);
					strcat(line, "");
					*line_index = strlen(line);
				}
				else
					strcat(line, words[i]);\
				if(i != array_count(words) - 1) {
					char tmp[2];
					tmp[0] = separators[i];
					tmp[1] = '\0';
					strcat(line, tmp);
				}
			}
		}
	}

	array_free(words);
	xfree(start);
	xfree(separators);
	return;
}

void ncurses_complete_clear()
{
	array_free(completions);
	completions = NULL;
}

