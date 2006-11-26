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

#include "AmApi.h"
#include "log.h"
#include "AmSession.h"

AmDynInvoke::AmDynInvoke() {}
AmDynInvoke::~AmDynInvoke() {}

void AmDynInvoke::invoke(const string& method, const AmArgArray& args, AmArgArray& ret)
{
    throw NotImplemented(method);
}

AmDynInvokeFactory::AmDynInvokeFactory(const string& name)
    : AmPluginFactory(name) 
{
}

AmSessionFactory::AmSessionFactory(const string& name)
  : AmPluginFactory(name), mod_conf(AmConfig::defaultSessionTimerConfig)
{
}

int AmSessionFactory::configureModule(AmConfigReader& cfg) {
  return mod_conf.readFromConfig(cfg);
}

void AmSessionFactory::configureSession(AmSession* sess) {
    //SessionTimer::sess->configureSessionTimer(mod_conf);
}

AmSessionEventHandlerFactory::AmSessionEventHandlerFactory(const string& name)
	: AmPluginFactory(name) 
{
}
