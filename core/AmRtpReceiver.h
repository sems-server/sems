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
/** @file AmRtpReceiver.h */
#ifndef _AmRtpReceiver_h_
#define _AmRtpReceiver_h_

#include "AmThread.h"

#include <sys/select.h>

#include <map>
using std::greater;

class AmRtpStream;

/**
 * \brief receiver for RTP for all streams.
 *
 * The RtpReceiver receives RTP packets for all streams 
 * that are registered to it. It places the received packets in 
 * the stream's buffer. 
 */
class AmRtpReceiver: public AmThread {

  typedef std::map<int, AmRtpStream*, greater<int> > Streams;

  static AmRtpReceiver* _instance;

    
  Streams  streams;
  AmMutex  streams_mut;

  //fd_set   fds;
  struct pollfd* fds;
  unsigned int   nfds;
  AmMutex        fds_mut;
    
  AmRtpReceiver();
  ~AmRtpReceiver();
    
  void run();
  void on_stop();
    
public:
  static AmRtpReceiver* instance();
  void addStream(int sd, AmRtpStream* stream);
  void removeStream(int sd);
};

#endif

// Local Variables:
// mode:C++
// End:
