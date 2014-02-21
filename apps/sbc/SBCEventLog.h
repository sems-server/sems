/*
 * Copyright (C) 2012-2013 FRAFOS GmbH
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

#ifndef _SBCEventLog_h_
#define _SBCEventLog_h_

#include "singleton.h"
#include "AmArg.h"
#include "AmSipMsg.h"
#include "AmBasicSipDialog.h"

#include <memory>
#include <string>
#include <map>
using std::auto_ptr;
using std::string;
using std::map;

struct SBCEventLogHandler
{
  virtual void logEvent(long int timestamp, const string& id, 
			const string& type, const AmArg& ev)=0;
};

class _SBCEventLog
{
  auto_ptr<SBCEventLogHandler> log_handler;

protected:
  _SBCEventLog() {}
  ~_SBCEventLog() {}

public:
  void useMonitoringLog();
  void setEventLogHandler(SBCEventLogHandler* lh);
  void logEvent(const string& id, const string& type, const AmArg& event);

  void logCallStart(const AmSipRequest& req, const string& local_tag,
		    const string& from_remote_ua, const string& to_remote_ua,
		    int code, const string& reason);


  void logCallEnd(const AmSipRequest& req,
		  const string& local_tag,
		  const string& reason,
		  struct timeval* tv);

  void logCallEnd(const AmBasicSipDialog* dlg,
		  const string& reason,
		  struct timeval* tv);
};

typedef singleton<_SBCEventLog> SBCEventLog;

#endif
