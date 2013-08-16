/*
 * Copyright (C) 2013 Stefan Sayer
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

#ifndef _SBCCallRegistry_H
#define _SBCCallRegistry_H

#include "AmThread.h"

#include <string>
using std::string;
#include <map>

struct SBCCallRegistryEntry
{
  string ltag;
  string rtag;
  string callid;
  
  SBCCallRegistryEntry() { }
SBCCallRegistryEntry(const string& callid, const string& ltag, const string& rtag)
  : ltag(ltag), rtag(rtag), callid(callid) { }
};

class SBCCallRegistry 
{
  static AmMutex registry_mutex;
  static std::map<string, SBCCallRegistryEntry> registry;

 public:
  SBCCallRegistry() { }
  ~SBCCallRegistry() { }

  static void addCall(const string& ltag, const SBCCallRegistryEntry& other_dlg);
  static void updateCall(const string& ltag, const string& other_rtag);
  static bool lookupCall(const string& ltag, SBCCallRegistryEntry& other_dlg);
  static void removeCall(const string& ltag);
};

#endif                           
