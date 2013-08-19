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

#include "SBCCallRegistry.h"
#include "log.h"

AmMutex SBCCallRegistry::registry_mutex;
std::map<string, SBCCallRegistryEntry> SBCCallRegistry::registry;

void SBCCallRegistry::addCall(const string& ltag, const SBCCallRegistryEntry& other_dlg) {
  registry_mutex.lock();
  registry[ltag] = other_dlg;
  registry_mutex.unlock();

  DBG("SBCCallRegistry: Added call '%s' - mapped to: '%s'/'%s'/'%s'\n", ltag.c_str(), other_dlg.ltag.c_str(), other_dlg.rtag.c_str(), other_dlg.callid.c_str());
}

void SBCCallRegistry::updateCall(const string& ltag, const string& other_rtag) {
  registry_mutex.lock();

  std::map<string, SBCCallRegistryEntry>::iterator it = registry.find(ltag);
  if (it != registry.end()) {
    it->second.rtag = other_rtag;
  }

  registry_mutex.unlock();

  DBG("SBCCallRegistry: Updated call '%s' - rtag to: '%s'\n", ltag.c_str(), other_rtag.c_str());
}

bool SBCCallRegistry::lookupCall(const string& ltag, SBCCallRegistryEntry& other_dlg) {
  bool res = false;

  registry_mutex.lock();
  std::map<string, SBCCallRegistryEntry>::iterator it = registry.find(ltag);
  if (it != registry.end()) {
    res = true;
    other_dlg = it->second;
  }
  registry_mutex.unlock();

  if (res) {
    DBG("SBCCallRegistry: found call mapping '%s' -> '%s'/'%s'/'%s'\n",
	ltag.c_str(), other_dlg.ltag.c_str(), other_dlg.rtag.c_str(), other_dlg.callid.c_str());
  } else {
    DBG("SBCCallRegistry: no call mapping found for '%s'\n", ltag.c_str());
  }
  return res;
}

void SBCCallRegistry::removeCall(const string& ltag) {
  registry_mutex.lock();
  registry.erase(ltag);
  registry_mutex.unlock();  

  DBG("SBCCallRegistry: removed entry for call '%s'\n", ltag.c_str());
}
