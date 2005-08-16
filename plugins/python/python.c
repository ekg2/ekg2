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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <Python.h>

#include <ekg/commands.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/scripts.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

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
//	python_load(params[0], quiet);
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

int python_variable_changed(script_t *scr, script_var_t *scr_var, char *newval)
{
	return 0;
}

int python_timers(script_t *scr, script_timer_t *time, int type)
{
	int python_handle_result;
	python_private_t *p = python_private(scr);

	if (!(p->handle_comm = python_get_func(p->module, time->private))) {
		debug("func %s in script %s not found, deleting comm \n", time->private, scr->path);
		return SCRIPT_HANDLE_UNBIND;
	}
	PYTHON_HANDLE_HEADER(comm, "(s)", "dupa");
	;
	PYTHON_HANDLE_FOOTER()
	if (p->handle_comm) {
		Py_XDECREF(p->handle_comm);
	}
	p->handle_comm = NULL;
	return python_handle_result;
}

int python_commands(script_t *scr, script_command_t *comm, char **params)
{
	int python_handle_result;
	python_private_t *p = python_private(scr);

	if (!(p->handle_comm = python_get_func(p->module, comm->private))) {
		debug("func %s in script %s not found, deleting comm \n", comm->private, scr->path);
		return SCRIPT_HANDLE_UNBIND;
	}
	PYTHON_HANDLE_HEADER(comm, "(ss)", comm->comm, 
//					    params)
					    params[0] ? params[0] : "")
	;
	PYTHON_HANDLE_FOOTER()
	if (p->handle_comm) {
		Py_XDECREF(p->handle_comm);
	}
	p->handle_comm = NULL;
	return python_handle_result;
}


int python_keypressed(script_t *s, int *_ch)
{
	int ch = *_ch;
	int python_handle_result;
	python_private_t *p = python_private(s);

	PYTHON_HANDLE_HEADER(keypress, "(i)", ch);
	PYTHON_HANDLE_FOOTER()
}

/**
 * python_protocol_status()
 *
 * handle status events
 *
 */

int python_protocol_status(script_t *s, char **__session, char **__uid, char **__status, char **__descr)
{
	char *session = *__session;
	char *uid = *__uid;
	char *status = *__status;
	char *descr = *__descr;
	int python_handle_result;
	python_private_t *p = python_private(s);

	PYTHON_HANDLE_HEADER(status, "(ssss)", session, uid, status, descr)
	;
	PYTHON_HANDLE_FOOTER()
}

/**
 * python_protocol_message()
 *
 * handle message events
 *
 */

int python_protocol_message(script_t *scr, char **__session, char **__uid, char ***__rcpts, char **__text, uint32_t **__format, time_t *__send, int  *__class)
{
	char *session = *__session;
	char *uid = *__uid;
	char **rcpts = *__rcpts;
	char *text = *__text;
	uint32_t *format = *__format;
	time_t sent = *__send;
	int class = *__class;

        int level;
	char * target;
	userlist_t *u;
	session_t *s;
	int python_handle_result;
	python_private_t *p = python_private(scr);

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
		PYTHON_HANDLE_HEADER(msg_own, "(sss)", session, target, text)
		;
		PYTHON_HANDLE_FOOTER();
	} else {
		PYTHON_HANDLE_HEADER(msg, "(ssisii)", session, uid, class, text, (int) sent, level)
		;
		PYTHON_HANDLE_FOOTER();
	}
}

/**
 * python_protocol_connected()
 *
 * handle connect events
 *
 */

int python_protocol_connected(script_t *s, char **__session)
{
	char *session = *__session;
	int python_handle_result;
	python_private_t *p = python_private(s);

	PYTHON_HANDLE_HEADER(connect, "(s)", session)
	;
	PYTHON_HANDLE_FOOTER()
}

/**
 * python_protocol_disconnected()
 *
 * handle disconnect events
 *
 */

int python_protocol_disconnected(script_t *s, char **__session)
{
	char *session = *__session;
	int python_handle_result;
	python_private_t *p = python_private(s);

	PYTHON_HANDLE_HEADER(disconnect, "(s)", session)
	;
	PYTHON_HANDLE_FOOTER()
}

int python_query(script_t *scr, script_query_t *scr_que, void **args)
{
#define ARG_CHARP(x)    *(char **) args[x]
#define ARG_INTPP(x)    *(int  **) args[x]
#define ARG_TIMEP(x)    *(time_t **) args[x]

#define ARG_CHARPP(x)   *(char ***) args[x]
#define ARG_INTPPP(x)   *(int  ***) args[x]
#define ARG_VOIDPP(x)   *(void ***) args[x]
#define ARG_UINTPPP(x)  *(uint32_t ***) args[x]

#define ARG_CHARPPP(x)  *(char ****) args[x]
#define SCRIPT_HANDLER_BACKWARD(x) \
        char *name = scr_que->query_name;\
        \
	if (!xstrcmp(name, "protocol-message")) return x##_protocol_message(scr, ARG_CHARPP(0), ARG_CHARPP(1), ARG_CHARPPP(2), ARG_CHARPP(3), ARG_UINTPPP(4), ARG_TIMEP(5), ARG_INTPP(6));\
        else if (!xstrcmp(name, "protocol-disconnected")) return x##_protocol_disconnected(scr, ARG_CHARPP(0));\
        else if (!xstrcmp(name, "protocol-connected"))  return x##_protocol_connected(scr, ARG_CHARPP(0));\
        else if (!xstrcmp(name, "protocol-status"))     return x##_protocol_status(scr, ARG_CHARPP(0), ARG_CHARPP(1), ARG_CHARPP(2), ARG_CHARPP(3));\
        else if (!xstrcmp(name, "ui-keypress"))         return x##_keypressed(scr, ARG_INTPP(0));\
        else

	SCRIPT_HANDLER_BACKWARD(python)
	return -1;
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

script_t *python_find_script(PyObject *module)
{
	SCRIPT_FINDER(slang == &python_lang);
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
	PyObject *module, *init;
	python_private_t *p;

	module = PyImport_ImportModule(s->name);

	if (!module) {
		print("script_not_found", s->name);
		PyErr_Print();
		return -1;
	}

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

	p = xmalloc(sizeof(python_private_t));
	p->module                  = module;
	
	if ((p->deinit                  = python_get_func(module, "deinit")));

	if ((p->handle_msg              = python_get_func(module, "handle_msg")))
		script_query_bind(&python_lang, s, "protocol-message",      &python_query);	
		
	if ((p->handle_msg_own          = python_get_func(module, "handle_msg_own")))
		script_query_bind(&python_lang, s, "protocol-message",      &python_query);
		
	if ((p->handle_status           = python_get_func(module, "handle_status")))
		script_query_bind(&python_lang, s, "protocol-status",       &python_query);;
		
	if ((p->handle_status_own       = python_get_func(module, "handle_status_own")))
		script_query_bind(&python_lang, s, "protocol-status",       &python_query);
		
	if ((p->handle_connect          = python_get_func(module, "handle_connect")))
		script_query_bind(&python_lang, s, "protocol-connected",    &python_query);
		
	if ((p->handle_disconnect       = python_get_func(module, "handle_disconnect")))
		script_query_bind(&python_lang, s, "protocol-disconnected", &python_query);
		
	if ((p->handle_keypress         = python_get_func(module, "handle_keypress")))
		script_query_bind(&python_lang, s, "ui-keypress",           &python_query);

	script_private_set(s, p);

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
	python_private_t *p = python_private(s);
	debug("m->deinit = %p, hmm?\n", p->deinit);
	if (p->deinit) {
		PyObject *res = PyObject_CallFunction(p->deinit, "()");
		Py_XDECREF(res);
		Py_XDECREF(p->deinit);
	}
	Py_XDECREF(p->handle_msg);
	Py_XDECREF(p->handle_msg_own);
	Py_XDECREF(p->handle_connect);
	Py_XDECREF(p->handle_disconnect);
	Py_XDECREF(p->handle_status);
	Py_XDECREF(p->handle_status_own);
	Py_XDECREF(p->handle_keypress); 
	Py_XDECREF(p->module);
	
	xfree(p);
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

	if (getenv("PYTHONPATH")) {
		char *tmp = saprintf("%s:%s", getenv("PYTHONPATH"), prepare_path("scripts", 0));
#ifdef HAVE_SETENV
		setenv("PYTHONPATH", tmp, 1);
#else
		{
			char *s = saprintf("PYTHONPATH=%s", tmp);
			putenv(s);
			xfree(s);
		}
#endif
		xfree(tmp);
	} else {
#ifdef HAVE_SETENV
		setenv("PYTHONPATH", prepare_path("scripts", 0), 1);
#else
		{
			char *s = saprintf("PYTHONPATH=%s", prepare_path("scripts", 0));
			putenv(s);
			xfree(s);
		}
#endif
	}

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
#if 0
	format_add("python_eval_error", _("%! Error running code\n"), 1);
	format_add("python_list", _("%> %1\n"), 1);
	format_add("python_list_empty", _("%! No scripts loaded\n"), 1);
	format_add("python_loaded", _("%) Script loaded\n"), 1);
	format_add("python_removed", _("%) Script removed\n"), 1);
	format_add("python_need_name", _("%! No filename given\n"), 1);
	format_add("python_error", _("%! Error %T%1%n\n"), 1);
	format_add("python_not_found", _("%! Can't find script %T%1%n\n"), 1);
	format_add("python_wrong_location", _("%! Script have to be in %T%1%n (don't add path)\n"), 1);
#endif
        return 0;
}

/**
 * python_plugin_destroy()
 *
 * remove plugin
 *
 */

int python_plugin_destroy()
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
/* procedure wywolujaca formatki nie trzeba wywolywac z python_plugin_init() */
	command_add(&python_plugin, "python:eval",   "?",  python_command_eval,   0, NULL);
	command_add(&python_plugin, "python:run",    "?",  python_command_run,    0, NULL);
	command_add(&python_plugin, "python:load",   "?",  python_command_load,   0, NULL);
	command_add(&python_plugin, "python:unload", "?",  python_command_unload, 0, NULL);
	command_add(&python_plugin, "python:list",    "",  python_command_list,   0, NULL);
	
// int script_query_bind(scriptlang_t *s, script_t *scr, char *query_name, int argc, void *handler)



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
