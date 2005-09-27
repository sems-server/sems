/*
 * $Id: Announcement.cpp,v 1.7.8.4 2005/08/31 13:54:29 rco Exp $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#include "Announcement.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "AmSessionScheduler.h"

#include "sems.h"
#include "log.h"

#define MOD_NAME "announcement"

EXPORT_FACTORY(AnnouncementFactory,MOD_NAME);

string AnnouncementFactory::AnnouncePath;
string AnnouncementFactory::AnnounceFile;

AnnouncementFactory::AnnouncementFactory(const string& _app_name)
  : AmStateFactory(_app_name)
{
}

int AnnouncementFactory::onLoad()
{
    char* p = 0;

    if(m_config.reloadModuleConfig(MOD_NAME) != 1) {
      ERROR("missing configuration for " MOD_NAME "." 
	    MOD_NAME " module will probably not work.\n");
      AnnouncePath = ANNOUNCE_PATH;
      AnnounceFile = ANNOUNCE_FILE;

      string announce_file = AnnouncePath + AnnounceFile;
      if(!file_exists(announce_file)){
	ERROR("default file for announcement module does not exist ('%s').\n",
	      announce_file.c_str());
      }
      return 0;
    }

    if( ((p = m_config.getValueForKey("announce_path")) != NULL) && (*p != '\0') )
	AnnouncePath = p;
    else {
	WARN("no announce_path specified in configuration\n");
	WARN("file for module announcement.\n");
	AnnouncePath = ANNOUNCE_PATH;
    }
    if( !AnnouncePath.empty() 
	&& AnnouncePath[AnnouncePath.length()-1] != '/' )
	AnnouncePath += "/";

    if( ((p = m_config.getValueForKey("default_announce")) != NULL) && (*p != '\0') )
	AnnounceFile = p;
    else {
	WARN("no default_announce specified in configuration\n");
	WARN("file for module announcement.\n");
	AnnounceFile = ANNOUNCE_FILE;
    }

    string announce_file = AnnouncePath + AnnounceFile;
    if(!file_exists(announce_file)){
	ERROR("default file for announcement module does not exist ('%s').\n",
	      announce_file.c_str());
    }

    return 0;
}

AmDialogState* AnnouncementFactory::onInvite(AmCmd& cmd)
{
    string announce_path = AnnouncePath;
    string announce_file = announce_path + cmd.domain 
	+ "/" + cmd.user + ".wav";

    DBG("trying '%s'\n",announce_file.c_str());
    if(file_exists(announce_file))
	goto end;

    announce_file = announce_path + cmd.user + ".wav";
    DBG("trying '%s'\n",announce_file.c_str());
    if(file_exists(announce_file))
	goto end;

    announce_file = AnnouncePath + AnnounceFile;
    
end:
    return new AnnouncementDialog(announce_file);
}

AnnouncementDialog::AnnouncementDialog(const string& filename)
    : filename(filename)
{
}

AnnouncementDialog::~AnnouncementDialog()
{
}

void AnnouncementDialog::onSessionStart(AmRequest* req)
{
    if(wav_file.open(filename,AmAudioFile::Read))
	throw string("AnnouncementDialog::onSessionStart: Cannot open file\n");
    
    getSession()->setOutput(&wav_file);
}

void AnnouncementDialog::onBye(AmRequest* req)
{
    stopSession();
}
