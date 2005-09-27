/*
 * $Id: IvrMediaHandler.cpp,v 1.5.4.1 2005/09/02 13:47:46 rco Exp $
 * Copyright (C) 2002-2003 Fhg Fokus
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
//#include "IvrPython.h"
#include "IvrMediaHandler.h"
#include "log.h"

IvrMediaHandler::IvrMediaHandler()
  : closed(false),
    recordConnector(this, false), 
    playConnector(this, true)  
{
}

int IvrMediaHandler::registerForeignEventQueue(AmEventQueue* newScriptEventQueue) {
  int ret = 0;
  //  mutexScriptEventQueue.lock();
  scriptEventQueue = newScriptEventQueue;
  recordConnector.setScriptEventQueue(newScriptEventQueue);
  playConnector.setScriptEventQueue(newScriptEventQueue);
  //   mutexScriptEventQueue.unlock();
  return ret;
}

void IvrMediaHandler::unregisterForeignEventQueue() {
  mutexScriptEventQueue.lock();
  scriptEventQueue = 0;
  recordConnector.setScriptEventQueue(0);
  playConnector.setScriptEventQueue(0);
  mutexScriptEventQueue.unlock();
}

IvrMediaHandler::~IvrMediaHandler()
{
  DBG("Media Handler  being destroyed...\n");
  emptyMediaQueue();
}

int IvrMediaHandler::enqueueMediaFile(string fileName, bool front, bool loop) 
{
  AmAudioFile* wav_file = new AmAudioFile();
  //  DBG("Queue: enqueuing %d with out = %d.\n", (int)wav_file, (int)wav_file->out.get());
  wav_file->loop.set(loop);
  if(wav_file->open(fileName.c_str(),AmAudioFile::Read)){
    ERROR("IvrMediaHandler::enqueueMediaFile: Cannot open file %s\n", fileName.c_str());
    return -1;
  }
  
  if (front || mediaOutQueue.empty())
    playConnector.setActiveMedia(wav_file);
  
  if (front) {
    mediaOutQueue.push_front(wav_file);
  } else {
    mediaOutQueue.push_back(wav_file);
  }
  return 0; //ok
}

int IvrMediaHandler::emptyMediaQueue() {
  playConnector.setActiveMedia(0); 
  // we own all the MediaWrappers
  for (std::deque<AmAudioFile*>::iterator it = mediaOutQueue.begin(); 
       it != mediaOutQueue.end(); it++) {
    delete *it;
  }
  mediaOutQueue.clear();
  return 0;
}

int IvrMediaHandler::startRecording(string& filename) {
    return recordConnector.startRecording(filename);
}

int IvrMediaHandler::stopRecording() {
    return recordConnector.stopRecording();
}

int IvrMediaHandler::pauseRecording() {
    return recordConnector.pauseRecording();   
}

int IvrMediaHandler::resumeRecording() {
    return recordConnector.resumeRecording();
}

void IvrMediaHandler::close(){
  DBG("IvrMediaHandler::close(). closing record connector...\n");
  recordConnector.close();
  DBG("closing play connector...");
  playConnector.close();
  DBG("done.");
  closed = true;
}

IvrAudioConnector* IvrMediaHandler::getPlayConnector() {
    return &playConnector;
}

IvrAudioConnector* IvrMediaHandler::getRecordConnector() {
    return &recordConnector;
}

// this is called by the out connector
AmAudio* IvrMediaHandler::getNewOutMedia() 
{ 
  if (mediaOutQueue.empty()) {
    DBG("Empty queue.\n");
    return 0; // return -1;
  }
  
  if (!mediaOutQueue.empty()) {
    delete mediaOutQueue.front();
    mediaOutQueue.pop_front();
  }

  if (mediaOutQueue.empty()) {
	DBG("Empty media queue after pop.\n");
	if (scriptEventQueue) {
	  DBG("Posting IvrScriptEvent::IVR_MediaQueueEmpty into scriptEventQueue.\n");
	  scriptEventQueue->postEvent(new IvrScriptEvent(IvrScriptEvent::IVR_MediaQueueEmpty)); 
	} else {
	  DBG("no scriptEventQueue to notify available.\n");
	}
	  
	// TODO: wait until event processed? 
	if (mediaOutQueue.empty())
	    return 0;
	// if queue filled in onEmpty callback return new 
    }
    AmAudioFile* res = mediaOutQueue.front();

    return res;
}

/* IvrAudioConnector connects the MediaHandler to RtpStream. 
 * 
 */
IvrAudioConnector::IvrAudioConnector(IvrMediaHandler* mh, bool isPlay) 
  : AmAudio(new AmAudioSimpleFormat(IVR_AUDIO_CODEC)),
    scriptEventQueue(0), mediaHandler(mh),
    isPlayConnector(isPlay), activeMedia(0),
    closed(false),
    mediaInRunning(false), mediaIn(0)
{
}

IvrAudioConnector::~IvrAudioConnector()
{
    DBG("IvrAudioConnector::~IvrAudioConnector\n");
  close();
}

void IvrAudioConnector::close() {
  if (!closed) {
    if ((!isPlayConnector) && mediaIn) { // recording file is ours
      mediaIn->close();
      delete mediaIn;
      mediaIn = 0;
    }
    closed = true;
  }
}

void IvrAudioConnector::setScriptEventQueue(AmEventQueue* newScriptEventQueue) 
{
    scriptEventQueue = newScriptEventQueue;
    //   if (dtmfDetector)
    //     dtmfDetector->setDestinationEventQueue(newScriptEventQueue);
}

void IvrAudioConnector::setActiveMedia(AmAudio* newMedia) {
    DBG("setting active media...\n");
    activeMedia = newMedia;
}

int IvrAudioConnector::get(unsigned int user_ts, unsigned char* buffer, unsigned int nb_samples) 
{
    unsigned int size = samples2bytes(nb_samples);

  // DBG("IvrAudioConnector::get(%d, %d)\n", user_ts, size);
  if (!isPlayConnector) {
    ERROR("streamGet of IvrAudioConnector with type \"record\" called."
	  " There must be something wrong here.\n");
    return -1;
  }
  
  if (closed)
    return -1;
  
  if (!activeMedia) {
      // send empty packets (containing all 0s) when there is no media
      // being played
      memset(buffer, 0, size);
      return size;
  }
  
  int ret = activeMedia->get(user_ts, buffer, nb_samples);
  //DBG("Got %d.", ret);
  while (ret<0) {
    activeMedia = mediaHandler->getNewOutMedia();
    if (!activeMedia) {
      memset(buffer, 0, size);
      return size;
    }

    ret = activeMedia->get(user_ts, buffer, nb_samples);
  }

  return ret;
}

int IvrAudioConnector::put(unsigned int user_ts, unsigned char* buffer, unsigned int size) 
{
  if (closed)
    return -1;
  
  if (isPlayConnector) {
    ERROR("streamPut of IvrAudioConnector with type \"play\" called. There must be something wrong here.\n");
    return -1;
  }
  
  if (mediaIn && mediaInRunning) {
      return mediaIn->put(user_ts, buffer, size);
  }

  return size;
}


int IvrAudioConnector::startRecording(string& filename) 
{  
    if (mediaIn) {
	DBG("closing previous recording...\n");
	stopRecording();
    }

  
   
    unsigned int dot_pos = filename.rfind('.');
    string ext = filename.substr(dot_pos+1);

    AmAudioFile* rec_file = new AmAudioFile();
    if(rec_file->open(filename.c_str(),AmAudioFile::Write)){
	ERROR("AmRtpStream::record(): Cannot open file\n");
	delete rec_file;
	return -1;
    }

    mediaIn = rec_file;
    mediaInRunning = true;
    return 0;
}

int IvrAudioConnector::stopRecording() 
{ 
    mediaInRunning = false;
    if (mediaIn) {

	mediaIn->close();
	delete mediaIn;
	mediaIn = 0;
    }

    return 0;
}

int IvrAudioConnector::pauseRecording() { 
    mediaInRunning = false;
    return 0;
}

int IvrAudioConnector::resumeRecording() { 
    if (mediaIn)
	mediaInRunning = true;
    else 
      return -1; // error

    return 0;
}

// int IvrAudioConnector::enableDTMFDetection() 
// {
//     DBG("record connnector enabling dtmf detection...\n");
//     if (!dtmfDetector)
// 	dtmfDetector = new IvrDtmfDetector();
//     if (!scriptEventQueue) {
// 	DBG("missing script event queue!!!!!\n");
//     } 
    //dtmfDetector->setDestinationEventQueue(scriptEventQueue);
    //detectionRunning = true;
//     return 0;
// }

// int IvrAudioConnector::disableDTMFDetection() 
// {
    //detectionRunning = false;    
//     if (dtmfDetector)
// 	delete dtmfDetector;
//     dtmfDetector = 0;
//     return 0;
// }

// int IvrAudioConnector::pauseDTMFDetection() {
//     if (!detectionRunning)
//       return -1;
//     detectionRunning = false;
//     return 0;
// }

// int IvrAudioConnector::resumeDTMFDetection() {
//     if (dtmfDetector)
// 	detectionRunning = true;
//     else 
//       return -1;
//     return 0;
// }
