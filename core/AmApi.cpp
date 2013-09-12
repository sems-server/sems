/*
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2006 iptego GmbH
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

#include "AmApi.h"
#include "log.h"
#include "AmSession.h"
#include "AmB2BMedia.h" // just because of statistics in reply to OPTIONS

AmDynInvoke::AmDynInvoke() {}
AmDynInvoke::~AmDynInvoke() {}

void AmDynInvoke::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  throw NotImplemented(method);
}

AmDynInvokeFactory::AmDynInvokeFactory(const string& name)
  : AmPluginFactory(name) 
{
}

AmSessionFactory::AmSessionFactory(const string& name)
  : AmPluginFactory(name)
{
}

AmSession* AmSessionFactory::onInvite(const AmSipRequest& req, const string& app_name,
				      AmArg& session_params) {
  WARN(" discarding session parameters to new session.\n");
  map<string,string> app_params;
  return onInvite(req,app_name,app_params);
}

AmSession* AmSessionFactory::onRefer(const AmSipRequest& req, const string& app_name, const map<string,string>& app_params)
{
  throw AmSession::Exception(488,"Not accepted here");
}

AmSession* AmSessionFactory::onRefer(const AmSipRequest& req, const string& app_name,
				     AmArg& session_params)
{
  WARN(" discarding session parameters to new session.\n");
  map<string,string> app_params;
  return onRefer(req,app_name,app_params);
}

int AmSessionFactory::configureModule(AmConfigReader& cfg) {
  return 0;//mod_conf.readFromConfig(cfg);
}

void AmSessionFactory::configureSession(AmSession* sess) {
  //SessionTimer::sess->configureSessionTimer(mod_conf);
}

void AmSessionFactory::onOoDRequest(const AmSipRequest& req)
{

  if (req.method == SIP_METH_OPTIONS) {
    replyOptions(req);
    return;
  }

  INFO("sorry, we don't support beginning a new session with "
       "a '%s' message\n", req.method.c_str());

  AmSipDialog::reply_error(req,501,"Not Implemented");
  return;
}

void AmSessionFactory::replyOptions(const AmSipRequest& req) {
    string hdrs;
    if (!AmConfig::OptionsTranscoderInStatsHdr.empty()) {
      string usage;
      B2BMediaStatistics::instance()->reportCodecReadUsage(usage);

      hdrs += AmConfig::OptionsTranscoderInStatsHdr + ": ";
      hdrs += usage;
      hdrs += CRLF;
    }
    if (!AmConfig::OptionsTranscoderOutStatsHdr.empty()) {
      string usage;
      B2BMediaStatistics::instance()->reportCodecWriteUsage(usage);

      hdrs += AmConfig::OptionsTranscoderOutStatsHdr + ": ";
      hdrs += usage;
      hdrs += CRLF;
    }

    // Basic OPTIONS support
    if (AmConfig::OptionsSessionLimit &&
	(AmSession::getSessionNum() >= AmConfig::OptionsSessionLimit)) {
      // return error code if near to overload
      AmSipDialog::reply_error(req,
          AmConfig::OptionsSessionLimitErrCode, 
          AmConfig::OptionsSessionLimitErrReason,
          hdrs);
      return;
    }

    if (AmConfig::ShutdownMode) {
      // return error code if in shutdown mode
      AmSipDialog::reply_error(req,
          AmConfig::ShutdownModeErrCode,
          AmConfig::ShutdownModeErrReason,
          hdrs);
      return;
    }

    AmSipDialog::reply_error(req, 200, "OK", hdrs);

}

// void AmSessionFactory::postEvent(AmEvent* ev) {
//   ERROR("unhandled Event in %s module\n", getName().c_str());
//   delete ev;
// }

AmSessionEventHandlerFactory::AmSessionEventHandlerFactory(const string& name)
  : AmPluginFactory(name) 
{
}

bool AmSessionEventHandlerFactory::onInvite(const AmSipRequest& req, 
					    AmArg& session_params,
					    AmConfigReader& cfg) {
  WARN("discarding session parameters for new session.\n");
  return onInvite(req, cfg);
}


AmLoggingFacility::AmLoggingFacility(const string& name) 
  : AmPluginFactory(name) 
{
}
