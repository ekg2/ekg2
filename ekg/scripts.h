#ifndef EKG_SCRIPTS_H
#define EKG_SCRIPTS_H

#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/stuff.h>
#include <ekg/vars.h>
#include <sys/types.h>

#define SCRIPT_HANDLE_UNBIND    -666

#define SCRIPT_BIND_TIMER 1
#define SCRIPT_BIND_COMMAND 2
#define SCRIPT_BIND_VARIABLE 3
#define SCRIPT_BIND_QUERY 4

// #define SCR_ARG_CHAR    1
#define SCR_ARG_CHARP   2 
#define SCR_ARG_CHARPP  3 
#define SCR_ARG_INT     4

#define MAX_ARGS 20

// #define DEBUG(args...) debug(args); /* very verbose; szczegolnie keypress... */
#define DEBUG(args...) ;

typedef struct {
	void *lang;
	char *name;
	char *path;
	void *private;
} script_t;
list_t scripts;

typedef struct {
	script_t 	*scr;
	struct timer 	*self;
	char 		*name;
	int 		removed;
	void 		*private;
} script_timer_t; 
list_t script_timers;

typedef struct {
	script_t 	*scr;
	variable_t 	*var;
	char 		*name;
	char 		*value;
	void 		*private;
} script_var_t; 
list_t script_vars;
 
typedef struct {
	script_t 	*scr;
	char 		*query_name;
	int 		argc;
	int             argv_type[MAX_ARGS];

	void 		*private;
} script_query_t; 
list_t script_queries;

typedef struct {
	script_t *scr; /* pointer to script struct */
	char *comm;    /* for instance /np */

	void *private; /* w perlu jest to char *, w pythonie ( !PyObject-= ; char *) .... */
} script_command_t; /* multi */
list_t script_commands_bindings; /* too long */

typedef struct {
	script_t *scr;
	void *watch; /* watch_t chyba */
	void *data;
	
	void *private;
} script_watch_t;
list_t script_watches;

typedef int (scriptlang_initialize_t)();
typedef int (scriptlang_finalize_t)();
typedef int (script_load_t)(script_t *);
typedef int (script_unload_t)(script_t *);
typedef int (script_handler_command_t)(script_t *, script_command_t *, char **);
typedef int (script_handler_timer_t)  (script_t *, script_timer_t *, int);
typedef int (script_handler_var_t)    (script_t *, script_var_t *,   char *);
typedef int (script_handler_query_t)  (script_t *, script_query_t *, void **);
typedef int (script_handler_watch_t)  (script_t *, script_watch_t *, int, int, int);

typedef struct {
	char *name;     // perl, python, php *g* and so on.
	char *ext; 	//  .pl,    .py, .php ...
	int  prio;
	plugin_t *plugin;

	scriptlang_initialize_t *init;
	scriptlang_finalize_t   *deinit;

	script_load_t *script_load;
	script_unload_t *script_unload;

	script_handler_query_t		*script_handler_query;
	script_handler_command_t        *script_handler_command;
	script_handler_timer_t 		*script_handler_timer;
	script_handler_var_t		*script_handler_var;
	script_handler_watch_t		*script_handler_watch;
	
	void *private;
} scriptlang_t;
list_t scriptlang;

#define SCRIPT_FINDER(bool)\
	script_t *scr = NULL;\
	scriptlang_t *slang = NULL;\
	list_t l;\
	\
	for (l = scripts; l; l = l->next) {\
		scr = l->data;\
		slang = scr->lang;\
		if (bool)\
			return scr;\
	}\
	return NULL;

#define SCRIPT_DEFINE(x, y)\
	extern int x##_load(script_t *);\
	extern int x##_unload(script_t *);\
	\
	extern int x##_commands(script_t *, script_command_t *, char **);\
	extern int x##_timers(script_t *, script_timer_t *, int );\
	extern int x##_variable_changed(script_t *, script_var_t *, char *);\
	extern int x##_query(script_t *, script_query_t *, void **);\
	extern int x##_watches(script_t *, script_watch_t *, int, int, int);\
	\
        scriptlang_t x##_lang = { \
                name: #x, \
		plugin: &x##_plugin, \
                ext: y, \
\
		init: x##_initialize,\
		deinit: x##_finalize, \
\
                script_load: x##_load, \
		script_unload: x##_unload, \
\
		script_handler_query  : x##_query,\
		script_handler_command: x##_commands,\
		script_handler_timer  : x##_timers,\
		script_handler_var    : x##_variable_changed,\
		script_handler_watch  : x##_watches,\
        }

#define script_private_get(s) (s->private)
#define script_private_set(s, p) (s->private = p)

int script_unload_lang(scriptlang_t *s);

int script_list(scriptlang_t *s);
int script_unload_name(scriptlang_t *s, char *name);
int script_load(scriptlang_t *s, char *name);

int scriptlang_register(scriptlang_t *s, int prio);
int scriptlang_unregister(scriptlang_t *s);

int scripts_init();

script_t *script_find(scriptlang_t *s, char *name);

int script_query_unbind(script_query_t *squery, int from);
int script_command_unbind(script_command_t *scr_comm, int free);
int script_timer_unbind(script_timer_t *stimer, int free);
int script_var_unbind(script_var_t *data, int free);

script_command_t *script_command_bind(scriptlang_t *s, script_t *scr, char *command, void *handler);
script_timer_t *script_timer_bind(scriptlang_t *s, script_t *scr, int freq, void *handler);
script_query_t *script_query_bind(scriptlang_t *s, script_t *scr, char *query_name, void *handler);
script_var_t *script_var_add(scriptlang_t *s, script_t *scr, char *name, char *value, void *handler);
script_watch_t *script_watch_add(scriptlang_t *s, script_t *scr, int fd, int type, int persist, void *handler, void *data);

int script_variables_free(int free);
int script_variables_write();
#endif
/* BINDING && UNBINDING */

#define SCRIPT_UNBIND_HEADER(x)\

#define SCRIPT_BIND_HEADER(x) \
        x *temp = xmalloc(sizeof(x)); \
	\
	debug("[script_bind] (struct %s) mem = 0x%x scr = 0x%x priv = 0x%x\n", #x, temp, scr, handler);\
        temp->scr  = scr;\
/*	temp->name = name; */\
	temp->private = handler;

#define SCRIPT_BIND_FOOTER(y) \
	list_add(&y, temp, 0);\
	return temp;

/* HANDLERS */
#define SCRIPT_HANDLER_HEADER(x) \
	script_t        *_scr;\
	scriptlang_t    *_slang;\
	x		*_handler;\
	int 		ret = SCRIPT_HANDLE_UNBIND;

#define SCRIPT_HANDLER_FOOTER(y, _args...) \
	if ((_scr = temp->scr) && ((_slang = _scr->lang)))  _handler = _slang->y;\
        else                                                _handler = temp->private;\
        if (_handler)\
                ret = _handler(_scr, temp, _args); \
        else {\
                debug("[%s] (_handler == NULL)\n", #y);\
                ret = SCRIPT_HANDLE_UNBIND;\
        }\
	\
	if (ret == SCRIPT_HANDLE_UNBIND) { debug("[%s] script or scriptlang want to delete this handler\n", #y); }\
	if (ret == SCRIPT_HANDLE_UNBIND)

#define SCRIPT_HANDLER_MULTI_FOOTER(y, _args...)\
/* tutaj jakis cos moze, lista list_t * ? i potem jechanie po kolei ? albo arrayik ? */\
	if ((_scr = temp->scr) && ((_slang = _scr->lang)))  _handler = _slang->y;\
        else                                                _handler = temp->private;\
        if (_handler)\
                ret = _handler(_scr, temp, _args); \
        else {\
                debug("[%s] (_handler == NULL)\n", #y);\
                ret = SCRIPT_HANDLE_UNBIND;\
        }\
	\
	if (ret == SCRIPT_HANDLE_UNBIND) { debug("[%s] script or scriptlang want to delete this handler\n", #y); }\
	if (ret == SCRIPT_HANDLE_UNBIND)
