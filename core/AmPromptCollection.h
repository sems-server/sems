
/*
 * $Id: AmPromptFileCollection.cpp 288 2007-03-28 16:32:02Z sayer $
 *
 * Copyright (C) 2007 iptego GmbH
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
/** @file AmPromptCollection.h */
#ifndef AM_PROMPT_COLLECTION_H
#define AM_PROMPT_COLLECTION_H

/**
 *
 * Example how to use:
 * 
 *  AM_PROMPT_START;
 *  AM_PROMPT_ADD("enter_pin", "path/to/default/enter_pin.wav");
 *  AM_PROMPT_ADD("ok", "path/to/default/ok.wav");
 *  AM_PROMPT_END(prompts, cfg, APP_NAME);
 */

#define AM_PROMPT_START \
{   \
  std::vector<std::pair<string, string> > _prompt_names

#define AM_PROMPT_ADD(_name, _default_file) \
 _prompt_names.push_back(std::make_pair(_name, _default_file))

#define AM_PROMPT_END(_prompts, _cfg, _MOD_NAME) \
  _prompts.configureModule(_cfg, _prompt_names, _MOD_NAME); \
 }

#include <map>
#include <string>
#include <utility>
using std::string;

#include "AmCachedAudioFile.h"
#include "AmPlaylist.h"
#include "AmConfigReader.h"

class AudioFileEntry;

/**
 * \brief manages AmAudioFiles with name for a session.
 */
class AmPromptCollection {

  // loaded files
  std::map<string, AudioFileEntry*> store;

  // opened objects
  std::map<long, vector<AmCachedAudioFile*> > items;
  // mutex for the above
  AmMutex items_mut;

 public:
  AmPromptCollection();
  ~AmPromptCollection();
  
  /**
   * get configuration for announcements from cfg,
   * check for file existence
   * @param announcements : name, default file for announcement
   */
  int configureModule(AmConfigReader& cfg, 
		      vector<std::pair<string, string> >& announcements,
		      const char* mod_name); 
  /**
   * add a prompt with explicit filename
   */
  int setPrompt(const string& name, 
		const string& filename,
		const char* mod_name);
  /**
   * add the announcement identified with  @param name 
   * to the playlist @list
   */
  int addToPlaylist(const string& name, long sess_id, 
		    AmPlaylist& list, bool front=false);
  /**
   * cleanup allocated object of sess_id
   */
  void cleanup(long sess_id);
};

/** 
 *  \brief AmAudioFile with filename and open flag 
 */

class AudioFileEntry : public AmAudioFile {
  AmFileCache cache;
  bool isopen;

public:
  AudioFileEntry();
  ~AudioFileEntry();
  
  int load(const std::string& filename);
  bool isOpen() { return isopen; }

  AmCachedAudioFile* getAudio();
};

#endif
