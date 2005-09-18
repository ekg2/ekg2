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

#include <sys/types.h>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <Python.h>

#include <ekg/commands.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/scripts.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

#include "python.h"
#include "python-config.h"
#include "python-session.h"
#include "python-plugin.h"

// * ***************************************************************************
// *
// * ekg module
// *
// * ***************************************************************************

/**
 * ekg_cmd_command()
 *
 * run ekg command
 *
 */

PyObject *ekg_cmd_command(PyObject * self, PyObject * args)
{
        char *command = NULL;

        if (!PyArg_ParseTuple(args, "s", &command)) {
                return NULL;
        }
        command_exec(NULL, NULL, command, 0);	// run command for current session
        Py_INCREF(Py_None);
        return Py_None;
}

/**
 * ekg_cmd_command_bind()
 *
 */

PyObject *ekg_cmd_command_bind(PyObject * self, PyObject * args)
{
        char *bind_command = NULL;
        PyObject *callback = NULL;
        PyObject *module   = NULL;
        script_t * scr;

        if (!PyArg_ParseTuple(args, "sO", &bind_command, &callback)) {
                return NULL;
        }

        if (!PyCallable_Check(callback)) {
                print("generic-error", _("Second parameter to command_bind is not callable"));
                PyErr_SetString(PyExc_TypeError, _("Parameter must be callable"));
                return NULL;
        }
        Py_XINCREF(callback);

        module = PyObject_GetAttrString(callback, "__module__");
        scr = python_find_script(module);

        debug("[python] binding command %s to python function\n", bind_command);
        script_command_bind(&python_lang, scr, bind_command, callback);

        Py_INCREF(Py_None);
        return Py_None;
}

/**
 * ekg_cmd_timer_bind()
 *
 */

PyObject *ekg_cmd_timer_bind(PyObject * self, PyObject * args)
{
        PyObject *callback = NULL;
        PyObject *module   = NULL;
        script_t * scr;
        int freq;

        if (!PyArg_ParseTuple(args, "iO", &freq, &callback)) {
                return NULL;
        }

        if (!PyCallable_Check(callback)) {
                print("generic-error", _("Second parameter to command_bind is not callable"));
                PyErr_SetString(PyExc_TypeError, _("Parameter must be callable"));
                return NULL;
        }
        Py_XINCREF(callback);

        module = PyObject_GetAttrString(callback, "__module__");
        scr = python_find_script(module);

        script_timer_bind(&python_lang, scr, freq, callback);

        Py_INCREF(Py_None);
        return Py_None;
}


/**
 * ekg_cmd_echo()
 *
 * print text in "generic" format
 *
 */

PyObject *ekg_cmd_echo(PyObject * self, PyObject * args)
{
    char *str = NULL;
    int quiet = 0;

    if (!PyArg_ParseTuple(args, "s", &str)) {
	return NULL;
    }
    printq("generic", str);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * ekg_cmd_printf()
 *
 * extended display
 *
 */

PyObject *ekg_cmd_printf(PyObject * self, PyObject * pyargs)
{
    char *format = "generic", *args[9];
    int i;

    for (i = 0; i < 9; i++)
	args[i] = "";

    if (!PyArg_ParseTuple
	(pyargs, "s|sssssssss:printf", &format, &args[0], &args[1],
	 &args[2], &args[3], &args[4], &args[5], &args[6], &args[7],
	 &args[8]))
	return NULL;

    print(format, args[0], args[1], args[2], args[3], args[4], args[5],
	  args[6], args[7], args[8]);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * ekg_cmd_debug()
 *
 * record debug message
 *
 */

PyObject *ekg_cmd_debug(PyObject * self, PyObject * pyargs)
{
    char *str = NULL;
    char *args[9];

    if (!PyArg_ParseTuple
	(pyargs, "s|sssssssss:debug", &str, &args[0], &args[1], &args[2],
	 &args[3], &args[4], &args[5], &args[6], &args[7], &args[8]))
	return NULL;

    debug(str, args[0], args[1], args[2], args[3], args[4], args[5],
	  args[6], args[7], args[8]);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * ekg_cmd_plugins()
 *
 * return plugin list
 *
 */

PyObject *ekg_cmd_plugins(PyObject * self, PyObject * pyargs)
{
    PyObject *list;
    list_t l;
    int len = 0;

    for (l = plugins; l; l = l->next) {
		len++;
    }

    list = PyList_New(len);
    len = 0;

    for (l = plugins; l; l = l->next) {
		plugin_t *p = l->data;
		PyList_SetItem(list, len, PyString_FromString(p->name));
		len++;
    }
    Py_INCREF(list);
    return list;
}

/**
 * ekg_cmd_getPlugin()
 *
 * return plugin object
 *
 */

PyObject *ekg_cmd_getPlugin(PyObject * self, PyObject * pyargs)
{
    ekg_pluginObj *pyplugin;
    char buf[100];
    char *name = NULL;
    list_t l;
    int prio = -1;

    if (!PyArg_ParseTuple(pyargs, "s:getPlugin", &name))
		return NULL;

    debug("[python] checking for plugin '%s'\n", name);

    for (l = plugins; l; l = l->next) {
		plugin_t *p = l->data;
                if (!strcmp(p->name, name)) {
                        prio = p->prio;
                }
    }

    debug("[python] Building object for plugin '%s'\n", name);
    pyplugin = PyObject_New(ekg_pluginObj, &ekg_plugin_type);
    pyplugin->loaded = prio < 0 ? 0 : 1;
    pyplugin->prio = prio < 0 ? 0 : prio;
    pyplugin->name = xmalloc(xstrlen(name)+1);
    xstrcpy(pyplugin->name, name);
    Py_INCREF(pyplugin);
    return (PyObject *)pyplugin;
}

/**
 * ekg_cmd_sessions()
 *
 * return session list
 *
 */

PyObject *ekg_cmd_sessions(PyObject * self, PyObject * pyargs)
{
    PyObject *list;
    list_t l;
    int len = 0;

    for (l = sessions; l; l = l->next) {
		len++;
    }

    list = PyList_New(len);
    len = 0;

    for (l = sessions; l; l = l->next) {
		session_t *p = l->data;
		PyList_SetItem(list, len, PyString_FromString(p->uid));
		len++;
    }
    Py_INCREF(list);
    return list;
}

/**
 * ekg_cmd_getSession()
 *
 * return session object
 *
 */

PyObject *ekg_cmd_getSession(PyObject * self, PyObject * pyargs)
{
    ekg_sessionObj *pysession;
    char buf[100];
    char *name = NULL;
    session_t *s;

    if (!PyArg_ParseTuple(pyargs, "s:getSession", &name))
		return NULL;

    debug("[python] checking for  '%s' session\n", name);
    s = session_find((const char *) name);

    if (!s) {
		snprintf(buf, 99, "Can't find session '%s'", name);
		PyErr_SetString(PyExc_KeyError, buf);
		Py_INCREF(Py_None);
		return Py_None;
    }

    debug("[python] Building object for '%s' session\n", name);
	pysession = PyObject_New(ekg_sessionObj, &ekg_session_type);
	pysession->name = xmalloc(xstrlen(name)+1);
	xstrcpy(pysession->name, name);
    Py_INCREF(pysession);
    return (PyObject *)pysession;
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
