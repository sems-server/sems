/*
 * DIS 7 (IEEE 1278.1-2012) radio communications PDUs
 *
 * Self-contained big-endian encoder/decoder for the handful of PDUs the
 * SIP to DIS gateway needs: Transmitter, Signal, Receiver and Entity State.
 * OpenDIS does not implement the DIS 7 Transmitter and Signal PDUs, so they
 * are marshalled here directly from the standard's field layout.
 *
 * The decoders only look at the fixed part of each PDU and accept protocol
 * versions 4 to 7: the fixed layouts of the radio PDUs did not change between
 * them (DIS 7 turned header padding into the PDU status byte and Transmitter
 * PDU padding into the variable transmitter parameter count).
 */

#ifndef _DIS7_PDU_H
#define _DIS7_PDU_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <vector>

namespace dis7 {

const uint8_t PROTOCOL_VERSION = 7;

enum PduType {
  PDU_ENTITY_STATE = 1,
  PDU_TRANSMITTER  = 25,
  PDU_SIGNAL       = 26,
  PDU_RECEIVER     = 27
};

enum ProtocolFamily {
  FAMILY_ENTITY_INFORMATION   = 1,
  FAMILY_RADIO_COMMUNICATIONS = 4
};

enum TransmitState {
  TX_OFF                 = 0,
  TX_ON_NOT_TRANSMITTING = 1,
  TX_ON_TRANSMITTING     = 2
};

enum ReceiverState {
  RX_OFF              = 0,
  RX_ON_NOT_RECEIVING = 1,
  RX_ON_RECEIVING     = 2
};

/* Signal PDU encoding scheme: the top two bits are the encoding class
   (0 = encoded audio), the lower 14 bits the encoding type (SISO-REF-010). */
const uint16_t ENCODING_CLASS_MASK = 0xC000;
const uint16_t ENCODING_TYPE_MASK  = 0x3FFF;

enum EncodingType {
  ENC_MULAW8         = 1,   // 8-bit mu-law (ITU-T G.711)
  ENC_PCM16_BE       = 4,   // 16-bit linear PCM, two's complement, big endian
  ENC_PCM8_UNSIGNED  = 5,   // 8-bit linear PCM, unsigned
  ENC_PCM16_LE       = 100  // 16-bit linear PCM, two's complement, little endian
};

const size_t HEADER_SIZE             = 12;
const size_t TRANSMITTER_PDU_SIZE    = 104; // without variable records
const size_t SIGNAL_PDU_HEADER_SIZE  = 32;  // without the data field
const size_t RECEIVER_PDU_SIZE       = 36;
const size_t ENTITY_STATE_PDU_SIZE   = 144; // without variable parameters
const size_t MAX_PDU_SIZE            = 8192;

struct EntityId {
  uint16_t site;
  uint16_t application;
  uint16_t entity;

  EntityId() : site(0), application(0), entity(0) {}
  EntityId(uint16_t s, uint16_t a, uint16_t e)
    : site(s), application(a), entity(e) {}
};

/** Radio Reference ID + Radio Number */
struct RadioId {
  EntityId entity;
  uint16_t radio;

  RadioId() : radio(0) {}
  RadioId(const EntityId& e, uint16_t r) : entity(e), radio(r) {}

  /** packs the identifier into one integer, e.g. for use as a map key */
  uint64_t key() const {
    return ((uint64_t)entity.site << 48) | ((uint64_t)entity.application << 32)
      | ((uint64_t)entity.entity << 16) | radio;
  }

  bool operator==(const RadioId& o) const { return key() == o.key(); }
  bool operator<(const RadioId& o) const { return key() < o.key(); }
};

struct PduHeader {
  uint8_t  version;
  uint8_t  exercise;
  uint8_t  type;
  uint8_t  family;
  uint32_t timestamp;
  uint16_t length;
  uint8_t  status;

  PduHeader()
    : version(PROTOCOL_VERSION), exercise(1), type(0), family(0),
      timestamp(0), length(0), status(0) {}
};

struct EntityType {
  uint8_t  kind;
  uint8_t  domain;
  uint16_t country;
  uint8_t  category;
  uint8_t  subcategory;
  uint8_t  specific;
  uint8_t  extra;

  EntityType()
    : kind(0), domain(0), country(0), category(0),
      subcategory(0), specific(0), extra(0) {}
};

/** Radio Type record (entity kind 7) */
struct RadioType {
  uint8_t  kind;
  uint8_t  domain;
  uint16_t country;
  uint8_t  category;
  uint8_t  nomenclature_version;
  uint16_t nomenclature;

  RadioType()
    : kind(7), domain(0), country(0), category(0),
      nomenclature_version(0), nomenclature(0) {}
};

struct ModulationType {
  uint16_t spread_spectrum;
  uint16_t major_modulation; // not "major": <sys/types.h> defines a macro
  uint16_t detail;
  uint16_t radio_system;

  // default: amplitude modulation (major 1, detail 2), generic radio
  ModulationType()
    : spread_spectrum(0), major_modulation(1), detail(2), radio_system(1) {}
};

struct TransmitterPdu {
  PduHeader      header;
  RadioId        radio;
  RadioType      radio_type;
  uint8_t        transmit_state;
  uint8_t        input_source;
  double         antenna_location[3];          // world coordinates (ECEF, m)
  float          relative_antenna_location[3]; // entity coordinates (m)
  uint16_t       antenna_pattern_type;
  uint64_t       frequency;                    // Hz
  float          bandwidth;                    // Hz
  float          power;                        // dBm
  ModulationType modulation;
  uint16_t       crypto_system;
  uint16_t       crypto_key_id;

  TransmitterPdu();
};

struct SignalPdu {
  PduHeader            header;
  RadioId              radio;
  uint16_t             encoding_scheme;
  uint16_t             tdl_type;
  uint32_t             sample_rate;
  uint16_t             data_length;  // bits
  uint16_t             samples;
  std::vector<uint8_t> data;

  SignalPdu()
    : encoding_scheme(ENC_MULAW8), tdl_type(0), sample_rate(8000),
      data_length(0), samples(0) {}
};

struct ReceiverPdu {
  PduHeader header;
  RadioId   radio;
  uint16_t  receiver_state;
  float     received_power;     // dBm
  RadioId   transmitter;

  ReceiverPdu() : receiver_state(RX_OFF), received_power(0) {}
};

struct EntityStatePdu {
  PduHeader  header;
  EntityId   entity;
  uint8_t    force_id;
  EntityType entity_type;
  double     location[3];      // world coordinates (ECEF, m)
  uint32_t   appearance;
  char       marking[11];      // ASCII, zero padded
  uint32_t   capabilities;

  EntityStatePdu();
};

/** Serialise a PDU; the header type, family and length are filled in. */
void encode(const TransmitterPdu& pdu, std::vector<uint8_t>& out);
void encode(const SignalPdu& pdu, std::vector<uint8_t>& out);
void encode(const ReceiverPdu& pdu, std::vector<uint8_t>& out);
void encode(const EntityStatePdu& pdu, std::vector<uint8_t>& out);

/** Decode the PDU header; fails if the buffer is shorter than a header,
    than the length the header announces or the version is unsupported. */
bool decodeHeader(const uint8_t* buf, size_t len, PduHeader& hdr);

/** Decode the fixed part of a PDU; return false on a malformed PDU. */
bool decode(const uint8_t* buf, size_t len, TransmitterPdu& pdu);
bool decode(const uint8_t* buf, size_t len, SignalPdu& pdu);
bool decode(const uint8_t* buf, size_t len, ReceiverPdu& pdu);

/** DIS timestamp: units of 3600 s / 2^31 past the hour, shifted left by one;
    the least significant bit is set for absolute (clock synchronised) time. */
uint32_t timestamp(const struct timespec& realtime, bool absolute);

/** WGS-84 geodetic position (degrees, metres) to geocentric ECEF metres. */
void geodeticToEcef(double lat_deg, double lon_deg, double alt_m, double ecef[3]);

} // namespace dis7

#endif
