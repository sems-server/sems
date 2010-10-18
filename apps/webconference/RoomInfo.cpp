
#include "RoomInfo.h"
#include "WebConference.h"

#include <string.h>
#include "log.h"

void ConferenceRoomParticipant::updateAccess(const struct timeval& now) {
  memcpy(&last_access_time, &now, sizeof(struct timeval));
}

bool ConferenceRoomParticipant::expired(const struct timeval& now) {
  if (Finished != status)
    return false;

  if (WebConferenceFactory::ParticipantExpiredDelay < 0)
    return false;

  struct timeval diff;
  timersub(&now,&last_access_time,&diff);
  return (diff.tv_sec > 0) &&
    (unsigned int)diff.tv_sec > (unsigned int)WebConferenceFactory::ParticipantExpiredDelay;
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
					     const string& reason,
					     struct timeval& now) {
  status = new_status;
  last_reason = reason;
  updateAccess(now);
}

ConferenceRoom::ConferenceRoom() {
  gettimeofday(&last_access_time, NULL);
}

void ConferenceRoom::cleanExpired() {
  struct timeval now;
  gettimeofday(&now, NULL);  
  bool is_updated = false;

  list<ConferenceRoomParticipant>::iterator it=participants.begin(); 
  while (it != participants.end()) {
    if (it->expired(now)) {
      participants.erase(it);
      it=participants.begin();
      is_updated = true;
    } else 
      it++;
  }

  if (is_updated)
    memcpy(&last_access_time, &now, sizeof(struct timeval)); 
}

AmArg ConferenceRoom::asArgArray() {
  cleanExpired();
  AmArg res;
  res.assertArray(0); // make array from it

  for (list<ConferenceRoomParticipant>::iterator it=participants.begin(); 
       it != participants.end(); it++) {
    res.push(it->asArgArray());
  }
  return res;
}

vector<string> ConferenceRoom::participantLtags() {
  cleanExpired();
  vector<string> res;
  for (list<ConferenceRoomParticipant>::iterator it=participants.begin(); 
       it != participants.end(); it++) {
    res.push_back(it->localtag);
  }
  return res;
}

void ConferenceRoom::newParticipant(const string& localtag, 
				    const string& number) {
  gettimeofday(&last_access_time, NULL);

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
  gettimeofday(&last_access_time, NULL);

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
  gettimeofday(&last_access_time, NULL);

  bool res = false;
  for (list<ConferenceRoomParticipant>::iterator it=participants.begin(); 
       it != participants.end(); it++) {
    if (it->localtag == part_tag) {
      it->updateStatus(newstatus, reason, last_access_time);
      res = true;
      break;
    }
  }

  cleanExpired();
  return res;
}

bool ConferenceRoom::expired() {
  struct timeval now;
  gettimeofday(&now, NULL);
  return expired(now);
}

bool ConferenceRoom::expired(const struct timeval& now) {
  if (!participants.empty())
    return false;

  if (WebConferenceFactory::RoomExpiredDelay < 0)
    return false;

  struct timeval diff;
  timersub(&now,&last_access_time,&diff);
  return (diff.tv_sec > 0) &&
    (unsigned int)diff.tv_sec > (unsigned int)WebConferenceFactory::RoomExpiredDelay;
}

bool ConferenceRoom::hard_expired(const struct timeval& now) {
  return expiry_time && (now.tv_sec > expiry_time);
}
