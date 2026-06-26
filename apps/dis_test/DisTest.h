/*
 * DIS Integration Test Module for SEMS
 * 
 * Simple module that:
 * - Generates sinus wave audio (for listening test)
 * - Sends DIS EntityStatePDU packets to DIS gateway
 * - Tests integration with DIS infrastructure
 */

#ifndef _DIS_TEST_H
#define _DIS_TEST_H

#include "AmSession.h"
#include "AmConfigReader.h"
#include "AmAudio.h"
#include "AmThread.h"

#include <memory>
#include <atomic>

// Forward declaration
class DISPacketSender;

/**
 * Sinus wave audio source (400Hz test tone)
 */
class SinusAudioSource : public AmAudio {
private:
  double freq_hz_;         // Tone frequency in Hz
  double amplitude_;       // Volume (0.0-1.0)
  int16_t max_val_;        // Max PCM value for amplitude scaling

public:
  SinusAudioSource(double freq_hz = 400.0, double amplitude = 0.5);
  virtual ~SinusAudioSource() {}

protected:
  /** Fill the internal sample buffer with the generated sinus tone. */
  int read(unsigned int user_ts, unsigned int size);
  /** This is a source only; writing is not supported. */
  int write(unsigned int user_ts, unsigned int size);
};

/**
 * DIS Test Session - generates sinus audio and sends DIS packets
 */
class DisTestSession : public AmSession {
private:
  std::unique_ptr<SinusAudioSource> sinus_;
  std::unique_ptr<DISPacketSender> dis_sender_;
  std::string session_id_;
  unsigned int packet_count_;
  
public:
  DisTestSession();
  virtual ~DisTestSession();
  
  void onSessionStart();
  void onBye(const AmSipRequest& req);
  void onSessionTimeout();
};

/**
 * Factory for DIS test sessions
 */
class DisTestFactory : public AmSessionFactory {
private:
  static double sinus_frequency_;
  static double sinus_amplitude_;
  static std::string dis_gateway_addr_;
  static unsigned short dis_gateway_port_;
  static bool dis_multicast_mode_;
  static std::string dis_multicast_addr_;
  static unsigned int dis_send_interval_ms_;
  static unsigned char dis_exercise_id_;
  static unsigned short dis_site_id_;
  static unsigned short dis_application_id_;
  static unsigned short dis_entity_id_;
  static unsigned char dis_force_id_;
  static unsigned short dis_entity_country_;
  static unsigned char dis_entity_category_;

public:
  static double getSinusFrequency() { return sinus_frequency_; }
  static double getSinusAmplitude() { return sinus_amplitude_; }
  static const string& getGatewayAddr() { return dis_gateway_addr_; }
  static unsigned short getGatewayPort() { return dis_gateway_port_; }
  static bool isMulticastMode() { return dis_multicast_mode_; }
  static const string& getMulticastAddr() { return dis_multicast_addr_; }
  static unsigned int getSendIntervalMs() { return dis_send_interval_ms_; }
  static unsigned char getExerciseId() { return dis_exercise_id_; }
  static unsigned short getSiteId() { return dis_site_id_; }
  static unsigned short getApplicationId() { return dis_application_id_; }
  static unsigned short getEntityId() { return dis_entity_id_; }
  static unsigned char getForceId() { return dis_force_id_; }
  static unsigned short getEntityCountry() { return dis_entity_country_; }
  static unsigned char getEntityCategory() { return dis_entity_category_; }

  DisTestFactory(const string& name);
  virtual ~DisTestFactory() {}
  
  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
                      const map<string, string>& app_params);
};

#endif
