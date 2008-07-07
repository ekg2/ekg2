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

#include "debug.h"
#include "dynstuff.h"
#include "scripts.h"
#include "xmalloc.h"

#include "commands.h"	/* commands */
#include "protocol.h"	/* queries */
#include "stuff.h"	/* timer */
#include "vars.h"	/* vars */

#include "queries.h"

/* TODO && BUGS 
 * - cleanup.
 * - multiple handler for commands && var_changed. 
 * - memleaks ?
 */

script_t	*scripts;
scriptlang_t	*scriptlang;

static list_t script_timers;
static list_t script_plugins;
static list_t script_vars;
static list_t script_queries;
static list_t script_commands;
static list_t script_watches;

static COMMAND(script_command_handlers);
static TIMER(script_timer_handlers);
static void script_var_changed(const char *var);
static QUERY(script_query_handlers);
static WATCHER(script_handle_watch);
static int script_plugin_theme_init( /* plugin_t *p */ );

static int scripts_autoload(scriptlang_t *scr);
static char *script_find_path(const char *name);
/****************************************************************************************************/

scriptlang_t *scriptlang_from_ext(char *name)
{
	scriptlang_t *s;
	char *ext = xrindex(name, '.');
	
	if (!ext) return NULL;
	
	for (s = scriptlang; s; s = s->next) {
		if (!xstrcmp(ext, s->ext))
			return s;
	}
	return NULL;
}

int scriptlang_register(scriptlang_t *s)
{
	LIST_ADD2(&scriptlang, s);

	s->init();
	
	if (!in_autoexec)
		scripts_autoload(s);
	return 0;
}

int scriptlang_unregister(scriptlang_t *s)
{
	script_unload_lang(s);
	s->deinit();
	LIST_UNLINK2(&scriptlang, s);
	
	return 0;
}

/**************************************************************************************/

int script_autorun(char *scriptname, 
		   int isautorun /* 0 - turn off ; 1 - turn on ; -1 off->on  on->off */) {
/*
 * yeah i know it could be faster, better and so on, but it was written for special event and it look's like like it look... ;>
 * and it's short, easy to understand etc.. ;>
 */
	int ret = -1;
	int old_errno = 0;

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

		errno = 0;
		if (path) {
			/* XXX sanity scriptname */
			const char *autorunpath = prepare_pathf("scripts/autorun/%s%s", scriptname, ext ? ext : "");

			if (autorunpath && mkdir_recursive(autorunpath, 0) == 0) {
				debug("[SCRIPT_AUTORUN] symlink from %s to %s ... retcode:", path, autorunpath);
#ifndef NO_POSIX_SYSTEM
				ret = symlink(path, autorunpath);
#endif
				debug("%d\n", ret);
			}

			if (!autorunpath)
				errno = ENAMETOOLONG;
		}
		xfree(path);
		if (ret && isautorun == -1)
			isautorun = 0;
		else    isautorun = 1;

		old_errno = errno;
	}
	if (!isautorun) {
		const char *path1 = prepare_pathf("scripts/autorun/%s", scriptname);

		if (path1) {
			char *path = script_find_path(path1);

			if (path && path1) {
				debug("[SCRIPT_AUTORUN] unlinking %s... ", path);
				ret = unlink(path);
				debug("%d\n", ret);
			} else isautorun = -1;

			xfree(path);
		} else errno = ENAMETOOLONG;
	}
	if (!ret)
		print("script_autorun_succ", scriptname, (isautorun == 1) ? "added to" : "removed from");
	else if (isautorun == -1) 
		print("script_autorun_unkn", scriptname, "", strerror(errno)); /* i think only when there isn't such a file but i'm not sure */
	else
		print("script_autorun_fail", scriptname, (isautorun == 1) ? "to add to" : "to remove from", strerror(errno));
	return ret;
}

int script_reset(scriptlang_t *scr)
{
	scriptlang_t *s;
	
	for (s = scriptlang; s; s = s->next) {
		script_unload_lang(s);
    		s->deinit();
		
		s->init();
		scripts_autoload(s);
	}
	return 0;
}

int script_list(scriptlang_t *s)
{
	script_t *scr;
	scriptlang_t *lang;
	int i = 0;
	
	for (scr = scripts; scr; scr = scr->next) {
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

static char *script_find_path(const char *name) {
	FILE 		*fajl;
	char 		*ext;
	char 		*nametmp;
	char 		*path = NULL;

	scriptlang_t 	*s = scriptlang;

	nametmp = xstrdup(name);
	while ((ext = xrindex(nametmp, '.')) || s) {
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
		if (!s) return NULL;
		nametmp  = saprintf("%s%s", name, s->ext);
		s = s->next;
	}
	return NULL;
}

int script_unload(script_t *scr)
{
	typedef struct { script_t *scr; } tmpstruct;

	scriptlang_t *slang = scr->lang;
	void 	     *t;    /* t comes from temporary !from timer ;> */
	list_t 	      l;

	scr->inited = 0;

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

	print("script_removed", scr->name, scr->path, slang->name);
	
	xfree(scr->name);
	xfree(scr->path);
	LIST_REMOVE2(&scripts, scr, NULL);

	return 0;
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

	for (scr = scripts; scr;) {
		script_t *next	= scr->next;
		
		lang = scr->lang;
		if (!s || scr->lang == s) {
			script_unload(scr);
		}

		scr = next;
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
		scr->inited = 1;
		
		LIST_ADD2(&scripts, scr); /* BUG: this should be before `script_loaded`...  */

		ret = slang->script_load(scr);

/*		debug("[script] script_load ret == %d\n", ret); */
		
		if (ret < 1) {
			if (ret == -1)
				print("script_incorrect", scr->name, scr->path, slang->name);
			else if (ret == 0)
				print("script_incorrect2", scr->name, scr->path, slang->name); /* "script has no handler or error in getting handlers." */
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
	
        while ((line = read_file(f, 0)))
		script_var_add(NULL, NULL, line, NULL, NULL);

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

script_command_t *script_command_find(const char *name)
{
	script_command_t *temp;
	list_t l;
	for (l = script_commands; l; l = l->next) {
		temp = l->data;
		if (!xstrcmp(name, temp->self->name))
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
		commands_remove(temp->self);
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
		timer_free(temp->self);
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
		SCRIPT_BIND_HEADER(script_var_t);
		temp->name  = xstrdup(name);
		temp->value = xstrdup(value);
		temp->self = variable_add(NULL, name, VAR_STR, 1, &(temp->value), &script_var_changed, NULL, NULL);
		SCRIPT_BIND_FOOTER(script_vars);
	} 
	
	return tmp;
}

script_command_t *script_command_bind(scriptlang_t *s, script_t *scr, char *command, void *handler) 
{
	SCRIPT_BIND_HEADER(script_command_t);
	temp->self = command_add(NULL, command, ("?"), script_command_handlers, COMMAND_ISSCRIPT, NULL);
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
	tempname   = saprintf("scr_%p", temp); /* truly unique ;p */
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

script_query_t *script_query_bind(scriptlang_t *s, script_t *scr, char *qname, void *handler)
{
	SCRIPT_BIND_HEADER(script_query_t);

#define NEXT_ARG(y) temp->argv_type[temp->argc] = y; temp->argc++;

/* hacki */
	if (!xstrcmp(qname, "protocol-disconnected"))		temp->hack = 1;
	else if (!xstrcmp(qname, "protocol-status"))		temp->hack = 2;
	else if (!xstrcmp(qname, "protocol-message"))		temp->hack = 3;
	else if (!xstrcmp(qname, "protocol-message-post"))	temp->hack = 4;
	else if (!xstrcmp(qname, "protocol-message-received"))	temp->hack = 5;

	if (!xstrcmp(qname, "protocol-disconnected-2"))		qname = "protocol-disconnected";
	else if (!xstrcmp(qname, "protocol-status-2"))		qname = "protocol-status";
	else if (!xstrcmp(qname, "protocol-message-2"))		qname = "protocol-message";
	else if (!xstrcmp(qname, "protocol-message-post-2"))	qname = "protocol-message-post";
	else if (!xstrcmp(qname, "protocol-message-received-2"))qname = "protocol-message-received";

/* IRC */
	if (!xstrncmp(qname, "irc-protocol-numeric", sizeof("irc-protocol-numeric")-1)) {
		/* XXX, obciaz nazwe do irc-protocl-numeric i wrzucic to ponizej do queries.h */
		NEXT_ARG(QUERY_ARG_CHARP);
		NEXT_ARG(QUERY_ARG_CHARPP);
	}
/* other */
	else {
		int i;
		for (i = 0; i < QUERY_EXTERNAL; i++) {
			if (!xstrcmp(qname, (query_name(i)))) {
				const struct query_def *q = query_struct(i);
				int j = 0;

				while (j < QUERY_ARGS_MAX && q->params[j] != QUERY_ARG_END) {
					NEXT_ARG(q->params[j++]);
				}

				break;
			}
		}
	}
#undef NEXT_ARG
	temp->self = query_connect(s->plugin, qname, script_query_handlers, temp);
	SCRIPT_BIND_FOOTER(script_queries);
}

/*****************************************************************************************************************/

static void script_var_changed(const char *var) {
	script_var_t     *temp = script_var_find(var);
/*	if (in_autoexec) ... */
	SCRIPT_HANDLER_HEADER(script_handler_var_t);
/*	debug("[script_variable_changed] varname = %s newvalue = %s\n", var, temp->value); */
	SCRIPT_HANDLER_MULTI_FOOTER(script_handler_var, temp->value);
	return;
}

static WATCHER(script_handle_watch)
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

static COMMAND(script_command_handlers)
{
	script_command_t *temp = script_command_find(name);

	SCRIPT_HANDLER_HEADER(script_handler_command_t);
	SCRIPT_HANDLER_MULTI_FOOTER(script_handler_command, (char **) params) {
		script_command_unbind(temp, 1);
	}
	return ret;
}

static int script_plugin_theme_init( /* plugin_t *p */ ) 
{
/* TODO: it will be slow! foreach scriptplugin call format initializer.  (?) */
	return 0;
}

static TIMER(script_timer_handlers) {
	script_timer_t *temp = data;
	SCRIPT_HANDLER_HEADER(script_handler_timer_t);
	SCRIPT_HANDLER_FOOTER(script_handler_timer, type) {
		if (!type) {
			return -1; /* timer_free(temp->self); */
		}
	}
	if (type)
		script_timer_unbind(temp, 0);
	return 0;
}

static QUERY(script_query_handlers)
{
	script_query_t	*temp = data;
	void 		*args[MAX_ARGS];
	void		*args2[MAX_ARGS];
	int		i;
	script_query_t saved;
	char *status = NULL;			/* for temp->hack == 2 */
	int ign_level = 0;

	SCRIPT_HANDLER_HEADER(script_handler_query_t);

	for (i=0; i < temp->argc; i++) 
		args2[i] = args[i] = (void *) va_arg(ap, void *);

	if (temp->hack)
		memcpy(&saved, temp, sizeof(script_query_t));

	switch (temp->hack) {
		case 0:	break;			/* without hack, thats gr8! */

		case 1:				/* scripts protocol-disconnected (v 1.0) 
							- takes only (reason) */
			temp->argv_type[0] = QUERY_ARG_CHARP;	/* OK */
			temp->argc = 1;
			break;
		case 2:				/* scripts protocol-status (v 1.0) 
							- takes (session, uid, status, descr) 
							- takes char *status, instead of int status */
			{
				temp->argc = 4;
				temp->argv_type[0] = QUERY_ARG_CHARP;	/* OK */
				temp->argv_type[1] = QUERY_ARG_CHARP;	/* OK */

				temp->argv_type[2] = QUERY_ARG_CHARP;	/* status: int -> char * */
				status = xstrdup(ekg_status_string(*((int *) args2[2]), 0));	/* status, int -> char * */
				args[2] = &status;

				temp->argv_type[3] = QUERY_ARG_CHARP;	/* OK */
				temp->argv_type[4] = QUERY_ARG_CHARP;	/* OK */

				break;
			}
		case 3:
		case 4:
		case 5:				/* scripts protocol-message, protocol-message-post, protocol-message-received (v 1.0) 
							- ts (session, uid, class, text, sent_time, ignore_level)
							- vs (session, uid, rcpts, text, format, sent, class, seq, secure) [protocol-message-post, protocol-message-recv]
							- vs (session, uid, rcpts, text, format, sent, class, seq, dobeep, secure) [protocol-message]
						 */
			{
				temp->argc = 6;

				temp->argv_type[0] = QUERY_ARG_CHARP;	/* session, OK */
				temp->argv_type[1] = QUERY_ARG_CHARP;	/* uid, OK */

				temp->argv_type[2] = QUERY_ARG_INT;	/* class, N_OK, BAD POS */
				args[2] = args2[6];

				temp->argv_type[3] = QUERY_ARG_CHARP;	/* text, OK */

				temp->argv_type[4] = QUERY_ARG_INT;	/* sent_time, N_OK, BAD POS */
				args[4] = args2[5];

				temp->argv_type[5] = QUERY_ARG_INT;	/* ignore_level, N_OK, DONTEXISTS */
				/* XXX, find ign_level */
				args[5] = &ign_level;

				break;
			}

		default:
			debug("script_query_handlers() unk temp->hack: %d assuming 0.\n", temp->hack);
			break;
	}

	SCRIPT_HANDLER_FOOTER(script_handler_query, (void **) &args);

	if (temp->hack) {
		memcpy(temp, &saved, sizeof(script_query_t));

		switch (temp->hack) {
			case 2:
				/* XXX, status CHANGED BY SCRIPT !!! args2[i] <==> args[i] */
				xfree(status);
				break;
			case 3:
			case 4:
			case 5:
				/* XXX, ignore level changed by script !!! */
				break;
		}
	}

	return ret;
}

/********************************************************************************/

/* from python.c  python_autorun() 
 *  load  scripts from `path`
 */

static int scripts_loaddir(scriptlang_t *s, const char *path)
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
	scriptlang_t *s = NULL;
	char 	     *tmp = NULL;
	char	     *param0 = NULL;

	if (xstrcmp(name, ("script"))) { /* script:*    */
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

static int scripts_autoload(scriptlang_t *scr)
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
