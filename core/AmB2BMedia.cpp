#include "AmB2BMedia.h"
#include "AmAudio.h"
#include "amci/codecs.h"
#include <string.h>
#include <strings.h>
#include "AmB2BSession.h"
#include "AmRtpReceiver.h"
#include "sip/msg_logger.h"

#include <algorithm>
#include <stdexcept>

using namespace std;

#define TRACE DBG
#define UNDEFINED_PAYLOAD (-1)

/** class for computing payloads for relay the simpliest way - allow relaying of
 * all payloads supported by remote party */
static B2BMediaStatistics b2b_stats;

static const string zero_ip("0.0.0.0");

static void replaceRtcpAttr(SdpMedia &m, const string& relay_address, int rtcp_port)
{
  for (std::vector<SdpAttribute>::iterator a = m.attributes.begin(); a != m.attributes.end(); ++a) {
    try {
      if (a->attribute == "rtcp") {
        RtcpAddress addr(a->value);
        addr.setPort(rtcp_port);
        if (addr.hasAddress()) addr.setAddress(relay_address);
        a->value = addr.print();
      }
    }
    catch (const exception &e) {
      DBG("can't replace RTCP address: %s\n", e.what());
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////

void B2BMediaStatistics::incCodecWriteUsage(const string &codec_name)
{
  if (codec_name.empty()) return;

  AmLock lock(mutex);
  map<string, int>::iterator i = codec_write_usage.find(codec_name);
  if (i != codec_write_usage.end()) i->second++;
  else codec_write_usage[codec_name] = 1;
}

void B2BMediaStatistics::decCodecWriteUsage(const string &codec_name)
{
  if (codec_name.empty()) return;

  AmLock lock(mutex);
  map<string, int>::iterator i = codec_write_usage.find(codec_name);
  if (i != codec_write_usage.end()) {
    if (i->second > 0) i->second--;
  }
}

void B2BMediaStatistics::incCodecReadUsage(const string &codec_name)
{
  if (codec_name.empty()) return;

  AmLock lock(mutex);
  map<string, int>::iterator i = codec_read_usage.find(codec_name);
  if (i != codec_read_usage.end()) i->second++;
  else codec_read_usage[codec_name] = 1;
}

void B2BMediaStatistics::decCodecReadUsage(const string &codec_name)
{
  if (codec_name.empty()) return;

  AmLock lock(mutex);
  map<string, int>::iterator i = codec_read_usage.find(codec_name);
  if (i != codec_read_usage.end()) {
    if (i->second > 0) i->second--;
  }
}

B2BMediaStatistics *B2BMediaStatistics::instance()
{
  return &b2b_stats;
}
    
void B2BMediaStatistics::reportCodecWriteUsage(string &dst)
{
  if (codec_write_usage.empty()) {
    dst = "pcma=0"; // to be not empty
    return;
  }

  bool first = true;
  dst.clear();
  AmLock lock(mutex);
  for (map<string, int>::iterator i = codec_write_usage.begin();
      i != codec_write_usage.end(); ++i) 
  {
    if (first) first = false;
    else dst += ",";
    dst += i->first;
    dst += "=";
    dst += int2str(i->second);
  }
}

void B2BMediaStatistics::reportCodecReadUsage(string &dst)
{
  if (codec_read_usage.empty()) {
    dst = "pcma=0"; // to be not empty
    return;
  }

  bool first = true;
  dst.clear();
  AmLock lock(mutex);
  for (map<string, int>::iterator i = codec_read_usage.begin();
      i != codec_read_usage.end(); ++i) 
  {
    if (first) first = false;
    else dst += ",";
    dst += i->first;
    dst += "=";
    dst += int2str(i->second);
  }
}
    
void B2BMediaStatistics::getReport(const AmArg &args, AmArg &ret)
{
  AmArg write_usage;
  AmArg read_usage;

  { // locked area
    AmLock lock(mutex);

    for (map<string, int>::iterator i = codec_write_usage.begin();
        i != codec_write_usage.end(); ++i) 
    {
      AmArg avp;
      avp["codec"] = i->first;
      avp["count"] = i->second;
      write_usage.push(avp);
    }

    for (map<string, int>::iterator i = codec_read_usage.begin();
        i != codec_read_usage.end(); ++i) 
    {
      AmArg avp;
      avp["codec"] = i->first;
      avp["count"] = i->second;
      read_usage.push(avp);
    }
  }

  ret["write"] = write_usage;
  ret["read"] = read_usage;
}

//////////////////////////////////////////////////////////////////////////////////

void AudioStreamData::initialize(AmB2BSession *session)
{
  stream = new AmRtpAudio(session, session->getRtpInterface());
  stream->setRtpRelayTransparentSeqno(session->getRtpRelayTransparentSeqno());
  stream->setRtpRelayTransparentSSRC(session->getRtpRelayTransparentSSRC());
  stream->setRtpRelayFilterRtpDtmf(session->getEnableDtmfRtpFiltering());
  if (session->getEnableDtmfRtpDetection())
    stream->force_receive_dtmf = true;
  force_symmetric_rtp = session->getRtpRelayForceSymmetricRtp();
  enable_dtmf_transcoding = session->getEnableDtmfTranscoding();
  session->getLowFiPLs(lowfi_payloads);
  stream->setLocalIP(session->localMediaIP());
}

AudioStreamData::AudioStreamData(AmB2BSession *session):
  in(NULL), initialized(false),
  dtmf_detector(NULL), dtmf_queue(NULL),
  relay_enabled(false), relay_port(0),
  outgoing_payload(UNDEFINED_PAYLOAD),
  incoming_payload(UNDEFINED_PAYLOAD),
  force_symmetric_rtp(false),
  enable_dtmf_transcoding(false),
  muted(false), relay_paused(false), receiving(true)
{
  if (session) initialize(session);
  else stream = NULL; // not initialized yet
}

void AudioStreamData::changeSession(AmB2BSession *session)
{
  if (!stream) {
    // the stream was not created yet
    TRACE("delayed stream initialization for session %p\n", session);
    if (session) initialize(session);
  }
  else {
    // the stream is already created

    if (session) {
      stream->changeSession(session);

      /* FIXME: do we want to reinitialize the stream?
      stream->setRtpRelayTransparentSeqno(session->getRtpRelayTransparentSeqno());
      stream->setRtpRelayTransparentSSRC(session->getRtpRelayTransparentSSRC());
      force_symmetric_rtp = session->getRtpRelayForceSymmetricRtp();
      enable_dtmf_transcoding = session->getEnableDtmfTranscoding();
      session->getLowFiPLs(lowfi_payloads);
      stream->setLocalIP(session->localMediaIP());
      ...
      }*/
    }
    else clear(); // free the stream and other stuff because it can't be used anyway
  }
}


void AudioStreamData::clear()
{
  resetStats();
  if (in) {
    //in->close();
    //delete in;
    in = NULL;
  }
  if (stream) {
    delete stream;
    stream = NULL;
  }
  clearDtmfSink();
  initialized = false;
}

void AudioStreamData::stopStreamProcessing()
{
  if (stream) stream->stopReceiving();
}

void AudioStreamData::resumeStreamProcessing()
{
  if (stream) stream->resumeReceiving();
}

void AudioStreamData::setRelayStream(AmRtpStream *other)
{
  if (!stream) return;

  if (relay_address.empty()) {
    DBG("not setting relay for empty relay address\n");
    stream->disableRtpRelay();
    return;
  }

  if (relay_enabled && other) {
    stream->setRelayStream(other);
    stream->setRelayPayloads(relay_mask);
    if (!relay_paused)
      stream->enableRtpRelay();

    stream->setRAddr(relay_address, relay_port, relay_port+1);
  }
  else {
    // nothing to relay or other stream not set
    stream->disableRtpRelay();
  }
}

void AudioStreamData::setRelayPayloads(const SdpMedia &m, RelayController *ctrl) {
  ctrl->computeRelayMask(m, relay_enabled, relay_mask);
}

void AudioStreamData::setRelayDestination(const string& connection_address, int port) {
  relay_address = connection_address; relay_port = port;
}

void AudioStreamData::setRelayPaused(bool paused) {
  if (paused == relay_paused) {
    DBG("relay already paused for stream [%p], ignoring\n", stream);
    return;
  }

  relay_paused = paused;
  DBG("relay %spaused, stream [%p]\n", relay_paused?"":"not ", stream);

  if (NULL != stream) {
    if (relay_paused)
      stream->disableRtpRelay();
    else 
      stream->enableRtpRelay();
  }
}

void AudioStreamData::clearDtmfSink()
{
  if (dtmf_detector) {
    delete dtmf_detector;
    dtmf_detector = NULL;
  }
  if (dtmf_queue) {
    delete dtmf_queue;
    dtmf_queue = NULL;
  }
}

void AudioStreamData::setDtmfSink(AmDtmfSink *dtmf_sink)
{
  // TODO: optimize: clear & create the dtmf_detector only if the dtmf_sink changed
  clearDtmfSink();

  if (dtmf_sink && stream) {
    dtmf_detector = new AmDtmfDetector(dtmf_sink);
    dtmf_queue = new AmDtmfEventQueue(dtmf_detector);
    dtmf_detector->setInbandDetector(AmConfig::DefaultDTMFDetector, stream->getSampleRate());

    if(!enable_dtmf_transcoding && lowfi_payloads.size()) {
      string selected_payload_name = stream->getPayloadName(stream->getPayloadType());
      for(vector<SdpPayload>::iterator it = lowfi_payloads.begin();
          it != lowfi_payloads.end(); ++it){
        DBG("checking %s/%i PL type against %s/%i\n",
            selected_payload_name.c_str(), stream->getPayloadType(),
            it->encoding_name.c_str(), it->payload_type);
        if(selected_payload_name == it->encoding_name) {
          enable_dtmf_transcoding = true;
          break;
        }
      }
    }
  }
}

bool AudioStreamData::initStream(PlayoutType playout_type,
    AmSdp &local_sdp, AmSdp &remote_sdp, int media_idx)
{
  resetStats();

  if (!stream) {
    initialized = false;
    return false;
  }

  // TODO: try to init only in case there are some payloads which can't be relayed
  stream->forceSdpMediaIndex(media_idx);

  stream->setOnHold(false); // just hack to do correctly mute detection in stream->init
  if (stream->init(local_sdp, remote_sdp, force_symmetric_rtp) == 0) {
    stream->setPlayoutType(playout_type);
    initialized = true;

//    // do not unmute if muted because of 0.0.0.0 remote IP (the mute flag is set during init)
//    if (!stream->muted()) stream->setOnHold(muted);

  } else {
    initialized = false;
    DBG("stream initialization failed\n");
    // there still can be payloads to be relayed (if all possible payloads are
    // to be relayed this needs not to be an error)
  }
  stream->setOnHold(muted);
  stream->setReceiving(receiving);

  return initialized;
}

void AudioStreamData::sendDtmf(int event, unsigned int duration_ms)
{
  if (stream) stream->sendDtmf(event,duration_ms);
}

void AudioStreamData::resetStats()
{
  if (outgoing_payload != UNDEFINED_PAYLOAD) {
    b2b_stats.decCodecWriteUsage(outgoing_payload_name);
    outgoing_payload = UNDEFINED_PAYLOAD;
    outgoing_payload_name.clear();
  }
  if (incoming_payload != UNDEFINED_PAYLOAD) {
    b2b_stats.decCodecReadUsage(incoming_payload_name);
    incoming_payload = UNDEFINED_PAYLOAD;
    incoming_payload_name.clear();
  }
}

void AudioStreamData::updateSendStats()
{
  if (!initialized) {
    resetStats();
    return;
  }

  int payload = stream->getPayloadType();
  if (payload != outgoing_payload) { 
    // payload used to send has changed

    // decrement usage of previous payload if set
    if (outgoing_payload != UNDEFINED_PAYLOAD) 
      b2b_stats.decCodecWriteUsage(outgoing_payload_name);
    
    if (payload != UNDEFINED_PAYLOAD) {
      // remember payload name (in lowercase to simulate case insensitivity)
      outgoing_payload_name = stream->getPayloadName(payload);
      transform(outgoing_payload_name.begin(), outgoing_payload_name.end(), 
          outgoing_payload_name.begin(), ::tolower);
      b2b_stats.incCodecWriteUsage(outgoing_payload_name);
    }
    else outgoing_payload_name.clear();
    outgoing_payload = payload;
  }
}

void AudioStreamData::updateRecvStats(AmRtpStream *s)
{
  if (!initialized) {
    resetStats();
    return;
  }

  int payload = s->getLastPayload();
  if (payload != incoming_payload) { 
    // payload used to send has changed

    // decrement usage of previous payload if set
    if (incoming_payload != UNDEFINED_PAYLOAD) 
      b2b_stats.decCodecReadUsage(incoming_payload_name);
    
    if (payload != UNDEFINED_PAYLOAD) {
      // remember payload name (in lowercase to simulate case insensitivity)
      incoming_payload_name = stream->getPayloadName(payload);
      transform(incoming_payload_name.begin(), incoming_payload_name.end(), 
          incoming_payload_name.begin(), ::tolower);
      b2b_stats.incCodecReadUsage(incoming_payload_name);
    }
    else incoming_payload_name.clear();
    incoming_payload = payload;
  }
}

int AudioStreamData::writeStream(unsigned long long ts, unsigned char *buffer, AudioStreamData &src)
{
  if (!initialized) return 0;
  if (stream->getOnHold()) return 0; // ignore hold streams?

  unsigned int f_size = stream->getFrameSize();
  if (stream->sendIntReached(ts)) {
    // A leg is ready to send data
    int sample_rate = stream->getSampleRate();
    int got = 0;
    if (in) got = in->get(ts, buffer, sample_rate, f_size);
    else {
      if (!src.isInitialized()) return 0;
      AmRtpAudio *src_stream = src.getStream();
      if (src_stream->checkInterval(ts)) {
        got = src_stream->get(ts, buffer, sample_rate, f_size);
        if (got > 0) {
          updateRecvStats(src_stream);
          if (dtmf_queue && enable_dtmf_transcoding) { 
	    dtmf_queue->putDtmfAudio(buffer, got, ts);
	  }
        }
      }
    }
    if (got < 0) return -1;
    if (got > 0) {
      // we have data to be sent
      updateSendStats();
      return stream->put(ts, buffer, sample_rate, got);
    }
  }
  return 0;
}

void AudioStreamData::mute(bool set_mute)
{
  DBG("mute(%s) - RTP stream [%p]\n", set_mute?"true":"false", stream);
 
  if (stream) {
    stream->setOnHold(set_mute);
    if (muted != set_mute) stream->clearRTPTimeout();
  }
  muted = set_mute;
}

void AudioStreamData::setReceiving(bool r) {
  DBG("setReceiving(%s) - RTP stream [%p]\n", r?"true":"false", stream);
  if (stream) {
    stream->setReceiving(r);
  }
  receiving = r;
}

//////////////////////////////////////////////////////////////////////////////////

AmB2BMedia::RelayStreamPair::RelayStreamPair(AmB2BSession *_a, AmB2BSession *_b)
: a(_a, _a ? _a->getRtpInterface() : -1),
  b(_b, _b ? _b->getRtpInterface() : -1)
{
  a.enableRawRelay();
  b.enableRawRelay();
}

AmB2BMedia::AmB2BMedia(AmB2BSession *_a, AmB2BSession *_b): 
  ref_cnt(0), // everybody who wants to use must add one reference itselves
  a(_a), b(_b),
  callgroup(AmSession::getNewId()),
  have_a_leg_local_sdp(false), have_a_leg_remote_sdp(false),
  have_b_leg_local_sdp(false), have_b_leg_remote_sdp(false),
  playout_type(ADAPTIVE_PLAYOUT),
  //playout_type(SIMPLE_PLAYOUT),
  a_leg_muted(false), b_leg_muted(false),
  relay_paused(false),
  logger(NULL)
{ 
}

AmB2BMedia::~AmB2BMedia()
{
  if (logger) dec_ref(logger);
}

void AmB2BMedia::addToMediaProcessor() {
  addReference(); // AmMediaProcessor's reference
  AmMediaProcessor::instance()->addSession(this, callgroup);
}

void AmB2BMedia::addToMediaProcessorUnsafe() {
  ref_cnt++; // AmMediaProcessor's reference
  AmMediaProcessor::instance()->addSession(this, callgroup);
}

void AmB2BMedia::addReference() {
  mutex.lock();
  ref_cnt++;
  mutex.unlock();
}

bool AmB2BMedia::releaseReference() {
  mutex.lock();
  int r = --ref_cnt;
  mutex.unlock();
  if (r==0) {
    DBG("last reference to AmB2BMedia [%p] cleared, destroying\n", this);
    delete this;
  }
  return (r == 0); 
}

void AmB2BMedia::changeSession(bool a_leg, AmB2BSession *new_session)
{
  AmLock lock(mutex);
  changeSessionUnsafe(a_leg, new_session);
}

void AmB2BMedia::changeSessionUnsafe(bool a_leg, AmB2BSession *new_session)
{
  TRACE("changing %s leg session to %p\n", a_leg ? "A" : "B", new_session);
  if (a_leg) a = new_session;
  else b = new_session;

  bool needs_processing = a && b && a->getRtpRelayMode() == AmB2BSession::RTP_Transcoding;

  // update all streams
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    // stop processing first to avoid unexpected results
    i->a.stopStreamProcessing();
    i->b.stopStreamProcessing();

    // replace session
    if (a_leg) {
      i->a.changeSession(new_session);
    }
    else {
      i->b.changeSession(new_session);
    }

    updateStreamPair(*i);

    if (i->requiresProcessing()) needs_processing = true;

    // reset logger (needed if a stream changes)
    i->setLogger(logger);

    // return back for processing if needed
    i->a.resumeStreamProcessing();
    i->b.resumeStreamProcessing();
  }

  for (RelayStreamIterator j = relay_streams.begin(); j != relay_streams.end(); ++j) {
    AmRtpStream &a = (*j)->a;
    AmRtpStream &b = (*j)->b;

    // FIXME: is stop & resume receiving needed here?
    if (a_leg)
      a.changeSession(new_session);
    else
      b.changeSession(new_session);
  }

  if (needs_processing) {
    if (!isProcessingMedia()) {
      addToMediaProcessorUnsafe();
    }
  }
  else if (isProcessingMedia()) AmMediaProcessor::instance()->removeSession(this);

  TRACE("session changed\n");
}

int AmB2BMedia::writeStreams(unsigned long long ts, unsigned char *buffer)
{
  int res = 0;
  AmLock lock(mutex);
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    if (i->a.writeStream(ts, buffer, i->b) < 0) { res = -1; break; }
    if (i->b.writeStream(ts, buffer, i->a) < 0) { res = -1; break; }
  }
  return res;
}

void AmB2BMedia::processDtmfEvents()
{
  AmLock lock(mutex);
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    i->a.processDtmfEvents();
    i->b.processDtmfEvents();
  }

  if (a) a->processDtmfEvents();
  if (b) b->processDtmfEvents();
}

void AmB2BMedia::sendDtmf(bool a_leg, int event, unsigned int duration_ms)
{
  AmLock lock(mutex);
  if(!audio.size())
    return;

  // send the DTMFs using the first available stream
  if(a_leg) {
    audio[0].a.sendDtmf(event,duration_ms);
  }
  else {
    audio[0].b.sendDtmf(event,duration_ms);
  }
}

void AmB2BMedia::clearAudio(bool a_leg)
{
  TRACE("clear %s leg audio\n", a_leg ? "A" : "B");
  AmLock lock(mutex);

  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    // remove streams from AmRtpReceiver first! (always both?)
    i->a.stopStreamProcessing();
    i->b.stopStreamProcessing();
    if (a_leg) i->a.clear();
    else i->b.clear();
  }

  for (RelayStreamIterator j = relay_streams.begin(); j != relay_streams.end(); ++j) {
    if ((*j)->a.hasLocalSocket())
      AmRtpReceiver::instance()->removeStream((*j)->a.getLocalSocket());
    if ((*j)->b.hasLocalSocket())
      AmRtpReceiver::instance()->removeStream((*j)->b.getLocalSocket());
  }

  // forget sessions to avoid using them once clearAudio is called
  changeSessionUnsafe(a_leg, NULL);

  if (!a && !b) {
    audio.clear(); // both legs cleared
    for (RelayStreamIterator j = relay_streams.begin(); j != relay_streams.end(); ++j) {
      delete *j;
    }
    relay_streams.clear();
  }
}

void AmB2BMedia::clearRTPTimeout()
{
  AmLock lock(mutex);

  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    i->a.clearRTPTimeout();
    i->b.clearRTPTimeout();
  }
}

bool AmB2BMedia::canRelay(const SdpMedia &m)
{
  return (m.transport == TP_RTPAVP) ||
    (m.transport == TP_RTPSAVP) ||
    (m.transport == TP_UDP) ||
    (m.transport == TP_UDPTL);
}

void AmB2BMedia::createStreams(const AmSdp &sdp)
{
  AudioStreamIterator astreams = audio.begin();
  RelayStreamIterator rstreams = relay_streams.begin();
  vector<SdpMedia>::const_iterator m = sdp.media.begin();
  int idx = 0;
  bool create_audio = astreams == audio.end();
  bool create_relay = rstreams == relay_streams.end();

  for (; m != sdp.media.end(); ++m, ++idx) {

    // audio streams
    if (m->type == MT_AUDIO) {
      if (create_audio) {
        AudioStreamPair pair(a, b, idx);
        pair.a.mute(a_leg_muted);
        pair.b.mute(b_leg_muted);
        audio.push_back(pair);
        audio.back().setLogger(logger);
      }
      else if (++astreams == audio.end()) create_audio = true; // we went through the last audio stream
    }

    // non-audio streams that we can relay
    else if(canRelay(*m))
    {
      if (create_relay) {
	relay_streams.push_back(new RelayStreamPair(a, b));
        relay_streams.back()->setLogger(logger);
      }
      else if (++rstreams == relay_streams.end()) create_relay = true; // we went through the last relay stream
    }
  }
}

void AmB2BMedia::replaceConnectionAddress(AmSdp &parser_sdp, bool a_leg, 
					  const string& relay_address,
					  const string& relay_public_address)
{
  AmLock lock(mutex);

  SdpConnection orig_conn = parser_sdp.conn; // needed for the 'quick workaround' for non-audio media

  // place relay_address in connection address
  if (!parser_sdp.conn.address.empty() && (parser_sdp.conn.address != zero_ip)) {
    parser_sdp.conn.address = relay_public_address;
    DBG("new connection address: %s",parser_sdp.conn.address.c_str());
  }

  // we need to create streams if they are not already created
  createStreams(parser_sdp);

  string replaced_ports;

  AudioStreamIterator audio_stream_it = audio.begin();
  RelayStreamIterator relay_stream_it = relay_streams.begin();

  std::vector<SdpMedia>::iterator it = parser_sdp.media.begin();
  for (; it != parser_sdp.media.end() ; ++it) {
  
    // FIXME: only UDP streams are handled for now
    if (it->type == MT_AUDIO) {

      if( audio_stream_it == audio.end() ){
	// strange... we should actually have a stream for this media line...
	DBG("audio media line does not have coresponding audio stream...\n");
	continue;
      }

      if(it->port) { // if stream active
	if (!it->conn.address.empty() && (parser_sdp.conn.address != zero_ip)) {
	  it->conn.address = relay_public_address;
	  DBG("new stream connection address: %s",it->conn.address.c_str());
	}
	try {
	  if (a_leg) {
	    audio_stream_it->a.setLocalIP(relay_address);
	    it->port = audio_stream_it->a.getLocalPort();
            replaceRtcpAttr(*it, relay_address, audio_stream_it->a.getLocalRtcpPort());
	  }
	  else {
	    audio_stream_it->b.setLocalIP(relay_address);
	    it->port = audio_stream_it->b.getLocalPort();
            replaceRtcpAttr(*it, relay_address, audio_stream_it->b.getLocalRtcpPort());
	  }
	  if(!replaced_ports.empty()) replaced_ports += "/";
	  replaced_ports += int2str(it->port);
	} catch (const string& s) {
	  ERROR("setting port: '%s'\n", s.c_str());
	  throw string("error setting RTP port\n");
	}
      }
      ++audio_stream_it;
    }
    else if(canRelay(*it)) {

      if( relay_stream_it == relay_streams.end() ){
	// strange... we should actually have a stream for this media line...
	DBG("media line does not have a coresponding relay stream...\n");
	continue;
      }

      if(it->port) { // if stream active
	if (!it->conn.address.empty() && (parser_sdp.conn.address != zero_ip)) {
	  it->conn.address = relay_public_address;
	  DBG("new stream connection address: %s",it->conn.address.c_str());
	}
	try {
	  if (a_leg) {
	    if(!(*relay_stream_it)->a.hasLocalSocket()){
	      (*relay_stream_it)->a.setLocalIP(relay_address);
	    }
	    it->port = (*relay_stream_it)->a.getLocalPort();
            replaceRtcpAttr(*it, relay_address, (*relay_stream_it)->a.getLocalRtcpPort());
	  }
	  else {
	    if(!(*relay_stream_it)->b.hasLocalSocket()){
	      (*relay_stream_it)->b.setLocalIP(relay_address);
	    }
	    it->port = (*relay_stream_it)->b.getLocalPort();
            replaceRtcpAttr(*it, relay_address, (*relay_stream_it)->b.getLocalRtcpPort());
	  }
	  if(!replaced_ports.empty()) replaced_ports += "/";
	  replaced_ports += int2str(it->port);
	} catch (const string& s) {
	  ERROR("setting port: '%s'\n", s.c_str());
	  throw string("error setting RTP port\n");
	}
      }
      ++relay_stream_it;
    }
    else {
      // quick workaround to allow direct connection of non-supported streams (i.e.
      // those which are not relayed or transcoded): propagate connection
      // address - might work but need not (to be tested with real clients
      // instead of simulators)
      if (it->conn.address.empty()) it->conn = orig_conn;
      continue;
    }
  }

  if (it != parser_sdp.media.end()) {
    // FIXME: create new streams here?
    WARN("trying to relay SDP with more media lines than "
	 "relay streams initialized (%zu)\n",audio.size()+relay_streams.size());
  }

  DBG("replaced connection address in SDP with %s:%s.\n",
      relay_public_address.c_str(), replaced_ports.c_str());
}
      
static const char* 
_rtp_relay_mode_str(const AmB2BSession::RTPRelayMode& relay_mode)
{
  switch(relay_mode){
  case AmB2BSession::RTP_Direct:
    return "RTP_Direct";
  case AmB2BSession::RTP_Relay:
    return "RTP_Relay";
  case AmB2BSession::RTP_Transcoding:
    return "RTP_Transcoding";
  }

  return "";
}

void AmB2BMedia::updateStreamPair(AudioStreamPair &pair)
{
  bool have_a = have_a_leg_local_sdp && have_a_leg_remote_sdp;
  bool have_b = have_b_leg_local_sdp && have_b_leg_remote_sdp;

  TRACE("updating stream in A leg\n");
  pair.a.setDtmfSink(b);
  if (pair.b.getInput()) pair.a.setRelayStream(NULL); // don't mix relayed RTP into the other's input
  else pair.a.setRelayStream(pair.b.getStream());
  if (have_a) pair.a.initStream(playout_type, a_leg_local_sdp, a_leg_remote_sdp, pair.media_idx);

  TRACE("updating stream in B leg\n");
  pair.b.setDtmfSink(a);
  if (pair.a.getInput()) pair.b.setRelayStream(NULL); // don't mix relayed RTP into the other's input
  else pair.b.setRelayStream(pair.a.getStream());
  if (have_b) pair.b.initStream(playout_type, b_leg_local_sdp, b_leg_remote_sdp, pair.media_idx);

  TRACE("audio streams updated\n");
}

void AmB2BMedia::updateAudioStreams()
{
  // SDP was updated
  TRACE("handling SDP change, A leg: %c%c, B leg: %c%c\n",
      have_a_leg_local_sdp ? 'X' : '-',
      have_a_leg_remote_sdp ? 'X' : '-',
      have_b_leg_local_sdp ? 'X' : '-',
      have_b_leg_remote_sdp ? 'X' : '-');

  // if we have all necessary information we can initialize streams and start
  // their processing
  if (audio.empty() && relay_streams.empty()) return; // no streams

  bool have_a = have_a_leg_local_sdp && have_a_leg_remote_sdp;
  bool have_b = have_b_leg_local_sdp && have_b_leg_remote_sdp;

  if (!(
      (have_a || have_b)
      )) return;

  bool needs_processing = a && b && a->getRtpRelayMode() == AmB2BSession::RTP_Transcoding;

  // initialize streams to be able to relay & transcode (or use local audio)
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    i->a.stopStreamProcessing();
    i->b.stopStreamProcessing();

    updateStreamPair(*i);

    if (i->requiresProcessing()) needs_processing = true;

    i->a.resumeStreamProcessing();
    i->b.resumeStreamProcessing();
  }

  // start media processing (only if transcoding or regular audio processing
  // required)
  // Note: once we send local SDP to the other party we have to expect RTP but
  // we need to be fully initialised (both legs) before we can correctly handle
  // the media, right?
  if (needs_processing) {
    if (!isProcessingMedia()) {
      addToMediaProcessorUnsafe();
    }
  }
  else if (isProcessingMedia()) AmMediaProcessor::instance()->removeSession(this);
}

void AmB2BMedia::updateRelayStream(AmRtpStream *stream, AmB2BSession *session,
				   const string& connection_address,
				   const SdpMedia &m, AmRtpStream *relay_to)
{
  static const PayloadMask true_mask(true);

  stream->stopReceiving();
  if(m.port) {
    if (session) {
      // propagate session settings
      stream->setPassiveMode(session->getRtpRelayForceSymmetricRtp());
      stream->setRtpRelayTransparentSeqno(session->getRtpRelayTransparentSeqno());
      stream->setRtpRelayTransparentSSRC(session->getRtpRelayTransparentSSRC());
      // if (!stream->hasLocalSocket()) stream->setLocalIP(session->advertisedIP());
    }
    stream->setRelayStream(relay_to);
    stream->setRelayPayloads(true_mask);
    if (!relay_paused)
      stream->enableRtpRelay();
    stream->setRAddr(connection_address,m.port,m.port+1);
    if((m.transport != TP_RTPAVP) && (m.transport != TP_RTPSAVP))
      stream->enableRawRelay();
    stream->resumeReceiving();
  }
  else {
    DBG("disabled stream");
  }
}

void AmB2BMedia::updateStreams(bool a_leg, const AmSdp &local_sdp, const AmSdp &remote_sdp, RelayController *ctrl)
{
  TRACE("%s (%c): updating streams with local & remote SDP\n",
      a_leg ? (a ? a->getLocalTag().c_str() : "NULL") : (b ? b->getLocalTag().c_str() : "NULL"),
      a_leg ? 'A': 'B');

  /*string s;
  local_sdp.print(s);
  INFO("local SDP: %s\n", s.c_str());
  remote_sdp.print(s);
  INFO("remote SDP: %s\n", s.c_str());*/

  AmLock lock(mutex);
  // streams should be created already (replaceConnectionAddress called
  // before updateLocalSdp uses/assignes their port numbers)

  // save SDP: FIXME: really needed to store instead of just to use?
  if (a_leg) {
    a_leg_local_sdp = local_sdp;
    a_leg_remote_sdp = remote_sdp;
    have_a_leg_local_sdp = true;
    have_a_leg_remote_sdp = true;
  }
  else {
    b_leg_local_sdp = local_sdp;
    b_leg_remote_sdp = remote_sdp;
    have_b_leg_local_sdp = true;
    have_b_leg_remote_sdp = true;
  }

  // create missing streams
  createStreams(local_sdp); // FIXME: remote_sdp?

  // compute relay mask for every stream
  // Warning: do not apply the new mask unless the offer answer succeeds?
  // we can safely apply the changes once we have local & remote SDP (i.e. the
  // negotiation is finished) otherwise we might handle the RTP in a wrong way

  AudioStreamIterator astream = audio.begin();
  RelayStreamIterator rstream = relay_streams.begin();
  for (vector<SdpMedia>::const_iterator m = remote_sdp.media.begin(); m != remote_sdp.media.end(); ++m) {
    const string& connection_address = (m->conn.address.empty() ? remote_sdp.conn.address : m->conn.address);

    if (m->type == MT_AUDIO) {
      // initialize relay mask in the other(!) leg and relay destination for stream in current leg
      TRACE("relay payloads in direction %s\n", a_leg ? "B -> A" : "A -> B");
      if (a_leg) {
        astream->b.setRelayPayloads(*m, ctrl);
        astream->a.setRelayDestination(connection_address, m->port);
      }
      else {
        astream->a.setRelayPayloads(*m, ctrl);
        astream->b.setRelayDestination(connection_address, m->port);
      }
      ++astream;
    }

    else {
      if (!canRelay(*m)) continue;
      if (rstream == relay_streams.end()) continue;

      RelayStreamPair& relay_stream = **rstream;

      if(a_leg) {
	DBG("updating A-leg relay_stream");
        updateRelayStream(&relay_stream.a, a, connection_address, *m, &relay_stream.b);
      }
      else {
	DBG("updating B-leg relay_stream");
        updateRelayStream(&relay_stream.b, b, connection_address, *m, &relay_stream.a);
      }
      ++rstream;
    }
  }

  updateAudioStreams();

  TRACE("streams updated with SDP\n");
}

void AmB2BMedia::stop(bool a_leg)
{
  TRACE("stop %s leg\n", a_leg ? "A" : "B");
  clearAudio(a_leg);
  // remove from processor only if both A and B leg stopped
  if (isProcessingMedia() && (!a) && (!b)) {
    AmMediaProcessor::instance()->removeSession(this);
  }
}

void AmB2BMedia::onMediaProcessingTerminated()
{
  AmMediaSession::onMediaProcessingTerminated();

  // release reference held by AmMediaProcessor
  releaseReference();
}

bool AmB2BMedia::replaceOffer(AmSdp &sdp, bool a_leg)
{
  TRACE("replacing offer with a local one\n");
  createStreams(sdp); // create missing streams

  AmLock lock(mutex);

  try {

    AudioStreamIterator as = audio.begin();
    for (vector<SdpMedia>::iterator m = sdp.media.begin(); m != sdp.media.end(); ++m) {
      if (m->type == MT_AUDIO && as != audio.end()) {
        // generate our local offer
        TRACE("... making audio stream offer\n");
        if (a_leg) as->a.getSdpOffer(as->media_idx, *m);
        else as->b.getSdpOffer(as->media_idx, *m);
        ++as;
      }
      else {
        TRACE("... making non-audio/uninitialised stream inactive\n");
        m->send = false;
        m->recv = false;
      }
    }

  }
  catch (...) {
    TRACE("hold SDP offer creation failed\n");
    return true;
  }

  TRACE("hold SDP offer generated\n");

  return true;
}

void AmB2BMedia::setMuteFlag(bool a_leg, bool set)
{
  AmLock lock(mutex);
  if (a_leg) a_leg_muted = set;
  else b_leg_muted = set;
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    if (a_leg) i->a.mute(set);
    else i->b.mute(set);
  }
}

void AmB2BMedia::setFirstStreamInput(bool a_leg, AmAudio *in)
{
  AmLock lock(mutex);
  //for ( i != audio.end(); ++i) {
  if (!audio.empty()) {
    AudioStreamIterator i = audio.begin();
    if (a_leg) i->a.setInput(in);
    else i->b.setInput(in);
    updateAudioStreams();
  }
  else {
    if (in) {
      ERROR("BUG: can't set %s leg's first stream input, no streams\n", a_leg ? "A": "B");
    }
  }
}

void AmB2BMedia::createHoldAnswer(bool a_leg, const AmSdp &offer, AmSdp &answer, bool use_zero_con)
{
  // because of possible RTP relaying our payloads need not to match the remote
  // party's payloads (i.e. we might need not understand the remote party's
  // codecs)
  // As a quick hack we may use just copy of the original SDP with all streams
  // deactivated to avoid sending RTP to us (twinkle requires at least one
  // non-disabled stream in the response so we can not set all ports to 0 to
  // signalize that we don't want to receive anything)

  AmLock lock(mutex);

  answer = offer;
  answer.media.clear();

  if (use_zero_con) answer.conn.address = zero_ip;
  else {
    if (a_leg) { if (a) answer.conn.address = a->advertisedIP(); }
    else { if (b) answer.conn.address = b->advertisedIP(); }

    if (answer.conn.address.empty()) answer.conn.address = zero_ip; // we need something there
  }

  AudioStreamIterator i = audio.begin();
  vector<SdpMedia>::const_iterator m;
  for (m = offer.media.begin(); m != offer.media.end(); ++m) {
    answer.media.push_back(SdpMedia());
    SdpMedia &media = answer.media.back();
    media.type = m->type;

    if (media.type != MT_AUDIO) { media = *m ; media.port = 0; continue; } // copy whole media line except port
    if (m->port == 0) { media = *m; ++i; continue; } // copy whole inactive media line

    if (a_leg) i->a.getSdpAnswer(i->media_idx, *m, media);
    else i->b.getSdpAnswer(i->media_idx, *m, media);

    media.send = false; // should be already because the stream should be on hold
    media.recv = false; // what we would do with received data?

    if (media.payloads.empty()) {
      // we have to add something there
      if (!m->payloads.empty()) media.payloads.push_back(m->payloads[0]);
    }
    break;
  }
}

void AmB2BMedia::setRtpLogger(msg_logger* _logger)
{
  if (logger) dec_ref(logger);
  logger = _logger;
  if (logger) inc_ref(logger);

  // walk through all the streams and use logger for them
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) i->setLogger(logger);
  for (RelayStreamIterator j = relay_streams.begin(); j != relay_streams.end(); ++j) (*j)->setLogger(logger);
}

void AmB2BMedia::setRelayDTMFReceiving(bool enabled) {
  DBG("relay_streams.size() = %zd, audio_streams.size() = %zd\n", relay_streams.size(), audio.size());
  for (RelayStreamIterator j = relay_streams.begin(); j != relay_streams.end(); j++) {
    DBG("force_receive_dtmf %sabled for [%p]\n", enabled?"en":"dis", &(*j)->a);
    DBG("force_receive_dtmf %sabled for [%p]\n", enabled?"en":"dis", &(*j)->b);
    (*j)->a.force_receive_dtmf = enabled;
    (*j)->b.force_receive_dtmf = enabled;
  }

  for (AudioStreamIterator j = audio.begin(); j != audio.end(); j++) {
    DBG("force_receive_dtmf %sabled for [%p]\n", enabled?"en":"dis", j->a.getStream());
    DBG("force_receive_dtmf %sabled for [%p]\n", enabled?"en":"dis", j->b.getStream());
    if (NULL != j->a.getStream())
      j->a.getStream()->force_receive_dtmf = enabled;
    
    if (NULL != j->b.getStream())
      j->b.getStream()->force_receive_dtmf = enabled;
  }
}

/** set receving of RTP/relay streams (not receiving=drop incoming packets) */
void AmB2BMedia::setReceiving(bool receiving_a, bool receiving_b) {
  AmLock lock(mutex); // TODO: is this necessary?

  DBG("relay_streams.size() = %zd, audio_streams.size() = %zd\n", relay_streams.size(), audio.size());
  for (RelayStreamIterator j = relay_streams.begin(); j != relay_streams.end(); j++) {
    DBG("setReceiving(%s) A relay stream [%p]\n", receiving_a?"true":"false", &(*j)->a);
    (*j)->a.setReceiving(receiving_a);
    DBG("setReceiving(%s) B relay stream [%p]\n", receiving_b?"true":"false", &(*j)->b);
    (*j)->b.setReceiving(receiving_b);
  }

  for (AudioStreamIterator j = audio.begin(); j != audio.end(); j++) {
    DBG("setReceiving(%s) A audio stream [%p]\n", receiving_a?"true":"false", j->a.getStream());
    j->a.setReceiving(receiving_a);
    DBG("setReceiving(%s) B audio stream [%p]\n", receiving_b?"true":"false", j->b.getStream());
    j->b.setReceiving(receiving_b);
  }

}

void AmB2BMedia::pauseRelay() {
  DBG("relay_streams.size() = %zd, audio_streams.size() = %zd\n", relay_streams.size(), audio.size());
  relay_paused = true;
  for (RelayStreamIterator j = relay_streams.begin(); j != relay_streams.end(); j++) {
    (*j)->a.disableRawRelay();
    (*j)->b.disableRawRelay();
  }

  for (AudioStreamIterator j = audio.begin(); j != audio.end(); j++) {
    j->a.setRelayPaused(true);
    j->b.setRelayPaused(true);
  }
}

void AmB2BMedia::restartRelay() {
  DBG("relay_streams.size() = %zd, audio_streams.size() = %zd\n", relay_streams.size(), audio.size());
  relay_paused = false;
  for (RelayStreamIterator j = relay_streams.begin(); j != relay_streams.end(); j++) {
    (*j)->a.enableRawRelay();
    (*j)->b.enableRawRelay();
  }

  for (AudioStreamIterator j = audio.begin(); j != audio.end(); j++) {
    j->a.setRelayPaused(false);
    j->b.setRelayPaused(false);
  }
}

void AudioStreamData::debug()
{
  DBG("\tmuted: %s\n", muted ? "yes" : "no");
  if(stream) {
    stream->debug();
  }
  else
    DBG("\t<null> <-> <null>");
}

// print debug info
void AmB2BMedia::debug()
{
  // walk through all the streams
  DBG("B2B media session %p ('%s' <-> '%s'):",
      this,
      a ? a->getLocalTag().c_str() : "?",
      b ? b->getLocalTag().c_str() : "?");
  DBG("\tOA status: %c%c / %c%c",
      have_a_leg_local_sdp ? 'X' : '-',
      have_a_leg_remote_sdp ? 'X' : '-',
      have_b_leg_local_sdp ? 'X' : '-',
      have_b_leg_remote_sdp ? 'X' : '-');

  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    DBG(" - audio stream (A):\n");
    i->a.debug();
    DBG(" - audio stream (B):\n");
    i->b.debug();
  }

  for (RelayStreamIterator j = relay_streams.begin(); 
       j != relay_streams.end(); ++j) {

    DBG(" - relay stream (A):\n");
    (*j)->a.debug();
    DBG(" - relay stream (B):\n");
    (*j)->b.debug();
  }
}
