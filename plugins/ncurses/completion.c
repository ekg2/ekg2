/* $Id$ */

#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
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
	
        for(i=0; i < strlen(text); i++)
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
		char *without_sess_id = strchr(c->name, ':');

		if (!strncasecmp(text, c->name, len) && !array_item_contains(completions, c->name, 1))
			array_add(&completions, saprintf("%s%s%s", slash, dash, c->name));
		else if (without_sess_id && !array_item_contains(completions, without_sess_id + 1, 1) && !strncasecmp(text, without_sess_id + 1, len)) 
			array_add(&completions, saprintf("%s%s%s", slash, dash, without_sess_id + 1));
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

static void sessions_generator(const char *text, int len)
{
        list_t l;

        for (l = sessions; l; l = l->next) {
                session_t *v = l->data;
                if (*text == '-') {
                        if (!strncasecmp(text + 1, v->uid, len - 1))
                                array_add_check(&completions, saprintf("-%s", v->uid), 1);
                } else {
                        if (!strncasecmp(text, v->uid, len))
                                array_add_check(&completions, xstrdup(v->uid), 1);
                }
        }
}

static void sessions_var_generator(const char *text, int len)
{
        list_t l;
	int i;
        const char *words[] = { "uid", "password", "auto_connect", "alias", "server", NULL };

        for (i = 0; words[i]; i++) {
		if(*text == '-') {
	        	if (!strncasecmp(text + 1, words[i], len - 1))
        	        	array_add_check(&completions, saprintf("-%s", words[i]), 1);
		} else {
			if (!strncasecmp(text, words[i], len))
                                array_add_check(&completions, xstrdup(words[i]), 1);
		}
	}
 
	for (l = sessions; l; l = l->next) {
                session_t *v = l->data;

		for(i = 0; v->params && v->params[i]; i++) {
                	if (*text == '-') {
                        	if (!strncasecmp(text + 1, v->params[i]->key, len - 1)) 
                                	array_add_check(&completions, saprintf("-%s", v->params[i]->key), 1);
	                } else {
        	                if (!strncasecmp(text, v->params[i]->key, len))
                	                array_add_check(&completions, xstrdup(v->params[i]->key), 1);
 	               }
		}
        }
}

static void sessions_all_generator(const char *text, int len)
{
	sessions_var_generator(text, len);
	sessions_generator(text, len);
}

static struct {
	char ch;
	void (*generate)(const char *text, int len);
} generators[] = {
	{ 'u', known_uin_generator },
	{ 'U', unknown_uin_generator },
	{ 'c', command_generator },
	{ 'x', empty_generator },
	{ 'i', ignored_uin_generator },
	{ 'b', blocked_uin_generator },
	{ 'v', variable_generator },
	{ 'd', dcc_generator },
	{ 'p', python_generator },
	{ 'w', window_generator },
	{ 'f', file_generator },
	{ 'e', events_generator },
        { 's', sessions_generator },
	{ 'S', sessions_var_generator },
        { 'A', sessions_all_generator },
	{ 'I', ignorelevels_generator },
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
 */
void ncurses_complete(int *line_start, int *line_index, char *line)
{
	char *start, *cmd, **words, *separators;
	int i, count, word, j, words_count, word_current;

	start = xmalloc(strlen(line) + 1);
	count = 0;
	/* 
	 * je¶li uzbierano ju¿ co¶ to próbujemy wy¶wietliæ wszystkie mo¿liwo¶ci 
	 */
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

	/* zerujemy co mamy */
	words = NULL;

	/* podziel (uwzglêdniaj±c cudzys³owia)*/
	for (i = 0; i < strlen(line); i++) {
		if(line[i] == '"')
			for(j = 0,  i++; i < strlen(line) && line[i] != '"'; i++, j++)
				start[j] = line[i];
		else
			for(j = 0; i < strlen(line) && !xisspace(line[i]) && line[i] != ','; j++, i++)
				start[j] = line[i];
		start[j] = '\0';
		/* "przewijamy" wiêksz± ilo¶æ spacji */
		for(i++; i < strlen(line) && (xisspace(line[i]) || line[i] == ','); i++);
		i--;
		array_add(&words, saprintf("%s", start));
	}
	
	/* je¿eli ostatnie znaki to spacja, albo przecinek to trzeba dodaæ jeszcze pusty wyraz do words */
	if (strlen(line) > 1 && (line[strlen(line) - 1] == ' ' || line[strlen(line) - 1] == ','))
		array_add(&words, xstrdup(""));

/*	 for(i = 0; i < array_count(words); i++)
		debug("words[i = %d] = \"%s\"\n", i, words[i]);     */

	/* inicjujemy pamiêc dla separators */
	if (words != NULL)
		separators = xmalloc(array_count(words));
	else
		separators = NULL;

	/* sprawd¼, gdzie jeste¶my (uwzgêdniaj±c cudzys³owia) i dodaj separatory*/
	for (word = 0, i = 0; i < strlen(line); i++, word++) {
		if(line[i] == '"')  {
			for(j = 0, i++; i < strlen(line) && line[i] != '"'; j++, i++)
				start[j] = line[i];
		} else {
			for(j = 0; i < strlen(line) && !xisspace(line[i]) && line[i] != ','; j++, i++)
				start[j] = line[i];
		}
		/* "przewijamy */
		for(i++; i < strlen(line) && (xisspace(line[i]) || line[i] == ','); i++);
		/* ustawiamy znak koñca */
		start[j] = '\0';
		/* je¿eli to koniec linii, to koñczymy t± zabawê */
		if(i >= strlen(line))
	    		break;
		/* obni¿amy licznik o 1, ¿eby wszystko by³o okey, po "przewijaniu" */
		i--;
		/* hmm, jeste¶my ju¿ na wyrazie wskazywany przez kursor ? */
                if(i >= *line_index)
            		break;
	}

	/* dodajmy separatory - pewne rzeczy podobne do pêtli powy¿ej */
	for (i = 0, j = 0; i < strlen(line); i++, j++) {
		if(line[i] == '"')  {
			for(i++; i < strlen(line) && line[i] != '"'; i++);
			if(i < strlen(line)) 
				separators[j] = line[i + 1];
		} else {
			for(; i < strlen(line) && !xisspace(line[i]) && line[i] != ','; i++);
			separators[j] = line[i];
		}

		for(i++; i < strlen(line) && (xisspace(line[i]) || line[i] == ','); i++);
		i--;
	}

	if (separators)
		separators[j] = '\0'; // koniec ciagu
	
	/* aktualny wyraz bez uwzgledniania przecinkow */
	for (i = 0, words_count = 0, word_current = 0; i < strlen(line); i++, words_count++) {
		for(; i < strlen(line) && !xisspace(line[i]); i++)
			if(line[i] == '"') 
				for(i++; i < strlen(line) && line[i] != '"'; i++);
		for(i++; i < strlen(line) && xisspace(line[i]); i++);
		if(i >= strlen(line))
			word_current = words_count + 1;
		i--;
                /* hmm, jeste¶my ju¿ na wyrazie wskazywany przez kursor ? */
                if(i >= *line_index)
                        break;
	}

	/* trzeba pododawaæ trochê do liczników w spefycicznych (patrz warunki) sytuacjach */
	if((xisspace(line[strlen(line) - 1]) || line[strlen(line) - 1] == ',') && word + 1== array_count(words) -1 )
		word++;
	if(xisspace(line[strlen(line) - 1]) && words_count == word_current)
		word_current++;
	if(xisspace(line[strlen(line) - 1]))
		words_count++;

/*	debug("word = %d\n", word);
	debug("start = \"%s\"\n", start);
	debug("words_count = %d\n", words_count);	 

	 for(i = 0; i < strlen(separators); i++)
		debug("separators[i = %d] = \"%c\"\n", i, separators[i]);   */

	cmd = saprintf("/%s ", (config_tab_command) ? config_tab_command : "chat");

	/* nietypowe dope³nienie nicków przy rozmowach */
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

                array_free(completions);
                array_free(words);
		xfree(start);
		xfree(separators);
		xfree(cmd);
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

		for (l = commands; l; l = l->next) {
			command_t *c = l->data;
			char *name = (strchr(c->name, ':')) ? strchr(c->name, ':') + 1 : c->name;
			int len = strlen(name);
			char *cmd = (line[0] == '/') ? line + 1 : line;
			
			if (!strncasecmp(name, cmd, len) && xisspace(cmd[len])) {
				params = c->params;
				abbrs = 1;
				break;
			}

			for (len = 0; cmd[len] && cmd[len] != ' '; len++);

			if (!strncasecmp(name, cmd, len)) {
				params = c->params;
				abbrs++;
			} else
				if (params && abbrs == 1)
					break;
		}
		
		if (params && abbrs == 1) {
			for (i = 0; generators[i].ch; i++) {
				if (generators[i].ch == params[word_current - 2]) {
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
	}

	count = array_count(completions);
	
	/* 
	 * je¶li jest tylko jedna mo¿lwio¶æ na dope³nienie to drukujemy co mamy,
	 * ewentualnie bierzemy czê¶æ wyrazów w cudzys³owia ...
	 * i uwa¿amy oczywi¶cie na \001 (patrz funkcje wy¿ej
	 */
	if (count == 1) {
		line[0] = '\0';		
		for(i = 0; i < array_count(words); i++) {
			if(i == word) {
				if(strchr(completions[0],  '\001')) {
					if(completions[0][0] == '"')
						strncat(line, completions[0] + 2, strlen(completions[0]) - 2 - 1 );
					else
						strncat(line, completions[0] + 1, strlen(completions[0]) - 1);
				} else
			    		strcat(line, completions[0]);
				*line_index = strlen(line) + 1;
			} else {
				if(strchr(words[i], ' '))
					strcat(line, saprintf("\"%s\"", words[i]));
				else
					strcat(line, words[i]);
			}
			if((i == array_count(words) - 1 && line[strlen(line) - 1] != ' ' ))
				strcat(line, " ");
			else if (line[strlen(line) - 1] != ' ')
                                strcat(line, saprintf("%c", separators[i]));
		}
		array_free(completions);
		completions = NULL;
	}

	/*
	 * gdy jest wiêcej mo¿liwo¶ci to robimy podobnie jak wy¿ej tyle, ¿e czasem
	 * trzeba u¿yæ cudzys³owia tylko z jednej storny, no i trzeba dope³niæ do pewnego miejsca
	 * w sumie proste rzeczy, ale jak widaæ jest trochê opcji ...
	 */
	if (count > 1) {
		int common = 0;
		int tmp = 0;
		int quotes = 0;

	    	/* for(i = 0; completions[i]; i++)
                	debug("completions[i] = %s\n", completions[i]); */
		/*
		 * mo¿e nie za ³adne programowanie, ale skuteczne i w sumie jedyne w 100% spe³niaj±ce	
	 	 * wymagania dope³niania (uwzglêdnianie cudzyws³owiów itp...)
		 */
		for(i=1, j = 0; ; i++, common++) {
			for(j=0; j < count; j++) {
//				if (!completions[j][i])
//					break;
				if(completions[j][0] == '"') 
					quotes = 1;
				if(completions[j][0] == '"' && completions[0][0] != '"')
					tmp = strncasecmp(completions[0], completions[j] + 1, i);
				else if(completions[0][0] == '"' && completions[j][0] != '"')
					tmp = strncasecmp(completions[0] + 1, completions[j], i);
				else
					tmp = strncasecmp(completions[0], completions[j], i);
				 /* debug("strncasecmp(\"%s\", \"%s\", %d) = %d\n", completions[0], completions[j], i, strncasecmp(completions[0], completions[j], i));  */
				if (tmp)
					break;
                        }
			if (tmp)
				break;
		}
		
		/* debug("common :%d\n", common); */

		if (strlen(line) + common < LINE_MAXLEN) {
		
			line[0] = '\0';
			for(i = 0; i < array_count(words); i++) {
				if(i == word) {
					if(quotes == 1 && completions[0][0] != '"') 
						strcat(line, "\"");

					if(completions[0][0] == '"')
						common++;
						
					if(completions[0][common - 1] == '"')
						common--;

					strncat(line, str_tolower(completions[0]), common);
					*line_index = strlen(line);
				} else {
					if(strrchr(words[i], ' '))
						strcat(line, saprintf("\"%s\"", words[i]));
					else
						strcat(line, words[i]);
				}

				if(separators[i]) {
					strcat(line, saprintf("%c", separators[i]));
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

