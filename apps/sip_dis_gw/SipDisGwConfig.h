/*
 * Configuration of the SIP to DIS radio gateway
 */

#ifndef _SIP_DIS_GW_CONFIG_H
#define _SIP_DIS_GW_CONFIG_H

#include "DisNetwork.h"

#include <stdint.h>

#include <map>
#include <string>

class AmConfigReader;

struct SipDisGwConfig {
  enum PttMode { PTT_VOX, PTT_DTMF, PTT_ALWAYS, PTT_LISTEN };

  DisGwConfig dis;

  std::map<std::string, uint64_t> radios; // name -> frequency (Hz)
  bool allow_dialed_frequency;

  PttMode      ptt_mode;
  double       vox_threshold_dbfs;
  unsigned int vox_hang_ms;
  unsigned int vox_preroll_ms;
  unsigned int ptt_max_ms;

  SipDisGwConfig();

  /** Read the module configuration; returns false on invalid settings. */
  bool load(const AmConfigReader& cfg);

  /** Map the user part of the request URI to a frequency. */
  bool lookupFrequency(const std::string& user, uint64_t& frequency) const;

  /** "118100000", "118.1 MHz", "118100kHz" -> Hz */
  static bool parseFrequency(const std::string& s, uint64_t& hz);
};

#endif
