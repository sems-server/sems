/*
 * Configuration of the SIP to DIS radio gateway
 */

#include "SipDisGwConfig.h"

#include "AmConfigReader.h"
#include "log.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <strings.h>

#include <vector>

namespace {

std::string trim(const std::string& s)
{
  size_t b = s.find_first_not_of(" \t");
  if(b == std::string::npos)
    return "";
  size_t e = s.find_last_not_of(" \t");
  return s.substr(b, e - b + 1);
}

bool parseBool(const std::string& s, bool& v)
{
  if(s == "1" || s == "yes" || s == "true" || s == "on") { v = true; return true; }
  if(s == "0" || s == "no" || s == "false" || s == "off") { v = false; return true; }
  return false;
}

bool parseULong(const std::string& s, unsigned long min, unsigned long max,
                unsigned long& v)
{
  std::string t = trim(s);
  if(t.empty() || !isdigit((unsigned char)t[0]))
    return false;

  char* end = NULL;
  errno = 0;
  unsigned long long r = strtoull(t.c_str(), &end, 10);
  if(errno || *end || r < min || r > max)
    return false;

  v = (unsigned long)r;
  return true;
}

bool parseDouble(const std::string& s, double& v)
{
  std::string t = trim(s);
  if(t.empty())
    return false;

  char* end = NULL;
  double r = strtod(t.c_str(), &end);
  if(*end || !isfinite(r))
    return false;

  v = r;
  return true;
}

/** "7.1.840.1.0.0" or "7:1:840:1:0:0" */
bool parseTuple(const std::string& s, size_t count, std::vector<unsigned long>& v)
{
  v.clear();
  std::string t = trim(s);
  size_t pos = 0;

  while(true) {
    size_t sep = t.find_first_of(".:", pos);
    unsigned long n;
    if(!parseULong(t.substr(pos, sep == std::string::npos ? sep : sep - pos),
                   0, 65535, n))
      return false;
    v.push_back(n);

    if(sep == std::string::npos)
      break;
    pos = sep + 1;
  }

  return v.size() == count;
}

/* Helper that reads one parameter if it is present and reports bad values.
   Missing parameters keep the defaults of SipDisGwConfig. */
class Params {
  const AmConfigReader& cfg;
  bool ok;

  bool bad(const char* name, const char* expected) {
    ERROR("sip_dis_gw: invalid %s '%s' (expected %s)\n", name,
          cfg.getParameter(name).c_str(), expected);
    ok = false;
    return false;
  }

public:
  explicit Params(const AmConfigReader& c) : cfg(c), ok(true) {}
  bool valid() const { return ok; }

  bool has(const char* name) const { return !trim(cfg.getParameter(name)).empty(); }
  std::string str(const char* name) const { return trim(cfg.getParameter(name)); }

  void flag(const char* name, bool& v) {
    if(has(name) && !parseBool(str(name), v))
      bad(name, "yes or no");
  }

  template<class T>
  void num(const char* name, unsigned long min, unsigned long max, T& v) {
    unsigned long n;
    if(!has(name))
      return;
    if(!parseULong(str(name), min, max, n)) {
      bad(name, "an integer in range");
      return;
    }
    v = (T)n;
  }

  bool real(const char* name, double& v) {
    if(!has(name))
      return false;
    if(!parseDouble(str(name), v))
      return bad(name, "a number");
    return true;
  }

  bool tuple(const char* name, size_t count, std::vector<unsigned long>& v) {
    if(!has(name))
      return false;
    if(!parseTuple(str(name), count, v))
      return bad(name, "dot separated integers");
    for(size_t i = 0; i < v.size(); i++) {
      // country (and nomenclature) are 16 bit, everything else 8 bit
      if(v[i] > 255 && !(i == 2 || (count == 6 && i == 5)))
        return bad(name, "8 bit fields below 256");
    }
    return true;
  }

  bool frequency(const char* name, uint64_t& v) {
    if(!has(name))
      return false;
    if(!SipDisGwConfig::parseFrequency(str(name), v))
      return bad(name, "a frequency such as 118100000, 118.1MHz or 118100kHz");
    return true;
  }

  bool fail(const char* name, const char* expected) { return bad(name, expected); }
};

} // namespace

SipDisGwConfig::SipDisGwConfig()
  : allow_dialed_frequency(true), ptt_mode(PTT_VOX),
    vox_threshold_dbfs(-40), vox_hang_ms(600), vox_preroll_ms(40),
    ptt_max_ms(60000)
{
}

bool SipDisGwConfig::parseFrequency(const std::string& s, uint64_t& hz)
{
  std::string t = trim(s);
  if(t.empty() || !(isdigit((unsigned char)t[0]) || t[0] == '.'))
    return false;

  char* end = NULL;
  double v = strtod(t.c_str(), &end);
  std::string unit = trim(end);

  double mult;
  if(unit.empty() || !strcasecmp(unit.c_str(), "hz"))
    mult = 1;
  else if(!strcasecmp(unit.c_str(), "khz"))
    mult = 1e3;
  else if(!strcasecmp(unit.c_str(), "mhz"))
    mult = 1e6;
  else if(!strcasecmp(unit.c_str(), "ghz"))
    mult = 1e9;
  else
    return false;

  v *= mult;
  if(!isfinite(v) || v < 1 || v > 1e12)
    return false;

  hz = (uint64_t)llround(v);
  return true;
}

bool SipDisGwConfig::lookupFrequency(const std::string& user, uint64_t& frequency) const
{
  std::map<std::string, uint64_t>::const_iterator it = radios.find(user);
  if(it != radios.end()) {
    frequency = it->second;
    return true;
  }

  if(!allow_dialed_frequency || user.empty())
    return false;

  // digits only: kHz (118100), with a decimal point: MHz (118.100)
  bool dot = false;
  for(size_t i = 0; i < user.size(); i++) {
    if(user[i] == '.' && !dot)
      dot = true;
    else if(!isdigit((unsigned char)user[i]))
      return false;
  }

  return parseFrequency(user + (dot ? "MHz" : "kHz"), frequency);
}

bool SipDisGwConfig::load(const AmConfigReader& cfg)
{
  Params p(cfg);
  std::vector<unsigned long> t;

  // network
  if(p.has("dis_mode")) {
    std::string mode = p.str("dis_mode");
    if(mode == "broadcast")
      dis.mode = DisGwConfig::BROADCAST;
    else if(mode == "multicast")
      dis.mode = DisGwConfig::MULTICAST;
    else if(mode == "unicast")
      dis.mode = DisGwConfig::UNICAST;
    else
      p.fail("dis_mode", "broadcast, multicast or unicast");
  }
  if(p.has("dis_address"))
    dis.address = p.str("dis_address");
  else if(dis.mode == DisGwConfig::MULTICAST)
    dis.address = "239.1.2.3";
  else if(dis.mode == DisGwConfig::UNICAST)
    dis.address = "127.0.0.1";
  p.num("dis_port", 1, 65535, dis.port);
  dis.interface_addr = p.str("dis_interface");
  p.num("dis_multicast_ttl", 0, 255, dis.multicast_ttl);
  p.flag("dis_multicast_loop", dis.multicast_loop);

  // identity
  p.num("dis_exercise_id", 1, 255, dis.exercise_id);
  p.num("dis_site_id", 1, 65534, dis.entity.site);
  p.num("dis_application_id", 1, 65534, dis.entity.application);
  p.num("dis_entity_id", 1, 65534, dis.entity.entity);
  p.flag("dis_absolute_timestamps", dis.absolute_timestamps);

  // radio
  if(p.tuple("radio_entity_type", 6, t)) {
    dis.radio_type.kind = (uint8_t)t[0];
    dis.radio_type.domain = (uint8_t)t[1];
    dis.radio_type.country = (uint16_t)t[2];
    dis.radio_type.category = (uint8_t)t[3];
    dis.radio_type.nomenclature_version = (uint8_t)t[4];
    dis.radio_type.nomenclature = (uint16_t)t[5];
  }
  if(p.has("modulation")) {
    if(!parseTuple(p.str("modulation"), 4, t))
      p.fail("modulation", "spread.major.detail.system");
    else {
      dis.modulation.spread_spectrum = (uint16_t)t[0];
      dis.modulation.major_modulation = (uint16_t)t[1];
      dis.modulation.detail = (uint16_t)t[2];
      dis.modulation.radio_system = (uint16_t)t[3];
    }
  }
  p.num("input_source", 0, 255, dis.input_source);

  uint64_t hz;
  if(p.frequency("transmit_bandwidth", hz))
    dis.bandwidth = (float)hz;
  double d;
  if(p.real("transmit_power", d))
    dis.power = (float)d;
  if(p.has("frequency_tolerance")) {
    if(p.str("frequency_tolerance") == "0")
      dis.frequency_tolerance = 0;
    else
      p.frequency("frequency_tolerance", dis.frequency_tolerance);
  }

  double lat = 0, lon = 0, alt = 0;
  p.real("antenna_latitude", lat);
  p.real("antenna_longitude", lon);
  p.real("antenna_altitude", alt);
  if(lat < -90 || lat > 90)
    p.fail("antenna_latitude", "-90 to 90 degrees");
  if(lon < -180 || lon > 180)
    p.fail("antenna_longitude", "-180 to 180 degrees");
  dis7::geodeticToEcef(lat, lon, alt, dis.antenna_location);

  // audio
  if(p.has("encoding")) {
    std::string enc = p.str("encoding");
    if(enc == "mulaw")
      dis.encoding = dis7::ENC_MULAW8;
    else if(enc == "pcm16be")
      dis.encoding = dis7::ENC_PCM16_BE;
    else if(enc == "pcm16le")
      dis.encoding = dis7::ENC_PCM16_LE;
    else
      p.fail("encoding", "mulaw, pcm16be or pcm16le");
  }
  p.num("signal_frame_ms", 10, 100, dis.frame_ms);
  p.num("jitter_prebuffer_ms", 0, 1000, dis.jitter_prebuffer_ms);
  p.num("jitter_max_ms", 20, 5000, dis.jitter_max_ms);
  p.flag("local_loop", dis.local_loop);

  // timing
  p.num("transmitter_heartbeat_ms", 100, 60000, dis.transmitter_heartbeat_ms);
  p.num("receiver_heartbeat_ms", 100, 60000, dis.receiver_heartbeat_ms);
  p.num("remote_transmitter_timeout_ms", 200, 600000, dis.remote_timeout_ms);
  p.num("tx_idle_ms", 100, 60000, dis.tx_idle_ms);

  // optional PDUs
  p.flag("publish_receiver", dis.publish_receiver);
  p.flag("publish_entity_state", dis.publish_entity_state);
  p.num("entity_heartbeat_ms", 100, 60000, dis.entity_heartbeat_ms);
  p.num("entity_force_id", 0, 255, dis.force_id);
  if(p.tuple("entity_type", 7, t)) {
    dis.entity_type.kind = (uint8_t)t[0];
    dis.entity_type.domain = (uint8_t)t[1];
    dis.entity_type.country = (uint16_t)t[2];
    dis.entity_type.category = (uint8_t)t[3];
    dis.entity_type.subcategory = (uint8_t)t[4];
    dis.entity_type.specific = (uint8_t)t[5];
    dis.entity_type.extra = (uint8_t)t[6];
  }
  if(p.has("entity_marking"))
    dis.entity_marking = p.str("entity_marking");

  // radios: radio_<name>=<frequency>
  radios.clear();
  for(std::map<std::string, std::string>::const_iterator it = cfg.begin();
      it != cfg.end(); ++it) {
    if(it->first.compare(0, 6, "radio_") || it->first == "radio_entity_type")
      continue;

    std::string name = it->first.substr(6);
    uint64_t f;
    if(name.empty() || !p.frequency(it->first.c_str(), f))
      continue;
    radios[name] = f;
  }
  p.flag("allow_dialed_frequency", allow_dialed_frequency);

  // push-to-talk
  if(p.has("ptt_mode")) {
    std::string mode = p.str("ptt_mode");
    if(mode == "vox")
      ptt_mode = PTT_VOX;
    else if(mode == "dtmf")
      ptt_mode = PTT_DTMF;
    else if(mode == "always")
      ptt_mode = PTT_ALWAYS;
    else if(mode == "listen")
      ptt_mode = PTT_LISTEN;
    else
      p.fail("ptt_mode", "vox, dtmf, always or listen");
  }
  if(p.real("vox_threshold_dbfs", d)) {
    if(d > 0 || d < -96)
      p.fail("vox_threshold_dbfs", "-96 to 0");
    else
      vox_threshold_dbfs = d;
  }
  p.num("vox_hang_ms", 0, 10000, vox_hang_ms);
  p.num("vox_preroll_ms", 0, 500, vox_preroll_ms);
  p.num("ptt_max_ms", 0, 3600000, ptt_max_ms);

  // the playout buffer would otherwise raise its cap to the prebuffer
  if(dis.jitter_prebuffer_ms > dis.jitter_max_ms) {
    ERROR("sip_dis_gw: jitter_prebuffer_ms (%u) exceeds jitter_max_ms (%u)\n",
          dis.jitter_prebuffer_ms, dis.jitter_max_ms);
    return false;
  }

  if(radios.empty() && !allow_dialed_frequency) {
    ERROR("sip_dis_gw: no radio_<name> configured and allow_dialed_frequency=no\n");
    return false;
  }

  return p.valid();
}
