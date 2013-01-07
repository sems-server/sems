/*
 * Copyright (C) 2002-2003 Fhg Fokus
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
#ifndef _AmSessionEventHandler_h
#define _AmSessionEventHandler_h

#include "AmArg.h"
#include "AmSipMsg.h"
#include "AmSipEvent.h"
#include "AmSipDialog.h"
#include <string>
using std::string;

class AmConfigReader;

/**
 * \brief Interface for SIP signaling plugins that
 *        change requests or replies using hooks (ex: session timer).
 * 
 *        From this type of plugins, an AmSessionEventHandler
 *        can be added to an AmSession. AmSessionEventHandler functions
 *        are called as hooks by the Session's event handler function.
 *
 *  Signaling plugins must inherit from this class.
 */
class AmSessionEventHandler
  : public AmObject
{
public:
  bool destroy;

  AmSessionEventHandler()
    : destroy(true) {}

  virtual ~AmSessionEventHandler() {}

  /** Returns -1 on error, 0 else. */
  virtual int configure(AmConfigReader& conf)
  { return 0; }

  /* 
   * All the methods return true if the event processing 
   * shall be stopped after them.
   */
  virtual bool process(AmEvent*)
  { return false; }

  virtual bool onSipRequest(const AmSipRequest& req)
  { return false; }

  virtual bool onSipReply(const AmSipRequest& req, 
  			  const AmSipReply& reply, 
  			  AmBasicSipDialog::Status old_dlg_status)
  { return false; }

  virtual bool onSendRequest(AmSipRequest& req, int& flags)
  { return false; }

  virtual bool onSendReply(const AmSipRequest& req, AmSipReply& reply, int& flags)
  { return false; }

  virtual void onRequestSent(const AmSipRequest& req) {}

  virtual void onReplySent(const AmSipRequest& req, const AmSipReply& reply) {}

  virtual void onRemoteDisappeared(const AmSipReply& reply) {}

  virtual void onLocalTerminate(const AmSipReply& reply) {}

  virtual void onFailure() {}
};


#if __GNUC__ < 3
#define CALL_EVENT_H(method,args...) \
            do{\
                vector<AmSessionEventHandler*>::iterator evh = ev_handlers.begin(); \
                bool stop = false; \
                while((evh != ev_handlers.end()) && !stop){ \
                    stop = (*evh)->method( ##args ); \
                    evh++; \
		} \
		if(stop) \
                    return; \
            }while(0)
#else
#define CALL_EVENT_H(method,...) \
            do{\
                vector<AmSessionEventHandler*>::iterator evh = ev_handlers.begin(); \
                bool stop = false; \
                while((evh != ev_handlers.end()) && !stop){ \
                    stop = (*evh)->method( __VA_ARGS__ ); \
                    evh++; \
		} \
		if(stop) \
                    return; \
            }while(0)
#endif

#endif
