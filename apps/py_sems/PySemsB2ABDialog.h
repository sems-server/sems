/*
 * $Id: PySemsDialog.h,v 1.26.2.1 2005/09/02 13:47:46 rco Exp $
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

#include "AmApi.h"
#include "AmB2ABSession.h"
#include "AmPlaylist.h"
#include "PySems.h"

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
};

#endif
