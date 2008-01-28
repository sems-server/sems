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
/** @file AmIcmpWatcher.h */
#ifndef _AmIcmpWatcher_h_
#define _AmIcmpWatcher_h_

#include "AmThread.h"

#include <map>

#define ICMP_BUF_SIZE 512

class AmRtpStream;

/** \brief thread that watches ICMP reports  */
class AmIcmpWatcher: public AmThread
{
  static AmIcmpWatcher* _instance;

  /* RAW socket descriptor */
  int raw_sd;

  /* RTP Stream map */
  std::map<int,AmRtpStream*> stream_map;
  AmMutex               stream_map_m;

  /* constructor & destructor are
   * private as we want a singleton.
   */
  AmIcmpWatcher();
  ~AmIcmpWatcher();

  void run();
  void on_stop();

 public:
  static AmIcmpWatcher* instance();
  void addStream(int localport, AmRtpStream* str);
  void removeStream(int localport);
};

/** \brief one-shot thread: report an ICMP error to the rtp stream */
class IcmpReporter: public AmThread
{
  AmRtpStream* rtp_str;
  void run();
  void on_stop();
 public:
  IcmpReporter(AmRtpStream* str);
};

#endif
