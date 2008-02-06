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

#ifndef _EmailTemplate_h_
#define _EmailTemplate_h_

#include <map>
#include <string>
using std::string;

class AmMail;

typedef std::map<std::string,std::string> EmailTmplDict;

/** \brief loads, processes and outputs an email template file */
class EmailTemplate
{
 public:
  string tmpl_file;
  string subject;
  string to;
  string from;
  string body;
  string header;

  int parse(char* buffer);
  string replaceVars(const string& str,const EmailTmplDict& dict) const;

 public:
  /* return 0 if success */
  int load(const string& filename);

  // throws error string.
  AmMail getEmail(const EmailTmplDict& dict) const;
};

#endif
