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

/** @file log.h */
#ifndef _log_h_
#define _log_h_

#include <sys/types.h>	/* pid_t */
#include <stdio.h>
#include <unistd.h>	/* getpid() */
#include <pthread.h>	/* pthread_self() */
#include <execinfo.h>   /* backtrace_symbols() */


#ifdef __cplusplus
extern "C" {
# if 0
}
# endif
#endif

/**
 * @{ Log levels
 */
enum Log_Level {
  L_ERR = 0,
  L_WARN,
  L_INFO,
  L_DBG
};
/** @} */

#define FIX_LOG_LEVEL(level) \
  ((level) < L_ERR ? L_ERR : ((level) > L_DBG ? L_DBG : (level)))

#ifdef __cplusplus
# ifdef PRETTY_FUNCTION_LOG
#  define FUNC_NAME __PRETTY_FUNCTION__
# else
#  define FUNC_NAME __FUNCTION__
#endif
#else
# define FUNC_NAME __FUNCTION__
#endif

#ifdef __linux
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
# include <linux/unistd.h>
# include <sys/syscall.h>
# define GET_PID() syscall(__NR_gettid)
#else
# define GET_PID() getpid()
#endif

#ifdef _DEBUG
# ifndef NO_THREADID_LOG
#  define GET_TID() pthread_self()
#  define LOC_FMT   " [#%lx/%u] [%s, %s:%d]"
#  define LOC_DATA  (unsigned long)tid_, pid_, FUNC_NAME, __FILE__, __LINE__
# else
#  define GET_TID() 0
#  define LOC_FMT   " [%u] [%s %s:%d]"
#  define LOC_DATA  pid_, FUNC_NAME,  __FILE__, __LINE__
# endif
#else
# define GET_TID()   0
# define LOC_FMT   " [%u/%s:%d]"
# define LOC_DATA  pid_, __FILE__, __LINE__
#endif

#ifdef LOG_LOC_DATA_ATEND
#define COMPLETE_LOG_FMT "%s: %s" LOC_FMT "\n", log_level2str[level_], msg_, LOC_DATA
#else
#define COMPLETE_LOG_FMT LOC_FMT " %s: %s" "\n", LOC_DATA, log_level2str[level_], msg_
#endif

#ifndef LOG_BUFFER_LEN
#define LOG_BUFFER_LEN 4096
#endif

/* The underscores in parameter and local variable names are there to
   avoid collisions. */
#define _LOG(level__, fmt, args...)					\
  do {									\
    int level_ = FIX_LOG_LEVEL(level__);				\
									\
    if ((level_) <= log_level) {					\
      pid_t pid_ = GET_PID();						\
      pthread_t tid_ = GET_TID();					\
      char msg_[LOG_BUFFER_LEN];					\
      int n_ = snprintf(msg_, sizeof(msg_), fmt, ##args);		\
      if (msg_[n_ - 1] == '\n') msg_[n_ - 1] = '\0';			\
									\
      if (log_stderr) {							\
	fprintf(stderr, COMPLETE_LOG_FMT);				\
	fflush(stderr);							\
      }									\
      run_log_hooks(level_, pid_, tid_, FUNC_NAME, __FILE__, __LINE__, msg_); \
    }									\
  } while(0)

/**
 * @{ Logging macros
 */

#define CAT_ERROR(error_category, fmt, args... ) \
  _LOG(L_ERR, error_category " " fmt, ##args)
#define CAT_WARN(error_category, fmt, args... ) \
_LOG(L_WARN, error_category " " fmt, ##args)
#define CAT_INFO(error_category, fmt, args... ) \
  _LOG(L_INFO, error_category " " fmt, ##args)
#define CAT_DBG(error_category, fmt, args... ) \
  _LOG(L_DBG, error_category " " fmt, ##args)

#define CATEGORIZED_PREFIX    "SNMP:"
#define CATEGORY_ERROR      CATEGORIZED_PREFIX "0"
#define CATEGORY_WARNING    CATEGORIZED_PREFIX "1"
#define CATEGORY_INFO       CATEGORIZED_PREFIX "2"
#define CATEGORY_DEBUG      CATEGORIZED_PREFIX "3"

#ifdef USE_LOG_CATEGORY_PREFIXES
# define ERROR_CATEGORY_EGENERAL CATEGORY_ERROR   ".0" " "
# define ERROR_CATEGORY_WGENERAL CATEGORY_WARNING ".0" " "
# define ERROR_CATEGORY_IGENERAL CATEGORY_INFO    ".0" " "
# define ERROR_CATEGORY_DGENERAL CATEGORY_DEBUG   ".0" " "
#else
# define ERROR_CATEGORY_EGENERAL
# define ERROR_CATEGORY_WGENERAL
# define ERROR_CATEGORY_IGENERAL
# define ERROR_CATEGORY_DGENERAL
#endif

#define ERROR(fmt, args...) CAT_ERROR(ERROR_CATEGORY_EGENERAL, fmt, ##args)
#define WARN(fmt, args...)  CAT_WARN(ERROR_CATEGORY_WGENERAL, fmt, ##args)
#define INFO(fmt, args...)  CAT_INFO(ERROR_CATEGORY_IGENERAL, fmt, ##args)
#define DBG(fmt, args...)   CAT_DBG(ERROR_CATEGORY_DGENERAL, fmt, ##args)

/** @} */

extern int log_level;
extern int log_stderr;
extern const char* log_level2str[];

void init_logging(void);
void run_log_hooks(int, pid_t, pthread_t, const char*, const char*, int, char*);

#ifndef DISABLE_SYSLOG_LOG
int set_syslog_facility(const char*);
#endif

void log_stacktrace(int ll);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
/* ...only for C++ */
class AmLoggingFacility;
void register_log_hook(AmLoggingFacility*);
#endif

#endif /* !_log_h_ */
