#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "char.h"
#include "debug.h"
#include "dynstuff.h"
#include "scripts.h"
#include "xmalloc.h"

#include "commands.h"	/* commands */
#include "protocol.h"	/* queries */
#include "stuff.h"	/* timer */
#include "vars.h"	/* vars */

/* TODO && BUGS 
 * - cleanup.
 * - multiple handler for commands && var_changed. 
 * - memleaks ?
 */

list_t	scripts;
list_t	script_timers;
list_t	script_plugins;
list_t	script_vars;
list_t	script_queries;
list_t	script_commands;
list_t	script_watches;
list_t	scriptlang;


COMMAND(script_command_handlers);
TIMER(script_timer_handlers);
void script_var_changed(const CHAR_T *var);
QUERY(script_query_handlers);
WATCHER(script_handle_watch);
int script_plugin_theme_init( /* plugin_t *p */ );

int scripts_autoload(scriptlang_t *scr);
char *script_find_path(char *name);
/****************************************************************************************************/

scriptlang_t *scriptlang_from_ext(char *name)
{
	scriptlang_t *s;
	list_t l;
	char *ext = xrindex(name, '.');
	
	if (!ext) return NULL;
	
	for (l = scriptlang; l; l = l->next) {
		s = l->data;
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
	
	if (!in_autoexec)
		scripts_autoload(s);
	return 0;
}

int scriptlang_unregister(scriptlang_t *s)
{
	script_unload_lang(s);
	s->deinit();
	
	return list_remove(&scriptlang, s, 0);
}

/**************************************************************************************/

int script_autorun(char *scriptname, 
		   int isautorun /* 0 - turn off ; 1 - turn on ; -1 off->on  on->off */) {
/*
 * yeah i know it could be faster, better and so on, but it was written for special event and it look's like like it look... ;>
 * and it's short, easy to understand etc.. ;>
 */
	int ret = -1;

	if (!scriptname) {
/*
 TODO: list script from autorun dir.
 script_autorun_list
  %1 - filename
  %2 - readlink.
 */
		return 0;
	}
	if (isautorun) {
		char *path = script_find_path(scriptname);
		char *ext  = NULL;
		if (!(xrindex(scriptname, '.')))
			ext = xrindex(path, '.');
/* TODO: maybe we should check if (ext) belongs to any scriptlang... ? and in script_find_path() ? 
 * to avoid stupid user mistakes... but i don't think ;>
 */
		if (path) {
			char *autorunpath = saprintf("%s/%s%s", prepare_path("scripts/autorun", 0), scriptname, (ext) ? ext : ""); 
			debug("[SCRIPT_AUTORUN] makeing symlink from %s to %s ...", path, autorunpath);
#ifndef NO_POSIX_SYSTEM
			ret = symlink(path, autorunpath);
#endif
			debug("%d\n", ret);
			xfree(autorunpath);
		}
		xfree(path);
		if (ret && isautorun == -1)
			isautorun = 0;
		else    isautorun = 1;
	}
	if (!isautorun) {
		char *path1 = saprintf("%s/%s", prepare_path("scripts/autorun", 0), scriptname);
		char *path = script_find_path(path1); 
		if (path && path1) {
			debug("[SCRIPT_AUTORUN] unlinking %s... ", path);
			ret = unlink(path);
			debug("%d\n", ret);
		} else isautorun = -1;
		xfree(path);
		xfree(path1);
	}
	if (!ret)
		print("script_autorun_succ", scriptname, (isautorun == 1) ? "added to" : "removed from");
	else if (isautorun == -1) 
		print("script_autorun_unkn", scriptname, strerror(errno)); /* i think only when there isn't such a file but i'm not sure */
	else
		print("script_autorun_fail", scriptname, (isautorun == 1) ? "to add to" : "to remove from", strerror(errno));
	return ret;
}

int script_reset(scriptlang_t *scr)
{
	list_t l;
	scriptlang_t *s;
	
	for (l = scriptlang; l; l = l->next) {
		s = l->data;

		script_unload_lang(s);
    		s->deinit();
		
		s->init();
		scripts_autoload(s);
	}
	return 0;
}

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
			print("script_varlist", v->self->name, v->value, v->private);
			i++;
		}
        }
	if (!i)
		print("script_varlist_empty");
        return i;
}

/***********************************************************************************/

char *script_find_path(char *name) {
	FILE 		*fajl;
	char 		*ext;
	char 		*nametmp;
	char 		*path = NULL;

	scriptlang_t 	*s = NULL;
	list_t 		l  = scriptlang;

	nametmp = xstrdup(name);
	while ((ext = xrindex(nametmp, '.')) || l) {
		if (ext) {
			if (nametmp[0] == '/' && (fajl = (fopen(nametmp, "r")))) {
				fclose(fajl);
				return nametmp;
			}
			path = saprintf("%s/%s",prepare_path("scripts", 0), nametmp);
			fajl = fopen(path, "r");

			if (!fajl) {
				xfree(path);
				path = saprintf("%s/scripts/%s", DATADIR, nametmp);
				fajl = fopen(path, "r");
			}
/* etc.. */
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
	typedef struct { script_t *scr; } tmpstruct;

	scriptlang_t *slang = scr->lang;
	void 	     *t;    /* t comes from temporary !from timer ;> */
	list_t 	      l;

#define s(x) ((tmpstruct *) x)
/* przeszukac liste timerow i komand, jak cos to je wywalic */
	for (l = script_timers; l;)   { t = l->data; l = l->next; if (!t) continue;
                if (s(t)->scr == scr) { script_timer_unbind(t, 1); } }

	for (l = script_commands; l;) { t = l->data; l = l->next; if (!t) continue;
                if (s(t)->scr == scr) { script_command_unbind(t, 1); } }

	for (l = script_vars; l;)     { t = l->data; l = l->next; if (!t) continue;
                if (s(t)->scr == scr) { script_var_unbind(t, 1); } }

	for (l = script_queries; l;)  { t = l->data; l = l->next; if (!t) continue;
                if (s(t)->scr == scr) { script_query_unbind(t, 1); } }

	for (l = script_watches; l;)  { t = l->data; l = l->next; if (!t) continue;
		if (s(t)->scr == scr) { script_watch_unbind(t, 1); } }
#undef s
	
	if (slang->script_unload(scr))
		return -1;

	print("script_removed", scr->name);
	
	xfree(scr->name);
	xfree(scr->path);
	return list_remove(&scripts, scr, 1);
}

script_t *script_find(scriptlang_t *s, char *name)
{
	SCRIPT_FINDER ((( scr->lang == s || !s)) && !xstrcmp(name, scr->name));
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
		/* error */
		return -1;
	}
	
	return 0;
}

int script_unload_lang(scriptlang_t *s)
{
	scriptlang_t *lang;
	script_t *scr;
	list_t l;

	for (l = scripts; l;) {
		scr = l->data;
		l   = l->next;
		if (!scr) 
			continue;
		
		lang = scr->lang;
		if (!s || scr->lang == s) {
			script_unload(scr);
		}
	}
	return 0;
}

int script_load(scriptlang_t *s, char *tname)
{
	scriptlang_t	*slang;
	script_t	*scr;
	struct stat 	st;
	char 		*path, *name2, *name = NULL;
	int 		ret;

	if (!xstrlen(tname)) {
		print("script_need_name");
		return -1;
	}

	if (s && !xrindex(tname, '.'))
		name = saprintf("%s%s", tname, s->ext);
	else    name = xstrdup(tname);
	
	if ((path = script_find_path(name))) {
		if (stat(path, &st) || S_ISDIR(st.st_mode)) {
			/* scripts_loaddir(path) (?) */
			xfree(path);
			xfree(name);
			print("generic_error", strerror(EISDIR));
			return -1;
		}
		slang = (s) ? s : scriptlang_from_ext(path);

		if (!slang || xstrcmp(xrindex(path, '.'), slang->ext)) {
                        if (slang) { /* internal error shouldn't happen */
                                debug("[script_ierror] slang = 0x%x path = %s slang = %s slangext = %s\n", slang, path, slang->name, slang->ext);
                                print("generic_error", _("internal script handling ERROR, script not loaded."));
                        } else {
                                debug("[script] extension = %s\n", xrindex(path, '.'));
                                print("generic_error", _("Can't recognize script type"));
                        }
			xfree(path);
			xfree(name);
			return -1;
		}

		name2 = xstrdup(xrindex(path, '/')+1);
		name2[xstrlen(name2) - xstrlen(slang->ext)] = 0;

		if ((scr = script_find(slang, name2))) { /* if script with the same name is loaded then ...*/
			debug("[script] the same script loaded unloading it!\n");
			script_unload(scr); /*... unload old one. */
		}

		scr = xmalloc(sizeof(script_t));
		scr->path = xstrdup(path);
		scr->name = name2;
		scr->lang = slang;
		
		list_add(&scripts, scr, 0); /* BUG: this should be before `script_loaded`...  */

		ret = slang->script_load(scr);

/*		debug("[script] script_load ret == %d\n", ret); */
		
		if (ret < 1) {
			if (ret == -1)
				print("generic_error", "@script_load error");
			else if (ret == 0)
				print("generic_error", "@script_load script has no handler or error in getting handlers.");
			xfree(path);
			xfree(name);
			script_unload(scr);
			return -1;
		
		}
		print("script_loaded", scr->name, scr->path, slang->name);
	}
	else 
		print("script_not_found", name);
	
	xfree(path);
	xfree(name);
	return 0;
}

int script_variables_read() {
	FILE *f;
        char *line;

	if (!(f = fopen(prepare_path("scripts-var", 0), "r"))) {
		debug("Error opening script variable file..\n");
		return -1;
	}
	
        while ((line = read_file(f))) {
		script_var_add(NULL, NULL, line, NULL, NULL);
                xfree(line);
        }

        fclose(f);
	return 0;
}

int script_variables_free(int free) {
	FILE *f = fopen(prepare_path("scripts-var", 0), "w");
	list_t l;
	
	if (!f && !free) 
		return -1;
	
        for (l = script_vars; l; l = l->next) {
		script_var_t *v = l->data;
		
		if (f)
			fprintf(f, "%s\n", v->name);
		if (free) {
/*			xfree(v->value); variables_free() free it. */
			xfree(v->private); /* should be NULL here. */
			xfree(v->name);
			xfree(v);
		}
        }
	if (f)
		fclose(f);
	
	if (free)
		list_destroy(script_vars, 0);
	return 0;
}

int script_variables_write() {
	return script_variables_free(0);
}

script_command_t *script_command_find(const CHAR_T *name)
{
	script_command_t *temp;
	list_t l;
	for (l = script_commands; l; l = l->next) {
		temp = l->data;
		if (!xwcscmp(name, temp->self->name))
			return temp;
	
	}
	return NULL;
}

script_var_t *script_var_find(const char *name)
{
	list_t l;
#if 0 /* i don't remember that code... */
	if (!variable_find(name))
		return NULL;
#endif	
        for (l = script_vars; l; l = l->next) {
		script_var_t *v = l->data;
                if (!xstrcasecmp(v->name, name)) {
			return v;
		}
        }
        return NULL;
}

/**********************************************************************************************************************/

int script_command_unbind(script_command_t *temp, int free)
{
	int notfound = 1; /* TODO */
	SCRIPT_UNBIND_HANDLER(SCRIPT_COMMANDTYPE, temp->private);
	if (notfound)
		command_freeone(temp->self);
	return list_remove(&script_commands, temp, 1);
}


int script_query_unbind(script_query_t *temp, int free)
{
	SCRIPT_UNBIND_HANDLER(SCRIPT_QUERYTYPE, temp->private);
	query_free(temp->self);	
	return list_remove(&script_queries, temp, 1);
}

int script_plugin_destroy(/* plugin_t *p */ )
/* and what i can do here ? */
{
/* ok somethink i can */
        script_plugin_t *temp = NULL;
        list_t l;
        for (l = script_plugins; l; l = l->next) {
		if (temp) {
			debug("Err @ script_plugin_destroy more that 1 script as plugin, plugin_destroying must be rewritten!\n");
			return -1;
		} else temp = l->data;
	}

	SCRIPT_UNBIND_HANDLER(SCRIPT_PLUGINTYPE, temp->private);
	plugin_unregister(temp->self);
	xfree(temp->self->name);
	xfree(temp->self);
	return list_remove(&script_plugins, temp, 1);
}								

int script_timer_unbind(script_timer_t *temp, int remove)
{
	if (temp->removed) return -1;
	temp->removed = 1;
	if (remove) 
		timer_freeone(temp->self);
	SCRIPT_UNBIND_HANDLER(SCRIPT_TIMERTYPE, temp->private);
	return list_remove(&script_timers, temp, 0 /* 0 is ok */);
}

int script_watch_unbind(script_watch_t *temp, int remove)
{
	if (temp->removed) return -1;
	temp->removed = 1;
	if (remove)
		watch_free(temp->self);
/* TODO: testit */
	SCRIPT_UNBIND_HANDLER(SCRIPT_WATCHTYPE, temp->private, temp->data);
	return list_remove(&script_watches, temp, 1);
}

int script_var_unbind(script_var_t *temp, int free)
{
	SCRIPT_UNBIND_HANDLER(SCRIPT_VARTYPE, temp->private);
	temp->scr = NULL; 
	temp->private = NULL;
	return 0;
}

/****************************************************************************************************/

script_var_t *script_var_add(scriptlang_t *s, script_t *scr, char *name, char *value, void *handler)
{
	script_var_t *tmp;
	tmp = script_var_find(name);
	if (tmp) {
		tmp->scr = scr;
		tmp->private = handler;
		if (in_autoexec) /* i think it is enough, not tested. */
			variable_set(name, value, 0);
	} else if (!tmp) {
		CHAR_T *aname;
		SCRIPT_BIND_HEADER(script_var_t);
		aname = normal_to_wcs(name);
		temp->name  = xstrdup(name);
		temp->value = xstrdup(value);
		temp->self = variable_add(NULL, aname, VAR_STR, 1, &(temp->value), &script_var_changed, NULL, NULL);
		free_utf(aname);
		SCRIPT_BIND_FOOTER(script_vars);
	} 
	
	return tmp;
}

script_command_t *script_command_bind(scriptlang_t *s, script_t *scr, char *command, void *handler) 
{
	CHAR_T *acommand;
	SCRIPT_BIND_HEADER(script_command_t);
	acommand = normal_to_wcs(command);
	temp->self = command_add(NULL, acommand, TEXT("?"), script_command_handlers, COMMAND_ISSCRIPT, NULL);
	free_utf(acommand);
	SCRIPT_BIND_FOOTER(script_commands);
}

script_plugin_t *script_plugin_init(scriptlang_t *s, script_t *scr, char *name, plugin_class_t pclass, void *handler)
{
	SCRIPT_BIND_HEADER(script_plugin_t);
	temp->self = xmalloc(sizeof(plugin_t));
	temp->self->name = xstrdup(name);
	temp->self->pclass = pclass;
	temp->self->destroy = script_plugin_destroy;
	temp->self->theme_init = script_plugin_theme_init; 
	plugin_register(temp->self, -254 /* default */);							
	SCRIPT_BIND_FOOTER(script_plugins);
}

script_timer_t *script_timer_bind(scriptlang_t *s, script_t *scr, int freq, void *handler)
{
	char *tempname;
	SCRIPT_BIND_HEADER(script_timer_t);
	tempname   = saprintf("scr_%x", (int) temp); /* truly unique ;p */
	temp->self = timer_add(NULL, (const char *) tempname, freq, 1, &script_timer_handlers, (void *) temp);
	xfree(tempname);
	SCRIPT_BIND_FOOTER(script_timers);
} 

script_watch_t *script_watch_add(scriptlang_t *s, script_t *scr, int fd, int type, void *handler, void *data)
{
	SCRIPT_BIND_HEADER(script_watch_t);
	temp->data = data;
	temp->self = watch_add(s->plugin, fd, type, script_handle_watch, temp);
	SCRIPT_BIND_FOOTER(script_watches);
}

script_query_t *script_query_bind(scriptlang_t *s, script_t *scr, char *query_name, void *handler)
{
	SCRIPT_BIND_HEADER(script_query_t);
/* argc i argv_type uzupelnic... z czego ? xstrcmp() ?  */
#define CHECK(x) if (!xstrcmp(query_name, x)) 
#define CHECK_(x) if (!xstrncmp(query_name, x, xstrlen(x)))
#define NEXT_ARG(y) temp->argv_type[temp->argc] = y; temp->argc++;

/* PROTOCOL */
	CHECK("protocol-disconnected")      { NEXT_ARG(SCR_ARG_CHARP);	} /* XXX */
	else CHECK("protocol-connected")    { NEXT_ARG(SCR_ARG_CHARP);	} /* XXX */
	else CHECK("protocol-status")       { NEXT_ARG(SCR_ARG_CHARP); 
					      NEXT_ARG(SCR_ARG_CHARP); 
					      NEXT_ARG(SCR_ARG_CHARP); 
					      NEXT_ARG(SCR_ARG_CHARP);	}
	else CHECK("protocol-message")      { NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARPP);
					      NEXT_ARG(SCR_ARG_CHARP);
/*					      NEXT_ARG(SCR_ARG_UNITPP); */
					      NEXT_ARG(SCR_ARG_INT);	/* time_t */
					      NEXT_ARG(SCR_ARG_INT);	}
	else CHECK("protocol-message-post")      { NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARPP);
					      NEXT_ARG(SCR_ARG_CHARP);
/*					      NEXT_ARG(SCR_ARG_UNITPP); */
					      NEXT_ARG(SCR_ARG_INT);	/* time_t */
					      NEXT_ARG(SCR_ARG_INT);	}
	else CHECK("protocol-message-received") { NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARPP);
					      NEXT_ARG(SCR_ARG_CHARP);
/*					      NEXT_ARG(SCR_ARG_UNITPP); */
					      NEXT_ARG(SCR_ARG_INT);	/* time_t */
					      NEXT_ARG(SCR_ARG_INT);	}
	else CHECK("protocol-message-sent") { NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARP);  }
	else CHECK("protocol-validate-uid") { NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_INT);	} 
/* USERLIST */
	else CHECK("userlist-added")	    { NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARP); 
					      NEXT_ARG(SCR_ARG_INT);	}
	else CHECK("userlist-changed")      { NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARP);	}
	else CHECK("variable-changed")	    { NEXT_ARG(SCR_ARG_CHARP);	}
/* UI */
	else CHECK("ui-keypress")	    { NEXT_ARG(SCR_ARG_INT); 	}
	else CHECK("ui-is-initialized")	    { NEXT_ARG(SCR_ARG_INT); 	}
	else CHECK("ui-window-new")	    { NEXT_ARG(SCR_ARG_WINDOW); }
	else CHECK("ui-window-switch")	    { NEXT_ARG(SCR_ARG_WINDOW); }
	else CHECK("ui-window-print")	    { NEXT_ARG(SCR_ARG_WINDOW); 
					      NEXT_ARG(SCR_ARG_FSTRING);}
/* IRC */
	else CHECK("irc-kick")		    { NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARP);	}
	else CHECK("irc-protocol-message")  { NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_INT);
					      NEXT_ARG(SCR_ARG_INT);
					      NEXT_ARG(SCR_ARG_INT);
					      NEXT_ARG(SCR_ARG_CHARP);  }
	else CHECK_("irc-protocol-numeric") { NEXT_ARG(SCR_ARG_CHARP);
					      NEXT_ARG(SCR_ARG_CHARPP);	}
/* other */
	else                                {                           }

#undef CHECK
#undef CHECK_
#undef NEXT_ARG

	temp->self = query_connect(s->plugin, query_name, script_query_handlers, temp);
	SCRIPT_BIND_FOOTER(script_queries);
}

/*****************************************************************************************************************/

void script_var_changed(const CHAR_T *var) {
	script_var_t     *temp = script_var_find(var);
/*	if (in_autoexec) ... */
	SCRIPT_HANDLER_HEADER(script_handler_var_t);
/*	debug("[script_variable_changed] varname = %s newvalue = %s\n", var, temp->value); */
	SCRIPT_HANDLER_MULTI_FOOTER(script_handler_var, temp->value);
	return;
}

WATCHER(script_handle_watch)
{
	script_watch_t *temp = data;

	SCRIPT_HANDLER_HEADER(script_handler_watch_t);
	SCRIPT_HANDLER_FOOTER(script_handler_watch, type, fd, watch) {
		if (!type) {
			return -1; /* watch_free(temp->self); */
		}
	}
	if (type)
		script_watch_unbind(temp, 0);
	return 0;
}

COMMAND(script_command_handlers)
{
	PARASC
	script_command_t *temp = script_command_find(name);

	SCRIPT_HANDLER_HEADER(script_handler_command_t);
	SCRIPT_HANDLER_MULTI_FOOTER(script_handler_command, (char **) params) {
		script_command_unbind(temp, 1);
	}
	return ret;
}

int script_plugin_theme_init( /* plugin_t *p */ ) 
{
/* TODO: it will be slow! foreach scriptplugin call format initializer.  (?) */
	return 0;
}

TIMER(script_timer_handlers)
{
	script_timer_t *temp = data;
	SCRIPT_HANDLER_HEADER(script_handler_timer_t);
	debug("::: -> %s %d\n", temp->private, type);
	SCRIPT_HANDLER_FOOTER(script_handler_timer, type) {
		if (!type) {
			return -1; /* timer_freeone(temp->self); */
		}
	}
	if (type)
		script_timer_unbind(temp, 0);
	return 0;
}

QUERY(script_query_handlers)
{
	script_query_t	*temp = data;
	void 		*args[MAX_ARGS];
	int		i;
	SCRIPT_HANDLER_HEADER(script_handler_query_t);
	
	for (i=0; i < temp->argc; i++) 
		args[i] = (void *) va_arg(ap, void *);
	
	SCRIPT_HANDLER_FOOTER(script_handler_query, (void **) &args);

	return ret;
}

/********************************************************************************/

/* from python.c  python_autorun() 
 *  load  scripts from `path`
 */

int scripts_loaddir(scriptlang_t *s, const char *path)
{
	struct dirent *d;
	struct stat st;
	char *tmp;
	scriptlang_t *slang;
	int i = 0;
	DIR *dir;
	
	if (!(dir = opendir(path)))
		return 0;

	while ((d = readdir(dir))) {
		tmp = saprintf("%s/%s", path, d->d_name);

		if (stat(tmp, &st) || S_ISDIR(st.st_mode)) {
			xfree(tmp);
			continue;
		}
		if (!(slang = scriptlang_from_ext(tmp)) || (s != NULL && s != slang) ) { 
			xfree(tmp);
			continue;
		}
		debug("[script] autoloading %s from %s scr = %s\n", d->d_name, path, slang->name);
		if (!script_load(NULL, tmp)) i++;
		xfree(tmp);
	}

	closedir(dir);
	return i;
}

COMMAND(cmd_script)
{
	PARASC
	scriptlang_t *s = NULL;
	char 	     *tmp = NULL;
	char	     *param0 = NULL;

	if (xwcscmp(name, TEXT("script"))) { /* script:*    */
		tmp = (char *) name; 
		param0 = (char *) params[0];
	} else if (params[0]) {        /* script --*  */
		tmp = (char *) params[0]+2;
		param0 = (char *) params[1];
	}
/*	s = param0 ? */

        if (xstrlen(tmp) < 1)
                return script_list(NULL); 
        else {
		if (xstrlen(params[0]) > 0) { /* somethink like we have in /plugin ;> e.g /script +dns /script -irc */
			if (params[0][0] == '+')
				return script_load(NULL, (char *) params[0]+1);
			else if (params[0][0] == '-' && params[0][1] != '-')
				return script_unload_name(NULL, (char *) params[0]+1);
		}

		if (!xstrcmp(tmp, "load")) 
			return script_load(NULL, param0);
		else if (!xstrcmp(tmp, "unload"))
			return script_unload_name(NULL, param0);
		else if (!xstrcmp(tmp, "list"))
			return script_list(s);
		else if (!xstrcmp(tmp, "varlist"))
			return script_var_list(NULL /*s*/);
		else if (!xstrcmp(tmp, "reset"))
			return script_reset(s);
		else if (!xstrcmp(tmp, "autorun"))
			return script_autorun(param0, -1);
	}
	return -1;
}

/*
 * load scripts from $CONFIGDIR/scripts/autorun ($CONFIGDIR - ~/.ekg2/        || ~/.ekg2/perl || ... )
 * load scripts from $DATADIR/scripts/autorun   ($DATADIR   - /usr/share/ekg2 || /usr/local/share/ekg2 || ...) (Turned off ;>)
 */

int scripts_autoload(scriptlang_t *scr)
{
	int i = 0;
/*	i += scripts_loaddir(scr, DATADIR"/scripts/autorun"); */       /* I don't think it will be useful */
	i += scripts_loaddir(scr, prepare_path("scripts/autorun", 0)); /* we ought to load only scripts from home dir. */
	debug("[SCRIPTS_AUTOLOAD] DONE: (re)loaded %d scripts\n", i);
	return i;
}
#if 0
int script_postinit(void *data, va_list ap)
{
	return scripts_autoload(NULL);
}
#endif
int scripts_init()
{
	command_add(NULL, TEXT("script")        , TEXT("p ?"),	cmd_script, 0, "--list --load --unload --varlist --reset"); /* todo  ?!!? */
	command_add(NULL, TEXT("script:load")   , TEXT("f"),	cmd_script, 0, "");
	command_add(NULL, TEXT("script:unload") , TEXT("?"),	cmd_script, 0, "");
	command_add(NULL, TEXT("script:list")   , TEXT("?"),	cmd_script, 0, "");
	command_add(NULL, TEXT("script:reset")  , TEXT("?"),	cmd_script, 0, "");
	command_add(NULL, TEXT("script:varlist"), TEXT("?"),	cmd_script, 0, "");
	command_add(NULL, TEXT("script:autorun"), TEXT("?"),	cmd_script, 0, "");
	script_variables_read();
#if 0
	query_connect(NULL, "config-postinit",     script_postinit, NULL);
#else
	scripts_autoload(NULL);
#endif
	return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=8 sw=8
 */
