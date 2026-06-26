/*
 * DIS Integration Test Module Implementation
 */

#include "DisTest.h"
#include "AmUtils.h"
#include "AmConfig.h"
#include "log.h"

#include <dis6/EntityStatePdu.h>
#include <dis6/EntityID.h>
#include <dis6/EntityType.h>
#include <dis6/Vector3Double.h>
#include <dis6/Vector3Float.h>
#include <dis6/Orientation.h>
#include <dis6/utils/DataStream.h>

#include <cmath>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#define MOD_NAME "dis_test"

EXPORT_SESSION_FACTORY(DisTestFactory, MOD_NAME);

double DisTestFactory::sinus_frequency_ = 400.0;
double DisTestFactory::sinus_amplitude_ = 0.5;
std::string DisTestFactory::dis_gateway_addr_ = "127.0.0.1";
unsigned short DisTestFactory::dis_gateway_port_ = 3000;
bool DisTestFactory::dis_multicast_mode_ = true;
std::string DisTestFactory::dis_multicast_addr_ = "224.0.0.1";
unsigned int DisTestFactory::dis_send_interval_ms_ = 100;
unsigned char DisTestFactory::dis_exercise_id_ = 1;
unsigned short DisTestFactory::dis_site_id_ = 1;
unsigned short DisTestFactory::dis_application_id_ = 200;
unsigned short DisTestFactory::dis_entity_id_ = 1;
unsigned char DisTestFactory::dis_force_id_ = 1;
unsigned short DisTestFactory::dis_entity_country_ = 840;
unsigned char DisTestFactory::dis_entity_category_ = 50;

static bool str2bool(const string& value, bool default_value) {
  if (value.empty()) return default_value;
  if (value == "1" || value == "true" || value == "yes" || value == "on") return true;
  if (value == "0" || value == "false" || value == "no" || value == "off") return false;
  return default_value;
}

static unsigned short to_ushort(const string& value, unsigned short default_value) {
  if (value.empty()) return default_value;
  int v = atoi(value.c_str());
  if (v < 0 || v > 65535) return default_value;
  return (unsigned short)v;
}

static unsigned int to_uint(const string& value, unsigned int default_value) {
  if (value.empty()) return default_value;
  long v = atol(value.c_str());
  if (v < 0) return default_value;
  return (unsigned int)v;
}

static unsigned char to_uchar(const string& value, unsigned char default_value) {
  if (value.empty()) return default_value;
  int v = atoi(value.c_str());
  if (v < 0 || v > 255) return default_value;
  return (unsigned char)v;
}

static double to_double(const string& value, double default_value) {
  if (value.empty()) return default_value;
  char* end = nullptr;
  double v = strtod(value.c_str(), &end);
  if (end == value.c_str()) return default_value;
  return v;
}

// ============================================================================
// SinusAudioSource Implementation
// ============================================================================

SinusAudioSource::SinusAudioSource(double freq_hz, double amplitude)
  : AmAudio(), freq_hz_(freq_hz), amplitude_(amplitude), max_val_(32767)
{
  DBG("SinusAudioSource: freq=%.1f Hz, amplitude=%.2f, rate=%d Hz\n",
      freq_hz_, amplitude_, (int)SYSTEM_SAMPLECLOCK_RATE);
}

int SinusAudioSource::read(unsigned int user_ts, unsigned int size)
{
  // The internal buffer holds PCM16 samples at SYSTEM_SAMPLECLOCK_RATE.
  // Generate the tone directly from the timestamp so the phase stays
  // continuous across successive reads.
  short *s = (short *)((unsigned char *)samples);
  unsigned int nb_samples = PCM16_B2S(size);

  for (unsigned int i = 0; i < nb_samples; i++, s++) {
    double t = (double)(user_ts + i);
    double value = sin(2.0 * M_PI * freq_hz_ * t / (double)SYSTEM_SAMPLECLOCK_RATE);
    *s = (short)(value * amplitude_ * max_val_);
  }

  return size;
}

int SinusAudioSource::write(unsigned int user_ts, unsigned int size)
{
  (void)user_ts;
  (void)size;
  return -1;
}

// ============================================================================
// DISPacketSender Implementation (OpenDIS EntityStatePDU)
// ============================================================================

class DISPacketSender : public AmThread {
private:
  int socket_;
  struct sockaddr_in addr_;
  bool multicast_mode_;
  unsigned int send_interval_ms_;
  unsigned char exercise_id_;
  unsigned short site_id_;
  unsigned short application_id_;
  unsigned short entity_id_;
  unsigned char force_id_;
  unsigned short entity_country_;
  unsigned char entity_category_;
  std::atomic<bool> running_;
  std::atomic<unsigned int> packet_count_;
  bool valid_;

  std::vector<char> marshalEntityStatePdu(uint32_t sim_time_ms) {
    DIS::EntityStatePdu pdu;
    pdu.setProtocolVersion(6);
    pdu.setExerciseID(exercise_id_);
    pdu.setTimestamp(sim_time_ms);

    DIS::EntityID entity;
    entity.setSite(site_id_);
    entity.setApplication(application_id_);
    entity.setEntity(entity_id_);
    pdu.setEntityID(entity);

    pdu.setForceId(force_id_);

    DIS::EntityType type;
    type.setEntityKind(1);         // Platform
    type.setDomain(1);             // Land
    type.setCountry(entity_country_);
    type.setCategory(entity_category_);
    type.setSubcategory(0);
    type.setSpecific(0);
    type.setExtra(0);
    pdu.setEntityType(type);
    pdu.setAlternativeEntityType(type);

    DIS::Vector3Float velocity;
    velocity.setX(0.0f);
    velocity.setY(0.0f);
    velocity.setZ(0.0f);
    pdu.setEntityLinearVelocity(velocity);

    DIS::Vector3Double location;
    location.setX(0.0);
    location.setY(0.0);
    location.setZ(0.0);
    pdu.setEntityLocation(location);

    DIS::Orientation orientation;
    orientation.setPsi(0.0f);
    orientation.setTheta(0.0f);
    orientation.setPhi(0.0f);
    pdu.setEntityOrientation(orientation);

    pdu.setEntityAppearance(0);
    pdu.setLength((unsigned short)pdu.getMarshalledSize());

    DIS::DataStream ds(DIS::BIG);
    pdu.marshal(ds);

    std::vector<char> out;
    out.resize(ds.size());
    for (size_t i = 0; i < ds.size(); i++) {
      out[i] = ds[(unsigned int)i];
    }

    return out;
  }

protected:
  void run() {
    if (!valid_) {
      WARN("DISPacketSender: invalid sender, not starting\n");
      return;
    }

    struct timespec ts_start;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    uint64_t start_ms = (ts_start.tv_sec * 1000LL) + (ts_start.tv_nsec / 1000000LL);

    INFO("DISPacketSender: started (multicast=%s)\n", multicast_mode_ ? "yes" : "no");

    while (running_) {
      struct timespec ts_now;
      clock_gettime(CLOCK_MONOTONIC, &ts_now);
      uint64_t now_ms = (ts_now.tv_sec * 1000LL) + (ts_now.tv_nsec / 1000000LL);
      uint32_t sim_time_ms = (uint32_t)(now_ms - start_ms);

      std::vector<char> pdu = marshalEntityStatePdu(sim_time_ms);
      ssize_t sent = sendto(socket_, (const char*)pdu.data(), pdu.size(), 0,
                            (struct sockaddr*)&addr_, sizeof(addr_));

      if (sent < 0) {
        WARN("DISPacketSender: sendto failed: %s\n", strerror(errno));
      } else {
        packet_count_++;
        DBG("DISPacketSender: sent packet #%u (size=%zu, sim_time=%u ms)\n",
            packet_count_.load(), pdu.size(), sim_time_ms);
      }

      usleep(send_interval_ms_ * 1000);
    }

    INFO("DISPacketSender: stopped (sent %u packets)\n", packet_count_.load());
  }

  void on_stop() {
    running_ = false;
  }

public:
  DISPacketSender(const string& addr, unsigned short port, bool multicast,
                  unsigned int send_interval_ms,
                  unsigned char exercise_id,
                  unsigned short site_id,
                  unsigned short application_id,
                  unsigned short entity_id,
                  unsigned char force_id,
                  unsigned short entity_country,
                  unsigned char entity_category)
    : socket_(-1), multicast_mode_(multicast), send_interval_ms_(send_interval_ms),
      exercise_id_(exercise_id), site_id_(site_id), application_id_(application_id),
      entity_id_(entity_id), force_id_(force_id), entity_country_(entity_country),
      entity_category_(entity_category), running_(false), packet_count_(0), valid_(false)
  {
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ < 0) {
      ERROR("DISPacketSender: socket() failed: %s\n", strerror(errno));
      return;
    }

    if (multicast_mode_) {
      unsigned char ttl = 16;
      if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl,
                     sizeof(ttl)) < 0) {
        WARN("DISPacketSender: setsockopt IP_MULTICAST_TTL failed\n");
      }

      unsigned char loop = 1;
      if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_LOOP, (const char*)&loop,
                     sizeof(loop)) < 0) {
        WARN("DISPacketSender: setsockopt IP_MULTICAST_LOOP failed\n");
      }
    }

    memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = inet_addr(addr.c_str());

    if (addr_.sin_addr.s_addr == INADDR_NONE) {
      ERROR("DISPacketSender: invalid destination address '%s'\n", addr.c_str());
      return;
    }

    valid_ = true;

    INFO("DISPacketSender: created socket (addr=%s:%u, multicast=%s)\n",
         addr.c_str(), port, multicast_mode_ ? "yes" : "no");
  }

  ~DISPacketSender() {
    if (socket_ >= 0) {
      close(socket_);
    }
  }

  void start() {
    if (!valid_) {
      WARN("DISPacketSender: start skipped, sender not valid\n");
      return;
    }
    running_ = true;
    AmThread::start();
  }

  void stop() {
    running_ = false;
    join();
  }

  unsigned int getPacketCount() const {
    return packet_count_;
  }
};

// ============================================================================
// DisTestSession Implementation
// ============================================================================

DisTestSession::DisTestSession()
  : packet_count_(0)
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  session_id_ = int2str((unsigned int)(ts.tv_sec & 0xFFFFFFFFU)) + "_" + int2str((unsigned int)(ts.tv_nsec & 0xFFFFFFFFU));

  INFO("DisTestSession: created (id=%s)\n", session_id_.c_str());
}

DisTestSession::~DisTestSession()
{
  // Safety net: make sure the sender thread is stopped before its
  // members are destroyed, even if onBye/onSessionTimeout did not run.
  if (dis_sender_) {
    dis_sender_->stop();
  }

  INFO("DisTestSession: destroyed (id=%s, packets=%u)\n",
       session_id_.c_str(), packet_count_);
}

void DisTestSession::onSessionStart()
{
  INFO("DisTestSession: onSessionStart (id=%s)\n", session_id_.c_str());

  sinus_.reset(new SinusAudioSource(
    DisTestFactory::getSinusFrequency(),
    DisTestFactory::getSinusAmplitude()));
  setOutput(sinus_.get());

  INFO("DisTestSession: sinus generator started (output set)\n");

  dis_sender_.reset(new DISPacketSender(
    DisTestFactory::isMulticastMode() ? DisTestFactory::getMulticastAddr() : DisTestFactory::getGatewayAddr(),
    DisTestFactory::getGatewayPort(),
    DisTestFactory::isMulticastMode(),
    DisTestFactory::getSendIntervalMs(),
    DisTestFactory::getExerciseId(),
    DisTestFactory::getSiteId(),
    DisTestFactory::getApplicationId(),
    DisTestFactory::getEntityId(),
    DisTestFactory::getForceId(),
    DisTestFactory::getEntityCountry(),
    DisTestFactory::getEntityCategory()));
  dis_sender_->start();

  INFO("DisTestSession: DIS packet sender started\n");
}

void DisTestSession::onBye(const AmSipRequest& req)
{
  INFO("DisTestSession: received BYE (id=%s)\n", session_id_.c_str());
  (void)req;

  if (dis_sender_) {
    dis_sender_->stop();
    packet_count_ = dis_sender_->getPacketCount();
    INFO("DisTestSession: DIS sender stopped (sent %u packets)\n", packet_count_);
  }

  setOutput(nullptr);
  setStopped();
}

void DisTestSession::onSessionTimeout()
{
  INFO("DisTestSession: session timeout (id=%s)\n", session_id_.c_str());

  if (dis_sender_) {
    dis_sender_->stop();
    packet_count_ = dis_sender_->getPacketCount();
    INFO("DisTestSession: DIS sender stopped (sent %u packets)\n", packet_count_);
  }

  setOutput(nullptr);
  setStopped();
}

// ============================================================================
// DisTestFactory Implementation
// ============================================================================

DisTestFactory::DisTestFactory(const string& name)
  : AmSessionFactory(name)
{
}

int DisTestFactory::onLoad()
{
  AmConfigReader cfg;
  if (cfg.loadPluginConf(MOD_NAME)) {
    WARN("DisTest: no configuration file found, using defaults\n");
  }

  dis_multicast_mode_ = str2bool(cfg.getParameter("dis_multicast_mode", "true"), true);
  dis_multicast_addr_ = cfg.getParameter("dis_multicast_addr", "224.0.0.1");
  dis_gateway_addr_ = cfg.getParameter("dis_gateway_addr", "127.0.0.1");
  dis_gateway_port_ = to_ushort(cfg.getParameter("dis_gateway_port", "3000"), 3000);

  sinus_frequency_ = to_double(cfg.getParameter("sinus_frequency", "400.0"), 400.0);
  sinus_amplitude_ = to_double(cfg.getParameter("sinus_amplitude", "0.5"), 0.5);

  if (sinus_frequency_ < 10.0) sinus_frequency_ = 10.0;
  if (sinus_frequency_ > 3900.0) sinus_frequency_ = 3900.0;
  if (sinus_amplitude_ < 0.0) sinus_amplitude_ = 0.0;
  if (sinus_amplitude_ > 1.0) sinus_amplitude_ = 1.0;

  dis_send_interval_ms_ = to_uint(cfg.getParameter("dis_send_interval_ms", "100"), 100);
  if (dis_send_interval_ms_ < 20) dis_send_interval_ms_ = 20;
  if (dis_send_interval_ms_ > 5000) dis_send_interval_ms_ = 5000;

  dis_exercise_id_ = to_uchar(cfg.getParameter("dis_exercise_id", "1"), 1);
  dis_site_id_ = to_ushort(cfg.getParameter("dis_site_id", "1"), 1);
  dis_application_id_ = to_ushort(cfg.getParameter("dis_application_id", "200"), 200);
  dis_entity_id_ = to_ushort(cfg.getParameter("dis_entity_id", "1"), 1);
  dis_force_id_ = to_uchar(cfg.getParameter("dis_force_id", "1"), 1);
  dis_entity_country_ = to_ushort(cfg.getParameter("dis_entity_country", "840"), 840);
  dis_entity_category_ = to_uchar(cfg.getParameter("dis_entity_category", "50"), 50);

  INFO("DisTest: loaded (multicast=%s, addr=%s, port=%u, tone=%.1fHz amp=%.2f, interval=%ums)\n",
       dis_multicast_mode_ ? "yes" : "no",
       dis_multicast_mode_ ? dis_multicast_addr_.c_str() : dis_gateway_addr_.c_str(),
       dis_gateway_port_, sinus_frequency_, sinus_amplitude_, dis_send_interval_ms_);

  INFO("DisTest: entity=(exercise=%u site=%u app=%u entity=%u force=%u country=%u category=%u)\n",
       dis_exercise_id_, dis_site_id_, dis_application_id_, dis_entity_id_,
       dis_force_id_, dis_entity_country_, dis_entity_category_);

  return 0;
}

AmSession* DisTestFactory::onInvite(const AmSipRequest& req,
                                     const string& app_name,
                                     const map<string, string>& app_params)
{
  (void)app_name;
  (void)app_params;
  INFO("DisTest: onInvite (from=%s, to=%s)\n",
       req.from.c_str(), req.to.c_str());

  return new DisTestSession();
}
