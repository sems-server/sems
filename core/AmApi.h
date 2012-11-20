/*
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2006 iptego GmbH
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
/** @file AmApi.h */
#ifndef _AmApi_h_
#define _AmApi_h_

#include "AmThread.h"
#include "AmSipMsg.h"
#include "AmConfig.h"
#include "AmConfigReader.h"
#include "AmArg.h"
#include "AmEventQueue.h"
#include "atomic_types.h"

#include <stdarg.h>

#include <string>
using std::string;

/**
 * \brief interface of the DynInvoke API
 */
class AmDynInvoke
{
 public:
  /** \brief NotImplemented result for DI API calls */
  struct NotImplemented {
    string what;
    NotImplemented(const string& w)
      : what(w) {}
  };

  AmDynInvoke();
  virtual ~AmDynInvoke();
  virtual void invoke(const string& method, const AmArg& args, AmArg& ret);
};

/**
 * \brief Base interface for plugin factories
 */
class AmPluginFactory
  : public virtual atomic_ref_cnt
{
  string plugin_name;

 public:
  AmPluginFactory(const string& name)
    : plugin_name(name) {}

  virtual ~AmPluginFactory() {}

  const string& getName() { return plugin_name; } 

  /**
   * Enables the plug-in to initialize whatever it needs.
   * Ex. load the configuration.
   * @return 0 everything was ok.
   * @return 1 on error.
   */
  virtual int onLoad()=0;
};

/**
 * \brief Interface of factory for plugins that provide a DI API
 * 
 * Factory for multi-purpose plugin classes,
 * classes that provide a DynInvoke (DI) API
 */
class AmDynInvokeFactory: public AmPluginFactory
{
 public:
  AmDynInvokeFactory(const string& name);
  virtual AmDynInvoke* getInstance()=0;
};


class AmSession;
class AmSessionEventHandler;
/**
 * \brief Interface for PluginFactories that can handle events in sessions
 */
class AmSessionEventHandlerFactory: public AmPluginFactory
{
 public:
  AmSessionEventHandlerFactory(const string& name);

  virtual AmSessionEventHandler* getHandler(AmSession*)=0;

  /**
   * @return false if session creation should be stopped
   */
  virtual bool onInvite(const AmSipRequest& req, AmConfigReader& cfg)=0;
  virtual bool onInvite(const AmSipRequest& req, AmArg& session_params, AmConfigReader& cfg);
};

/** \brief Interface for plugins to create sessions */
class AmSessionFactory: public AmPluginFactory
{

 protected:
  /**
   * This reads the module configuration from 
   * cfg into the modules mod_conf.
   */
  int configureModule(AmConfigReader& cfg);

 public:
  /**
   * This function applys the module configuration 
   */
  void configureSession(AmSession* sess);

  AmSessionFactory(const string& name);

  /**
   * Creates a dialog state on new UAS request.
   * @return 0 if the request is not acceptable.
   *
   * Warning:
   *   This method should not make any expensive
   *   processing as it would block the server.
   */
  virtual AmSession* onInvite(const AmSipRequest& req, const string& app_name,
			      const map<string,string>& app_params)=0;

  /**
   * Creates a dialog state on new UAC request. 
   * @param session_params parameters passed to the new session by the caller.
   * 
   * @return 0 if the request is not acceptable.
   *
   * Warning:
   *   This method should not make any expensive
   *   processing as it would block the server.
   */
  virtual AmSession* onInvite(const AmSipRequest& req, const string& app_name,
			      AmArg& session_params);

  /**
   * Creates a dialog state on new REFER with local-tag.
   * @return 0 if the request is not acceptable.
   *
   * Warning:
   *   This method should not make any expensive
   *   processing as it would block the server.
   */
  virtual AmSession* onRefer(const AmSipRequest& req, const string& app_name,
			     const map<string,string>& app_params);

  /**
   * Creates a dialog state on new REFER with local-tag.
   * Passes session_params to the new session.
   * @return 0 if the request is not acceptable.
   *
   * Warning:
   *   This method should not make any expensive
   *   processing as it would block the server.
   */
  virtual AmSession* onRefer(const AmSipRequest& req, const string& app_name,
			     AmArg& session_params);

  /**
   * Method to receive any out-of-dialog request 
   * other than INVITE and REFER
   *
   * Warning:
   *   This method should not make any expensive
   *   processing as it would block the thread 
   *   posting the event!
   */
  virtual void onOoDRequest(const AmSipRequest& req);
};

/** \brief Interface for plugins that implement a
 *     logging facility
 */
class AmLoggingFacility : public AmPluginFactory
{

 public:
  AmLoggingFacility(const string& name);
  virtual ~AmLoggingFacility() { }

  /**
   * This method is called when logging a message if the instance
   * is registered as a logging hook.
   *
   * @param level log level
   * @param pid   process ID
   * @param tid   thread ID
   * @param func  function name
   * @param file  file name
   * @param line  line number
   * @param msg   message
   */
  virtual void log(int level, pid_t pid, pthread_t tid, const char* func, const char* file, int line, char* msg) = 0;
};

#if  __GNUC__ < 3
#define EXPORT_FACTORY(fctname,class_name,args...) \
            extern "C" void* fctname()\
            {\
		return new class_name(##args);\
	    }
#else
#define EXPORT_FACTORY(fctname,class_name,...) \
            extern "C" void* fctname()\
            {\
		return new class_name(__VA_ARGS__);\
	    }
#endif

typedef void* (*FactoryCreate)();

#define STR(x) #x
#define XSTR(x) STR(x)

#define FACTORY_SESSION_EXPORT      session_factory_create
#define FACTORY_SESSION_EXPORT_STR  XSTR(FACTORY_SESSION_EXPORT)

#define EXPORT_SESSION_FACTORY(class_name,app_name) \
            EXPORT_FACTORY(FACTORY_SESSION_EXPORT,class_name,app_name)

#define FACTORY_SESSION_EVENT_HANDLER_EXPORT     sess_evh_factory_create
#define FACTORY_SESSION_EVENT_HANDLER_EXPORT_STR XSTR(FACTORY_SESSION_EVENT_HANDLER_EXPORT)

#define EXPORT_SESSION_EVENT_HANDLER_FACTORY(class_name,app_name) \
            EXPORT_FACTORY(FACTORY_SESSION_EVENT_HANDLER_EXPORT,class_name,app_name)

#define FACTORY_PLUGIN_EXPORT     base_plugin_create
#define FACTORY_PLUGIN_EXPORT_STR XSTR(FACTORY_PLUGIN_EXPORT)

#define EXPORT_PLUGIN_FACTORY(class_name,app_name) \
            EXPORT_FACTORY(FACTORY_PLUGIN_EXPORT,class_name,app_name)

#define FACTORY_PLUGIN_CLASS_EXPORT     plugin_class_create
#define FACTORY_PLUGIN_CLASS_EXPORT_STR XSTR(FACTORY_PLUGIN_CLASS_EXPORT)

#define EXPORT_PLUGIN_CLASS_FACTORY(class_name,app_name) \
            EXPORT_FACTORY(FACTORY_PLUGIN_CLASS_EXPORT,class_name,app_name)

#define FACTORY_SIP_EVENT_HANDLER_EXPORT     sip_evh_factory_create
#define FACTORY_SIP_EVENT_HANDLER_EXPORT_STR XSTR(FACTORY_SIP_EVENT_HANDLER_EXPORT)

#define EXPORT_SIP_EVENT_HANDLER_FACTORY(class_name,app_name) \
            EXPORT_FACTORY(FACTORY_SIP_EVENT_HANDLER_EXPORT,class_name,app_name)

#define FACTORY_LOG_FACILITY_EXPORT     log_facilty_factory_create
#define FACTORY_LOG_FACILITY_EXPORT_STR XSTR(FACTORY_LOG_FACILITY_EXPORT)

#define EXPORT_LOG_FACILITY_FACTORY(class_name,app_name) \
            EXPORT_FACTORY(FACTORY_LOG_FACILITY_EXPORT,class_name,app_name)

// ---------------- simplified SEMS plug-in interface  --------------------------
// - export module as basic SEMS plugin with EXPORT_MODULE_FUNC
// - in onLoad, register the capabilities you provide,
//    e.g. AmPlugIn::registerApplication(...), AmPlugIn::registerDIInterface(...) etc

#define EXPORT_MODULE_FUNC(class_name)	\
  extern "C" void* base_plugin_create()		\
  {						\
    return class_name::instance();		\
  }

#define EXPORT_MODULE_FACTORY(class_name) \
            EXPORT_MODULE_FUNC(class_name)


// - use DECLARE_MODULE_INSTANCE/DEFINE_MODULE_INSTANCE to save some typing when
//   creating plugins with DI Interface

#define DEFINE_MODULE_INSTANCE(class_name, mod_name)	\
							\
  class_name* class_name::_instance=0;			\
							\
  class_name* class_name::instance()			\
  {							\
  if(_instance == NULL)					\
    _instance = new class_name(mod_name);		\
  return _instance;					\
  }

#define DECLARE_MODULE_INSTANCE(class_name)	\
  static class_name* _instance;			\
  static class_name* instance();


#endif // _AmApi_h_
