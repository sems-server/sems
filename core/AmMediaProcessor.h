/*
 * Copyright (C) 2002-2003 Fhg Fokus
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
/** @file AmMediaProcessor.h */
#ifndef _AmMediaProcessor_h_
#define _AmMediaProcessor_h_

#include "AmEventQueue.h"
#include "amci/amci.h" // AUDIO_BUFFER_SIZE

#include <set>
using std::set;
#include <map>

struct SchedRequest;

/** Interface for basic media session processing.
 *
 * Media processor stores set of objects implementing this interface and
 * periodically triggers media processing on each of them.
 *
 * First it calls readStreams() method on all managed media sessions to read
 * from all streams first and then calls writeStreams() to send data out in all
 * these sessions.
 *
 * Once audio processing of all media sessions is done, media processor walks
 * through them once more and calls processDtmfEvents() on them to handle DTMF
 * events detected when reading data from media streams.
 */

class AmMediaSession
{
  private:
    AmCondition<bool> processing_media;

  public:
    AmMediaSession(): processing_media(false) { }
    virtual ~AmMediaSession() { }

    /** Read from all media streams.
     *
     * To preserve current media processing scheme it is needed to read from all
     * streams first and then write to them. This can be important for example
     * in case of conferences where we need to have media from all streams ready
     * for mixing them. 
     *
     * So the AmMediaProcessorThread first calls readStreams on all sessions and
     * then writeStreams on all sessions.
     *
     * \param ts timestamp for which the processing is currently running
     *
     * \param buffer multi-purpose space given from outside (AmMediaProcessorThread) 
     *
     * Buffer given as parametr is usable for anything, originally was intended for data
     * read from one stream before putting to another stream. 
     *
     * The reason for having this buffer as parameter is that buffer size for
     * audio processing is quite large (2K here) and thus allocating it on stack
     * on some architectures may be problematic. 
     *
     * On other hand having the buffer dynamically allocated for each media
     * session would significantly increase memory consumption per call. 
     *
     * So for now it seems to be the simplest way just to give the buffer as
     * parameter from AmMediaProcessorThread and reuse it in all sessions handled
     * by this thread (processing is done sequentially one session after another). */
    virtual int readStreams(unsigned long long ts, unsigned char *buffer) = 0;
    
    /** Write to all media streams.
     *
     * For the meaning of parameters see description of readStreams() method. */
    virtual int writeStreams(unsigned long long ts, unsigned char *buffer) = 0;

    /** Handle events in DTMF event queue.
     * 
     * DTMF events should be detected from RTP stream when reading data (see
     * readStreams()) and put into an event queue for later processing to avoid
     * blocking of audio processing for too long time. 
     *
     * This DTMF event queue should be processed then, within this method, which
     * is triggered by AmMediaProcessorThread once reading/writing from media
     * streams is finished. */
    virtual void processDtmfEvents() = 0;

    /** Reset all media processing elements. 
     *
     * Called as part of cleanup when removing session from media processor upon
     * an processing error (either readStreams() or writeStreams() returning
     * error). */
    virtual void clearAudio() = 0;

    /** Reset timeouts of all RTP streams related to this media session.
     *
     * Called during initialization when session starts to be processed by media
     * processor. */
    virtual void clearRTPTimeout() = 0;

    /** Callback function called when a session is added to media processor.  
     *
     * Default implementation sets internal variable usable for detection if the
     * object is in use by AmMediaProcessorThread. */
    virtual void onMediaProcessingStarted() { processing_media.set(true); }
    
    /* Callback function called when a session is removed from media processor.
     *
     * Default implementation sets internal variable usable for detection if the
     * object is in use by AmMediaProcessorThread. */
    virtual void onMediaProcessingTerminated() { processing_media.set(false); }
  
    /** Indicates if the object is used by media processor.
     * 
     * Returns value of internal variable for distinguishing if the object is
     * already added into media processor. It should be avoided to insert one
     * session into media processor multiple times.
     *
     * Note that using default implementation of onMediaProcessingStarted and
     * onMediaProcessingTerminated is required for proper function. */
    virtual bool isProcessingMedia() { return processing_media.get(); }
  
    /** Indicates if the object is used by media processor.
     * 
     * Seems to be duplicate to isProcessingMedia(). It was kept to reduce
     * number of changes in existing code. */
    virtual bool isDetached() { return !isProcessingMedia(); }
};

/**
 * \brief Media processing thread
 * 
 * This class implements a media processing thread.
 * It processes the media and triggers the sending of RTP
 * of all sessions added to it.
 */
class AmMediaProcessorThread :
  public AmThread,
  public AmEventHandler
{
  AmEventQueue    events;
  unsigned char   buffer[AUDIO_BUFFER_SIZE];
  set<AmMediaSession*> sessions;
  
  void processAudio(unsigned long long ts);
  /**
   * Process pending DTMF events
   */
  void processDtmfEvents();

  // AmThread interface
  void run();
  void on_stop();
  AmSharedVar<bool> stop_requested;
    
  // AmEventHandler interface
  void process(AmEvent* e);
public:
  AmMediaProcessorThread();
  ~AmMediaProcessorThread();

  inline void postRequest(SchedRequest* sr);
  
  unsigned int getLoad();
};

/**
 * \brief Media processing thread manager
 * 
 * This class implements the manager that assigns and removes 
 * the Sessions to the various \ref MediaProcessorThreads, 
 * according to their call group. This class contains the API 
 * for the MediaProcessor.
 */
class AmMediaProcessor
{
  static AmMediaProcessor* _instance;

  unsigned int num_threads; 
  AmMediaProcessorThread**  threads;
  
  std::map<string, unsigned int> callgroup2thread;
  std::multimap<string, AmMediaSession*> callgroupmembers;
  std::map<AmMediaSession*, string> session2callgroup;
  AmMutex group_mut;

  AmMediaProcessor();
  ~AmMediaProcessor();
	
  void removeFromProcessor(AmMediaSession* s, unsigned int r_type);
public:
  /** 
   * InsertSession     : inserts the session to the processor
   * RemoveSession     : remove the session from the processor
   * SoftRemoveSession : remove the session from the processor but leave it attached
   * ClearSession      : remove the session from processor and clear audio
   */
  enum { InsertSession, RemoveSession, SoftRemoveSession, ClearSession };

  static AmMediaProcessor* instance();

  void init();
  /** Add session s to processor */
  void addSession(AmMediaSession* s, const string& callgroup);
  /** Remove session s from processor */
  void removeSession(AmMediaSession* s);
  /** Remove session s from processor and clear its audio */
  void clearSession(AmMediaSession* s);
  /** Change the callgroup of a session (use with caution) */
  void changeCallgroup(AmMediaSession* s, 
		       const string& new_callgroup);

  void stop();
  static void dispose();
};


#endif






