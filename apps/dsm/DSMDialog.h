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
#ifndef _DSM_DIALOG_H
#define _DSM_DIALOG_H
#include "AmSession.h"
#include "AmPromptCollection.h"

#include "ampi/UACAuthAPI.h"

#include "DSMSession.h"
#include "DSMStateEngine.h"
#include "DSMStateDiagramCollection.h"

class DSMDialog : public AmSession,
		  public DSMSession,
		  public CredentialHolder
{
  std::auto_ptr<UACAuthCred> cred;
  
  DSMStateEngine engine;
  AmPromptCollection& prompts;
  DSMStateDiagramCollection& diags;
  string startDiagName;
  AmPlaylist playlist;

  vector<AmAudio*> audiofiles;
  AmAudioFile* rec_file;
  map<string, AmPromptCollection*> prompt_sets;

  bool checkVar(const string& var_name, const string& var_val);
public:
  DSMDialog(AmPromptCollection& prompts,
	    DSMStateDiagramCollection& diags,
	    const string& startDiagName,
	    UACAuthCred* credentials = NULL);
  ~DSMDialog();

  void onInvite(const AmSipRequest& req);
  void onSessionStart(const AmSipRequest& req);
  void onSessionStart(const AmSipReply& rep);
  void startSession();
  void onBye(const AmSipRequest& req);
  void onDtmf(int event, int duration_msec);

  void process(AmEvent* event);

  UACAuthCred* getCredentials();

  void addPromptSet(const string& name, AmPromptCollection* prompt_set);
  void setPromptSets(map<string, AmPromptCollection*>& new_prompt_sets);

  // DSMSession interface
  void playPrompt(const string& name, bool loop = false);
  void closePlaylist(bool notify);
  void playFile(const string& name, bool loop);
  void recordFile(const string& name);
  void stopRecord();

  void setPromptSet(const string& name);
  void addSeparator(const string& name);
  void connectMedia();
};

#endif
// Local Variables:
// mode:C++
// End:
