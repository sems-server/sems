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
/** @file AmSipEvent.h */
#ifndef AmSipEvent_h
#define AmSipEvent_h

#include "AmEvent.h"
#include "AmSipMsg.h"

class AmBasicSipDialog;

/** \brief base class for SIP events */
class AmSipEvent: public AmEvent
{
 public:
  AmSipEvent()
    : AmEvent(-1)
    {}

  AmSipEvent(const AmSipEvent& ev)
    : AmEvent(ev)
    {}

  virtual void operator() (AmBasicSipDialog* dlg)=0;
};

/** \brief UAS reply re-transmission timeout event */
class AmSipTimeoutEvent: public AmSipEvent
{
 public:

  enum EvType {
    _noEv=0,
    noACK,
    noPRACK,
  };

  EvType       type;

  unsigned int cseq;
  AmSipRequest req;
  AmSipReply   rpl;

  AmSipTimeoutEvent(EvType t, unsigned int cseq_num)
    : AmSipEvent(), type(t), cseq(cseq_num)
   {}
  AmSipTimeoutEvent(EvType t, AmSipRequest &_req, AmSipReply &_rpl)
    : AmSipEvent(), type(t), cseq(_req.cseq), req(_req), rpl(_rpl)
    {}

  virtual void operator() (AmBasicSipDialog* dlg);
};

/** \brief SIP request event */
class AmSipRequestEvent: public AmSipEvent
{
 public:
  AmSipRequest req;
    
  AmSipRequestEvent(const AmSipRequest& r)
    : AmSipEvent(), req(r)
    {}

  virtual void operator() (AmBasicSipDialog* dlg);
};

/** \brief SIP reply event */
class AmSipReplyEvent: public AmSipEvent
{
 public:
  AmSipReply reply;

  AmSipReplyEvent(const AmSipReply& r) 
    : AmSipEvent(),reply(r) {}

  virtual void operator() (AmBasicSipDialog* dlg);
};



#endif
