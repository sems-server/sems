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
/** @file sems.h */
#ifndef _ans_machine_h_
#define _ans_machine_h_

#define SEMS_VERSION "${SEMS_VERSION}"
#define OS "${CMAKE_SYSTEM_NAME}"
#define ARCH "${CMAKE_SYSTEM_PROCESSOR}"

#define SEMS_APP_NAME "${SEMS_APP_NAME}"

#define CONFIG_FILE         "${SEMS_CFG_PREFIX}/etc/sems/sems.conf"
#define MOD_CFG_PATH        "${SEMS_CFG_PREFIX}/etc/sems/etc/"
#define ANNOUNCE_PATH       "${SEMS_AUDIO_PREFIX}/sems/audio"
#define ANNOUNCE_FILE       "default.wav"
#define PLUG_IN_PATH        "${SEMS_EXEC_PREFIX}/${SEMS_LIBDIR}/sems/plug-in"
#define RTP_LOWPORT         1024
#define RTP_HIGHPORT        0xffff
#define MAX_FORWARDS        70

#define DEFAULT_MAX_SHUTDOWN_TIME 10 // 10 seconds max for shutting down

#ifndef DISABLE_DAEMON_MODE
# define DEFAULT_DAEMON_MODE        true
# define DEFAULT_DAEMON_PID_FILE    "/var/local/run/sems.pid"
# define DEFAULT_DAEMON_UID         ""
# define DEFAULT_DAEMON_GID         ""
#endif

#define DEFAULT_SIGNATURE "Sip Express Media Server " \
		"(" SEMS_VERSION " (" ARCH "/" OS"))"

// session considered dead after 5 minutes no RTP
#define DEAD_RTP_TIME       5*60

/* Session Timer default configuration: */
#define DEFAULT_ENABLE_SESSION_TIMER 1
#define SESSION_EXPIRES              60 // seconds
#define MINIMUM_TIMER                5   //seconds

// threads to start for signaling/application
#define NUM_SESSION_PROCESSORS 10
// threads to start for RTP processing
#define NUM_MEDIA_PROCESSORS 1
// number of RTP receiver threads
#define NUM_RTP_RECEIVERS 1
// number of SIP servers to start
#define NUM_SIP_SERVERS 4

#define MAX_NET_DEVICES     32

extern const char* progname;

#endif
