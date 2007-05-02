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

#include "log.h"

#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#include <vector>
#include "AmApi.h"

int log_level=L_INFO;
int log_stderr=0;
vector<AmLoggingFacility*> log_hooks;


inline const char* level2txt(int level)
{
  switch(level){
  case L_ERR:  return "ERROR";
  case L_WARN: return "WARNING";
  case L_INFO: return "INFO";
  case L_DBG:  return "DEBUG";
  }
  return "";
}

void init_log()
{
  openlog(LOG_NAME, LOG_PID|LOG_CONS,L_FAC);
  setlogmask( -1 );
}

void dprint(int level, const char* fct, char* file, int line, char* fmt, ...)
{
  va_list ap;
    
  fprintf(stderr, "(%i) %s: %s (%s:%i): ",(int)getpid(), level2txt(level), fct, file, line);
  va_start(ap, fmt);
  vfprintf(stderr,fmt,ap);
  fflush(stderr);
  va_end(ap);
}

void log_print (int level, char* fmt, ...)
{
  va_list ap;

  fprintf(stderr, "(%i) %s: ",(int)getpid(),level2txt(level));
  va_start(ap, fmt);
  vfprintf(stderr,fmt,ap);
  fflush(stderr);
  va_end(ap);
}

void log_fac_print(int level, const char* fct, char* file, int line, char* fmt, ...)
{
  va_list ap;
  char logline[512];
  int len;

  if(log_hooks.empty()) return;

  // file, line etc
  len = snprintf(logline, sizeof(logline), "(%i) %s: %s (%s:%i): ",
                    (int)getpid(), level2txt(level), fct, file, line);
  // dbg msg
  va_start(ap, fmt);
  vsnprintf(logline+len, sizeof(logline)-len, fmt, ap);
  va_end(ap);

  for(unsigned i=0; i<log_hooks.size(); i++) log_hooks[i]->log(level, logline);
}

void register_logging_fac(void* vp)
{
  AmLoggingFacility* lf = static_cast<AmLoggingFacility*>(vp);
#ifdef _DEBUG
  assert(lf != NULL);
#endif
  log_hooks.push_back(lf);
}
