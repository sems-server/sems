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

#ifndef _PY_SEMS_H_
#define _PY_SEMS_H_

#define MOD_NAME "py_sems"

#include <Python.h>

#include "AmB2BSession.h"
#include "AmPlaylist.h"

#include <string>
#include <map>
using std::string;
using std::map;

class PySemsDialogBase;

struct PySemsScriptDesc
{
  enum DialogType {
    None = 0,
    Dialog,
    B2BDialog,
    B2ABDialog
  };

  PyObject* mod;
  PyObject* dlg_class;
  DialogType dt;

PySemsScriptDesc()
: mod(0), 
    dlg_class(0),
    dt(None)
  {}

PySemsScriptDesc(const PySemsScriptDesc& d)
: mod(d.mod), 
    dlg_class(d.dlg_class),
    dt(d.dt)
  {}

PySemsScriptDesc(PyObject* mod, 
		 PyObject* dlg_class,
		 DialogType dt)
: mod(mod),
    dlg_class(dlg_class),
    dt(dt)
  {}
};


/** \brief Factory for PySems sessions */
class PySemsFactory: public AmSessionFactory
{
  PyObject* py_sems_module;
  /*     string script_path; */
  string default_script;

  map<string,PySemsScriptDesc> mod_reg;

  void init_python_interpreter(const string& script_path);
  void set_sys_path(const string& script_path);
  void import_py_sems_builtins();

  PyObject* import_module(const char* modname);
  void import_object(PyObject* m,
		     char* name, 
		     PyTypeObject* type);

  /** @return true if everything ok */
  bool loadScript(const string& path);

  void setScriptPath(const string& path);
  bool checkCfg();

  AmSession* newDlg(const string& name);
    
 public:
  PySemsFactory(const string& _app_name);

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req);
};

/** \brief wrapper for pySems dialog bas class */
class PySemsDialogBase {
  PyObject  *py_mod;
  PyObject  *py_dlg;

 protected:
  bool callPyEventHandler(char* name, char* fmt, ...);

 public:
  PySemsDialogBase();
  ~PySemsDialogBase();

  // must be called before everything else.
  void setPyPtrs(PyObject *mod, PyObject *dlg);
};

#endif
