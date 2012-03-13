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

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

#ifndef DISABLE_SYSLOG_LOG
# include <syslog.h>
#endif

#include <vector>
#include <string>

#include "AmApi.h"	/* AmLoggingFacility */
#include "AmThread.h"   /* AmMutex */
#include "log.h"


int log_level  = AmConfig::LogLevel;	/**< log level */
int log_stderr = AmConfig::LogStderr;	/**< non-zero if logging to stderr */

/** Map log levels to text labels */
const char* log_level2str[] = { "ERROR", "WARNING", "INFO", "DEBUG" };

/** Registered logging hooks */
static vector<AmLoggingFacility*> log_hooks;
static AmMutex log_hooks_mutex;

#ifndef DISABLE_SYSLOG_LOG

/**
 * Syslog Logging Facility (built-in plug-in)
 */
class SyslogLogFac : public AmLoggingFacility {
  int facility;		/**< syslog facility */

  void init() {
    openlog("sems", LOG_PID | LOG_CONS, facility);
    setlogmask(-1);
  }

 public:
  SyslogLogFac() : AmLoggingFacility("syslog"), facility(LOG_DAEMON) {
    init();
  }

  ~SyslogLogFac() {
    closelog();
  }

  int onLoad() {
    /* unused (because it is a built-in plug-in */
    return 0;
  }

  bool setFacility(const char* str);
  void log(int, pid_t, pthread_t, const char*, const char*, int, char*);
};

static SyslogLogFac syslog_log;

/** Set syslog facility */
bool SyslogLogFac::setFacility(const char* str) {
  static int local_fac[] = {
    LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3,
    LOG_LOCAL4, LOG_LOCAL5, LOG_LOCAL6, LOG_LOCAL7,
  };

  int new_facility = -1;

  if (!strcmp(str, "DAEMON")){
    new_facility = LOG_DAEMON;
  }
  else if (!strcmp(str, "USER")) {
    new_facility = LOG_USER;
  }
  else if (strlen(str) == 6 && !strncmp(str, "LOCAL", 5) &&
           isdigit(str[5]) && str[5] - '0' < 8) {
    new_facility = local_fac[str[5] - '0'];
  }
  else {
    ERROR("unknown syslog facility '%s'\n", str);
    return false;
  }

  if (new_facility != facility) {
    facility = new_facility;
    closelog();
    init();
  }

  return true;
}

void SyslogLogFac::log(int level, pid_t pid, pthread_t tid, const char* func, const char* file, int line, char* msg)
{
  static const int log2syslog_level[] = { LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG };
#ifdef _DEBUG

# ifndef NO_THREADID_LOG
#  ifdef LOG_LOC_DATA_ATEND
  syslog(log2syslog_level[level], "%s: %s [#%lx] [%s %s:%d]",
	 log_level2str[level], msg, (unsigned long)tid, func, file, line);
#  else
  syslog(log2syslog_level[level], "[#%lx] [%s, %s:%d] %s: %s",
	 (unsigned long)tid, func, file, line, log_level2str[level], msg);
#  endif
# else /* NO_THREADID_LOG */
#  ifdef LOG_LOC_DATA_ATEND
  syslog(log2syslog_level[level], "%s: %s [%s] [%s:%d]",
      log_level2str[level], msg, func, file, line);
#  else 
  syslog(log2syslog_level[level], "[%s, %s:%d] %s: %s",
	 func, file, line, log_level2str[level], msg);
#  endif
# endif /* NO_THREADID_LOG */

#else /* !_DEBUG */
#  ifdef LOG_LOC_DATA_ATEND
  syslog(log2syslog_level[level], "%s: %s [%s:%d]",
      log_level2str[level], msg, file, line);
#  else
  syslog(log2syslog_level[level], "[%s:%d] %s: %s",
	 file, line, log_level2str[level], msg);
#  endif

#endif /* !_DEBUG */
}

int set_syslog_facility(const char* str)
{
  return (syslog_log.setFacility(str) == true);
}

#endif /* !DISABLE_SYSLOG_LOG */


/**
 * Initialize logging
 */
void init_logging()
{
  log_hooks.clear();

#ifndef DISABLE_SYSLOG_LOG
  register_log_hook(&syslog_log);
#endif

  INFO("Logging initialized\n");
}

/**
 * Run log hooks
 */
void run_log_hooks(int level, pid_t pid, pthread_t tid, const char* func, const char* file, int line, char* msg)
{
  log_hooks_mutex.lock();

  if (!log_hooks.empty()) {
    for (vector<AmLoggingFacility*>::iterator it = log_hooks.begin();
         it != log_hooks.end(); ++it) {
      (*it)->log(level, pid, tid, func, file, line, msg);
    }
  }

  log_hooks_mutex.unlock();
}

/**
 * Register the log hook
 */
void register_log_hook(AmLoggingFacility* fac)
{
  AmLock lock(log_hooks_mutex);
  log_hooks.push_back(fac);
}
