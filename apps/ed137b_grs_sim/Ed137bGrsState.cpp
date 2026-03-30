/*
 * ED-137B Ground Radio Station (GRS) Simulator - State & Change Logger
 */

#include "Ed137bGrsState.h"
#include "Ed137bGrsSipHelper.h"
#include "log.h"

#include <cstring>
#include <ctime>
#include <sys/time.h>

// --- Ed137bGrsState ---

map<string, string> Ed137bGrsState::toMap() const
{
  map<string, string> m;
  m["frequency"]       = frequency;
  m["channel_spacing"] = channel_spacing;
  m["txrx_mode"]       = txrx_mode;
  m["radio_type"]      = radio_type;
  m["bss"]             = bss;
  m["squelch_ctrl"]    = squelch_ctrl;
  m["climax"]          = climax ? "true" : "false";
  m["priority"]        = priority;
  m["wg67_version"]    = wg67_version;
  return m;
}

void Ed137bGrsState::fromSdpMap(const map<string, string>& attrs)
{
  auto get = [&](const string& key) -> string {
    auto it = attrs.find(key);
    return (it != attrs.end()) ? it->second : "";
  };

  string v;
  v = get(ED137B_SDP_TYPE);     if (!v.empty()) radio_type = v;
  v = get(ED137B_SDP_FREQ);     if (!v.empty()) frequency = v;
  v = get(ED137B_SDP_TXRXMODE); if (!v.empty()) txrx_mode = v;
  v = get(ED137B_SDP_CLD);      if (!v.empty()) channel_spacing = v;
  v = get(ED137B_SDP_BSS);      if (!v.empty()) bss = v;
  v = get(ED137B_SDP_SQC);      if (!v.empty()) squelch_ctrl = v;
  climax = (get(ED137B_SDP_CLIMAX) == "true");
}

// --- Ed137bGrsChangeLogger ---

Ed137bGrsChangeLogger::Ed137bGrsChangeLogger()
  : log_fp(NULL)
{
}

Ed137bGrsChangeLogger::~Ed137bGrsChangeLogger()
{
  close();
}

bool Ed137bGrsChangeLogger::open(const string& path)
{
  AmLock l(log_mutex);
  if (log_fp) fclose(log_fp);

  log_path = path;
  log_fp = fopen(path.c_str(), "a");
  if (!log_fp) {
    ERROR("ED137B-GRS: cannot open log file '%s': %s\n", path.c_str(), strerror(errno));
    return false;
  }
  INFO("ED137B-GRS: logging to '%s'\n", path.c_str());
  return true;
}

void Ed137bGrsChangeLogger::close()
{
  AmLock l(log_mutex);
  if (log_fp) {
    fclose(log_fp);
    log_fp = NULL;
  }
}

void Ed137bGrsChangeLogger::logSessionStart(const string& callid,
                                            const Ed137bGrsState& state)
{
  string msg = "SESSION_START:";
  msg += " freq=" + state.frequency;
  msg += " mode=" + state.txrx_mode;
  msg += " spacing=" + state.channel_spacing;
  msg += " type=" + state.radio_type;
  msg += " wg67=" + state.wg67_version;
  if (!state.bss.empty()) msg += " bss=" + state.bss;
  if (!state.squelch_ctrl.empty()) msg += " sqc=" + state.squelch_ctrl;
  if (state.climax) msg += " climax";
  writeLine(callid, msg);
}

void Ed137bGrsChangeLogger::logSessionEnd(const string& callid)
{
  writeLine(callid, "SESSION_END");
}

bool Ed137bGrsChangeLogger::logChanges(const string& callid,
                                       const Ed137bGrsState& old_state,
                                       const Ed137bGrsState& new_state)
{
  static const struct {
    const char* key;
    const char* label;
  } fields[] = {
    {"frequency",       "FREQ_CHANGE"},
    {"channel_spacing", "SPACING_CHANGE"},
    {"txrx_mode",       "TXRXMODE_CHANGE"},
    {"radio_type",      "TYPE_CHANGE"},
    {"bss",             "BSS_CHANGE"},
    {"squelch_ctrl",    "SQC_CHANGE"},
    {"climax",          "CLIMAX_CHANGE"},
    {"priority",        "PRIORITY_CHANGE"},
    {"wg67_version",    "WG67_CHANGE"},
    {NULL, NULL}
  };

  map<string, string> old_map = old_state.toMap();
  map<string, string> new_map = new_state.toMap();
  bool changed = false;

  for (int i = 0; fields[i].key; i++) {
    const string& old_val = old_map[fields[i].key];
    const string& new_val = new_map[fields[i].key];
    if (old_val != new_val) {
      string msg = string(fields[i].label) + ": " + old_val + " -> " + new_val;
      writeLine(callid, msg);
      INFO("ED137B-GRS [%s] %s\n", callid.c_str(), msg.c_str());
      changed = true;
    }
  }
  return changed;
}

void Ed137bGrsChangeLogger::logEvent(const string& callid, const string& event)
{
  writeLine(callid, event);
}

void Ed137bGrsChangeLogger::writeLine(const string& callid, const string& msg)
{
  AmLock l(log_mutex);
  if (!log_fp) return;

  fprintf(log_fp, "%s [%s] %s\n", timestamp().c_str(), callid.c_str(), msg.c_str());
  fflush(log_fp);
}

string Ed137bGrsChangeLogger::timestamp()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);

  struct tm tm;
  localtime_r(&tv.tv_sec, &tm);

  char buf[32];
  int len = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
  snprintf(buf + len, sizeof(buf) - len, ".%03d", (int)(tv.tv_usec / 1000));
  return string(buf);
}
