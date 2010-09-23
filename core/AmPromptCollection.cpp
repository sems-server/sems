/*
 * Copyright (C) 2007 iptego GmbH
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

#include "AmPromptCollection.h"
#include "AmUtils.h"
#include "log.h"

AmPromptCollection::AmPromptCollection() 
{
}

AmPromptCollection::~AmPromptCollection() 
{
  // clean up
  for (std::map<std::string, AudioFileEntry*>::iterator it=
	 store.begin(); it != store.end();it++)
    delete it->second;
}

int AmPromptCollection::configureModule(AmConfigReader& cfg, 
					std::vector<std::pair<std::string, std::string> >& announcements,
					const char* mod_name) {
  int res = 0;
  for (std::vector<std::pair<std::string, std::string> >::iterator it=
	 announcements.begin(); it != announcements.end(); it++) {
    string fname = cfg.getParameter(it->first, "");
    if (fname.empty()){
      WARN("using default file '%s' for '%s' prompt in '%s' module\n",
	   it->second.c_str(), it->first.c_str(), mod_name);
      fname = it->second;
    }

    if (0 != setPrompt(it->first, fname, mod_name))
      res = -1;
  }

  return res;
}

int AmPromptCollection::setPrompt(const std::string& name, 
				  const std::string& filename,
				  const char* mod_name) {
  if (!file_exists(filename)) {
    ERROR("'%s' prompt for module %s does not exist at '%s'.\n", 
	  name.c_str(), mod_name, filename.c_str());
    return -1;
  }
 
  AudioFileEntry* af = new AudioFileEntry();
  if (af->load(filename)) {
    ERROR("Could not load '%s' prompt for module %s at '%s'.\n", 
	  name.c_str(), mod_name, filename.c_str());
    delete af;
    return -1;
  }
  DBG("adding prompt '%s' to prompt collection.\n", 
      name.c_str());
  store[name]=af;
  return 0;
}



AudioFileEntry::AudioFileEntry()
  : isopen(false)
{
}

AudioFileEntry::~AudioFileEntry() {
}

int AudioFileEntry::load(const std::string& filename) {
  int res = cache.load(filename);
  isopen = !res;
  return res;
}

AmCachedAudioFile* AudioFileEntry::getAudio(){
  if (!isopen)
    return NULL;
  return new AmCachedAudioFile(&cache);
}

bool AmPromptCollection::hasPrompt(const string& name) {
  string s = name;
  std::map<std::string, AudioFileEntry*>::iterator it=store.begin();

  while (it != store.end()) {
    if (!strcmp(it->first.c_str(), s.c_str()))
      break;
    it++;
  }
  return it != store.end();

}

int AmPromptCollection::addToPlaylist(const std::string& name, long sess_id, 
				      AmPlaylist& list, bool front, 
				      bool loop) {
  string s = name;
  std::map<std::string, AudioFileEntry*>::iterator it=store.begin();

  while (it != store.end()) {
    if (!strcmp(it->first.c_str(), s.c_str()))
      break;
    it++;
  }
  if (it == store.end()) {
    WARN("'%s' prompt not found!\n", name.c_str());
    return -1;
  }

  DBG("adding '%s' prompt to playlist at the %s'\n", it->first.c_str(), 
      front ? "front":"back");

  AmCachedAudioFile* af = it->second->getAudio();
  if (NULL == af) {
    return -2;
  }

  if (loop) 
    af->loop.set(true);

  if (front)
    list.addToPlayListFront(new AmPlaylistItem(af,NULL));
  else 
    list.addToPlaylist(new AmPlaylistItem(af,NULL));
  
  items_mut.lock();
  items[sess_id].push_back(af);
  items_mut.unlock();

  return 0;
}


void AmPromptCollection::cleanup(long sess_id) {
  items_mut.lock();
  for (std::vector<AmCachedAudioFile*>::iterator it = 
	 items[sess_id].begin(); it!=items[sess_id].end(); it++)
    delete *it;
  items.erase(sess_id);
  items_mut.unlock();
}
