/*
 * Copyright (C) 2011 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef _CC_PREPAID_XMLRPC_H
#define _CC_PREPAID_XMLRPC_H

#include "AmApi.h"

#include "SBCCallProfile.h"

#include <map>

/**
 * sample call control module
 */
class PrepaidXMLRPC : public AmDynInvoke
{
  static PrepaidXMLRPC* _instance;

  string serverAddress;
  unsigned int port;
  string uri;

  std::map<string, unsigned int> credits;
  AmMutex credits_mut;


  /** @returns credit for pin, found=false if pin wrong */
  int getCredit(string pin, bool& found);
  /** @returns remaining credit */
  int subtractCredit(string pin, int amount, bool& found);

  /** adds some amount */
  int addCredit(string pin, int amount);
  /** sets the value to some amount */
  int setCredit(string pin, int amount);


  void start(const string& cc_name, const string& ltag, SBCCallProfile* call_profile,
	     int start_ts_sec, int start_ts_usec, const AmArg& values,
	     int timer_id, AmArg& res);
  void connect(const string& cc_name, const string& ltag, SBCCallProfile* call_profile,
	       const string& other_ltag,
	       int connect_ts_sec, int connect_ts_usec);
  void end(const string& cc_name, const string& ltag, SBCCallProfile* call_profile,
	   int start_ts_sec, int start_ts_usec,
	   int connect_ts_sec, int connect_ts_usec,
	   int end_ts_sec, int end_ts_usec);

 public:
  PrepaidXMLRPC();
  ~PrepaidXMLRPC();
  static PrepaidXMLRPC* instance();
  void invoke(const string& method, const AmArg& args, AmArg& ret);
  int onLoad();
};

#endif 
