/*
 * $Id: PySems.h,v 1.15.2.1 2005/09/02 13:47:46 rco Exp $
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
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

#ifdef PY_SEMS_WITH_TTS
#include "flite.h"
#endif

#include <string>
#include <map>
using std::string;
using std::map;

class PySemsDialog;

struct PySemsScriptDesc
{
    PyObject* mod;
    PyObject* dlg_class;

    PySemsScriptDesc()
	: mod(0), 
	  dlg_class(0)
    {}

    PySemsScriptDesc(const PySemsScriptDesc& d)
	: mod(d.mod), 
	  dlg_class(d.dlg_class)
    {}

    PySemsScriptDesc(PyObject* mod, 
		  PyObject* dlg_class)
	: mod(mod),
	  dlg_class(dlg_class)
    {}
};


class PySemsFactory: public AmSessionFactory
{
    PyObject* py_sems_module;
    string script_path;
    string default_script;

    map<string,PySemsScriptDesc> mod_reg;

    AmDynInvokeFactory* user_timer_fact;

    void init_python_interpreter();
    void import_py_sems_builtins();

    void import_module(const char* modname);
    void import_object(PyObject* m, 
		       char* name, 
		       PyTypeObject* type);

    /** @return true if everything ok */
    bool loadScript(const string& path);

    void setScriptPath(const string& path);
    bool checkCfg();

    PySemsDialog* newDlg(const string& name);
    
 public:
    PySemsFactory(const string& _app_name);

    int onLoad();
    AmSession* onInvite(const AmSipRequest& req);
};


class PySemsDialog : public AmB2BCallerSession
{
    PyObject  *py_mod;
    PyObject  *py_dlg;

    bool callPyEventHandler(char* name, char* fmt, ...);

public:
    AmDynInvoke* user_timer;
    AmPlaylist playlist;

    PySemsDialog();
    PySemsDialog(AmDynInvoke* user_timer);
    ~PySemsDialog();

    // must be called before everything else.
    void setPyPtrs(PyObject *mod, PyObject *dlg);
    
    void onSessionStart(const AmSipRequest& req);

    // @see AmEventHandler
    void process(AmEvent* event);

};

#endif
