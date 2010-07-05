/*
 * $Id$
 *
 * Copyright (C) 2008 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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
#include "DSMStateDiagramCollection.h"

#include "DSMChartReader.h"
#include <fstream>
using std::ifstream;

DSMStateDiagramCollection::DSMStateDiagramCollection() {
}

DSMStateDiagramCollection::~DSMStateDiagramCollection() {
}

bool DSMStateDiagramCollection::loadFile(const string& filename, const string& name, 
					 const string& mod_path, bool debug_dsm, bool check_dsm) {
  DBG("loading DSM '%s' from '%s'\n", name.c_str(), filename.c_str());

  DSMChartReader cr;

  ifstream ifs(filename.c_str());
  if (!ifs.good()) {
    ERROR("loading state diagram '%s'\n",
	  filename.c_str());
    return false;
  }

  diags.push_back(DSMStateDiagram(name));
  string s;
  while (ifs.good() && !ifs.eof()) {
    string r;
    getline(ifs, r);
    // skip comments
    size_t fpos  = r.find_first_not_of(" \t");
    if (fpos != string::npos &&
	r.length() > fpos+1 &&
	r.substr(fpos, 2) == "--") 
      continue;

    s += r + "\n";
  }
  if (debug_dsm) {
    DBG("dsm text\n------------------\n%s\n------------------\n", s.c_str());
  }
  if (!cr.decode(&diags.back(), s, mod_path, this, mods)) {
    ERROR("DonkeySM decode script error!\n");
    return false;
  }
  if (check_dsm) {
    string report;
    if (!diags.back().checkConsistency(report)) {
      WARN("consistency check failed on '%s' from file '%s':\n", 
	   name.c_str(), filename.c_str());
      WARN("------------------------------------------\n"
	   "%s\n"
	   "------------------------------------------\n", report.c_str());
    } else {
      DBG("DSM '%s' passed consistency check\n", name.c_str());
    }
  }

  return true;
}

bool DSMStateDiagramCollection::hasDiagram(const string& name) {
  for (vector<DSMStateDiagram>::iterator it=
	 diags.begin(); it != diags.end(); it++) 
    if (it->getName() == name)
      return true;
  
  return false;
}

vector<string> DSMStateDiagramCollection::getDiagramNames() {
  vector<string> res; 
  for (vector<DSMStateDiagram>::iterator it=
	 diags.begin(); it != diags.end(); it++) 
    res.push_back(it->getName());
  return res;
}

void DSMStateDiagramCollection::addToEngine(DSMStateEngine* e) {
  DBG("adding %zd diags to engine\n", diags.size());
  for (vector <DSMStateDiagram>::iterator it = 
	 diags.begin(); it != diags.end(); it++) 
    e->addDiagram(&(*it));
  
  e->addModules(mods);
}
