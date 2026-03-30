/*
 * ED-137B Ground Radio Station (GRS) Simulator - State & Change Logger
 *
 * Naming: "Ed137bGrs" prefix throughout this module.
 *   - ED-137B = EUROCAE standard revision B for VoIP in ATC
 *   - GRS     = Ground Radio Station (the radio equipment being simulated)
 *
 * Tracks GRS signalling state and logs all parameter changes
 * (frequency, channel spacing, txrxmode, etc.) to a dedicated log file.
 */

#ifndef _ED137B_GRS_STATE_H_
#define _ED137B_GRS_STATE_H_

#include "AmThread.h"
#include <string>
#include <map>
#include <cstdio>

using std::string;
using std::map;

// Signalling state of one simulated ED-137B Ground Radio Station
struct Ed137bGrsState {
  string frequency;
  string channel_spacing;
  string txrx_mode;
  string radio_type;
  string bss;
  string squelch_ctrl;
  bool   climax;
  string priority;
  string wg67_version;

  Ed137bGrsState()
    : climax(false) {}

  // Convert to key-value map for diffing
  map<string, string> toMap() const;

  // Update from key-value map (e.g., parsed SDP attributes)
  void fromSdpMap(const map<string, string>& attrs);
};

// Logs all ED-137B GRS signalling state changes to a dedicated file
class Ed137bGrsChangeLogger {
  FILE*    log_fp;
  string   log_path;
  AmMutex  log_mutex;

public:
  Ed137bGrsChangeLogger();
  ~Ed137bGrsChangeLogger();

  // Open log file (call from factory onLoad)
  bool open(const string& path);

  // Close log file
  void close();

  // Log a session start event with initial state
  void logSessionStart(const string& callid, const Ed137bGrsState& state);

  // Log a session end event
  void logSessionEnd(const string& callid);

  // Compare old and new state, log all changes. Returns true if any changed.
  bool logChanges(const string& callid,
                  const Ed137bGrsState& old_state,
                  const Ed137bGrsState& new_state);

  // Log a generic event line
  void logEvent(const string& callid, const string& event);

private:
  void writeLine(const string& callid, const string& msg);
  static string timestamp();
};

#endif // _ED137B_GRS_STATE_H_
