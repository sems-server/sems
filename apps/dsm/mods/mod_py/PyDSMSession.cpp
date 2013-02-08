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

#include "PyDSMSession.h"
#include "DSMStateEngine.h"
#include "log.h"
#include "DSMSession.h"
#include "AmSession.h"

extern "C" {

#define GET_SESS_PTR							\
  PyObject* ts_dict = PyThreadState_GetDict();				\
    PyObject* py_sc_sess = PyDict_GetItemString(ts_dict, "_dsm_sess_"); \
    if (NULL == py_sc_sess) {						\
      ERROR("retrieving the session pointer from TL dict\n");		\
      return NULL;							\
    }									\
									\
    DSMSession* sess = (DSMSession*)PyCObject_AsVoidPtr(py_sc_sess);	\
    if (NULL == sess) {							\
      ERROR("retrieving the session pointer from TL dict\n");		\
      return NULL;							\
    }									

  static PyObject* mod_py_setvar(PyObject*, PyObject* args)
  {
    char *varname;
    char *val;    
    if(!PyArg_ParseTuple(args,"ss",&varname,&val))
      return NULL;

    GET_SESS_PTR;
    
    DBG("set '%s' = '%s'\n", varname, val);
    sess->var[varname] = val; 

    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* mod_py_getvar(PyObject*, PyObject* args)
  {
    char *varname;
    if(!PyArg_ParseTuple(args,"s",&varname))
      return NULL;

    GET_SESS_PTR;
    
    DBG("returning '%s'\n", sess->var[varname].c_str());

    return PyString_FromString(sess->var[varname].c_str());
  }

  static PyObject* mod_py_getselect(PyObject*, PyObject* args)
  {
    char *varname;
    if(!PyArg_ParseTuple(args,"s",&varname))
      return NULL;

    GET_SESS_PTR;

    string res;

    AmSession* sc_sess = dynamic_cast<AmSession*>(sess);
    if (NULL == sc_sess) {
      ERROR("Not possible to cast to session object.\n");
      return NULL;
    }
  
    if (!strcmp("local_tag", varname))
      res = sc_sess->getLocalTag();	
    else if (!strcmp("user", varname))
      res = sc_sess->dlg->getUser();
    else if (!strcmp("domain", varname))
      res = sc_sess->dlg->getDomain();
    else if (!strcmp("remote_tag", varname))
      res = sc_sess->getRemoteTag();
    else if (!strcmp("callid", varname))
      res = sc_sess->getCallID();
    else if (!strcmp("local_uri", varname))
      res = sc_sess->dlg->getLocalUri();
    else if (!strcmp("remote_uri", varname))
      res = sc_sess->dlg->getRemoteUri();

    DBG("returning '%s'\n", res.c_str());

    return PyString_FromString(res.c_str());
  }

  static PyObject* mod_py_seterror(PyObject*, PyObject* args)
  {
    int errno;    
    if(!PyArg_ParseTuple(args,"i",&errno))
      return NULL;

    GET_SESS_PTR;
    
    DBG("setting errno '%i'\n", errno);
    sess->SET_ERRNO(errno);

    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* playPrompt(PyObject*, PyObject* args)
  {
    char *name;
    int loop = 0;

    if(!PyArg_ParseTuple(args,"s|i",&name, &loop))
      return NULL;

    GET_SESS_PTR;
    
    DBG("playPrompt('%s', loop=%s)\n", name, loop?"true":"false");
    try {
      sess->playPrompt(name, loop);
    } catch (DSMException& e) {
      PyErr_SetString(PyExc_RuntimeError, e.params["type"].c_str());
      return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* playFile(PyObject*, PyObject* args)
  {
    char *name;
    int loop = 0;
    int front = 0;

    if(!PyArg_ParseTuple(args,"s|ii", &name, &loop, &front))
      return NULL;

    GET_SESS_PTR;
    
    DBG("playFile('%s', loop=%s, front=%s)\n", name, 
	loop?"true":"false", front?"true":"false");
    try {
      sess->playFile(name, loop, front);
    } catch (DSMException& e) {
      PyErr_SetString(PyExc_RuntimeError, e.params["type"].c_str());
      return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* recordFile(PyObject*, PyObject* args)
  {
    char *name;
    if(!PyArg_ParseTuple(args,"s",&name))
      return NULL;

    GET_SESS_PTR;
    
    DBG("recordFile('%s')\n", name);
    try {
      sess->recordFile(name);
    } catch (DSMException& e) {
      PyErr_SetString(PyExc_RuntimeError, e.params["type"].c_str());
      return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* getRecordLength(PyObject*, PyObject* args)
  {
    GET_SESS_PTR;    
    unsigned int res =  sess->getRecordLength();
    DBG("record length %d\n",res);
    return PyInt_FromLong(res);
  }

  static PyObject* getRecordDataSize(PyObject*, PyObject* args)
  {
    GET_SESS_PTR;    
    unsigned int res =  sess->getRecordDataSize();
    DBG("record data size %d\n",res);
    return PyInt_FromLong(res);
  }

  static PyObject* stopRecord(PyObject*, PyObject* args)
  {
    GET_SESS_PTR;    
    DBG("stopping record.");
    sess->stopRecord();
    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* flushPlaylist(PyObject*, PyObject* args)
  {
    GET_SESS_PTR;
    
    DBG("flushPlaylist()\n");
    sess->flushPlaylist();
    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* setPromptSet(PyObject*, PyObject* args)
  {
    char *name;
    if(!PyArg_ParseTuple(args,"s",&name))
      return NULL;

    GET_SESS_PTR;
    
    DBG("setPromptSet('%s')\n", name);
    try {
      sess->setPromptSet(name);
    } catch (DSMException& e) {
      PyErr_SetString(PyExc_RuntimeError, e.params["type"].c_str());
      return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* addSeparator(PyObject*, PyObject* args)
  {
    char *name;
    int front = 0;

    if(!PyArg_ParseTuple(args,"s|i",&name, &front))
      return NULL;

    GET_SESS_PTR;
    
    DBG("addSeparator('%s', front=%s)\n", name, front?"true":"false");
    sess->addSeparator(name, front);
    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* connectMedia(PyObject*, PyObject* args)
  {
    GET_SESS_PTR;    
    DBG("connectMedia.");
    sess->connectMedia();
    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* disconnectMedia(PyObject*, PyObject* args)
  {
    GET_SESS_PTR;    
    DBG("disconnectMedia.");
    sess->disconnectMedia();
    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* mute(PyObject*, PyObject* args)
  {
    GET_SESS_PTR;    
    DBG("mute.");
    sess->mute();
    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* unmute(PyObject*, PyObject* args)
  {
    GET_SESS_PTR;    
    DBG("unmute.");
    sess->unmute();
    Py_INCREF(Py_None);
    return Py_None;
  }

  static PyObject* B2BconnectCallee(PyObject*, PyObject* args)
  {
    char *remote_party;
    char *remote_uri;
    int relayed_invite = 0;

    if(!PyArg_ParseTuple(args,"ss|i", &remote_party, 
			 &remote_uri, &relayed_invite))
      return NULL;

    GET_SESS_PTR;
    
    DBG("B2BconnectCallee('%s', '%s', relayed_invite=%s)\n", remote_party, 
	remote_uri, relayed_invite?"true":"false");
    sess->B2BconnectCallee(remote_party, remote_uri, relayed_invite);
    Py_INCREF(Py_None);
    return Py_None;
  }


  static PyObject* B2BterminateOtherLeg(PyObject*, PyObject* args)
  {
    GET_SESS_PTR;    
    DBG("B2BterminateOtherLeg.");
    sess->B2BterminateOtherLeg();
    Py_INCREF(Py_None);
    return Py_None;
  }


  PyMethodDef session_methods[] = {
    {"setvar",   (PyCFunction)mod_py_setvar, METH_VARARGS,"set a session's variable"},
    {"var",      (PyCFunction)mod_py_getvar, METH_VARARGS,"get a session's variable"},
    {"select",   (PyCFunction)mod_py_getselect, METH_VARARGS,"get a session's select"},
    {"setError", (PyCFunction)mod_py_seterror, METH_VARARGS,"set error (errno)"},

    {"playPrompt",       (PyCFunction)playPrompt, METH_VARARGS,"play a prompt"},
    {"playFile",         (PyCFunction)playFile,   METH_VARARGS,"play a file"},
    {"recordFile",       (PyCFunction)recordFile, METH_VARARGS,"start recording to a file"},
    {"getRecordLength",  (PyCFunction)getRecordLength, METH_NOARGS,"get the length of the current recording"},
    {"getRecordDataSize",(PyCFunction)getRecordDataSize, METH_NOARGS,"get the data size of the current recording"},
    {"stopRecord",      (PyCFunction)stopRecord, METH_NOARGS,"stop the running recording"},
    {"flushPlaylist",   (PyCFunction)flushPlaylist, METH_NOARGS,"flush the playlist"},
    {"setPromptSet",    (PyCFunction)setPromptSet, METH_VARARGS,"set prompt set"},
    {"addSeparator",    (PyCFunction)addSeparator, METH_VARARGS,"add a named separator to playlist"},
    {"connectMedia",    (PyCFunction)connectMedia, METH_NOARGS,"connect media (RTP processing)"},
    {"disconnectMedia", (PyCFunction)disconnectMedia, METH_NOARGS,"disconnect media (RTP processing)"},
    {"mute",            (PyCFunction)mute, METH_NOARGS,"mute RTP)"},
    {"unmute",          (PyCFunction)unmute, METH_NOARGS,"unmute RTP"},
    {"B2BconnectCallee", (PyCFunction)B2BconnectCallee, METH_VARARGS,"connect callee of B2B leg"},
    {"B2BterminateOtherLeg", (PyCFunction)B2BterminateOtherLeg, METH_NOARGS,"terminate other leg of B2B call"},

    {NULL}  /* Sentinel */
  };

}
