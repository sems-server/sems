/*
 * Copyright (C) 2002-2005 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#include "Statistics.h"
#include "StatsUDPServer.h"

#include <string>
using std::string;

EXPORT_PLUGIN_FACTORY(StatsFactory,MOD_NAME);

StatsFactory::StatsFactory(const std::string& _app_name)
  : AmPluginFactory(_app_name)
{
}

int StatsFactory::onLoad()
{
  StatsUDPServer* stat_srv = StatsUDPServer::instance();
  if(!stat_srv){
    ERROR("stats UDP server not initialized.\n");
    return -1;
  }

  return 0;
}




