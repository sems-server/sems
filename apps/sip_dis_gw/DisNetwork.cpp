/*
 * DIS network side of the SIP to DIS gateway
 */

#include "DisNetwork.h"

#include "log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>

/** a remote source that delivered no audio for this long is played out */
#define RX_SOURCE_IDLE_MS 500
/** how often the network thread sends heartbeats and expires state */
#define HOUSEKEEPING_INTERVAL_MS 100

DisGwConfig::DisGwConfig()
  : mode(BROADCAST), address("255.255.255.255"), port(3000),
    multicast_ttl(16), multicast_loop(true),
    exercise_id(1), entity(1, 200, 1), absolute_timestamps(false),
    input_source(0), bandwidth(25000), power(40), frequency_tolerance(500),
    encoding(dis7::ENC_MULAW8), frame_ms(20),
    jitter_prebuffer_ms(60), jitter_max_ms(400), local_loop(true),
    transmitter_heartbeat_ms(2000), receiver_heartbeat_ms(5000),
    remote_timeout_ms(4800), tx_idle_ms(1000),
    publish_receiver(true), publish_entity_state(false),
    entity_heartbeat_ms(5000), force_id(0), entity_marking("SEMS-GW")
{
  for(int i = 0; i < 3; i++)
    antenna_location[i] = 0;
}

LocalRadio::LocalRadio(const dis7::RadioId& id, uint64_t frequency,
                       size_t prebuffer_samples, size_t max_samples)
  : id(id), freq(frequency), rx(prebuffer_samples, max_samples),
    attached(true), transmit_state(dis7::TX_ON_NOT_TRANSMITTING),
    last_transmitter_ms(0), last_audio_ms(0),
    receiver_state(dis7::RX_ON_NOT_RECEIVING), receiving_from(0),
    last_receiver_ms(0)
{
}

DisNetwork::DisNetwork(const DisGwConfig& cfg)
  : cfg(cfg), sd(-1), running(false), last_entity_state_ms(0)
{
  memset(&dest, 0, sizeof(dest));
}

DisNetwork::~DisNetwork()
{
  if(sd >= 0)
    close(sd);
}

uint64_t DisNetwork::nowMs()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

bool DisNetwork::init()
{
  dest.sin_family = AF_INET;
  dest.sin_port = htons(cfg.port);
  if(inet_pton(AF_INET, cfg.address.c_str(), &dest.sin_addr) != 1) {
    ERROR("sip_dis_gw: invalid DIS address '%s'\n", cfg.address.c_str());
    return false;
  }

  struct in_addr if_addr;
  if_addr.s_addr = htonl(INADDR_ANY);
  if(!cfg.interface_addr.empty() &&
     inet_pton(AF_INET, cfg.interface_addr.c_str(), &if_addr) != 1) {
    ERROR("sip_dis_gw: invalid DIS interface address '%s'\n",
          cfg.interface_addr.c_str());
    return false;
  }

  sd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sd < 0) {
    ERROR("sip_dis_gw: socket(): %s\n", strerror(errno));
    return false;
  }

  // other DIS applications on this host usually listen on the same port
  int on = 1;
  if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    WARN("sip_dis_gw: SO_REUSEADDR: %s\n", strerror(errno));
#ifdef SO_REUSEPORT
  if(setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) < 0)
    WARN("sip_dis_gw: SO_REUSEPORT: %s\n", strerror(errno));
#endif

  if(cfg.mode == DisGwConfig::BROADCAST &&
     setsockopt(sd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
    ERROR("sip_dis_gw: SO_BROADCAST: %s\n", strerror(errno));
    return false;
  }

  struct sockaddr_in local;
  memset(&local, 0, sizeof(local));
  local.sin_family = AF_INET;
  local.sin_port = htons(cfg.port);
  local.sin_addr.s_addr = htonl(INADDR_ANY);
  if(bind(sd, (struct sockaddr*)&local, sizeof(local)) < 0) {
    ERROR("sip_dis_gw: bind to port %u: %s\n", cfg.port, strerror(errno));
    return false;
  }

  if(cfg.mode == DisGwConfig::MULTICAST) {
    if(!IN_MULTICAST(ntohl(dest.sin_addr.s_addr))) {
      ERROR("sip_dis_gw: '%s' is not a multicast address\n", cfg.address.c_str());
      return false;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr = dest.sin_addr;
    mreq.imr_interface = if_addr;
    if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
      ERROR("sip_dis_gw: joining %s: %s\n", cfg.address.c_str(), strerror(errno));
      return false;
    }

    if(!cfg.interface_addr.empty() &&
       setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &if_addr, sizeof(if_addr)) < 0)
      WARN("sip_dis_gw: IP_MULTICAST_IF: %s\n", strerror(errno));

    unsigned char ttl = (unsigned char)cfg.multicast_ttl;
    if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
      WARN("sip_dis_gw: IP_MULTICAST_TTL: %s\n", strerror(errno));

    unsigned char loop = cfg.multicast_loop ? 1 : 0;
    if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0)
      WARN("sip_dis_gw: IP_MULTICAST_LOOP: %s\n", strerror(errno));
  }

  // audio is sent from the media processor threads, which must never block
  int flags = fcntl(sd, F_GETFL, 0);
  if(flags < 0 || fcntl(sd, F_SETFL, flags | O_NONBLOCK) < 0) {
    ERROR("sip_dis_gw: O_NONBLOCK: %s\n", strerror(errno));
    return false;
  }

  running = true;
  return true;
}

void DisNetwork::dispose()
{
  // AmThread::stop() detaches the thread, so join() would not wait for it;
  // poll the thread's own stopped flag instead
  stop();
  while(!is_stopped())
    usleep(10000);
}

void DisNetwork::on_stop()
{
  running = false;
}

void DisNetwork::run()
{
  INFO("sip_dis_gw: DIS network thread started\n");

  std::vector<uint8_t> buf(dis7::MAX_PDU_SIZE);
  uint64_t last_housekeeping = 0;

  while(running) {
    struct pollfd pfd;
    pfd.fd = sd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int res = poll(&pfd, 1, HOUSEKEEPING_INTERVAL_MS / 2);
    if(res < 0 && errno != EINTR) {
      ERROR("sip_dis_gw: poll(): %s\n", strerror(errno));
      usleep(HOUSEKEEPING_INTERVAL_MS * 1000);
    }

    if(res > 0 && (pfd.revents & POLLIN)) {
      // drain the socket, but come back to the heartbeats in between
      for(int i = 0; i < 256; i++) {
        ssize_t len = recv(sd, &buf[0], buf.size(), 0);
        if(len < 0) {
          if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            WARN("sip_dis_gw: recv(): %s\n", strerror(errno));
          break;
        }
        handleDatagram(&buf[0], (size_t)len, nowMs());
      }
    }

    uint64_t now = nowMs();
    if(now - last_housekeeping >= HOUSEKEEPING_INTERVAL_MS) {
      housekeeping(now);
      last_housekeeping = now;
    }
  }

  INFO("sip_dis_gw: DIS network thread stopped\n");
}

bool DisNetwork::matches(uint64_t a, uint64_t b) const
{
  uint64_t diff = a > b ? a - b : b - a;
  return diff <= cfg.frequency_tolerance;
}

dis7::PduHeader DisNetwork::header() const
{
  dis7::PduHeader hdr;
  hdr.exercise = cfg.exercise_id;

  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  hdr.timestamp = dis7::timestamp(now, cfg.absolute_timestamps);
  return hdr;
}

void DisNetwork::send(const std::vector<uint8_t>& pdu)
{
  if(sd < 0)
    return;

  if(sendto(sd, &pdu[0], pdu.size(), 0,
            (const struct sockaddr*)&dest, sizeof(dest)) < 0 &&
     errno != EAGAIN && errno != EWOULDBLOCK)
    DBG("sip_dis_gw: sendto(): %s\n", strerror(errno));
}

std::vector<std::shared_ptr<LocalRadio> >
DisNetwork::listeners(uint64_t frequency, const LocalRadio* except)
{
  std::vector<std::shared_ptr<LocalRadio> > res;

  std::lock_guard<std::mutex> lock(radios_mut);
  for(std::map<uint16_t, std::shared_ptr<LocalRadio> >::iterator it =
        radios.begin(); it != radios.end(); ++it) {
    if(it->second.get() != except && matches(it->second->freq, frequency))
      res.push_back(it->second);
  }

  return res;
}

std::shared_ptr<LocalRadio> DisNetwork::attach(uint64_t frequency)
{
  size_t prebuffer = DIS_GW_SAMPLE_RATE * cfg.jitter_prebuffer_ms / 1000;
  size_t max_queued = DIS_GW_SAMPLE_RATE * cfg.jitter_max_ms / 1000;
  std::shared_ptr<LocalRadio> radio;

  {
    std::lock_guard<std::mutex> lock(radios_mut);

    // lowest free radio number of the gateway entity
    uint16_t number = 1;
    for(std::map<uint16_t, std::shared_ptr<LocalRadio> >::iterator it =
          radios.begin(); it != radios.end() && it->first == number; ++it)
      number++;

    if(number == 0xffff)
      return radio;

    radio.reset(new LocalRadio(dis7::RadioId(cfg.entity, number), frequency,
                               prebuffer, max_queued));
    radios[number] = radio;
  }

  std::lock_guard<std::mutex> lock(radio->state_mut);
  uint64_t now = nowMs();
  sendTransmitter(*radio, now);
  if(cfg.publish_receiver)
    sendReceiver(*radio, now);

  return radio;
}

void DisNetwork::detach(const std::shared_ptr<LocalRadio>& radio)
{
  if(!radio)
    return;

  {
    std::lock_guard<std::mutex> lock(radios_mut);
    std::map<uint16_t, std::shared_ptr<LocalRadio> >::iterator it =
      radios.find(radio->id.radio);
    if(it != radios.end() && it->second == radio)
      radios.erase(it);
  }

  std::lock_guard<std::mutex> lock(radio->state_mut);
  if(!radio->attached)
    return;

  uint64_t now = nowMs();
  radio->transmit_state = dis7::TX_OFF;
  sendTransmitter(*radio, now);
  if(cfg.publish_receiver) {
    radio->receiver_state = dis7::RX_OFF;
    radio->receiving_from = 0;
    sendReceiver(*radio, now);
  }
  radio->attached = false;
}

void DisNetwork::setTransmitting(LocalRadio& radio, bool transmitting)
{
  std::lock_guard<std::mutex> lock(radio.state_mut);
  if(!radio.attached)
    return;

  uint8_t state = transmitting ? dis7::TX_ON_TRANSMITTING
    : dis7::TX_ON_NOT_TRANSMITTING;
  if(radio.transmit_state == state)
    return;

  uint64_t now = nowMs();
  radio.transmit_state = state;
  radio.last_audio_ms = now;
  DBG("sip_dis_gw: radio %u %s\n", radio.id.radio,
      transmitting ? "keyed" : "unkeyed");
  sendTransmitter(radio, now);
}

void DisNetwork::sendAudio(LocalRadio& radio, const int16_t* pcm, size_t n)
{
  if(!n)
    return;

  {
    std::lock_guard<std::mutex> lock(radio.state_mut);
    if(!radio.attached || radio.transmit_state != dis7::TX_ON_TRANSMITTING)
      return;

    dis7::SignalPdu pdu;
    pdu.header = header();
    pdu.radio = radio.id;
    pdu.encoding_scheme = cfg.encoding;
    pdu.sample_rate = DIS_GW_SAMPLE_RATE;
    pdu.samples = (uint16_t)n;
    if(!disaudio::encodeSignalData(cfg.encoding, pcm, n, pdu.data))
      return;

    std::vector<uint8_t> out;
    dis7::encode(pdu, out);
    send(out);

    radio.last_audio_ms = nowMs();
  }

  if(!cfg.local_loop)
    return;

  std::vector<std::shared_ptr<LocalRadio> > others = listeners(radio.freq, &radio);
  uint64_t now = nowMs();
  for(size_t i = 0; i < others.size(); i++)
    others[i]->rx.push(radio.id.key(), pcm, n, now);
}

void DisNetwork::sendTransmitter(LocalRadio& radio, uint64_t now)
{
  dis7::TransmitterPdu pdu;
  pdu.header = header();
  pdu.radio = radio.id;
  pdu.radio_type = cfg.radio_type;
  pdu.transmit_state = radio.transmit_state;
  pdu.input_source = cfg.input_source;
  for(int i = 0; i < 3; i++)
    pdu.antenna_location[i] = cfg.antenna_location[i];
  pdu.frequency = radio.freq;
  pdu.bandwidth = cfg.bandwidth;
  pdu.power = cfg.power;
  pdu.modulation = cfg.modulation;

  std::vector<uint8_t> out;
  dis7::encode(pdu, out);
  send(out);

  radio.last_transmitter_ms = now;
}

void DisNetwork::sendReceiver(LocalRadio& radio, uint64_t now)
{
  dis7::ReceiverPdu pdu;
  pdu.header = header();
  pdu.radio = radio.id;
  pdu.receiver_state = radio.receiver_state;
  if(radio.receiver_state == dis7::RX_ON_RECEIVING) {
    uint64_t k = radio.receiving_from;
    pdu.transmitter = dis7::RadioId(
      dis7::EntityId((uint16_t)(k >> 48), (uint16_t)(k >> 32), (uint16_t)(k >> 16)),
      (uint16_t)k);
  }

  std::vector<uint8_t> out;
  dis7::encode(pdu, out);
  send(out);

  radio.last_receiver_ms = now;
}

void DisNetwork::sendEntityState(uint64_t now)
{
  dis7::EntityStatePdu pdu;
  pdu.header = header();
  pdu.entity = cfg.entity;
  pdu.force_id = cfg.force_id;
  pdu.entity_type = cfg.entity_type;
  for(int i = 0; i < 3; i++)
    pdu.location[i] = cfg.antenna_location[i];
  // fixed width, zero padded, not terminated
  memcpy(pdu.marking, cfg.entity_marking.data(),
         std::min(cfg.entity_marking.size(), sizeof(pdu.marking)));

  std::vector<uint8_t> out;
  dis7::encode(pdu, out);
  send(out);

  last_entity_state_ms = now;
}

void DisNetwork::handleDatagram(const uint8_t* buf, size_t len, uint64_t now)
{
  // a datagram may carry several bundled PDUs
  while(len >= dis7::HEADER_SIZE) {
    dis7::PduHeader hdr;
    if(!dis7::decodeHeader(buf, len, hdr))
      return;

    if(hdr.exercise == cfg.exercise_id &&
       hdr.family == dis7::FAMILY_RADIO_COMMUNICATIONS) {
      if(hdr.type == dis7::PDU_TRANSMITTER)
        handleTransmitter(buf, hdr.length, now);
      else if(hdr.type == dis7::PDU_SIGNAL)
        handleSignal(buf, hdr.length, now);
    }

    buf += hdr.length;
    len -= hdr.length;
  }
}

void DisNetwork::handleTransmitter(const uint8_t* buf, size_t len, uint64_t now)
{
  dis7::TransmitterPdu pdu;
  if(!dis7::decode(buf, len, pdu))
    return;

  // our own PDUs come back on broadcast and multicast loop
  if(pdu.radio.entity.site == cfg.entity.site &&
     pdu.radio.entity.application == cfg.entity.application)
    return;

  uint64_t key = pdu.radio.key();
  if(pdu.transmit_state == dis7::TX_OFF) {
    remotes.erase(key);
    return;
  }

  RemoteTransmitter& remote = remotes[key];
  if(remote.frequency != pdu.frequency || remote.state != pdu.transmit_state)
    DBG("sip_dis_gw: transmitter %u:%u:%u/%u on %llu Hz, state %u\n",
        pdu.radio.entity.site, pdu.radio.entity.application,
        pdu.radio.entity.entity, pdu.radio.radio,
        (unsigned long long)pdu.frequency, pdu.transmit_state);

  remote.frequency = pdu.frequency;
  remote.state = pdu.transmit_state;
  remote.last_seen_ms = now;
}

void DisNetwork::handleSignal(const uint8_t* buf, size_t len, uint64_t now)
{
  dis7::SignalPdu pdu;
  if(!dis7::decode(buf, len, pdu))
    return;

  if(pdu.radio.entity.site == cfg.entity.site &&
     pdu.radio.entity.application == cfg.entity.application)
    return;

  // the frequency of a signal comes from its transmitter's Transmitter PDU
  std::map<uint64_t, RemoteTransmitter>::iterator it = remotes.find(pdu.radio.key());
  if(it == remotes.end())
    return;

  RemoteTransmitter& remote = it->second;
  remote.last_seen_ms = now;

  if(!disaudio::canDecode(pdu.encoding_scheme) || !pdu.sample_rate ||
     pdu.sample_rate > 192000) {
    if(!remote.unsupported_logged) {
      WARN("sip_dis_gw: ignoring signal from %u:%u:%u/%u: encoding scheme 0x%04x,"
           " %u Hz\n", pdu.radio.entity.site, pdu.radio.entity.application,
           pdu.radio.entity.entity, pdu.radio.radio, pdu.encoding_scheme,
           pdu.sample_rate);
      remote.unsupported_logged = true;
    }
    return;
  }

  std::vector<std::shared_ptr<LocalRadio> > targets = listeners(remote.frequency, NULL);
  if(targets.empty())
    return;

  std::vector<int16_t> pcm;
  if(!disaudio::decodeSignalData(pdu.encoding_scheme, pdu.data, pdu.samples, pcm) ||
     pcm.empty())
    return;

  if(!remote.resampler || remote.resampler->inputRate() != pdu.sample_rate)
    remote.resampler.reset(new disaudio::LinearResampler(pdu.sample_rate,
                                                         DIS_GW_SAMPLE_RATE));
  std::vector<int16_t> pcm8k;
  remote.resampler->process(&pcm[0], pcm.size(), pcm8k);
  if(pcm8k.empty())
    return;

  for(size_t i = 0; i < targets.size(); i++)
    targets[i]->rx.push(pdu.radio.key(), &pcm8k[0], pcm8k.size(), now);
}

void DisNetwork::housekeeping(uint64_t now)
{
  for(std::map<uint64_t, RemoteTransmitter>::iterator it = remotes.begin();
      it != remotes.end();) {
    if(now - it->second.last_seen_ms > cfg.remote_timeout_ms)
      remotes.erase(it++);
    else
      ++it;
  }

  std::vector<std::shared_ptr<LocalRadio> > all;
  {
    std::lock_guard<std::mutex> lock(radios_mut);
    for(std::map<uint16_t, std::shared_ptr<LocalRadio> >::iterator it =
          radios.begin(); it != radios.end(); ++it)
      all.push_back(it->second);
  }

  for(size_t i = 0; i < all.size(); i++) {
    LocalRadio& radio = *all[i];
    uint64_t source = 0;
    bool receiving = radio.rx.expire(now, RX_SOURCE_IDLE_MS, &source);

    std::lock_guard<std::mutex> lock(radio.state_mut);
    if(!radio.attached)
      continue;

    // the caller stopped sending audio (hang up pending, DTX, ...)
    if(radio.transmit_state == dis7::TX_ON_TRANSMITTING &&
       now - radio.last_audio_ms > cfg.tx_idle_ms) {
      DBG("sip_dis_gw: radio %u unkeyed, no audio for %u ms\n",
          radio.id.radio, cfg.tx_idle_ms);
      radio.transmit_state = dis7::TX_ON_NOT_TRANSMITTING;
      sendTransmitter(radio, now);
    } else if(now - radio.last_transmitter_ms >= cfg.transmitter_heartbeat_ms) {
      sendTransmitter(radio, now);
    }

    if(cfg.publish_receiver) {
      uint16_t state = receiving ? dis7::RX_ON_RECEIVING : dis7::RX_ON_NOT_RECEIVING;
      if(!receiving)
        source = 0;
      if(state != radio.receiver_state || source != radio.receiving_from ||
         now - radio.last_receiver_ms >= cfg.receiver_heartbeat_ms) {
        radio.receiver_state = state;
        radio.receiving_from = source;
        sendReceiver(radio, now);
      }
    }
  }

  if(cfg.publish_entity_state &&
     (!last_entity_state_ms || now - last_entity_state_ms >= cfg.entity_heartbeat_ms))
    sendEntityState(now);
}
