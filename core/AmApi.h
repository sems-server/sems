/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2006 iptego GmbH
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _AmApi_h_
#define _AmApi_h_

#include "AmThread.h"
#include "AmSipRequest.h"
#include "AmSipReply.h"
#include "AmConfig.h"
#include "AmArg.h"

#include <string>
#include <map>
using std::map;
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
    virtual void invoke(const string& method, const AmArgArray& args, AmArgArray& ret);
};

/**
 * \brief Base interface for plugin factories
 */
class AmPluginFactory
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
     * @return true if session creation should be stopped
     */
    virtual bool onInvite(const AmSipRequest& req)=0;
};

/** \brief Interface for plugins to create sessions */
class AmSessionFactory: public AmPluginFactory
{

  AmSessionTimerConfig mod_conf;

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
     * Creates a dialog state on new request.
     * @return 0 if the request is not acceptable.
     *
     * Warning:
     *   This method should not make any expensive
     *   processing as it would block the server.
     */
    virtual AmSession* onInvite(const AmSipRequest& req)=0;

    /**
     * method to receive an Event that is posted
	 * to  the factory
	 *
     * Warning:
     *   This method should not make any expensive
     *   processing as it would block the thread 
	 *   posting the event!
     */
    virtual void postEvent(AmEvent* ev);	

};

/** \brief Interface for plugins that implement session-less 
 *     UA behaviour (e.g. registrar client, event notification 
 *     client)
 */
class AmSIPEventHandler : public AmPluginFactory 
{

public:
	AmSIPEventHandler(const string& name);
	virtual ~AmSIPEventHandler() { }

	/** will be called on incoming replies which do 
	 *  not belong to a dialog of a session in the 
	 *  SessionContainer.
	 *
	 *  @return true if reply was handled by plugin, false 
	 *          otherwise
	 */
	virtual bool onSipReply(const AmSipReply& rep) = 0;
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

#define FACTORY_PLUGIN_CLASS_EXPORT     plugin_class_create
#define FACTORY_PLUGIN_CLASS_EXPORT_STR XSTR(FACTORY_PLUGIN_CLASS_EXPORT)

#define EXPORT_PLUGIN_CLASS_FACTORY(class_name,app_name) \
            EXPORT_FACTORY(FACTORY_PLUGIN_CLASS_EXPORT,class_name,app_name)

#define FACTORY_SIP_EVENT_HANDLER_EXPORT     sip_evh_factory_create
#define FACTORY_SIP_EVENT_HANDLER_EXPORT_STR XSTR(FACTORY_SIP_EVENT_HANDLER_EXPORT)

#define EXPORT_SIP_EVENT_HANDLER_FACTORY(class_name,app_name) \
            EXPORT_FACTORY(FACTORY_SIP_EVENT_HANDLER_EXPORT,class_name,app_name)

#endif
// Local Variables:
// mode:C++
// End:
