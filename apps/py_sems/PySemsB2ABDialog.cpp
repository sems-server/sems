/* 
 * Copyright (C) 2006 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
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
#include "PySemsB2ABDialog.h"
#include "PySemsUtils.h"
PySemsB2ABDialog::PySemsB2ABDialog()
  : playlist(this)
{
}

PySemsB2ABDialog::~PySemsB2ABDialog()
{
}

void PySemsB2ABDialog::onBeforeDestroy() {
  AmB2ABCallerSession::onBeforeDestroy();
}

void PySemsB2ABDialog::onEarlySessionStart() {
  DBG("PySemsB2ABDialog::onEarlySessionStart\n");
  setInOut(&playlist,&playlist);
  AmB2ABCallerSession::onEarlySessionStart();
}

void PySemsB2ABDialog::onSessionStart()
{
  DBG("PySemsB2ABDialog::onSessionStart\n");
  setInOut(&playlist,&playlist);
  AmB2ABCallerSession::onSessionStart();
}

AmB2ABCalleeSession* PySemsB2ABDialog::createCalleeSession() {
  return new PySemsB2ABCalleeDialog(getLocalTag(), 
				    connector);
}

void PySemsB2ABDialog::process(AmEvent* event) 
{
  DBG("PySemsB2ABDialog::process\n");

  AmAudioEvent* audio_event = dynamic_cast<AmAudioEvent*>(event);
  if(audio_event && audio_event->event_id == AmAudioEvent::noAudio){

    callPyEventHandler("onEmptyQueue", NULL);
    event->processed = true;
  }
    
  AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(event);
  if(plugin_event && plugin_event->name == "timer_timeout") {

    callPyEventHandler("onTimer", "i", plugin_event->data.get(0).asInt());
    event->processed = true;
  }

  if (!event->processed)
    AmB2ABCallerSession::process(event);

  return;
}

void PySemsB2ABCalleeDialog::onB2ABEvent(B2ABEvent* ev) {
  PySemsB2ABEvent* py_ev = dynamic_cast<PySemsB2ABEvent*>(ev);
  if (NULL != py_ev) {
    DBG("calling onPyB2AB...\n");
    onPyB2ABEvent(py_ev);
  } else {
    AmB2ABCalleeSession::onB2ABEvent(ev);
  }
}


void PySemsB2ABCalleeDialog::onPyB2ABEvent(PySemsB2ABEvent* py_ev) {
  DBG("ignoring PySemsB2ABEvent\n");
  delete py_ev; //-- don't delete, ownership already been transfered to python?
}
