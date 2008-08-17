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

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/windows.h>

#include <ekg/commands.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

#include "python.h"
#include "python-ekg.h"
#include "python-window.h"

// * ***************************************************************************
// *
// * window object
// *
// * ***************************************************************************

/**
 * ekg_window_init()
 *
 * initialization of window object
 *
 */

int ekg_window_init(ekg_windowObj *self, PyObject *args, PyObject *kwds)
{
    PyObject * name;
    window_t *w;

    static char *kwlist[] = {"name", NULL};

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist,
				      &name))
	return -1;

    w = window_find(PyString_AsString(name));
    if (w) {
	    self->w = w;
    } else {
	    PyErr_SetString(PyExc_RuntimeError, _("Can't find window with that name"));
	    return 0;
    }
    return 0;
}

/**
 * ekg_window_get_attr()
 *
 * get attribute from window object
 *
 */

PyObject *ekg_window_get_attr(ekg_windowObj * self, char * attr)
{
	return Py_FindMethod(ekg_window_methods, (PyObject *) self, attr);
}

/**
 * ekg_window_dealloc()
 *
 * deallocation of window object
 *
 */

void ekg_window_dealloc(ekg_windowObj * o)
{
}

/**
 * ekg_window_repr()
 *
 * __repr__ method
 *
 */

PyObject *ekg_window_repr(ekg_windowObj *self)
{
	char buf[100];
	if (self->w) {
		snprintf(buf, 99, "<window #%i %s>", self->w->id, window_target(self->w));
	} else {
		xstrcpy(buf, "<window killed>");
	}
	return PyString_FromString(buf);
}

/**
 * ekg_window_str()
 *
 * __str__ method
 *
 */

PyObject *ekg_window_str(ekg_windowObj *self)
{
	if (!self->w) {
		PyErr_SetString(PyExc_RuntimeError, _("Window is killed"));
		return NULL;
	}
	return PyString_FromString(window_target(self->w));
}

/**
 * ekg_window_switch_to()
 *
 * switch to current window
 *
 */

PyObject *ekg_window_switch_to(ekg_windowObj * self, PyObject *args)
{
	if (!self->w) {
		PyErr_SetString(PyExc_RuntimeError, _("Window is killed"));
		return NULL;
	}

	debug("[python] Switching to window '%s'\n", self->w->target);

	window_switch(self->w->id);
	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * ekg_window_echo()
 *
 * print on given window
 *
 */

PyObject *ekg_window_echo(ekg_windowObj * self, PyObject *args)
{
	char *str = NULL;

	if (!self->w) {
		PyErr_SetString(PyExc_RuntimeError, _("Window is killed"));
		return NULL;
	}

	if (!PyArg_ParseTuple(args, "s", &str))
		return NULL;

	debug("[python] Printing on window '%s'\n", self->w->target);

	print_info(self->w->target, self->w->session, "generic", str);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * ekg_window_echo_format()
 *
 * print on window with given format
 *
 */

PyObject *ekg_window_echo_format(ekg_windowObj * self, PyObject *args)
{
	char *str = NULL;
	char *format = NULL;

	if (!self->w) {
		PyErr_SetString(PyExc_RuntimeError, _("Window is killed"));
		return NULL;
	}

	if (!PyArg_ParseTuple(args, "ss", &format, &str))
		return NULL;

	debug("[python] Printing on window '%s'\n", self->w->target);

	print_info(self->w->target, self->w->session, format, str);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * ekg_window_kill()
 *
 * print on window with given format
 *
 */

PyObject *ekg_window_kill(ekg_windowObj * self, PyObject *args)
{
	debug("[python] Killing window '%s'\n", self->w->target);

	window_kill(self->w);
	self->w = NULL;

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * ekg_window_next()
 *
 * return next window object
 *
 */

PyObject *ekg_window_next(ekg_windowObj * self, PyObject * pyargs)
{
	int id;
	window_t *w;

	id = self->w->id;

	if (!(w = window_exist(id+1))) {
		w = window_exist(1);
	}

	if (!w) {
		PyErr_SetString(PyExc_RuntimeError, "Window doesn't exist. Strange :/");
		return NULL;
	} else {
		debug("[python] Building object\n");
		return (PyObject *)python_build_window_w(w);
	}
}

/**
 * ekg_window_prev()
 *
 * return previous window object
 *
 */

PyObject *ekg_window_prev(ekg_windowObj * self, PyObject * pyargs)
{
	int id;
	window_t *w = NULL, *wnd;

	id = self->w->id;

	if (id < 2 || !(w = window_exist(id-1))) {
		for (wnd = windows; wnd; wnd = wnd->next) {
			if (wnd->floating)
				continue;

			if (wnd == window_current && wnd != windows)
				break;

			w = wnd;
		}

		if (!w->id) {
			for (wnd = windows; wnd; wnd = wnd->next) {
				if (!wnd->floating)
					w = wnd;
			}
		}
	}

	if (!w) {
		PyErr_SetString(PyExc_RuntimeError, "Window doesn't exist. Strange :/");
		return NULL;
	} else {
		debug("[python] Building object\n");
		return (PyObject *)python_build_window_w(w);
	}
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
