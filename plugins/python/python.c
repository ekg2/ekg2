/* $Id$ */

/*
 *  (C) Copyright 2004-2005 Leszek Krupiñski <leafnode@pld-linux.org>
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

#include "python.h"
#include "python-ekg.h"
#include "python-config.h"

#include <sys/types.h>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <Python.h>
#include <compile.h> 
#include <node.h>

#include <ekg/commands.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include <ekg/scripts.h>

int python_theme_init();
int python_exec(const char *);
int python_run(const char *filename);
int python_load(script_t *s);
int python_unload(script_t *s);
PyObject *python_get_func(PyObject *module, const char *name);

/**
 * python_plugin
 *
 * plugin definition
 */

PLUGIN_DEFINE(python, PLUGIN_SCRIPTING, python_theme_init);
SCRIPT_DEFINE(python, ".py");

// * ***************************************************************************
// *
// * Polecenia EKG
// *
// * ***************************************************************************

/**
 * python_command_eval()
 *
 * execute python code
 *
 */

COMMAND(python_command_eval)
{
	if (!params[0]) {
		print("not_enough_params", name);
		return -1;
	}
	python_exec(params[0]);
	return 0;
}

/**
 * python_command_run()
 *
 * run single python script
 *
 */

COMMAND(python_command_run)
{
	if (!params[0]) {
		print("not_enough_params", name);
		return -1;
	}
	python_run(params[0]);
	return 0;
}

/**
 * python_command_load()
 *
 * load python script
 *
 */

COMMAND(python_command_load)
{
	if (!params[0]) {
		print("not_enough_params", name);
		return -1;
	}
	script_load(&python_lang, (char *) params[0]);
	return 0;
}

/**
 * python_command_unload()
 *
 * unload python script
 *
 */

COMMAND(python_command_unload)
{
	script_unload_name(&python_lang, (char *) params[0]);
	return 0;
}

/**
 * python_command_list()
 *
 * list loaded python scripts
 *
 */

COMMAND(python_command_list)
{
	script_list(&python_lang);
	return 0;
}

// * ***************************************************************************
// *
// * Hooki
// *
// * ***************************************************************************

QUERY(python_print_version) 
{
	print("generic", "Python plugin for ekg2 running under Python " PY_VERSION);
	return 0;
}

int python_bind_free(script_t *scr, void *data /* niby to jest ale kiedys nie bedzie.. nie uzywac */, int type, void *private, ...)
{
	PyObject *handler = private;
	switch (type) {
		case(SCRIPT_QUERYTYPE):
		case(SCRIPT_COMMANDTYPE):
		case(SCRIPT_TIMERTYPE):
		    Py_XDECREF(handler);
		    break;
		case(SCRIPT_WATCHTYPE):
		case(SCRIPT_VARTYPE):
                    xfree((char *)private);
		    break;
	}
	return 0;
}

int python_variable_changed(script_t *scr, script_var_t *scr_var, char *newval)
{
	return 0;
}
int python_watches(script_t *scr, script_watch_t *scr_wat, int type, int fd, int watch)
{
	return 0;
}

int python_timers(script_t *scr, script_timer_t *time, int type)
{
	int python_handle_result;
	PyObject *obj = (PyObject *)time->private;

        if (!PyCallable_Check(obj)) {
		debug("[python] func not found, deleting timer\n");
		return SCRIPT_HANDLE_UNBIND;
	}
	PYTHON_HANDLE_HEADER(obj, "()");
	PYTHON_HANDLE_FOOTER(0);
}

int python_commands(script_t *scr, script_command_t *comm, char **params)
{
	int python_handle_result;
	PyObject *obj = (PyObject *)comm->private;

        if (!PyCallable_Check(obj)) {
		debug("[python] func %s not found, deleting comm\n", comm->comm);
		return SCRIPT_HANDLE_UNBIND;
	}
        PYTHON_HANDLE_HEADER(obj, "(ss)", comm->comm,
//				    	params)
					params[0] ? params[0] : "")
	;
	PYTHON_HANDLE_FOOTER(0)
	return python_handle_result;
}


int python_keypressed(PyObject *obj, script_t *s, int ch)
{
	int python_handle_result;
	PYTHON_HANDLE_HEADER(obj, "(i)", ch);
	PYTHON_HANDLE_FOOTER(0);
}

/**
 * python_protocol_status()
 *
 * handle status events
 *
 */

int python_protocol_status(PyObject *obj, script_t *s, char *session, char *uid, char *status, char *descr)
{
	int python_handle_result;
	PYTHON_HANDLE_HEADER(obj, "(ssss)", session, uid, status, descr);
	PYTHON_HANDLE_FOOTER(0);
}

/**
 * python_protocol_message()
 *
 * handle message events
 *
 */

int python_protocol_message(PyObject *obj, script_t *scr, char *session, char *uid, char **rcpts, char *text, uint32_t *format, time_t sent, int class)
{
        int level;
	char * target;
	userlist_t *u;
	session_t *s;
	int python_handle_result;
	// silence warning
	format = NULL;

	if (!(s = session_find(session)))
		return 0;

	u = userlist_find(s, uid);

	level = ignored_check(s, uid);

	if ((level == IGNORE_ALL) || (level & IGNORE_MSG))
		return 0;

	if (class == EKG_MSGCLASS_SENT || class == EKG_MSGCLASS_SENT_CHAT) {
		target = (rcpts) ? rcpts[0] : NULL;
/* zrobic inaczej... */
		obj = python_get_func(python_module(scr), "handle_msg_own"); 
		PYTHON_HANDLE_HEADER(obj, "(sss)", session, target, text);
		PYTHON_HANDLE_FOOTER(obj);
	} else {
		PYTHON_HANDLE_HEADER(obj, "(ssisii)", session, uid, class, text, (int) sent, level);
		PYTHON_HANDLE_FOOTER(0);
	}
}

/**
 * python_protocol_connected()
 *
 * handle connect events
 *
 */

int python_protocol_connected(PyObject *obj, script_t *s, char *session)
{
	int python_handle_result;
	PYTHON_HANDLE_HEADER(obj, "(s)", session);
	PYTHON_HANDLE_FOOTER(0)
}

/**
 * python_protocol_disconnected()
 *
 * handle disconnect events
 *
 */

int python_protocol_disconnected(PyObject *obj, script_t *s, char *session)
{
	int python_handle_result;
	PYTHON_HANDLE_HEADER(obj, "(s)", session);
	PYTHON_HANDLE_FOOTER(0);
}

int python_query(script_t *scr, script_query_t *scr_que, void **args)
{
/* @ scr_que->private handler of function */
#define ARG_INT(x)	(int) args[x]
#define ARG_INTP(x)     *(int  *) args[x]
#define ARG_TIMEP(x)    *(time_t *) args[x]
#define ARG_CHARPP(x)   *(char **) args[x]
#define ARG_CHARPPP(x)  *(char ***) args[x]
#define ARG_UINTPPP(x)  *(uint32_t **) args[x]

        char *name = scr_que->query_name;

	if (!xstrcmp(name, "protocol-message")) return python_protocol_message(scr_que->private, scr, ARG_CHARPP(0), ARG_CHARPP(1), ARG_CHARPPP(2) , ARG_CHARPP(3), ARG_UINTPPP(4), ARG_TIMEP(5), ARG_INT(6));
#if 1
        else if (!xstrcmp(name, "protocol-disconnected")) return python_protocol_disconnected(scr_que->private, scr, ARG_CHARPP(0));
        else if (!xstrcmp(name, "protocol-connected"))  return python_protocol_connected(scr_que->private, scr, ARG_CHARPP(0));
        else if (!xstrcmp(name, "protocol-status"))     return python_protocol_status(scr_que->private, scr, ARG_CHARPP(0), ARG_CHARPP(1), ARG_CHARPP(2), ARG_CHARPP(3));
        else if (!xstrcmp(name, "ui-keypress"))         return python_keypressed(scr_que->private, scr, ARG_INTP(0));
        else return -1;
#else
	else {
/* i have no idea how to do it, sorry. */
		int i;
		int python_handle_result;
		string_t st = string_init("(");
	        for (i=0; i < scr_que->argc; i++) {
        	        switch ( scr_que->argv_type[i] ) {
                	        case (SCR_ARG_INT):
					string_append_c(st, 'i');
                        	        break;
	                        case (SCR_ARG_CHARP):
					string_append_c(st, 's');
    	        	                break;
/*
        	                case (SCR_ARG_CHARPP):
					break;
*/
                        default:
                                debug("[NIMP] %s %d %d\n",scr_que->query_name, i, scr_que->argv_type[i]);
            	    }
		
		}
		string_append_c(st, ')');
		PYTHON_HANDLE_HEADER(scr_que->private, st->str, /* ??? */);
		string_free(st, 1);
    	}
#endif
}

// ********************************************************************************
// *
// * Funkcje pomocnicze
// *
// ********************************************************************************

/**
 * python_exec()
 *
 * run python code
 *
 *  - command - code to run
 *
 */

int python_exec(const char *command)
{
	debug("[python] Running command: %s\n", command);
	char *tmp;

	if (!command)
		return 0;

	tmp = saprintf("import ekg\n%s\n", command);

	if (PyRun_SimpleString(tmp) == -1) {
		print("script_eval_error");
		debug("[python] script evaluation failed\n");
	}
	xfree(tmp);

	return 0;
}

/**
 * python_run()
 *
 * run python script from file
 *
 * - filename - path to file to run
 *
 */

int python_run(const char *filename)
{
	FILE *f = fopen(filename, "r");

	if (!f) {
		print("script_not_found", filename);
		return -1;
	}

	PyRun_SimpleFile(f, (char*) filename);
	fclose(f);

	return 0;
}

/*
 * python_get_func()
 *
 * return function from module
 *
 */

PyObject *python_get_func(PyObject *module, const char *name)
{
	PyObject *result = PyObject_GetAttrString(module, (char*) name);

	if (result && !PyCallable_Check(result)) {
		Py_XDECREF(result);
		result = NULL;
	}

	return result;
}

int python_check_func(PyObject *module, const char *name)
{
	PyObject *result = python_get_func(module, name);
	if (result) {
		Py_XDECREF(result);
		return 1;
	}
	else
		return 0;
}


script_t *python_find_script(PyObject *module)
{
	SCRIPT_FINDER(slang == &python_lang && !xstrcmp(scr->name, PyString_AsString(module)));
}
/* returns somethink like it after formatink.
   20:47:56 ::: B³±d EOL while scanning single-quoted string @ /home/darkjames/.ekg2/python/scripts/autorun/sample.py:98
 */
char *python_geterror(script_t *s) {
	PyObject *exception, *v, *tb, *hook;
	string_t str = string_init(NULL);
/* TODO: check if we have really exception from python */
        PyErr_Fetch(&exception, &v, &tb);
	PyErr_NormalizeException(&exception, &v, &tb);
/*	PyErr_Display(exception, v, tb); */

	hook = PyObject_GetAttrString(v, "msg");
	string_append(str, PyString_AsString(hook));
	Py_DECREF(hook);

	string_append(str, " @ ");
	string_append(str, s->path);
	string_append_c(str, ':');

	hook = PyObject_GetAttrString(v, "lineno");
	string_append(str, itoa(PyInt_AsLong(hook)));
	Py_DECREF(hook);

	Py_DECREF(v);
	
	PyErr_Clear();
	return string_free(str, 0);
}

/*
 * python_load()
 *
 * load script with given details
 *
 *  - s - script_t * struct
 *
 */
int python_load(script_t *s)
{
	PyObject *init, *temp, *module = NULL;
	FILE *fp = fopen(s->path, "rb"); 
        node *n;
        if ((n = PyParser_SimpleParseFile(fp, s->path, Py_file_input))) {
		PyCodeObject *co = PyNode_CompileFlags(n, s->path, NULL);
	        PyNode_Free(n);
		if (co)
			module = PyImport_ExecCodeModuleEx(s->name, (PyObject *)co, s->path); 
	}
	fclose(fp);
	if (!module) {
		char *err = python_geterror(s);
		print("script_error", err);
		xfree(err);
		return 0;
	}
	debug("[python script loading] 0x%x\n", module);
	if ((init = PyObject_GetAttrString(module, "init"))) {
		if (PyCallable_Check(init)) {
			PyObject *result = PyObject_CallFunction(init, "()");

			if (result) {
				int resulti = PyInt_AsLong(result);

				if (!resulti) {

				}

				Py_XDECREF(result);
			}
		}

		Py_XDECREF(init);
	}
	script_private_set(s, module);
/* MSG */
	if ((temp = python_get_func(module, "handle_msg")))
		script_query_bind(&python_lang, s, "protocol-message", temp);
	else if (python_check_func(module, "handle_msg_own"))
		script_query_bind(&python_lang, s, "protocol-message", NULL);
/* STATUS */		
	if ((temp = python_get_func(module, "handle_status")))
		script_query_bind(&python_lang, s, "protocol-status", temp); 
/* CONNECT */		
	if ((temp = python_get_func(module, "handle_connect")))
		script_query_bind(&python_lang, s, "protocol-connected", temp);
/* DISCONNECT */		
	if ((temp = python_get_func(module, "handle_disconnect")))
		script_query_bind(&python_lang, s, "protocol-disconnected", temp);
/* KEYPRESS */		
	if ((temp = python_get_func(module, "handle_keypress")))
		script_query_bind(&python_lang, s, "ui-keypress", temp);

	PyErr_Clear();
	return 1;
}

/*
 * python_unload()
 *
 * remove script from memory
 *
 * 0/-1
 */
int python_unload(script_t *s)
{
	PyObject         *module = python_module(s);
	PyObject	 *obj;
        if (!module)
                return -1;
#if 0
	if ((obj = python_get_func(module, "deinit"))) {

		PyObject *res = PyObject_CallFunction(obj, "()");
		Py_XDECREF(res);
		Py_XDECREF(obj);
	}
Breakpoint 2, python_finalize () at python.c:632
632             Py_Finalize();
(gdb) step

Program received signal SIGABRT, Aborted.
0xb7e08921 in kill () from /lib/libc.so.6
(gdb)

without that works ? wtf ?!
#endif

	Py_XDECREF(module);
	script_private_set(s, NULL);
	return 0;
}

// ********************************************************************************
// *
// * Interpreter related functions
// *
// ********************************************************************************

/**
 * python_initialize()
 *
 * initialize interpreter
 *
 */

int python_initialize()
{
	PyObject *ekg, *ekg_config;
	/* new way of script loading doesn't require this code. */

	Py_Initialize();

	PyImport_AddModule("ekg");
	if (!(ekg = Py_InitModule("ekg", ekg_methods)))
		return -1;

        ekg_config = PyObject_NEW(PyObject, &ekg_config_type);
	PyModule_AddObject(ekg, "config", ekg_config);

	// Const - general

	PyModule_AddStringConstant(ekg, "VERSION", VERSION);

	// Const - message types

	PyModule_AddIntConstant(ekg, "MSGCLASS_MESSAGE",   EKG_MSGCLASS_MESSAGE);
	PyModule_AddIntConstant(ekg, "MSGCLASS_CHAT",      EKG_MSGCLASS_CHAT);
	PyModule_AddIntConstant(ekg, "MSGCLASS_SENT",      EKG_MSGCLASS_SENT);
	PyModule_AddIntConstant(ekg, "MSGCLASS_SENT_CHAT", EKG_MSGCLASS_SENT_CHAT);
	PyModule_AddIntConstant(ekg, "MSGCLASS_SYSTEM",    EKG_MSGCLASS_SYSTEM);

	// Const - status types

	PyModule_AddStringConstant(ekg, "STATUS_NA",            EKG_STATUS_NA);
	PyModule_AddStringConstant(ekg, "STATUS_AVAIL",         EKG_STATUS_AVAIL);
	PyModule_AddStringConstant(ekg, "STATUS_AWAY",          EKG_STATUS_AWAY);
	PyModule_AddStringConstant(ekg, "STATUS_AUTOAWAY",      EKG_STATUS_AUTOAWAY);
	PyModule_AddStringConstant(ekg, "STATUS_INVISIBLE",     EKG_STATUS_INVISIBLE);
	PyModule_AddStringConstant(ekg, "STATUS_XA",            EKG_STATUS_XA);
	PyModule_AddStringConstant(ekg, "STATUS_DND",           EKG_STATUS_DND);
	PyModule_AddStringConstant(ekg, "STATUS_FREE_FOR_CHAT", EKG_STATUS_FREE_FOR_CHAT);
	PyModule_AddStringConstant(ekg, "STATUS_BLOCKED",       EKG_STATUS_BLOCKED);
	PyModule_AddStringConstant(ekg, "STATUS_UNKNOWN",       EKG_STATUS_UNKNOWN);
	PyModule_AddStringConstant(ekg, "STATUS_ERROR",         EKG_STATUS_ERROR);

	// Const - ignore levels

	PyModule_AddIntConstant(ekg, "IGNORE_STATUS",       IGNORE_STATUS);
	PyModule_AddIntConstant(ekg, "IGNORE_STATUS_DESCR", IGNORE_STATUS_DESCR);
	PyModule_AddIntConstant(ekg, "IGNORE_MSG",          IGNORE_MSG);
	PyModule_AddIntConstant(ekg, "IGNORE_DCC",          IGNORE_DCC);
	PyModule_AddIntConstant(ekg, "IGNORE_EVENTS",       IGNORE_EVENTS);
	PyModule_AddIntConstant(ekg, "IGNORE_NOTIFY",       IGNORE_NOTIFY);
	PyModule_AddIntConstant(ekg, "IGNORE_XOSD",       IGNORE_XOSD);
	PyModule_AddIntConstant(ekg, "IGNORE_ALL",          IGNORE_ALL);

	return 0;
}

/**
 * python_finalize()
 *
 * clean interpreter, unload modules, scripts etc.
 *
 */

int python_finalize()
{
	Py_Finalize();
        return 0;
}

// ********************************************************************************
// *
// * Plugin support functions
// *
// ********************************************************************************

/**
 * python_theme_init()
 *
 * initialize theme formats
 *
 */

int python_theme_init() { 
        return 0;
}

/**
 * python_plugin_destroy()
 *
 * remove plugin
 *
 */

static int python_plugin_destroy()
{
	scriptlang_unregister(&python_lang);
	plugin_unregister(&python_plugin);
	return 0;
}

/**
 * python_plugin_init()
 *
 * inicjalizacja pluginu
 *
 */

int python_plugin_init(int prio)
{
	plugin_register(&python_plugin, prio);

	scriptlang_register(&python_lang, 1);
	command_add(&python_plugin, "python:eval",   "?",  python_command_eval,   0, NULL);
	command_add(&python_plugin, "python:run",    "?",  python_command_run,    0, NULL);
	command_add(&python_plugin, "python:load",   "?",  python_command_load,   0, NULL);
	command_add(&python_plugin, "python:unload", "?",  python_command_unload, 0, NULL);
	command_add(&python_plugin, "python:list",    "",  python_command_list,   0, NULL);

	query_connect(&python_plugin, "plugin-print-version", python_print_version, NULL);

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
