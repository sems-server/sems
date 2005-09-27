/*
 * $Id: IvrMediaHandler.h,v 1.3.4.1 2005/09/02 13:47:46 rco Exp $
 * Copyright (C) 2004 Fhg Fokus
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
 
#ifndef _IVR_MEDIA_HANDLER_H_
#define _IVR_MEDIA_HANDLER_H_

#include "Ivr.h"
#include "IvrEvents.h"

#include "AmThread.h"
#include "AmAudio.h"
#include "AmEventQueue.h"

 
#include "amci/codecs.h"
class IvrPython;

#include <queue>
#include <utility>
#include <string>

using std::auto_ptr;
using std::string;

#define IVR_AUDIO_CODEC CODEC_PCM16 // from amci/codecs.h

#define IVR_AUDIO_CHANNELS      1
#define IVR_AUDIO_RATE       8000
#define IVR_AUDIO_SAMPLE        2

class IvrMediaHandler;

// data flow
// rtpstream ==> connector rec ==> MediaHandler 
// MediaHandler ==> connector play ==> rtpstream

class IvrAudioConnector : public AmAudio 
{
 private:
    int deb_cnt;
    IvrMediaHandler* mediaHandler;
    AmAudio* activeMedia;
    bool isPlayConnector;
    bool closed;
    
    // for recording 
    bool mediaInRunning;
    AmAudio* mediaIn;
    
    AmEventQueue* scriptEventQueue;
    
 protected:
    int read(unsigned int user_ts, unsigned int size) { return -1; }
    int write(unsigned int user_ts, unsigned int size) { return -1; }
    
    int get(unsigned int user_ts, unsigned char* buffer, unsigned int nb_samples);
    int put(unsigned int user_ts, unsigned char* buffer, unsigned int size);
    
 public:  
    IvrAudioConnector(IvrMediaHandler* mh, bool isPlay);
    ~IvrAudioConnector();
    void setActiveMedia(AmAudio* newMedia);
    void setScriptEventQueue(AmEventQueue* newScriptEventQueue);
    
    int startRecording(string& filename);
    int stopRecording();
    int pauseRecording();
    int resumeRecording();

    void close();
};

class IvrMediaHandler : public IvrEventProducer
{
 private:
    bool closed;
   
    // AmAudio (mostly AmAudioFile) to be played to caller
    std::deque<AmAudioFile*> mediaOutQueue;
    AmMutex queueMutex;

    IvrAudioConnector recordConnector;
    IvrAudioConnector playConnector;

    AmEventQueue* scriptEventQueue;
    AmMutex mutexScriptEventQueue;
 public:
    IvrMediaHandler();
    int registerForeignEventQueue(AmEventQueue* scriptEventQueue);
    void unregisterForeignEventQueue();
    
    virtual ~IvrMediaHandler();
    
    void close();

    IvrAudioConnector*  getPlayConnector();
    IvrAudioConnector* getRecordConnector();
    
    AmAudio* getNewOutMedia();

    int enqueueMediaFile(string fileName, bool front = false, bool loop = false);
    int emptyMediaQueue();
    
    int startRecording(string& filename);
    int stopRecording();
    int pauseRecording();
    int resumeRecording();

/*     int enableDTMFDetection(); */
/*     int disableDTMFDetection();     */
};

#endif // _IVR_MEDIA_HANDLER_H_
