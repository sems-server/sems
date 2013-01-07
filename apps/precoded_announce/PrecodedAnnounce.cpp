/*
 * Copyright (C) 2008 iptego GmbH
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

#include "PrecodedAnnounce.h"

#include "log.h"
#include "AmConfigReader.h"
#include "AmUtils.h"

EXPORT_SESSION_FACTORY(PrecodedFactory,MOD_NAME);

PrecodedFactory::PrecodedFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int PrecodedFactory::onLoad()
{
    AmConfigReader cfg;
    if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf")))
	return -1;

    if (precoded_file.open(cfg.getParameter("announcement_file")) < 0) {
      ERROR("loading precoded file");
      return -1;
    }

    precoded_file.initPlugin();
    return 0;
}

AmSession* PrecodedFactory::onInvite(const AmSipRequest& req, const string& app_name,
				     const map<string,string>& app_params)
{
    return new PrecodedDialog(&precoded_file);
}

PrecodedDialog::PrecodedDialog(AmPrecodedFile* file_def)
  : file_def(file_def)
{
  RTPStream()->setPayloadProvider(file_def);
}

PrecodedDialog::~PrecodedDialog()
{
}

void PrecodedDialog::onSessionStart()
{
  AmPrecodedFileInstance* file = 
    file_def->getFileInstance(RTPStream()->getPayloadType());
  if (!file) {
    ERROR("no payload\n");
  }
  if (!file || file->open()) { 
    ERROR("PrecodedDialog::onSessionStart: Cannot open file\n");
    dlg->bye();
    setStopped();
    return;
  }
 
  setOutput(file);
  setReceiving(false);

  AmSession::onSessionStart();
}

void PrecodedDialog::onBye(const AmSipRequest& req)
{
    DBG("onBye: stopSession\n");
    setStopped();
}

