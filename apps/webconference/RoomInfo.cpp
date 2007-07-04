
#include "RoomInfo.h"
#include <string.h>
#include "log.h"

void ConferenceRoomParticipant::updateAccess(const struct timeval& now) {
  memcpy(&last_access_time, &now, sizeof(struct timeval));
}

bool ConferenceRoomParticipant::expired(const struct timeval& now) {
  if (Finished != status)
    return false;

  struct timeval diff;
  timersub(&now,&last_access_time,&diff);
  return (diff.tv_sec > 0) &&
    (unsigned int)diff.tv_sec > PARTICIPANT_EXPIRED_DELAY;
}

AmArg ConferenceRoomParticipant::asArgArray() {
  AmArg res;
  res.push(AmArg(localtag.c_str()));
  res.push(AmArg(number.c_str()));
  res.push(AmArg((int)status));
  res.push(AmArg(last_reason.c_str()));
  res.push(AmArg((int)muted));
  return res;
}

void ConferenceRoomParticipant::setMuted(int mute) {
  muted = mute;
}

void ConferenceRoomParticipant::updateStatus(ConferenceRoomParticipant::ParticipantStatus 
					     new_status, 
					     const string& reason) {
  status = new_status;
  last_reason = reason;
  struct timeval now;
  gettimeofday(&now, NULL);
  updateAccess(now);
}

void ConferenceRoom::cleanExpired() {
  struct timeval now;
  gettimeofday(&now, NULL);
  
  list<ConferenceRoomParticipant>::iterator it=participants.begin(); 
  while (it != participants.end()) {
    if (it->expired(now)) {
      participants.erase(it);
      it=participants.begin();
    } else 
      it++;
  }
}

AmArg ConferenceRoom::asArgArray() {
  cleanExpired();
  AmArg res;
  for (list<ConferenceRoomParticipant>::iterator it=participants.begin(); 
       it != participants.end(); it++) {
    res.push(it->asArgArray());
  }
  return res;
}

void ConferenceRoom::newParticipant(const string& localtag, 
				    const string& number) {
  participants.push_back(ConferenceRoomParticipant());
  participants.back().localtag = localtag;
  participants.back().number = number;
}

bool ConferenceRoom::hasParticipant(const string& localtag) {
  bool res = false;
  
  for (list<ConferenceRoomParticipant>::iterator it =participants.begin();
       it != participants.end();it++) 
    if (it->localtag == localtag) {
      res = true;
      break;
    }
  return res;
}

void ConferenceRoom::setMuted(const string& localtag, int mute) {
  for (list<ConferenceRoomParticipant>::iterator it =participants.begin();
       it != participants.end();it++) 
    if (it->localtag == localtag) {
      it->setMuted(mute);
      break;
    }
}

bool ConferenceRoom::updateStatus(const string& part_tag, 
				  ConferenceRoomParticipant::ParticipantStatus newstatus, 
				  const string& reason) {
  cleanExpired();
  
  bool res = false;
  list<ConferenceRoomParticipant>::iterator it=participants.begin(); 
  while (it != participants.end()) {
    if (it->localtag == part_tag) {
      it->updateStatus(newstatus, reason);
      res = true;
    }
    it++;     
  }
  return res;
}

