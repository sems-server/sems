
#ifndef ROOM_INFO_H
#define ROOM_INFO_H

#include <string>
using std::string;       

#include <list>
using std::list;

#include <sys/time.h> 


#include "AmArg.h"
#include "AmThread.h"

struct ConferenceRoomParticipant {
  enum ParticipantStatus {
    Disconnected = 0,
    Connecting,
    Ringing,
    Connected,
    Disconnecting,
    Finished
  };

  string localtag;
  string number;
  ParticipantStatus status;
  string last_reason; 

  int muted;
  
  struct timeval last_access_time;

  ConferenceRoomParticipant() 
    : status(Disconnected), muted(0) { }

  ~ConferenceRoomParticipant() { }

  inline void updateAccess(const struct timeval& now);
  inline bool expired(const struct timeval& now);

  inline void updateStatus(ParticipantStatus new_status, 
			   const string& reason,
			   struct timeval& now);

  inline void setMuted(int mute);
  
  AmArg asArgArray();
};

struct ConferenceRoom {
  string adminpin;

  struct timeval last_access_time;

  list<ConferenceRoomParticipant> participants;

  ConferenceRoom();
  ~ConferenceRoom() { }

  void cleanExpired();

  AmArg asArgArray();

  void newParticipant(const string& localtag, const string& number);

  bool updateStatus(const string& part_tag, 
		    ConferenceRoomParticipant::ParticipantStatus newstatus, 
		    const string& reason);

  bool hasParticipant(const string& localtag);

  void setMuted(const string& localtag, int mute);

  bool expired(const struct timeval& now);
  bool expired();
};


#endif
