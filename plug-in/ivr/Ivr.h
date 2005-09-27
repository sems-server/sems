/*
 * $Id: Ivr.h,v 1.15.2.1 2005/09/02 13:47:46 rco Exp $
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

#ifndef _IVR_H_
#define _IVR_H_

#define MOD_NAME "ivr"

#include <Python.h>

#include "AmApi.h"
#include "AmPlaylist.h"
#include "SemsConfiguration.h"

#ifdef IVR_WITH_TTS
#include "flite.h"
#endif

#include <string>
using std::string;

class IvrDialog;

class IvrFactory: public AmStateFactory
{
/* #ifdef IVR_WITH_TTS */
/*   bool tts_caching; */
/*   string tts_cache_path; */
/* #endif */

    string script_path;
    string default_script;

    void init_python_interpreter();
    void import_ivr_builtins();

    void import_object(PyObject* m, 
		       char* name, 
		       PyTypeObject* type);

    IvrDialog* loadScript(const string& path);
    void setScriptPath(const string& path);
    bool checkCfg();
    
 public:
    IvrFactory(const string& _app_name);

    int onLoad();
    AmDialogState* onInvite(AmCmd&);
};


class IvrDialog : public AmDialogState
{
    PyObject  *py_mod;
    PyObject  *py_dlg;

// #ifdef IVR_WITH_TTS
//   cst_voice* tts_voice;
//   bool tts_caching;
//   string tts_cache_path;
// #endif
  
    void callPyEventHandler(char* name);
    
    void process(AmEvent* event);

public:
    AmPlaylist playlist;

    IvrDialog();
    ~IvrDialog();

    // must be called before everything else.
    void setPyPtrs(PyObject *mod, PyObject *dlg);
    
    void onSessionStart(AmRequest* req);
    void onBye(AmRequest* req);
    void onDtmf(int event, int duration_msec);


    // TODO:
    void onBeforeCallAccept(AmRequest* req, 
			    unsigned int& reply_code, 
			    std::string& reply_reason);

    int  onOther(AmSessionEvent* event);

    int  onUACRequestStatus(AmRequestUACStatusEvent* event);

};

#endif
