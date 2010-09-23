/* 
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "PySemsUtils.h"

PyObject *
type_error(const char *msg)
{
  PyErr_SetString(PyExc_TypeError, msg);
  return NULL;
}

PyObject *
null_error(void)
{
  if (!PyErr_Occurred())
    PyErr_SetString(PyExc_SystemError,
		    "null argument to internal routine");
  return NULL;
}

PyObject *
PyObject_VaCallMethod(PyObject *o, char *name, char *format, va_list va)
{
  PyObject *args, *func = 0, *retval;

  if (o == NULL || name == NULL)
    return null_error();

  func = PyObject_GetAttrString(o, name);
  if (func == NULL) {
    PyErr_SetString(PyExc_AttributeError, name);
    return 0;
  }

  if (!PyCallable_Check(func))
    return type_error("call of non-callable attribute");

  if (format && *format) {
    args = Py_VaBuildValue(format, va);
  }
  else
    args = PyTuple_New(0);

  if (!args)
    return NULL;

  if (!PyTuple_Check(args)) {
    PyObject *a;

    a = PyTuple_New(1);
    if (a == NULL)
      return NULL;
    if (PyTuple_SetItem(a, 0, args) < 0)
      return NULL;
    args = a;
  }

  retval = PyObject_Call(func, args, NULL);

  Py_DECREF(args);
  Py_DECREF(func);

  return retval;
}
