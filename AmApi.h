/*
 * $Id: AmApi.h,v 1.9.2.1 2005/06/01 12:00:24 rco Exp $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "AmRequest.h"
#include "AmThread.h"
#include "AmSession.h"

#include <string>
#include <map>
using std::map;
using std::string;

class AmSession;
class AmSessionEvent;
class AmDialogState;

#define FACTORY_EXPORT_NAME app_state_factory
#define FACTORY_EXPORT_STR  "app_state_factory"
#define EXPORT_FACTORY(class_name,app_name) class_name FACTORY_EXPORT_NAME (app_name)

class AmStateFactory
{
public:
    string app_name;
    AmStateFactory(const string& _app_name);
    virtual ~AmStateFactory() {}

    /**
     * Enables the plug-in to initialize whatever it needs.
     * Ex. load the configuration.
     * @return 0 everything was ok.
     * @return 1 on error.
     */
    virtual int onLoad();

    /**
     * Creates a dialog state on new request.
     * @return 0 if the request is not acceptable.
     *
     * Warning:
     *   This method should not make any expensive
     *   processing as it would block the server.
     */
    virtual AmDialogState* onInvite(AmCmd& cmd) = 0;
};

/** 
 * All your dialog relevant datas should be 
 * contained in a class inherited from ApiDialogState 
 */

class AmDialogState: public AmEventQueue, public AmEventHandler
{
    AmStateFactory* state_factory;
    AmSession*      session;

protected:
    /** @see AmEventHandler::process(AmEvent* event) */
    void process(AmEvent* event);

public:
    AmDialogState();
    virtual ~AmDialogState() {}

    AmStateFactory* getStateFactory();
    AmSession* getSession();

    /**
     * This must be called at the end of the application
     * so that the session can end and later be destroyed.
     */
    void stopSession() { getSession()->setStopped(); }

    bool sessionStopped() { return getSession()->getStopped(); }

    /**
     * onBeforeCallAccept will be called on incoming 
     * request before the call gets definitely established.
     * 
     * Throw AmSession::Exception if you want to 
     * signal any error.
     */
    virtual void onBeforeCallAccept(AmRequest* req);

    /**
     * onSessionStart will be called after call setup.
     *
     * Throw AmSession::Exception if you want to 
     * signal any error.
     * 
     * Warning:
     *   The session ends with this method. 
     *   Sems will NOT send any BYE on his own.
     */
    virtual void onSessionStart(AmRequest* req)=0;

    /**
     * onBye will be called if a BYE has been 
     * sent or received.
     *
     * Warning:
     *   This method should not make any expensive
     *   processing as it would block the session 
     *   cleaner.
     */
    virtual void onBye(AmRequest* req)=0;

    /**
     * onError will be called if an error
     * occured during call setup.
     */
    virtual void onError(unsigned int code, const string& reason);

    /**
     * onOther will be called on any other
     * event (ex. REFER).
     *
     * Warning:
     *   This method should not make any expensive
     *   processing as it would block the session.
     *
     * @return !=0 if any error occured.
     */
    virtual int onSessionEvent(AmSessionEvent* event);

    /**
     * onUACRequestStatus will be called on the result of a 
     * RequestUAC  (ex. REFER).
     *
     * Warning:
     *   This method should not make any expensive
     *   processing as it would block the session.
     *
     * @return !=0 if any error occured.
     */
    virtual int onUACRequestStatus(AmRequestUACStatusEvent* event);

    virtual void onDtmf(int event, int duration_msec);

    // needed for session creation
    friend void AmSessionContainer::startSession(const string& hash_str, 
						 string& sess_key,
						 AmRequestUAS* req);

    friend AmSession* AmSessionContainer::createSession(const string& app_name, 
							AmRequest* req);

};

#endif
// Local Variables:
// mode:C++
// End:
