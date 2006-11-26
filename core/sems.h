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

#ifndef _ans_machine_h_
#define _ans_machine_h_

#define CONFIG_FILE         "/usr/local/etc/sems/sems.conf"
#define MOD_CFG_PATH        "/usr/local/etc/sems/"
#define SER_FIFO            "/tmp/ser_fifo"
#define FIFO_NAME           "/tmp/am_fifo"
#define SEND_METHOD         "unix"
#define ANNOUNCE_PATH       "/usr/local/lib/sems/audio"
#define ANNOUNCE_FILE       "default.wav"
#define PLUG_IN_PATH        "/usr/local/lib/sems/plug-in"
#define DEFAULT_ANNOUNCE    "default.wav"
#define SMTP_ADDRESS_IP     "localhost"
#define SMTP_PORT           25
#define DEFAULT_RECORD_TIME 30
#define DEFAULT_DAEMON_MODE 1
#define PREFIX_SEPARATOR    ""
#define RTP_LOWPORT         1024
#define RTP_HIGHPORT        0xffff

/* Session Timer: -ssa */
#define DEFAULT_ENABLE_SESSION_TIMER 1
#define SESSION_EXPIRES              10 // seconds
#define MINIMUM_TIMER                5   //seconds

#define NUM_MEDIA_PROCESSORS 1

#define MAX_NET_DEVICES     32

extern const char* progname;

#endif
