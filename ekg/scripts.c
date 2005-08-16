#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>


#include <ekg/windows.h>
#include <ekg/scripts.h>
#include <ekg/xmalloc.h>

COMMAND(script_command_handlers);

/* scriptlang main func */
        
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

/* scripts func */

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
	scriptlang_t *slang = scr->lang;
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
	list_t prev = scripts;	
	for (l = scripts; l;) {
		scr = l->data;
		lang = scr->lang;
		if (!s || scr->lang == s) {
			script_unload(scr);
			l = prev;
		}
		prev = l;
		l = l->next;
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
		scr->mask = slang->script_load(scr);

		debug("[script] sc_mask == %d\n", scr->mask);
		if (scr->mask < 1) {
			if (scr->mask == -1)
				print("generic_error", "@script_load error");
			else if (scr->mask == 0)
				print("generic_error", "@script_load script has no handler or error in getting handlers.");
			
			xfree(scr->path);
			xfree(scr->name);
			xfree(scr);
			
			xfree(path);
			list_remove(&scripts, scr, 0);
			
			return -1;
		
		}
		print("script_loaded", scr->name, scr->path, slang->name);
		
	
	}
	else print("script_not_found", name);
	
	xfree(path);
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

int script_command_unbind(script_command_t *scr_comm, int free)
{
	int notfound = 1;
// TODO: powinnismy sprawdzic czy to polecenie wystepuje w innych handlerach.., jedno polecenie bedzie moze byc podbindowane pod wiele handlerow !
	
	if (notfound)
		command_remove(NULL, scr_comm->comm);

	if (free) {
		xfree(scr_comm->private);
	}
	xfree(scr_comm->comm);
	xfree(scr_comm);
	
	
	list_remove(&script_commands_bindings, scr_comm, 0);
	return notfound;
	
}

int script_command_bind(scriptlang_t *s, script_t *scr, char *command, void *handler) 
{
	script_command_t *scrcomm;
	debug("[script] (bind command) 0x%x 0x%x %s 0x%x\n", s, scr, command, handler);
// TODO: sprawdzic czy wartosci sa ok.

	scrcomm = xmalloc(sizeof(script_command_t));
	
	scrcomm->scr = scr;
	scrcomm->comm = command;
	scrcomm->private = handler;
	
	list_add(&script_commands_bindings, scrcomm, 0);
	
	command_add(NULL, command, "?", script_command_handlers, 0, NULL);
	return 0;
}
/* HANDLERS */

COMMAND(script_command_handlers)
{
	script_command_t *scr_comm;
	script_t 	 *scr;
	scriptlang_t 	 *slang;
	list_t l;
	int i = 0;
	
	int ret = 0;
	
	debug("[script_command_handlers] %s %s\n", name, params[0]);
	// w name
	for (l = script_commands_bindings; l; l = l->next) {
		scr_comm = l->data;
		
		if (!xstrcmp(name, scr_comm->comm)) {
			debug("[script_command_handlers] found handler for %s\n", name);
			scr	 = scr_comm->scr;
			slang	 = scr->lang;
			
			ret = slang->script_handler_command(scr, scr_comm, (char **) params);
			if (ret == SCRIPT_COMMAND_DELETE) {
				debug("[script_command_handlers] script or scriptlang want to delete this handler\n");
				if (script_command_unbind(scr_comm, 1))
					return ret;
				return 0; 
// TODO: powinnismy leciec z lista dalej....
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

int script_protocol_message(void *data, va_list ap)
{
        char **__session = va_arg(ap, char**);
        char **__uid = va_arg(ap, char**);
        char ***__rcpts = va_arg(ap, char***);
        char **__text = va_arg(ap, char**);
        uint32_t **__format = va_arg(ap, uint32_t**);
        time_t *__sent = va_arg(ap, time_t*);
        int *__class = va_arg(ap, int*);
	
	SCRIPT_HANDLER(message, scr->mask & HAVE_HANDLE_MSG || scr->mask & HAVE_HANDLE_MSG_OWN, __session, __uid, __rcpts, __text, __format, __sent, __class);
}


int script_protocol_status(void *data, va_list ap)
{
	char **__session = va_arg(ap, char**);
	char **__uid = va_arg(ap, char**);
	char **__status = va_arg(ap, char**);
	char **__descr = va_arg(ap, char**);
	
	SCRIPT_HANDLER(status, scr->mask & HAVE_HANDLE_STATUS || scr->mask & HAVE_HANDLE_STATUS_OWN,  __session, __uid, __status, __descr);
}

int script_protocol_connected(void *data, va_list ap)
{
	char **__session = va_arg(ap, char**);
	
	SCRIPT_HANDLER(connect, scr->mask & HAVE_HANDLE_CONNECT, __session);
}

int script_keypressed(void *data, va_list ap)
{
	int *_ch = va_arg(ap, int*);
	
	SCRIPT_HANDLER(keypress, scr->mask & HAVE_HANDLE_KEYPRESS, _ch);
}


int script_protocol_disconnected(void *data, va_list ap)
{
	char **__session = va_arg(ap, char**);
	
	SCRIPT_HANDLER(disconnect, scr->mask & HAVE_HANDLE_DISCONNECT, __session);
}


/* scripts main func */


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
//		debug("[script] autoloading %s from %s\n", d->d_name, path);
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


int scripts_init()
{
	char *pathtmp;
	
        query_connect(NULL, "protocol-message",		script_protocol_message,      NULL);
        query_connect(NULL, "protocol-status",		script_protocol_status,       NULL);
        query_connect(NULL, "protocol-connected",	script_protocol_connected,    NULL);
        query_connect(NULL, "protocol-disconnected",	script_protocol_disconnected, NULL);
#ifdef HAVE_NCURSES
	query_connect(NULL, "ui-keypress",		script_keypressed,	      NULL); 
#endif
	
	pathtmp = saprintf("%s/autorun", DATADIR);
	scripts_autoload(pathtmp);
	xfree(pathtmp);
	
	pathtmp = saprintf("%s/autorun", prepare_path("scripts", 0));
	scripts_autoload(pathtmp);
	xfree(pathtmp);
	
	command_add(NULL, "testdddd", "?", script_command_handlers, 0, NULL);
	
	return 0;

}

