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
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

#include "python.h"
#include "python-config.h"

// * ***************************************************************************
// *
// * Modu³ ekg
// *
// * ***************************************************************************

/**
 * ekg_cmd_command()
 *
 * wykonanie polecenia ekg
 *
 */

PyObject * ekg_cmd_command(PyObject *self, PyObject *args)
{
        char *command = NULL;

        if (!PyArg_ParseTuple(args, "s", &command)) {
                return NULL;
        }
        command_exec(NULL, NULL, command, 0); // wykonanie polecenia dla bie¿±cej sesji
        Py_INCREF(Py_None);
        return Py_None;
}

/**
 * ekg_cmd_echo()
 *
 * wy¶wietlenie tekstu w formacie "generic"
 *
 */

PyObject * ekg_cmd_echo(PyObject *self, PyObject *args)
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
 * rozbudowane wy¶wietlanie tekstów
 *
 */

PyObject * ekg_cmd_printf(PyObject *self, PyObject *pyargs)
{
	char *format = "generic", *args[9];
	int i;

	for (i = 0; i < 9; i++)
		args[i] = "";

	if (!PyArg_ParseTuple(pyargs, "s|sssssssss:printf", &format, &args[0], &args[1], &args[2], &args[3], &args[4], &args[5], &args[6], &args[7], &args[8]))
		return NULL;

	print(format, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);

        Py_INCREF(Py_None);
        return Py_None;
}

/**
 * ekg_cmd_debug()
 *
 * tekst debugowy
 *
 */

PyObject * ekg_cmd_debug(PyObject *self, PyObject *pyargs)
{
        char *str = NULL;
        char *args[9];

	if (!PyArg_ParseTuple(pyargs, "s|sssssssss:debug", &str, &args[0], &args[1], &args[2], &args[3], &args[4], &args[5], &args[6], &args[7], &args[8]))
                return NULL;

        debug(str, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);

        Py_INCREF(Py_None);
        return Py_None;
}

/**
 * ekg_cmd_plugins()
 *
 * returns plugin list
 *
 */

PyObject * ekg_cmd_plugins(PyObject *self, PyObject *pyargs)
{
        PyObject * list;
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

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=8 sw=8
 */
