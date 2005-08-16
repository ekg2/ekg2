#include <ekg/plugins.h>
#include <ekg/protocol.h>

#include <sys/types.h>

#ifndef EKG_SCRIPTS_H
#define EKG_SCRIPTS_H

#define SCRIPT_COMMAND_DELETE	-666

/*      INTERNAME NAME          BITS    sub ekg2_$name in perl script */

#define HAVE_HANDLE_MSG         0x01    /* handle_msg */
#define HAVE_HANDLE_MSG_OWN     0x02    /* handle_msg_own */
#define HAVE_HANDLE_STATUS      0x04    /* handle_status */
#define HAVE_HANDLE_STATUS_OWN  0x08    /* handle_status_own */
#define HAVE_HANDLE_CONNECT     0x10    /* handle_connect */
#define HAVE_HANDLE_DISCONNECT  0x20    /* handle_disconnect */
#define HAVE_HANDLE_KEYPRESS    0x40    /* handle_keypress */
#define HAVE_INIT               0x80    /* init */
#define HAVE_DEINIT             0x100   /* deinit */
#define HAVE_ALL                HAVE_DEINIT | HAVE_INIT | HAVE_HANDLE_KEYPRESS | HAVE_HANDLE_DISCONNECT | HAVE_HANDLE_CONNECT | HAVE_HANDLE_STATUS_OWN | HAVE_HANDLE_STATUS | HAVE_HANDLE_MSG_OWN | HAVE_HANDLE_MSG


list_t scriptlang;
list_t scripts;
list_t script_commands_bindings; /* too long */


typedef struct {
	void *lang;
	char *name;
	char *path;
	void *private;
	
	int mask;
	
} script_t;

typedef struct {
	script_t *scr; /* pointer to script struct */
	char *comm;    /* for instance /np */
	void *private; /* w perlu jest to char *, w pythonie ( !PyObject-= ; char *) .... */
} script_command_t;


typedef int (scriptlang_initialize_t)();
typedef int (scriptlang_finalize_t)();

typedef int (script_handler_connected_t)(script_t *, char **);
typedef int (script_handler_disconnected_t)(script_t *, char **);
typedef int (script_handler_message_t)(script_t *, char **, char **, char ***, char **, uint32_t **, time_t *, int  *);
typedef int (script_handler_status_t)(script_t *, char **, char **, char **, char **);
typedef int (script_handler_keypressed_t)(script_t *, int *);

typedef int (script_load_t)(script_t *);
typedef int (script_unload_t)(script_t *);
typedef int (script_handler_command_t)(script_t *, script_command_t *, char **);

typedef struct {
	char *name;     // perl, python, php *g* and so on.
	char *ext; 	//  .pl,    .py, .php ...
	int  prio;
	
	plugin_t *plug;
	
	script_load_t *script_load;
	script_unload_t *script_unload;
	
	scriptlang_initialize_t *init;
	scriptlang_finalize_t   *deinit;
	
	script_handler_status_t		*script_handler_status;
	script_handler_message_t	*script_handler_message;
	script_handler_disconnected_t	*script_handler_disconnect;
	script_handler_connected_t	*script_handler_connect;
	script_handler_keypressed_t     *script_handler_keypress;
	script_handler_command_t        *script_handler_command;
	
	void *private;
} scriptlang_t;

// #define DEBUG(args...) debug(args); /* very verbose; szczegolnie keypress... */
#define DEBUG(args...) ;


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
		

#define SCRIPT_HANDLER(what, bool, args...)\
	list_t l;\
	script_t *scr;\
	scriptlang_t *s;\
	DEBUG("[script] handling %s\n", #what);\
	for (l = scripts; l; l = l->next) { \
		scr = l->data;\
		s = scr->lang;\
		if (bool) {\
			DEBUG("running %s script file %s\n", s->name, scr->path);\
			s->script_handler_##what(scr, args);\
		}\
	}\
	return 0;
		

#define SCRIPT_DEFINE(x, y)\
	extern int x##_load(script_t *);\
	extern int x##_unload(script_t *);\
	extern int x##_protocol_disconnected(script_t *, char **);\
	extern int x##_protocol_connected(script_t *, char **);\
	extern int x##_protocol_status(script_t *, char **, char **, char **, char **);\
	extern int x##_protocol_message(script_t *, char **, char **, char ***, char **, uint32_t **, time_t *, int  *);\
	extern int x##_keypressed(script_t *, int *);\
	extern int x##_commands(script_t *, script_command_t *, char **);\
        scriptlang_t x##_lang = { \
                name: #x, \
		plug: &x##_plugin, \
                ext: y, \
\
		init: x##_initialize,\
		deinit: x##_finalize, \
\
                script_load: x##_load, \
		script_unload: x##_unload, \
\
		script_handler_keypress: x##_keypressed,\
		script_handler_connect: x##_protocol_connected,\
		script_handler_disconnect: x##_protocol_disconnected,\
		script_handler_status: x##_protocol_status,\
		script_handler_message: x##_protocol_message,\
		\
		script_handler_command: x##_commands,\
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

int script_command_bind(scriptlang_t *s, script_t *scr, char *command, void *handler);

#endif
