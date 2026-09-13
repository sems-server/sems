/*
 * Copyright (C) 2007 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#ifndef _REG_AG_H_
#define _REG_AG_H_

#include "AmSession.h"
#include "AmConfigReader.h"
#include "AmThread.h"

#include <string>
using std::string;
#include <vector>
using std::vector;

struct RegInfo {
  string domain;
  string user;
  string display_name;
  string auth_user;
  string passwd;
  string proxy;
  string contact;

  string handle;
};

class RegThread : public AmThread {

  vector<RegInfo> registrations;

  /** set once the thread is asked to leave run(); doubles as the
      interruptible sleep between two registration check rounds */
  AmCondition<bool> stop_requested;

  void create_registration(RegInfo& ri);
  bool check_registration(const RegInfo& ri);

  /** wait up to msec milliseconds,
      @return true if a stop has been requested meanwhile */
  bool wait_or_stop(unsigned long msec);

 protected:
  void run();
  void on_stop();
 public:
  RegThread() : stop_requested(false) {}

  /** ask run() to leave its loop; does not wait for the thread */
  void request_stop() { stop_requested.set(true); }

  void add_reg(const RegInfo& ri);
  void postEvent(AmEvent* ev);
};

class RegistrationAgentFactory: public AmSessionFactory
{
  RegThread dialer;

 public:
  RegistrationAgentFactory(const string& _app_name);
  ~RegistrationAgentFactory();

  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);
  void postEvent(AmEvent* ev);
};

#endif

