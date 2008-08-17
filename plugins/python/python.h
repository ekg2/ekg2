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

#ifndef __PYTHON_H_
#define __PYTHON_H_

#include <sys/types.h>		/* on Solaris we need to include it before Python,
				   but I think this shouldn't make problems on others */
#include <Python.h>
#include <ekg/scripts.h>

#if !defined(Py_RETURN_NONE) // New in Python 2.4
static inline PyObject* doPy_RETURN_NONE()
{	Py_INCREF(Py_None); return Py_None; }
#define Py_RETURN_NONE return doPy_RETURN_NONE()
#endif

#if !defined(Py_RETURN_TRUE) // New in Python 2.4
static inline PyObject* doPy_RETURN_TRUE()
{Py_INCREF(Py_True); return Py_True;}
#	define Py_RETURN_TRUE return doPy_RETURN_TRUE()
#endif

#if !defined(Py_RETURN_FALSE) // New in Python 2.4
static inline PyObject* doPy_RETURN_FALSE()
{Py_INCREF(Py_False); return Py_False;}
#define Py_RETURN_FALSE return doPy_RETURN_FALSE()
#endif

extern scriptlang_t python_lang;

#define python_module(s) ((PyObject *) script_private_get(s)) /* obiekt modu³u */

#define PYTHON_HANDLE_HEADER(event, arg) \
{ \
	PyObject *__py_r; \
	PyObject *pArgs = arg;\
	python_handle_result = -1;\
	\
	__py_r = PyObject_Call(event, pArgs, NULL);\
	\
	if (__py_r && PyInt_Check(__py_r)) { \
		python_handle_result = PyInt_AsLong(__py_r); \
	} else if (!__py_r) {\
		char *err = python_geterror(scr);\
		print("script_error", err);\
		xfree(err);\
	}

#define PYTHON_HANDLE_FOOTER() \
	Py_XDECREF(__py_r); \
	Py_DECREF(pArgs);\
	\
}

int python_run(const char *filename);
int python_exec(const char *command);
int python_run(const char *filename);
int python_autorun();
int python_initialize();
int python_finalize();
int python_plugin_init();
script_t *python_find_script(PyObject *module);
int python_load(script_t *s);
int python_unload(script_t *s);
char *python_geterror(script_t *s);
PyObject *python_get_func(PyObject *module, const char *name); 


#endif

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=8 sw=8
 */
