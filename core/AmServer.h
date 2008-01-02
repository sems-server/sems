/*
 * $Id$
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
/** @file AmServer.h */
#ifndef _AmServer_h_
#define _AmServer_h_

#include "AmSipMsg.h"
#include "AmApi.h"


/**
 * \brief singleton, serve requests from ctrl interface
 *
 * The Server polls requests from the control interface and feeds 
 * them to registered handlers.
 */
class AmServer
{
private:
  /** 
   * Singleton pointer.
   * @see instance()
   */
  static AmServer* _instance;

  static AmCtrlInterface *ctrlIface;

  /** Avoid external instantiation. @see instance(). */
  ~AmServer(){}

public:
  /** Get a fifo server instance. */
  static AmServer* instance();

  /** Runs the fifo server. */
  void run();

  /** 
   * Register THE interface.
   * WARNING: only before the server starts up.
   */
  void regIface(const AmCtrlInterface *i);
  bool hasIface() { return ctrlIface != NULL; };

  static bool sendRequest(const AmSipRequest &, string &);
  static bool sendReply(const AmSipReply &);
  static string localURI(const string &displayName, 
      const string &userName, const string &hostName, 
      const string &uriParams, const string &hdrParams);
};

#endif

// Local Variables:
// mode:C++
// End:



