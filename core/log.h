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

#ifndef _log_h_
#define _log_h_

#include <syslog.h>

#ifdef __cplusplus
extern "C" {
#endif

#define L_ERR    0
#define L_WARN   1
#define L_INFO   2
#define L_DBG    3

  /* log facility (see syslog(3)) */
#define L_FAC  LOG_DAEMON
  /* priority at which we log */
#define DPRINT_PRIO LOG_DEBUG

#define LOG_NAME "Sems"

  extern int log_level;
  extern int log_stderr;

  void init_log();

  void dprint (int level, const char* fct, char* file, int line, char* fmt, ...);
  void log_print (int level, char* fmt, ...);
  void register_logging_fac(void*);
  void log_fac_print(int level, const char* fct, char* file, int line, char* fmt, ...);

#ifdef _DEBUG
#define LOG_PRINT(level, args... ) dprint( level, __FUNCTION__, __FILE__, __LINE__, ##args )
#else
#define LOG_PRINT(level, args... ) log_print( level , ##args )
#endif
#define LOG_FAC_PRINT(level, args... ) log_fac_print( level, __FUNCTION__, __FILE__, __LINE__, ##args )

#if  __GNUC__ < 3
#define _LOG(level,fmt...) LOG_PRINT(level,##fmt)
#else
#define _LOG(level, fmt, args...) \
          do{\
              if((level)<=log_level) {\
		  if(log_stderr)\
		      LOG_PRINT( level, fmt, ##args );\
		  else {\
		      switch(level){\
		      case L_ERR:\
			  syslog(LOG_ERR | L_FAC, "Error: (%s)(%s)(%i): " fmt, __FILE__, __FUNCTION__, __LINE__, ##args);\
			  break;\
		      case L_WARN:\
			  syslog(LOG_WARNING | L_FAC, "Warning: (%s)(%s)(%i): " fmt, __FILE__, __FUNCTION__, __LINE__, ##args);\
			  break;\
		      case L_INFO:\
			  syslog(LOG_INFO | L_FAC, "Info: (%s)(%s)(%i): " fmt, __FILE__, __FUNCTION__, __LINE__, ##args);\
			  break;\
		      case L_DBG:\
			  syslog(LOG_DEBUG | L_FAC, "Debug: (%s)(%s)(%i): " fmt, __FILE__, __FUNCTION__, __LINE__, ##args);\
			  break;\
		      }\
		  }\
              }\
              LOG_FAC_PRINT( level, fmt, ##args );\
	  }while(0)
#endif


#define DBG(args...) _LOG(L_DBG, ##args)
#define ERROR(args...) _LOG(L_ERR, ##args)
#define WARN(args...)  _LOG(L_WARN,  ##args)
#define INFO(args...)  _LOG(L_INFO,  ##args)

#ifdef __cplusplus
}
#endif

#endif
