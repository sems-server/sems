/* Copyright (C) 2002-2003 Fhg Fokus
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

#ifndef _IVR_H_
#define _IVR_H_

#define MOD_NAME "ivr"

#include <Python.h>

#include "AmB2BSession.h"
#include "AmPlaylist.h"

#ifdef IVR_WITH_TTS
#include "flite.h"
#endif

#include <string>
#include <map>
using std::string;
using std::map;

class IvrDialog;

/** \brief C++ wrapper for extra thread created by Python IVR script */
class PythonScriptThread : public AmThread {
  PyObject* py_thread_object;
 protected:
  void run();
  void on_stop();
 public:
 PythonScriptThread(PyObject* py_thread_object_) 
   : py_thread_object(py_thread_object_) { }
};

/** \brief binds a script module and the python dialog class */
struct IvrScriptDesc
{
  PyObject* mod;
  PyObject* dlg_class;

IvrScriptDesc()
: mod(0), 
    dlg_class(0)
  {}

IvrScriptDesc(const IvrScriptDesc& d)
: mod(d.mod), 
    dlg_class(d.dlg_class)
  {}

IvrScriptDesc(PyObject* mod, 
	      PyObject* dlg_class)
: mod(mod),
    dlg_class(dlg_class)
  {}
};

/** \brief session factory for python IVR sessions */
class IvrFactory: public AmSessionFactory
{
  static AmConfigReader cfg;

  PyObject* ivr_module;
  //string script_path;
  string default_script;

  map<string,IvrScriptDesc> mod_reg;

  static AmSessionEventHandlerFactory* session_timer_f;

  void init_python_interpreter(const string& script_path);
  void set_sys_path(const string& script_path);
  void import_ivr_builtins();

  void import_object(PyObject* m, 
		     const char* name, 
		     PyTypeObject* type);

  /** @return true if everything ok */
  bool loadScript(const string& path);

  //void setScriptPath(const string& path);
  bool checkCfg();

  IvrDialog* newDlg(const string& name);

  std::queue<PyObject*> deferred_threads;
  void start_deferred_threads();
   
 public:
  IvrFactory(const string& _app_name);

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);

  void addDeferredThread(PyObject* pyCallable);

  void setupSessionTimer(AmSession* s);
};

/** \brief python IVR wrapper for session base implementation */
class IvrDialog : public AmB2BCallerSession
{
  PyObject  *py_mod;
  PyObject  *py_dlg;

  bool callPyEventHandler(const char* name, const char* fmt, ...);
    
  void process(AmEvent* event);

  string b2b_callee_from_party;
  string b2b_callee_from_uri;

  void createCalleeSession();
 public:
  AmPlaylist playlist;

  IvrDialog();
  ~IvrDialog();

  // must be called before everything else.
  void setPyPtrs(PyObject *mod, PyObject *dlg);

  int transfer(const string& target);
  int refer(const string& target, int expires);
  int drop();
    
  void onInvite(const AmSipRequest& req);
  int  onSdpCompleted(const AmSdp& offer, const AmSdp& answer);
  void onSessionStart();

  void onBye(const AmSipRequest& req);
  void onDtmf(int event, int duration_msec);

  void onOtherBye(const AmSipRequest& req);
  bool onOtherReply(const AmSipReply& r);

  void onSipReply(const AmSipRequest& req,
		  const AmSipReply& reply, 
		  AmBasicSipDialog::Status old_dlg_status);
  void onSipRequest(const AmSipRequest& r);

  void onRtpTimeout();
    
  void connectCallee(const string& remote_party, const string& remote_uri,
		     const string& from_party, const string& from_uri);

};

#endif
