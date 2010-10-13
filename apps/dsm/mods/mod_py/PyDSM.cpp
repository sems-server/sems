/*
 * Copyright (C) 2009 IPTEGO GmbH
 * 
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "PyDSM.h"
#include "log.h"


extern "C" {

  static PyObject* mod_py_log(PyObject*, PyObject* args)
  {
    int level;
    char *msg;
    
    if(!PyArg_ParseTuple(args,"is",&level,&msg))
      return NULL;
    
    _LOG(level, "%s", msg);
  
    Py_INCREF(Py_None);
    return Py_None;
  }

#define DEF_LOG_FNC(suffix, func)			      \
  static PyObject* mod_py_##suffix(PyObject*, PyObject* args) \
  {							      \
    char *msg;						      \
    							      \
    if(!PyArg_ParseTuple(args,"s",&msg))		      \
      return NULL;					      \
    func("%s", msg);					      \
    							      \
    Py_INCREF(Py_None);					      \
    return Py_None;					      \
  }
  
  DEF_LOG_FNC(dbg,   DBG);
  DEF_LOG_FNC(info,  INFO);
  DEF_LOG_FNC(warn,  WARN);
  DEF_LOG_FNC(error, ERROR);

  PyMethodDef mod_py_methods[] = {
    {"log",   (PyCFunction)mod_py_log, METH_VARARGS,"Log a message using SEMS' logging system"},
    {"DBG",   (PyCFunction)mod_py_dbg, METH_VARARGS,"Log a message using SEMS' logging system, level debug"},
    {"INFO",  (PyCFunction)mod_py_info, METH_VARARGS,"Log a message using SEMS' logging system, level info"},
    {"WARN",  (PyCFunction)mod_py_warn, METH_VARARGS,"Log a message using SEMS' logging system, level warning"},
    {"ERROR", (PyCFunction)mod_py_error, METH_VARARGS,"Log a message using SEMS' logging system, level error"},
    {NULL}  /* Sentinel */
  };

}
