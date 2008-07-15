/*
 * $Id$
 * 
 * Copyright (C) 2006 iptego GmbH
 *
 * This file is part of sems, a free SIP media server.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef PY_SEMSB2ABDIALOG_H
#define PY_SEMSB2ABDIALOG_H

#include "PySems.h"
#include "AmApi.h"
#include "AmB2ABSession.h"
#include "AmPlaylist.h"

class PySemsB2ABCalleeDialog;

/** \brief pySems wrapper for base of pySems dialog classes */
class PySemsB2ABDialog : public AmB2ABCallerSession, 
  public PySemsDialogBase
{
 public:
  AmDynInvoke* user_timer;
  AmPlaylist playlist;

  PySemsB2ABDialog();
  PySemsB2ABDialog(AmDynInvoke* user_timer);
  ~PySemsB2ABDialog();

  void onSessionStart(const AmSipRequest& req);

  // @see AmEventHandler
  void process(AmEvent* event);

  AmB2ABCalleeSession* createCalleeSession();
};

/** \brief base class for events in Py-B2AB sessions */
struct PySemsB2ABEvent: public B2ABEvent
{
 public:
 PySemsB2ABEvent(int ev_id) 
   : B2ABEvent(ev_id)
  {}
};

/** \brief pySems wrapper for B leg in pysems B2AB session */
class PySemsB2ABCalleeDialog : public AmB2ABCalleeSession
{
 public:
 PySemsB2ABCalleeDialog(const string& other_local_tag)
   : AmB2ABCalleeSession(other_local_tag) { }

 protected:
  void onB2ABEvent(B2ABEvent* ev);

  virtual void onPyB2ABEvent(PySemsB2ABEvent* py_ev);
};
#endif
