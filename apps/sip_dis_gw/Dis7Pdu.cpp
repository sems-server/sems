/*
 * DIS 7 (IEEE 1278.1-2012) radio communications PDUs
 */

#include "Dis7Pdu.h"

#include <cmath>
#include <cstring>

namespace dis7 {

namespace {

class Writer {
  std::vector<uint8_t>& out;

public:
  explicit Writer(std::vector<uint8_t>& o) : out(o) { out.clear(); }

  void u8(uint8_t v) { out.push_back(v); }
  void u16(uint16_t v) { u8(v >> 8); u8(v & 0xff); }
  void u32(uint32_t v) { u16(v >> 16); u16(v & 0xffff); }
  void u64(uint64_t v) { u32(v >> 32); u32(v & 0xffffffff); }
  void f32(float v) { uint32_t u; memcpy(&u, &v, sizeof(u)); u32(u); }
  void f64(double v) { uint64_t u; memcpy(&u, &v, sizeof(u)); u64(u); }
  void zero(size_t n) { out.insert(out.end(), n, 0); }
  void bytes(const uint8_t* p, size_t n) { out.insert(out.end(), p, p + n); }

  void header(const PduHeader& h, uint8_t type, uint8_t family) {
    u8(h.version);
    u8(h.exercise);
    u8(type);
    u8(family);
    u32(h.timestamp);
    u16(0); // length, patched by finish()
    u8(h.status);
    u8(0);  // padding
  }

  void entityId(const EntityId& e) { u16(e.site); u16(e.application); u16(e.entity); }
  void radioId(const RadioId& r) { entityId(r.entity); u16(r.radio); }

  /** pad the PDU to a 32-bit boundary and store its length in the header */
  void finish() {
    while(out.size() % 4)
      u8(0);
    out[8] = (out.size() >> 8) & 0xff;
    out[9] = out.size() & 0xff;
  }
};

class Reader {
  const uint8_t* p;
  size_t len;
  size_t pos;

public:
  Reader(const uint8_t* b, size_t l) : p(b), len(l), pos(0) {}

  bool has(size_t n) const { return pos + n <= len; }
  void skip(size_t n) { pos += n; }
  size_t offset() const { return pos; }
  const uint8_t* cur() const { return p + pos; }

  uint8_t u8() { return p[pos++]; }
  uint16_t u16() { uint16_t v = (uint16_t)((p[pos] << 8) | p[pos+1]); pos += 2; return v; }
  uint32_t u32() { uint32_t v = (uint32_t)u16() << 16; return v | u16(); }
  uint64_t u64() { uint64_t v = (uint64_t)u32() << 32; return v | u32(); }
  float f32() { uint32_t u = u32(); float v; memcpy(&v, &u, sizeof(v)); return v; }
  double f64() { uint64_t u = u64(); double v; memcpy(&v, &u, sizeof(v)); return v; }

  EntityId entityId() {
    EntityId e;
    e.site = u16();
    e.application = u16();
    e.entity = u16();
    return e;
  }

  RadioId radioId() {
    RadioId r;
    r.entity = entityId();
    r.radio = u16();
    return r;
  }
};

/* validate the header and return a reader positioned after it, limited to
   the PDU's own length (a datagram may carry several bundled PDUs) */
bool open(const uint8_t* buf, size_t len, uint8_t type, size_t min_size,
          PduHeader& hdr, size_t& pdu_len)
{
  if(!decodeHeader(buf, len, hdr) || hdr.type != type || hdr.length < min_size)
    return false;

  pdu_len = hdr.length;
  return true;
}

} // namespace

TransmitterPdu::TransmitterPdu()
  : transmit_state(TX_OFF), input_source(0), antenna_pattern_type(0),
    frequency(0), bandwidth(0), power(0), crypto_system(0), crypto_key_id(0)
{
  for(int i = 0; i < 3; i++) {
    antenna_location[i] = 0;
    relative_antenna_location[i] = 0;
  }
}

EntityStatePdu::EntityStatePdu()
  : force_id(0), appearance(0), capabilities(0)
{
  for(int i = 0; i < 3; i++)
    location[i] = 0;
  memset(marking, 0, sizeof(marking));
}

void encode(const TransmitterPdu& pdu, std::vector<uint8_t>& out)
{
  Writer w(out);
  w.header(pdu.header, PDU_TRANSMITTER, FAMILY_RADIO_COMMUNICATIONS);
  w.radioId(pdu.radio);

  w.u8(pdu.radio_type.kind);
  w.u8(pdu.radio_type.domain);
  w.u16(pdu.radio_type.country);
  w.u8(pdu.radio_type.category);
  w.u8(pdu.radio_type.nomenclature_version);
  w.u16(pdu.radio_type.nomenclature);

  w.u8(pdu.transmit_state);
  w.u8(pdu.input_source);
  w.u16(0); // number of variable transmitter parameter records

  for(int i = 0; i < 3; i++)
    w.f64(pdu.antenna_location[i]);
  for(int i = 0; i < 3; i++)
    w.f32(pdu.relative_antenna_location[i]);

  w.u16(pdu.antenna_pattern_type);
  w.u16(0); // antenna pattern length
  w.u64(pdu.frequency);
  w.f32(pdu.bandwidth);
  w.f32(pdu.power);

  w.u16(pdu.modulation.spread_spectrum);
  w.u16(pdu.modulation.major_modulation);
  w.u16(pdu.modulation.detail);
  w.u16(pdu.modulation.radio_system);

  w.u16(pdu.crypto_system);
  w.u16(pdu.crypto_key_id);
  w.u8(0);  // length of modulation parameters
  w.zero(3);
  w.finish();
}

void encode(const SignalPdu& pdu, std::vector<uint8_t>& out)
{
  Writer w(out);
  w.header(pdu.header, PDU_SIGNAL, FAMILY_RADIO_COMMUNICATIONS);
  w.radioId(pdu.radio);
  w.u16(pdu.encoding_scheme);
  w.u16(pdu.tdl_type);
  w.u32(pdu.sample_rate);
  w.u16((uint16_t)(pdu.data.size() * 8));
  w.u16(pdu.samples);
  if(!pdu.data.empty())
    w.bytes(&pdu.data[0], pdu.data.size());
  w.finish();
}

void encode(const ReceiverPdu& pdu, std::vector<uint8_t>& out)
{
  Writer w(out);
  w.header(pdu.header, PDU_RECEIVER, FAMILY_RADIO_COMMUNICATIONS);
  w.radioId(pdu.radio);
  w.u16(pdu.receiver_state);
  w.u16(0); // padding
  w.f32(pdu.received_power);
  w.radioId(pdu.transmitter);
  w.finish();
}

void encode(const EntityStatePdu& pdu, std::vector<uint8_t>& out)
{
  Writer w(out);
  w.header(pdu.header, PDU_ENTITY_STATE, FAMILY_ENTITY_INFORMATION);
  w.entityId(pdu.entity);
  w.u8(pdu.force_id);
  w.u8(0); // number of variable parameter records

  for(int t = 0; t < 2; t++) { // entity type, alternative entity type
    w.u8(pdu.entity_type.kind);
    w.u8(pdu.entity_type.domain);
    w.u16(pdu.entity_type.country);
    w.u8(pdu.entity_type.category);
    w.u8(pdu.entity_type.subcategory);
    w.u8(pdu.entity_type.specific);
    w.u8(pdu.entity_type.extra);
  }

  w.zero(12); // linear velocity
  for(int i = 0; i < 3; i++)
    w.f64(pdu.location[i]);
  w.zero(12); // orientation
  w.u32(pdu.appearance);

  w.u8(1);    // dead reckoning algorithm: static
  w.zero(15); // other parameters
  w.zero(12); // linear acceleration
  w.zero(12); // angular velocity

  w.u8(1);    // marking character set: ASCII
  w.bytes((const uint8_t*)pdu.marking, sizeof(pdu.marking));
  w.u32(pdu.capabilities);
  w.finish();
}

bool decodeHeader(const uint8_t* buf, size_t len, PduHeader& hdr)
{
  if(!buf || len < HEADER_SIZE)
    return false;

  Reader r(buf, len);
  hdr.version = r.u8();
  hdr.exercise = r.u8();
  hdr.type = r.u8();
  hdr.family = r.u8();
  hdr.timestamp = r.u32();
  hdr.length = r.u16();
  hdr.status = r.u8();

  if(hdr.version < 4 || hdr.version > PROTOCOL_VERSION)
    return false;

  return hdr.length >= HEADER_SIZE && hdr.length <= len;
}

bool decode(const uint8_t* buf, size_t len, TransmitterPdu& pdu)
{
  size_t pdu_len;
  if(!open(buf, len, PDU_TRANSMITTER, TRANSMITTER_PDU_SIZE, pdu.header, pdu_len))
    return false;

  Reader r(buf, pdu_len);
  r.skip(HEADER_SIZE);
  pdu.radio = r.radioId();

  pdu.radio_type.kind = r.u8();
  pdu.radio_type.domain = r.u8();
  pdu.radio_type.country = r.u16();
  pdu.radio_type.category = r.u8();
  pdu.radio_type.nomenclature_version = r.u8();
  pdu.radio_type.nomenclature = r.u16();

  pdu.transmit_state = r.u8();
  pdu.input_source = r.u8();
  r.skip(2); // variable transmitter parameter count (DIS 7) or padding

  for(int i = 0; i < 3; i++)
    pdu.antenna_location[i] = r.f64();
  for(int i = 0; i < 3; i++)
    pdu.relative_antenna_location[i] = r.f32();

  pdu.antenna_pattern_type = r.u16();
  r.skip(2); // antenna pattern length
  pdu.frequency = r.u64();
  pdu.bandwidth = r.f32();
  pdu.power = r.f32();

  pdu.modulation.spread_spectrum = r.u16();
  pdu.modulation.major_modulation = r.u16();
  pdu.modulation.detail = r.u16();
  pdu.modulation.radio_system = r.u16();

  pdu.crypto_system = r.u16();
  pdu.crypto_key_id = r.u16();
  return true;
}

bool decode(const uint8_t* buf, size_t len, SignalPdu& pdu)
{
  size_t pdu_len;
  if(!open(buf, len, PDU_SIGNAL, SIGNAL_PDU_HEADER_SIZE, pdu.header, pdu_len))
    return false;

  Reader r(buf, pdu_len);
  r.skip(HEADER_SIZE);
  pdu.radio = r.radioId();
  pdu.encoding_scheme = r.u16();
  pdu.tdl_type = r.u16();
  pdu.sample_rate = r.u32();
  pdu.data_length = r.u16();
  pdu.samples = r.u16();

  size_t data_bytes = (pdu.data_length + 7) / 8;
  if(!r.has(data_bytes))
    return false;

  pdu.data.assign(r.cur(), r.cur() + data_bytes);
  return true;
}

bool decode(const uint8_t* buf, size_t len, ReceiverPdu& pdu)
{
  size_t pdu_len;
  if(!open(buf, len, PDU_RECEIVER, RECEIVER_PDU_SIZE, pdu.header, pdu_len))
    return false;

  Reader r(buf, pdu_len);
  r.skip(HEADER_SIZE);
  pdu.radio = r.radioId();
  pdu.receiver_state = r.u16();
  r.skip(2); // padding
  pdu.received_power = r.f32();
  pdu.transmitter = r.radioId();
  return true;
}

uint32_t timestamp(const struct timespec& realtime, bool absolute)
{
  double past_hour = (double)(realtime.tv_sec % 3600)
    + (double)realtime.tv_nsec / 1e9;
  double units = past_hour * (2147483648.0 / 3600.0);

  uint32_t ts = units >= 2147483647.0 ? 0x7fffffff : (uint32_t)units;
  return (ts << 1) | (absolute ? 1 : 0);
}

void geodeticToEcef(double lat_deg, double lon_deg, double alt_m, double ecef[3])
{
  const double a = 6378137.0;               // WGS-84 semi-major axis
  const double f = 1.0 / 298.257223563;     // WGS-84 flattening
  const double e2 = f * (2.0 - f);

  double lat = lat_deg * M_PI / 180.0;
  double lon = lon_deg * M_PI / 180.0;
  double sin_lat = sin(lat);
  double n = a / sqrt(1.0 - e2 * sin_lat * sin_lat);

  ecef[0] = (n + alt_m) * cos(lat) * cos(lon);
  ecef[1] = (n + alt_m) * cos(lat) * sin(lon);
  ecef[2] = (n * (1.0 - e2) + alt_m) * sin_lat;
}

} // namespace dis7
