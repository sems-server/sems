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
/** @file AmMail.h */
#ifndef _AmMail_h_
#define _AmMail_h_

#include "AmThread.h"
#include <stdio.h>

#include <string>
#include <vector>
#include <queue>
using std::string;

/**
 * \brief Email file attachement
 */
struct Attachement
{
  /** Local file name */
  //string fullname;
  FILE* fp;
    
  /** Proposed remote file name */
  string filename; 
  /** Declared content type */
  string content_type;

  //     Attachement(const string& _full, const string& _file="", const string& _ct="")
  // 	: fullname(_full), filename(_file), content_type(_ct) {}

  Attachement(FILE* _fp, const string& _file="", const string& _ct="")
    : fp(_fp), filename(_file), content_type(_ct) {}
};

typedef std::vector<Attachement> Attachements;

class AmMail;

/**
 * Function pointer to be called after mail processing.
 * @param mail The mail which just has been processed.
 */
typedef void (*MailCleanUpFunction)(AmMail* mail);

/**
 * \brief Email structure.
 * Supports basic email functions such as attachements.
 */
struct AmMail
{
public:
  string from;
  string subject;
  string body;
  string to;
  string header;

  /** Char set */
  string charset;

  Attachements attachements;

  /** @see MailCleanUpFunction. */
  MailCleanUpFunction clean_up;

  int error_count;

  AmMail(const string& _from, const string& _subject,
	 const string& _to, const string& _body = "",
	 const string& _header = "");

  ~AmMail();
};

/**
 * \brief Email Deamon (singleton).
 * It is designed as a singleton using a queue to get his work.
 * It wakes up only if there is anything to do.
 */
class AmMailDeamon: public AmThread
{
  static AmMailDeamon* _instance;

  AmMutex      event_fifo_mut;
  std::queue<AmMail*>   event_fifo;
  AmCondition<bool> _run_cond;

  AmMailDeamon() : _run_cond(false) {}
  AmMailDeamon(const AmMailDeamon&) : _run_cond(false) {}
  ~AmMailDeamon() {}

  void run();
  void on_stop();

public:
  static AmMailDeamon* instance();

  /**
   * Sends an email asynchronously.
   * @return <ul>
   *         <li>0 if succeded
   *         <li>-1 if failed
   *         </ul>
   */
  int sendQueued(AmMail* mail);
};

#endif

