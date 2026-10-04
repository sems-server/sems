/*
 * SIP to DIS radio gateway
 */

#include "SipDisGw.h"

#include "AmConfigReader.h"
#include "AmSessionContainer.h"
#include "AmUtils.h"
#include "log.h"

#ifndef MOD_NAME
#define MOD_NAME "sip_dis_gw"
#endif

#define DTMF_STAR  10
#define DTMF_POUND 11

static size_t msToSamples(unsigned int ms)
{
  return (size_t)DIS_GW_SAMPLE_RATE * ms / 1000;
}

// ============================================================================
// DisRadioAudio
// ============================================================================

DisRadioAudio::DisRadioAudio(const SipDisGwConfig& cfg,
                             const std::shared_ptr<DisNetwork>& net,
                             const std::shared_ptr<LocalRadio>& radio)
  : AmAudio(new AmAudioFormat(CODEC_PCM16, DIS_GW_SAMPLE_RATE)),
    cfg(cfg), net(net), radio(radio),
    vox(cfg.vox_threshold_dbfs, msToSamples(cfg.vox_hang_ms)),
    frame_samples(msToSamples(cfg.dis.frame_ms)),
    keyed(false), keyed_since_ms(0),
    ptt_request(cfg.ptt_mode == SipDisGwConfig::PTT_ALWAYS)
{
}

int DisRadioAudio::read(unsigned int user_ts, unsigned int size)
{
  (void)user_ts;

  // always deliver a frame, silence included, so that RTP keeps flowing
  radio->readAudio((int16_t*)(unsigned char*)samples, PCM16_B2S(size));
  return size;
}

int DisRadioAudio::write(unsigned int user_ts, unsigned int size)
{
  (void)user_ts;

  const int16_t* pcm = (const int16_t*)(unsigned char*)samples;
  size_t n = PCM16_B2S(size);
  uint64_t now = DisNetwork::nowMs();

  bool want = wantKeyed(pcm, n, now);
  if(want != keyed) {
    if(want) {
      keyed_since_ms = now;
      // voice activation reacts to the first loud frame: send what came
      // just before it as well, so that the first syllable is not clipped
      tx_frame.assign(preroll.begin(), preroll.end());
      preroll.clear();
    } else if(!tx_frame.empty()) {
      // last, partial frame of the transmission
      net->sendAudio(*radio, &tx_frame[0], tx_frame.size());
      tx_frame.clear();
    }
    keyed = want;
  }

  // also re-keys the radio after the network thread unkeyed it because the
  // caller's audio stopped for a while
  net->setTransmitting(*radio, keyed);

  if(keyed) {
    sendFrames(pcm, n);
  } else if(cfg.ptt_mode == SipDisGwConfig::PTT_VOX) {
    preroll.insert(preroll.end(), pcm, pcm + n);
    size_t max_preroll = msToSamples(cfg.vox_preroll_ms);
    if(preroll.size() > max_preroll)
      preroll.erase(preroll.begin(), preroll.begin() + (preroll.size() - max_preroll));
  }

  return size;
}

bool DisRadioAudio::wantKeyed(const int16_t* pcm, size_t n, uint64_t now)
{
  switch(cfg.ptt_mode) {
  case SipDisGwConfig::PTT_LISTEN:
    return false;

  case SipDisGwConfig::PTT_ALWAYS:
    return true;

  case SipDisGwConfig::PTT_DTMF:
    if(ptt_request && keyed && cfg.ptt_max_ms &&
       now - keyed_since_ms > cfg.ptt_max_ms) {
      INFO("sip_dis_gw: radio %u keyed for more than %u ms, unkeying\n",
           radio->radioId().radio, cfg.ptt_max_ms);
      ptt_request = false;
    }
    return ptt_request;

  case SipDisGwConfig::PTT_VOX:
  default:
    return vox.process(pcm, n);
  }
}

void DisRadioAudio::sendFrames(const int16_t* pcm, size_t n)
{
  tx_frame.insert(tx_frame.end(), pcm, pcm + n);

  size_t sent = 0;
  while(tx_frame.size() - sent >= frame_samples) {
    net->sendAudio(*radio, &tx_frame[sent], frame_samples);
    sent += frame_samples;
  }
  tx_frame.erase(tx_frame.begin(), tx_frame.begin() + sent);
}

// ============================================================================
// SipDisGwSession
// ============================================================================

SipDisGwSession::SipDisGwSession(const SipDisGwConfig& cfg,
                                 const std::shared_ptr<DisNetwork>& net,
                                 const std::string& radio_name, uint64_t frequency)
  : cfg(cfg), net(net), radio_name(radio_name), frequency(frequency)
{
}

SipDisGwSession::~SipDisGwSession()
{
  // the session may end without onBye(), e.g. when the BYE is ours
  releaseRadio();
}

void SipDisGwSession::releaseRadio()
{
  // detach the audio first: the media processor must not touch it anymore
  setInOut(NULL, NULL);

  if(radio) {
    INFO("sip_dis_gw: call %s released radio %u (%s)\n", getLocalTag().c_str(),
         radio->radioId().radio, radio_name.c_str());
    net->detach(radio);
    radio.reset();
  }
}

void SipDisGwSession::onSessionStart()
{
  radio = net->attach(frequency);
  if(!radio) {
    ERROR("sip_dis_gw: no free radio number for call %s\n", getLocalTag().c_str());
    dlg->bye();
    setStopped();
    return;
  }

  audio.reset(new DisRadioAudio(cfg, net, radio));
  setInOut(audio.get(), audio.get());

  INFO("sip_dis_gw: call %s on radio %u, '%s' = %llu Hz\n", getLocalTag().c_str(),
       radio->radioId().radio, radio_name.c_str(), (unsigned long long)frequency);

  AmSession::onSessionStart();
}

void SipDisGwSession::onDtmf(int event, int duration_msec)
{
  (void)duration_msec;

  if(cfg.ptt_mode != SipDisGwConfig::PTT_DTMF || !audio)
    return;

  if(event == DTMF_STAR)
    audio->setPtt(true);
  else if(event == DTMF_POUND)
    audio->setPtt(false);
}

void SipDisGwSession::onBye(const AmSipRequest& req)
{
  releaseRadio();
  AmSession::onBye(req);
}

void SipDisGwSession::onSessionTimeout()
{
  releaseRadio();
  AmSession::onSessionTimeout();
}

void SipDisGwSession::onRtpTimeout()
{
  releaseRadio();
  AmSession::onRtpTimeout();
}

// ============================================================================
// SipDisGwFactory
// ============================================================================

SipDisGwFactory::SipDisGwFactory(const std::string& name)
  : AmSessionFactory(name)
{
}

SipDisGwFactory::~SipDisGwFactory()
{
  // AmPlugIn's destructor releases the factories and then dlclose()s the
  // modules; the network thread runs code of this module
  if(net)
    net->dispose();
}

int SipDisGwFactory::onLoad()
{
  AmConfigReader conf;
  if(conf.loadPluginConf(MOD_NAME))
    WARN("sip_dis_gw: no configuration file, using defaults\n");

  if(!cfg.load(conf))
    return -1;

  net.reset(new DisNetwork(cfg.dis));
  if(!net->init())
    return -1;
  net->start();

  static const char* modes[] = { "broadcast", "multicast", "unicast" };
  static const char* ptt[] = { "vox", "dtmf", "always", "listen" };
  INFO("sip_dis_gw: DIS 7 %s to %s:%u, exercise %u, entity %u:%u:%u, ptt %s\n",
       modes[cfg.dis.mode], cfg.dis.address.c_str(), cfg.dis.port,
       cfg.dis.exercise_id, cfg.dis.entity.site, cfg.dis.entity.application,
       cfg.dis.entity.entity, ptt[cfg.ptt_mode]);

  for(std::map<std::string, uint64_t>::const_iterator it = cfg.radios.begin();
      it != cfg.radios.end(); ++it)
    INFO("sip_dis_gw: radio '%s' = %llu Hz\n", it->first.c_str(),
         (unsigned long long)it->second);

  return 0;
}

AmSession* SipDisGwFactory::onInvite(const AmSipRequest& req,
                                     const std::string& app_name,
                                     const std::map<std::string, std::string>& app_params)
{
  (void)app_name;
  (void)app_params;

  uint64_t frequency;
  if(!cfg.lookupFrequency(req.user, frequency)) {
    INFO("sip_dis_gw: no radio for '%s'\n", req.user.c_str());
    throw AmSession::Exception(404, "Unknown Radio");
  }

  return new SipDisGwSession(cfg, net, req.user, frequency);
}
