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

#ifndef _SYSLOG_CDR_H
#define _SYSLOG_CDR_H

#include "AmApi.h"
#include "AmThread.h"
#include "AmEventProcessingThread.h"

#include <sys/time.h>
#include <stdio.h>

#include <map>
#include <memory>

class CDRWriterThread;

#define CDR_EVENT_ID 7253872 // doesn't really matter

struct CDR {
  string ltag;
  string callid;
  string from_tag;
  string to_tag;
  int start_ts;
  int connect_ts;
  int end_ts;
  std::map<string, string> values;

CDR()
: start_ts(0), connect_ts(0), end_ts(0) { }

  string print_csv();
  string print_headers();
};

/**
 * accounting for generating CDR lines in CSV format in syslog
 */
class SyslogCDR : public AmDynInvoke
{
  static SyslogCDR* _instance;

  int level;
  string syslog_prefix;

  map<string, CDR*> cdrs;
  AmMutex cdrs_mut;

  void start(const string& ltag, const string& callid, const string& from_tag,
	     int start_ts, AmArg& values);
  void connect(const string& ltag, const string& callid, const string& to_tag,
	       int connect_ts);
  void end(const string& ltag, const string& callid, int end_ts);

 public:
  SyslogCDR();
  ~SyslogCDR();
  static SyslogCDR* instance();
  void invoke(const string& method, const AmArg& args, AmArg& ret);
  int onLoad();
};

#endif 
