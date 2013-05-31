/*
 * Copyright (C) 2010 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef _ParamReplacer_h_
#define _ParamReplacer_h_

#include <string>
using std::string;

#include "AmSipMsg.h"
#include "AmUriParser.h"

struct SBCCallProfile;

// $xy parameters replacement
string replaceParameters(const string& s,
			 const char* r_type,
			 const AmSipRequest& req,
			 const SBCCallProfile* call_profile,
			 const string& app_param,
			 AmUriParser& ruri_parser,
			 AmUriParser& from_parser,
			 AmUriParser& to_parser,
			 bool rebuild_ruri,
			 bool rebuild_from,
			 bool rebuild_to);

struct ParamReplacerCtx
{
  string app_param;
  AmUriParser ruri_parser;
  AmUriParser from_parser;
  AmUriParser to_parser;

  bool ruri_modified;
  bool from_modified;
  bool to_modified;

  const SBCCallProfile* call_profile;

  ParamReplacerCtx(const SBCCallProfile* call_profile=NULL)
    : ruri_modified(false), 
      from_modified(false), 
      to_modified(false),
      call_profile(call_profile)
  {}

  string replaceParameters(const string& s,
			   const char* r_type,
			   const AmSipRequest& req) {
    
    return ::replaceParameters(s,r_type,req,
			       call_profile,
			       app_param,
			       ruri_parser,
			       from_parser,
			       to_parser,
			       ruri_modified,
			       from_modified,
			       to_modified);
  }
};

#endif
