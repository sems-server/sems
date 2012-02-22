/*
 * Copyright (C) 2010-2011 Raphael Coeffic
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
/** @file AmOfferAnswer.h */
#ifndef AmOfferAnswer_h
#define AmOfferAnswer_h

#include "AmSdp.h"
#include "AmSipMsg.h"

class AmSipDialog;

class AmOfferAnswer 
{
public:
  enum OAState {
    OA_None=0,
    OA_OfferRecved,
    OA_OfferSent,
    OA_Completed,
    __max_OA
  };

private:
  OAState      state;
  OAState      saved_state;
  unsigned int cseq;
  AmSdp        sdp_remote;
  AmSdp        sdp_local;

  AmSipDialog* dlg;

  /** State maintenance */
  void saveState();
  int  checkStateChange();

  /** SDP handling */
  int  onRxSdp(unsigned int m_cseq, const AmMimeBody& body, const char** err_txt);
  int  onTxSdp(unsigned int m_cseq, const AmMimeBody& body);
  int  getSdpBody(string& sdp_body);

public:
  /** Constructor */
  AmOfferAnswer(AmSipDialog* dlg);

  /** Accessors */
  OAState getState();
  void setState(OAState n_st);
  const AmSdp& getLocalSdp();
  const AmSdp& getRemoteSdp();

  void clear();
  void clearTransitionalState();

  /** Event handlers */
  int onRequestIn(const AmSipRequest& req);
  int onReplyIn(const AmSipReply& reply);
  int onRequestOut(AmSipRequest& req);
  int onReplyOut(AmSipReply& reply);
  int onRequestSent(const AmSipRequest& req);
  int onReplySent(const AmSipReply& reply);
  void onNoAck(unsigned int ack_cseq);
};

#endif
