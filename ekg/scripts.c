#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include <ekg/stuff.h>
#include <ekg/windows.h>
#include <ekg/scripts.h>
#include <ekg/xmalloc.h>

/* TODO && BUGS 
 * - cleanup kodu!
 * - multiple handler for commands && var_changed.
 * - usuwanie listy list_remove()
 * - wszystkie memleaki won!
 */

COMMAND(script_command_handlers);
int script_timer_handlers(int type, void *d);
void script_var_changed(const char *var);
int script_query_handlers(void *data, va_list ap);

/********************************************************************************** scriptlang */

scriptlang_t *scriptlang_from_ext(char *ext)
{
	list_t l;
	scriptlang_t *s;
	
	if (!ext) return NULL;
	
	for (l = scriptlang; l; l = l->next) {
		s = l->data;
//		debug("[script_ext] %s %s\n", ext, s->ext);
		if (!xstrcmp(ext, s->ext))
			return s;
	}
	return NULL;
}

static int script_register_compare(void *data1, void *data2) 
{
	scriptlang_t *scr1 = data1;
	scriptlang_t *scr2 = data2;
	return scr1->prio - scr2->prio;
}

int scriptlang_register(scriptlang_t *s, int prio)
{
	s->prio = prio;
	list_add_sorted(&scriptlang, s, 0, script_register_compare);

	s->init();

	return 0;
}

int scriptlang_unregister(scriptlang_t *s)
{
	script_unload_lang(s);
	s->deinit();
	
	list_remove(&scriptlang, s, 0);
	
	return 0;
}

/**************************************************************************************/

int script_list(scriptlang_t *s)
{
	list_t l;
	script_t *scr;
	scriptlang_t *lang;
	int i = 0;
	
	for (l = scripts; l; l = l->next) {
		scr = l->data;
		lang = scr->lang;
		if (!s || scr->lang == s) {
			print("script_list", scr->name, scr->path, lang->name);
			i++;
		}
	
	}
	if (!i)
		print("script_list_empty");
	return i;
}

int script_var_list(script_t *scr)
{
	list_t l;
	int i = 0;
        for (l = script_vars; l; l = l->next) {
		script_var_t *v = l->data;
                if (!scr || v->scr == scr) {
			print("script_varlist", v->name, v->value, v->private);
			i++;
		}
        }
	if (!i)
		print("script_varlist_empty");

        return i;
}

/*****************    SCRIPTS MANGMENT *********************************************/

char *script_find_path(char *name) {
	FILE *fajl;
	char *ext;
	char *nametmp;
	char *path = NULL;
	
	list_t l = scriptlang;
	scriptlang_t *s = NULL;

	if (xstrlen(name) < 1) {
		print("script_need_name");
		return NULL;
	}

	if (name[0] == '/') {
		if ((fajl = (fopen(name, "r")))) {
			fclose(fajl);
			return xstrdup(name);
		}
	}
	nametmp = xstrdup(name);

	while ((ext = xrindex(nametmp, '.')) || l) {
		if (ext) {
			path = saprintf("%s/%s",prepare_path("scripts", 0), nametmp ) ;
			fajl = fopen(path, "r");
		
			if (!fajl) {
				xfree(path);
				path = saprintf("%s/%s", DATADIR, nametmp);
				fajl = fopen(path, "r");
			}

// etc..

			xfree(nametmp);
			if (!fajl) {
				xfree(path);
				path = NULL;
			}
			else {
				fclose(fajl);
				return path;
			}
		}
		if (!l) return NULL;
		s = l->data;
		nametmp  = saprintf("%s%s", name, s->ext);
		l = l->next;
	}
	return NULL;
}

int script_unload(script_t *scr)
{
	list_t l, prev;
	
	script_command_t *c;
	script_timer_t   *t;
	script_var_t     *v;
	script_query_t   *q;
	
	scriptlang_t *slang = scr->lang;
	
/* przeszukac liste timerow i komand, jak cos to je wywalic */

	for (prev = l = script_timers; l; l = l->next, prev = l) { t = l->data; if (!t) continue;
                if (t->scr == scr) { script_timer_unbind(t, 1);  l = prev; } }

	for (prev = l = script_commands_bindings; l; l = l->next, prev = l) { c = l->data; if (!c) continue;
                if (c->scr == scr) { script_command_unbind(c, 1);  l = prev; } }

	for (prev = l = script_vars; l; l = l->next, prev = l) { v = l->data; if (!v) continue;
                if (v->scr == scr) { v->scr = NULL; v->private = NULL; /* script_var_unbind(v, 1);   l = prev; */ } ; }

	for (prev = l = script_queries; l; l = l->next, prev = l) { q = l->data; if (!q) continue;
                if (q->scr == scr) { script_query_unbind(q, 1); l = prev; } }
		
	if (slang->script_unload(scr))
		return -1;

	print("script_removed", scr->name);
	
	xfree(scr->name);
	xfree(scr->path);
	xfree(scr);
	list_remove(&scripts, scr, 0);
	
	return 0;

}

script_t *script_find(scriptlang_t *s, char *name)
{
	SCRIPT_FINDER ((( scr->lang ==  s || !s)) && !xstrcmp(name, scr->name));
}

int script_unload_name(scriptlang_t *s, char *name)
{
	script_t *scr;

	if (xstrlen(name) < 1) {
		print("script_need_name");
		return -1;
	}
	scr = script_find(s, name);
	
	if (!scr) {
		print("script_not_found", name);
		return -1;
	}
	if (script_unload(scr)) {
	    // error
		return -1;
	}
	
	return 0;

}

int script_unload_lang(scriptlang_t *s)
{
	scriptlang_t *lang;
	script_t *scr;
	list_t l;
	list_t prev;

	for (prev = l = scripts; l; l = l->next, prev = l) {
		scr = l->data;
		if (!scr) 
			continue;
		
		lang = scr->lang;
		if (!s || scr->lang == s) {
			script_unload(scr);
			l = prev;
		}
		
	}
	return 0;

}

int script_load(scriptlang_t *s, char *name)
{
	scriptlang_t	*slang;
	script_t	*scr;

	char 		*path;
	struct stat 	st;
	char           *name2;
	int 		ret;
	
	if (s && !xrindex(name, '.')) {
// TODO dodac do name rozszerzenie `s->ext`
	}
	
	if ((path = script_find_path(name))) {
		if (stat(path, &st) || S_ISDIR(st.st_mode)) {
			// katalog ; zaladowac wszystkie skrypty z katalogu ?
			// scripts_autoload(path) ?
			xfree(path);
			print("generic_error", strerror(EISDIR));
			return -1;
		}
		if (!s) 
			slang = scriptlang_from_ext(xrindex(path, '.'));
		else 
			slang = s;
			
		if (!slang || xstrcmp(xrindex(path, '.'), slang->ext) /* na wszelki wypadek */ ) {
			debug("[script_ierror] slang = 0x%x path = %s slang = %s slangext = %s\n", slang, path, slang->name, slang->ext);
			print("generic_error", "internal script handling ERROR, script not loaded.");
			xfree(path);
			
			return -1;
		}

		name2 = xstrdup(xrindex(path, '/')+1);
		name2[xstrlen(name2) - xstrlen(slang->ext)] = 0;

/* sprawdzic czy skrypt jest zaladowany jesli tak to go unload */
		if (script_find(slang, name2)) {
			debug("[script] the same script loaded unloading it!\n");
			script_unload(script_find(slang, name2));
		}

		scr = xmalloc(sizeof(script_t));
		scr->path = xstrdup(path);
		scr->name = name2;
		
		list_add(&scripts, scr, 0); /* to powinno byc przed script_loaded... nie fajne. */
		
		scr->lang = slang;
		ret = slang->script_load(scr);

		debug("[script] ret == %d\n", ret);
		
		if (ret < 1) {
			if (ret == -1)
				print("generic_error", "@script_load error");
			else if (ret == 0)
				print("generic_error", "@script_load script has no handler or error in getting handlers.");
			script_unload(scr);
			
//			xfree(scr->path);
//			xfree(scr->name);
//			xfree(scr);
//    			list_remove(&scripts, scr, 0);			

			xfree(path);
			
			return -1;
		
		}
		print("script_loaded", scr->name, scr->path, slang->name);
		
	
	}
	else print("script_not_found", name);
	
	xfree(path);
	return 0;
}


/******************************* VARIABLES ****************************/


script_var_t *script_var_find(script_t *scr, const char *name)
{
	list_t l;
        for (l = script_vars; l; l = l->next) {
		script_var_t *v = l->data;
                if ( /*  (!scr || v->scr == scr) && */ !xstrcasecmp(v->name, name)) {
			return v;
		}
        }
        return NULL;
}



script_var_t *script_var_add(scriptlang_t *s, script_t *scr, char *name, char *value, void *handler)
{
	script_var_t *tmp;
	tmp = script_var_find(scr, name);
	debug("[script_variable_add] (%s = %s) scr=%x tmp=%x\n", name, value, scr, tmp);

	if (tmp) {
		tmp->scr = scr;
		tmp->private = handler;
		variable_set(name, value, 0);
	}
	else if (!tmp) {
		SCRIPT_BIND_HEADER(script_var_t);
	
		temp->name  = name;
		temp->value = value;
		variable_add(NULL, temp->name, VAR_STR, 1, &(temp->value), &script_var_changed, NULL, NULL);

		SCRIPT_BIND_FOOTER(script_vars);
	} 
	
	return tmp;
}


int script_variables_read() {
// czytamy z scripts-var
	FILE *f;
        char *line;

	if (!(f = fopen(prepare_path("scripts-var", 0), "r"))) {
		debug("Error opening script variable file..\n");
		return -1;
	}
	
        while ((line = read_file(f))) {
		script_var_add(NULL, NULL, xstrdup(line), NULL, NULL);
                xfree(line);
        }

        fclose(f);
	return 0;
}


int script_variables_free(int free) {
	FILE *f = fopen(prepare_path("scripts-var", 0), "w");
	
	if (!f) 
		return -1;
	
	list_t l;
	debug("[script_variables_free()]%s saveing vars...\n", (free) ? " freeing &&" : "");

        for (l = script_vars; l; l = l->next) {
		script_var_t *v = l->data;
		
		if (f)
			fprintf(f, "%s\n", v->name);
		if (free) {
			xfree(v->name);
//			xfree(v->value); // zwolnione juz przez variables_free()
			xfree(v->private);
			xfree(v);
		}
        }
	fclose(f);
	return 0;
}

int script_variables_write() {
	return script_variables_free(0);
}

/************** COMMANDS **********************************************************/

int script_command_unbind(script_command_t *scr_comm, int from)
{
	int notfound = 1;
// TODO: powinnismy sprawdzic czy to polecenie wystepuje w innych handlerach.., jedno polecenie bedzie moze byc podbindowane pod wiele handlerow !
	
	if (notfound)
		command_remove(NULL, scr_comm->comm);

	if (from) {
		xfree(scr_comm->private);
	}
	xfree(scr_comm->comm);
	xfree(scr_comm);
	
	list_remove(&script_commands_bindings, scr_comm, 0);
	return notfound;
	
}

script_command_t *script_command_bind(scriptlang_t *s, script_t *scr, char *command, void *handler) 
{
	SCRIPT_BIND_HEADER(script_command_t);
	
	temp->comm = command;
	command_add(NULL, temp->comm, "?", script_command_handlers, 0, NULL);
	
	SCRIPT_BIND_FOOTER(script_commands_bindings);
}

/************************************* QUERIES ***********************************************/
int script_query_unbind(script_query_t *squery, int from)
{
	scriptlang_t *slang = squery->scr->lang;
// TODO: tak jak w command_unbind jeden handler moze byc pod wiele skryptow.
	query_disconnect(slang->plugin, squery->query_name);
//	query_disconnect(NULL, squery->query_name);
	return list_remove(&script_queries, squery, 0);
}

script_query_t *script_query_bind(scriptlang_t *s, script_t *scr, char *query_name, void *handler)
{
	SCRIPT_BIND_HEADER(script_query_t);
	
	temp->query_name	= query_name;
	
// argc i argv_type uzupelnic... z czego ? xstrcmp() ? 

#define CHECK(x) if (!xstrcmp(query_name, x)) 
#define ARG(x, y) temp->argv_type[x] = y;

	CHECK("protocol-disconnected")   { temp->argc = 1; ARG(0, SCR_ARG_CHARP); }
	else CHECK("protocol-connected") { temp->argc = 1; ARG(0, SCR_ARG_CHARP); }
	else CHECK("protocol-status")    { temp->argc = 4; ARG(0, SCR_ARG_CHARP); ARG(1, SCR_ARG_CHARP); ARG(2, SCR_ARG_CHARP); ARG(3, SCR_ARG_CHARP); }
	else CHECK("protocol-message")   { temp->argc = 7; ARG(0, SCR_ARG_CHARP);
							   ARG(1, SCR_ARG_CHARP);
							   ARG(2, SCR_ARG_CHARPP);
							   ARG(3, SCR_ARG_CHARP);
//							   ARG(4, SCR_ARG_UNITPP);
							   ARG(5, SCR_ARG_INT); // time_t
							   ARG(6, SCR_ARG_INT); 
					 }
	else CHECK("ui-keypress")	 { temp->argc = 1; ARG(0, SCR_ARG_INT); }
	else                             { temp->argc = 0; }

#undef CHECK
#undef ARG	

//	query_connect(NULL, temp->query_name, script_query_handlers, temp);
// TOD: jesli handler dla tego pluginu nie istnieje to dodac.
	query_connect(s->plugin, temp->query_name, script_query_handlers, temp);


	SCRIPT_BIND_FOOTER(script_queries);
}

/************************************ TIMERS ************************************************/


int script_timer_unbind(script_timer_t *stimer, int from)
{
// stimer->removed -> 1 - w trakcie usuwania ; 2 -> blad skryptu i usuwanie || usunieto 3 -> usunieto

//	debug("[script_timer_unbind] %d %d\n", stimer, from );
	if (stimer->removed > 2) { /* na wszelki wypadek */
		debug("[script_ierror] stimer->removed = %d\n", stimer->removed);
		return 0;
	}
	if (!stimer->removed) stimer->removed = 1;
	
	script_timer_handlers(1, stimer);
	if (from) 
		timer_remove(NULL, stimer->self->name);
	xfree(stimer->private);
	xfree(stimer->name);
//	xfree(stimer); // timer_remove() za nas to zwalnia.
	stimer->removed = 3; /* na wszelki wypadek */
	
	list_remove(&script_timers, stimer, 0);
	return 0;
}

script_timer_t *script_timer_bind(scriptlang_t *s, script_t *scr, int freq, void *handler)
{
	SCRIPT_BIND_HEADER(script_timer_t);
	
	temp->name    = saprintf("scr_%x", (int) temp); /* truly unique ;p */
	temp->self    = timer_add(NULL, (const char *) temp->name, freq, 1, &script_timer_handlers, (void *) temp);
	
	SCRIPT_BIND_FOOTER(script_timers);
} 

/******************************************************************** HANDLERY DO SKRYPTOW */

void script_var_changed(const char *var) {
	script_var_t     *temp = script_var_find(NULL, var);
	SCRIPT_HANDLER_HEADER(script_handler_var_t);
	debug("[script_variable_changed] varname = %s newvalue = %s\n", var, temp->value);
	SCRIPT_HANDLER_FOOTER(script_handler_var, temp->value);
	
	return;
}

COMMAND(script_command_handlers)
{
	script_command_t *temp;
	SCRIPT_HANDLER_HEADER(script_handler_command_t);
	
	list_t l;
	int i = 0;
	
	debug("[script_command_handlers] %s %s\n", name, params[0]);

	for (l = script_commands_bindings; l; l = l->next) {
		temp = l->data;
		
		if (!xstrcmp(name, temp->comm)) {
			debug("[script_command_handlers] found handler for %s\n", name);
			
			SCRIPT_HANDLER_FOOTER(script_handler_command, (char **) params)	{
				if (script_command_unbind(temp, 1))
					return ret; // TODO: powinnismy leciec z lista dalej....
			}
			i++;
		}
	
	}
	if (!i) {
		debug("[script_command_handlers] no handlers for %s found, deleteing. ERROR ? fixed.\n", name);
		command_remove(NULL, name);
		return 0;
	}
	return ret;

}

int script_timer_handlers(int type, void *d)
{
	script_timer_t	*temp	= d;
	SCRIPT_HANDLER_HEADER(script_handler_timer_t);

	if (!temp->removed && type) { // support na cos czego ekg2 nie ma. i beda memleaki !
		script_timer_unbind(temp, 0);
		return 0;
	} else if (temp->removed > 1) {
		return -1;
	}

	SCRIPT_HANDLER_FOOTER(script_handler_timer, type) {
		temp->removed = 2;
		if (!type) {
			timer_remove(NULL, temp->self->name);
			script_timer_unbind(temp, 0);
		}
	}

	return 0;
}


int script_query_handlers(void *data, va_list ap)
{
	script_query_t	*temp = data;
	SCRIPT_HANDLER_HEADER(script_handler_query_t);
	
	void 		*args[MAX_ARGS];
	int		i;
	
	for (i=0; i < temp->argc; i++) 
		args[i] = (void *) va_arg(ap, void *);
	
	SCRIPT_HANDLER_FOOTER(script_handler_query, &args);

	return ret;
}


/*******************************************************************************MAIN FUNCTIONSSSSSS*/

/* from python.c  python_autorun() 
 *
 * load scripts from $CONFIG/scripts/autorun
 * load scripts from $PREFIX/scripts/autorun
 *
 */
int scripts_autoload(const char *path)
{
	struct dirent *d;
	struct stat st;
	char *tmp;
	DIR *dir;
	if (!(dir = opendir(path)))
		return 0;
	while ((d = readdir(dir))) {
		tmp = saprintf("%s/%s", path, d->d_name);

		if (stat(tmp, &st) || S_ISDIR(st.st_mode)) {
			// scripts_autoload(tmp) ?
			xfree(tmp);
			continue;
		}
		debug("[script] autoloading %s from %s\n", d->d_name, path);
		if (!scriptlang_from_ext(xrindex(tmp, '.'))) { 
			xfree(tmp);
			continue;
		}
		script_load(NULL, tmp);
		xfree(tmp);
	}

	closedir(dir);
	return 0;
}


COMMAND(script_cmd_load) { return script_load(NULL, (char *) params[0]); }
COMMAND(script_cmd_list) { return script_list(NULL); /* param[0] - slang ? */ }
COMMAND(script_cmd_varlist) { return script_var_list(NULL); /* param[0] - script ? */ }

COMMAND(cmd_script)
{
        if (xstrlen(params[0]) < 1)
                return script_cmd_list(name, params, session, target, quiet);
        else {
		if (!xstrcmp(params[0], "--load")) 
			return script_load(NULL, (char *) params[1]);
//            		return script_cmd_load(name, params[1], session, target, quiet);
		if (!xstrcmp(params[0], "--list"))
            		return script_list(NULL );
		if (!xstrcmp(params[0], "--varlist"))
			return script_var_list(NULL  /* params[1] */);
	}
	return -1;
}

int scripts_init()
{
	char *pathtmp;
        command_add(NULL, "script"        , "p ?", cmd_script, 0, "--list --load --varlist"); /* todo  ?!!? */
	command_add(NULL, "script:load"   , "f"  , script_cmd_load, 0, "");
	command_add(NULL, "script:list"   , "?"  , script_cmd_list, 0, "");
	command_add(NULL, "script:varlist", "?"  , script_cmd_varlist, 0, "");
	
	script_variables_read();

	pathtmp = saprintf("%s/autorun", DATADIR);
	scripts_autoload(pathtmp);
	xfree(pathtmp);
	
	pathtmp = saprintf("%s/autorun", prepare_path("scripts", 0));
	scripts_autoload(pathtmp);
	xfree(pathtmp);
	
	return 0;
}
