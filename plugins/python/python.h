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

#include <Python.h>

#ifndef Py_RETURN_TRUE
#define Py_RETURN_TRUE Py_INCREF(Py_True); return Py_True;
#define Py_RETURN_FALSE Py_INCREF(Py_False); return Py_False;
#endif

#include <ekg/dynstuff.h>
#include <ekg/scripts.h>

extern scriptlang_t python_lang;

#define python_module(s) ((PyObject *) script_private_get(s)) /* obiekt modu³u */

#define PYTHON_HANDLE_HEADER(event, args...) \
{ \
        PyObject *temp, *exc_typ, *exc_val, *exc_tb; \
        PyObject *pName, *pModule, *pDict, *pFunc, *pArgs, *pValue; \
        char * buffer; \
	\
	python_handle_result = -1;\
        pArgs = Py_BuildValue(args);\
	\
	PyObject *__py_r; \
	\
	__py_r = PyObject_Call((PyObject *)event, pArgs, NULL); \
        Py_DECREF(pArgs);\
	\
	if (!__py_r) { \
                    buffer = xmalloc(1024); \
                    PyErr_Fetch(&exc_typ, &exc_val, &exc_tb); \
                    PyErr_NormalizeException(&exc_typ, &exc_val, &exc_tb); \
                    pName = PyString_FromString("traceback"); \
                    pModule = PyImport_Import(pName); \
                    Py_DECREF(pName); \
                    \
                    temp = PyObject_Str(exc_typ); \
                    if (temp != NULL) { \
                            strcat(buffer, PyString_AsString(temp)); \
                            strcat(buffer, "\n"); \
                    } \
                    temp = PyObject_Str(exc_val); \
                    if (temp != NULL) { \
                            strcat(buffer, PyString_AsString(temp)); \
                    } \
                    Py_DECREF(temp); \
                    strcat(buffer, "\n"); \
                    \
                    if (exc_tb != NULL && pModule != NULL ) { \
                            pDict = PyModule_GetDict(pModule); \
                            pFunc = PyDict_GetItemString(pDict, "format_tb"); \
                            if (pFunc && PyCallable_Check(pFunc)) { \
                                    pArgs = PyTuple_New(1); \
                                    pArgs = PyTuple_New(1); \
                                    PyTuple_SetItem(pArgs, 0, exc_tb); \
                                    pValue = PyObject_CallObject(pFunc, pArgs); \
                                    if (pValue != NULL) { \
                                            int len = PyList_Size(pValue); \
                                            if (len > 0) { \
                                                    PyObject *t,*tt; \
                                                    char * buffer2; \
                                                    int i; \
                                                    for (i = 0; i < len; i++) { \
                                                            tt = PyList_GetItem(pValue,i); \
                                                            t = Py_BuildValue("(O)",tt); \
                                                            PyArg_ParseTuple(t,"s",&buffer2); \
                                                            strcat(buffer,buffer2); \
                                                            strcat(buffer, "\n"); \
                                                    } \
                                            } \
                                    } \
                                    Py_DECREF(pValue); \
                                    Py_DECREF(pArgs); \
                            } \
                    } \
                    Py_DECREF(pModule); \
                    print("script_error", buffer); \
                    PyErr_Restore(exc_typ, exc_val, exc_tb); \
                    PyErr_Clear(); \
            } \
	\
	python_handle_result = -1; \
	\
	if (__py_r && PyInt_Check(__py_r)) { \
		int tmp = PyInt_AsLong(__py_r); \
		\
		if (python_handle_result != 2 && tmp != 1) \
			python_handle_result = tmp; \
	} \
	\
	if (__py_r && PyTuple_Check(__py_r))
	
#define PYTHON_HANDLE_RESULT(args...) \
		if (!PyArg_ParseTuple(__py_r, args)) \
			PyErr_Print(); \
		else

#define PYTHON_HANDLE_FOOTER(x) \
	\
	Py_XDECREF(__py_r); \
	\
        {\
                if (x) { \
                        Py_XDECREF((PyObject *) x); } \
		if (python_handle_result == 0) return -1;\
		else return 0;\
	}\
}

int python_run(const char *filename);
int python_exec(const char *command);
int python_run(const char *filename);
int python_autorun();
int python_initialize();
int python_finalize();
int python_plugin_init();
script_t *python_find_script(PyObject *module);

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
