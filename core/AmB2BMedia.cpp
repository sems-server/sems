#include "AmB2BMedia.h"
#include "AmAudio.h"
#include "amci/codecs.h"
#include <string.h>
#include <strings.h>
#include "AmB2BSession.h"
#include "AmRtpReceiver.h"
#include "sip/msg_logger.h"

#include <algorithm>

#define TRACE DBG
#define UNDEFINED_PAYLOAD (-1)

/** class for computing payloads for relay the simpliest way - allow relaying of
 * all payloads supported by remote party */
class SimpleRelayController: public RelayController {
  public:
    virtual void computeRelayMask(const SdpMedia &m, bool &enable, PayloadMask &mask);
};

static B2BMediaStatistics b2b_stats;
static SimpleRelayController simple_relay_ctrl;

static const string zero_ip("0.0.0.0");

//////////////////////////////////////////////////////////////////////////////////

void SimpleRelayController::computeRelayMask(const SdpMedia &m, bool &enable, PayloadMask &mask)
{
  int te_pl = -1;
  enable = false;

  mask.clear();

  // walk through the media lines and find the telephone-event payload
  for (std::vector<SdpPayload>::const_iterator i = m.payloads.begin();
      i != m.payloads.end(); ++i)
  {
    // do not mark telephone-event payload for relay
    if(!strcasecmp("telephone-event",i->encoding_name.c_str())){
      te_pl = i->payload_type;
    }
    else {
      enable = true;
    }
  }

  if(!enable)
    return;

  if(te_pl > 0) { 
    TRACE("unmarking telephone-event payload %d for relay\n", te_pl);
    mask.set(te_pl);
  }

  TRACE("marking all other payloads for relay\n");
  mask.invert();
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
  muted(false)
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
  if (!stream) {
    ERROR("BUG: trying to set relay for NULL stream\n");
    return;
  }

  // FIXME: muted stream should not relay to the other?
  /* if (muted) {
    stream->disableRtpRelay();
    return;
  }*/

  if (relay_enabled && other) {
    stream->enableRtpRelay(relay_mask, other);
    stream->setRAddr(relay_address, relay_port, relay_port+1);
  }
  else {
    // nothing to relay or other stream not set
    stream->disableRtpRelay();
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
    // we have no stream so normal audio processing is not possible
    // FIXME: if we have no stream here (i.e. no session) how we got the local
    // and remote SDP?
    ERROR("BUG: trying to initialize stream before creation\n");
    initialized = false;
    return false; // it is bug with current AmB2BMedia implementation
  }

  // TODO: try to init only in case there are some payloads which can't be relayed
  stream->forceSdpMediaIndex(media_idx);
  if (stream->init(local_sdp, remote_sdp, force_symmetric_rtp) == 0) {
    stream->setPlayoutType(playout_type);
    initialized = true;
  } else {
    initialized = false;
    DBG("stream initialization failed\n");
    // there still can be payloads to be relayed (if all possible payloads are
    // to be relayed this needs not to be an error)
  }
  stream->setOnHold(muted);

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
  if (stream) {
    stream->setOnHold(set_mute);
    if (muted != set_mute) stream->clearRTPTimeout();
  }
  muted = set_mute;
}
//////////////////////////////////////////////////////////////////////////////////

AmB2BMedia::RelayStreamPair::RelayStreamPair(AmB2BSession *_a, AmB2BSession *_b)
: a(_a, _a ? _a->getRtpInterface() : -1),
  b(_b, _b ? _b->getRtpInterface() : -1)
{ }

AmB2BMedia::AmB2BMedia(AmB2BSession *_a, AmB2BSession *_b): 
  ref_cnt(0), // everybody who wants to use must add one reference itselves
  a(_a), b(_b),
  callgroup(AmSession::getNewId()),
  have_a_leg_local_sdp(false), have_a_leg_remote_sdp(false),
  have_b_leg_local_sdp(false), have_b_leg_remote_sdp(false),
  processing_started(false),
  playout_type(ADAPTIVE_PLAYOUT),
  //playout_type(SIMPLE_PLAYOUT),
  a_leg_muted(false), b_leg_muted(false),
  a_leg_on_hold(false), b_leg_on_hold(false),
  logger(NULL)
{ 
}

AmB2BMedia::~AmB2BMedia()
{
  if (logger) dec_ref(logger);
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

  // update all streams
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    // stop processing first to avoid unexpected results
    if (processing_started) {
      i->a.stopStreamProcessing();
      i->b.stopStreamProcessing();
    }

    // replace session
    if (a_leg) {
      i->a.changeSession(new_session);
    }
    else {
      i->b.changeSession(new_session);
    }

    if (processing_started) {
      // needed to reinitialize relay streams because the streams could change
      // and they are in use already (FIXME: ugly here, needs explicit knowledge
      // what AudioStreamData::changeSesion does)
      if (a) i->a.setRelayStream(i->b.getStream());
      if (b) i->b.setRelayStream(i->a.getStream());

      // needed to reinitialize audio processing in case the stream itself has
      // changed (FIXME: ugly again - see above and local/remote SDP might
      // already change since previous initialization!)
      if (a_leg) {
        if (a) { // we have the session
          TRACE("init A stream stuff\n");
          i->a.initStream(playout_type, a_leg_local_sdp, a_leg_remote_sdp, i->media_idx);
          i->a.setDtmfSink(b);
          i->b.setDtmfSink(new_session);
        }
      }
      else {
        if (b) { // we have the session
          TRACE("init B stream stuff\n");
          i->b.initStream(playout_type, b_leg_local_sdp, b_leg_remote_sdp, i->media_idx);
          i->b.setDtmfSink(a);
          i->a.setDtmfSink(new_session);
        }
      }
    }

    // reset logger (needed if a stream changes)
    i->setLogger(logger);

    // return back for processing if needed
    if (processing_started) {
      i->a.resumeStreamProcessing();
      i->b.resumeStreamProcessing();
    }
  }

  for (RelayStreamIterator j = relay_streams.begin(); j != relay_streams.end(); ++j) {
    AmRtpStream &a = (*j)->a;
    AmRtpStream &b = (*j)->a;

    /*if (a.hasLocalSocket())
      AmRtpReceiver::instance()->removeStream(a.getLocalSocket());
    if (b.hasLocalSocket())
      AmRtpReceiver::instance()->removeStream(b.getLocalSocket());*/

    a.changeSession(new_session);
    b.changeSession(new_session);

    /*if (a.hasLocalSocket())
      AmRtpReceiver::instance()->addStream(a.getLocalSocket(), &a);
    if (b.hasLocalSocket())
      AmRtpReceiver::instance()->addStream(b.getLocalSocket(), &b);*/
  }

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

static bool canRelay(const SdpMedia &m)
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
  static const string void_addr("0.0.0.0");
  AmLock lock(mutex);

  SdpConnection orig_conn = parser_sdp.conn; // needed for the 'quick workaround' for non-audio media

  // place relay_address in connection address
  if (!parser_sdp.conn.address.empty() && (parser_sdp.conn.address != void_addr)) {
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
	if (!it->conn.address.empty() && (parser_sdp.conn.address != void_addr)) {
	  it->conn.address = relay_public_address;
	  DBG("new stream connection address: %s",it->conn.address.c_str());
	}
	try {
	  if (a_leg) {
	    audio_stream_it->a.setLocalIP(relay_address);
	    it->port = audio_stream_it->a.getLocalPort();
	  }
	  else {
	    audio_stream_it->b.setLocalIP(relay_address);
	    it->port = audio_stream_it->b.getLocalPort();
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
	if (!it->conn.address.empty() && (parser_sdp.conn.address != void_addr)) {
	  it->conn.address = relay_public_address;
	  DBG("new stream connection address: %s",it->conn.address.c_str());
	}
	try {
	  if (a_leg) {
	    if(!(*relay_stream_it)->a.hasLocalSocket()){
	      (*relay_stream_it)->a.setLocalIP(relay_address);
	    }
	    it->port = (*relay_stream_it)->a.getLocalPort();
	  }
	  else {
	    if(!(*relay_stream_it)->b.hasLocalSocket()){
	      (*relay_stream_it)->b.setLocalIP(relay_address);
	    }
	    it->port = (*relay_stream_it)->b.getLocalPort();
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
	 "relay streams initialized (%lu)\n", 
	 (unsigned long)(audio.size()+relay_streams.size()));
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

void AmB2BMedia::onSdpUpdate()
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
      (have_a && have_b) ||
      (have_a && !audio.empty() && audio[0].a.getInput() && (!b)) ||
      (have_b && !audio.empty() && audio[0].b.getInput() && (!a))
      )) return;

  // clear all the stored flags (re-INVITEs or UPDATEs will negotiate new remote
  // & local SDPs so the current ones are not interesting later)
  have_a_leg_local_sdp = false;
  have_a_leg_remote_sdp = false;
  have_b_leg_local_sdp = false;
  have_b_leg_remote_sdp = false;

  processing_started = true;

  TRACE("starting media processing\n");

  // initialize streams to be able to relay & transcode (or use local audio)
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    i->a.stopStreamProcessing();
    i->b.stopStreamProcessing();

    if (have_a) {
      TRACE("initializing stream in A leg\n");
      i->a.setDtmfSink(b);
      i->a.setRelayStream(i->b.getStream());
      i->a.initStream(playout_type, a_leg_local_sdp, a_leg_remote_sdp, i->media_idx);
    }

    if (have_b) {
      TRACE("initializing stream in B leg\n");
      i->b.setDtmfSink(a);
      i->b.setRelayStream(i->a.getStream());
      i->b.initStream(playout_type, b_leg_local_sdp, b_leg_remote_sdp, i->media_idx);
    }

    i->a.resumeStreamProcessing();
    i->b.resumeStreamProcessing();
  }

  // start media processing (FIXME: only if transcoding or regular audio
  // processing required?)
  // Note: once we send local SDP to the other party we have to expect RTP but
  // we need to be fully initialised (both legs) before we can correctly handle
  // the media, right?
  if (!isProcessingMedia()) {
    ref_cnt++; // add reference (hold by AmMediaProcessor)
    AmMediaProcessor::instance()->addSession(this, callgroup);
  }
}

static void updateRelayStream(AmRtpStream *stream,
    AmB2BSession *session,
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
    stream->enableRtpRelay(true_mask,relay_to);
    stream->setRAddr(connection_address,m.port,m.port+1);
    if((m.transport != TP_RTPAVP) && (m.transport != TP_RTPSAVP))
      stream->enableRawRelay();
    stream->resumeReceiving();
  }
  else {
    DBG("disabled stream");
  }
}

bool AmB2BMedia::updateRemoteSdp(bool a_leg, const AmSdp &remote_sdp, RelayController *ctrl)
{
  bool ok = true;
  if (!ctrl) ctrl = &simple_relay_ctrl; // use default controller if none given

  AmLock lock(mutex);

  // save SDP
  if (a_leg) {
    a_leg_remote_sdp = remote_sdp;
    have_a_leg_remote_sdp = true;
  }
  else {
    b_leg_remote_sdp = remote_sdp;
    have_b_leg_remote_sdp = true;
  }

  // create missing streams
  createStreams(remote_sdp);

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

  onSdpUpdate();

  return ok;
}
    
bool AmB2BMedia::updateLocalSdp(bool a_leg, const AmSdp &local_sdp)
{
  bool ok = true;
  AmLock lock(mutex);
  // streams should be created already (replaceConnectionAddress called
  // before updateLocalSdp uses/assignes their port numbers)

  // save SDP
  if (a_leg) {
    a_leg_local_sdp = local_sdp;
    have_a_leg_local_sdp = true;
  }
  else {
    b_leg_local_sdp = local_sdp;
    have_b_leg_local_sdp = true;
  }

  // create missing streams
  createStreams(local_sdp);

  onSdpUpdate();

  return ok;
}

void AmB2BMedia::stop(bool a_leg)
{
  TRACE("stop %s leg\n", a_leg ? "A" : "B");
  clearAudio(a_leg);
  // remove from processor only if both A and B leg stopped
  if (isProcessingMedia() && (!a) && (!b)) {
    processing_started = false;
    AmMediaProcessor::instance()->removeSession(this);
  }
}

void AmB2BMedia::onMediaProcessingTerminated() 
{ 
  AmMediaSession::onMediaProcessingTerminated();
  processing_started = false;
  clearAudio();

  // release reference held by AmMediaProcessor
  if (releaseReference()) { 
    delete this; // this should really work :-D
  }
}

bool AmB2BMedia::createHoldRequest(AmSdp &sdp, bool a_leg, bool zero_connection, bool sendonly)
{
  AmB2BSession *session = (a_leg ? a : b);

  // session is needed to fill all the stuff and to have the streams initialised correctly
  if (!session) return false;

  sdp.clear();

  // FIXME: use original origin and continue in versioning? (the one used in
  // previous SDPs if any)
  // stolen from AmSession
  sdp.version = 0;
  sdp.origin.user = "sems";
  //offer.origin.sessId = 1;
  //offer.origin.sessV = 1;
  sdp.sessionName = "sems";
  sdp.conn.network = NT_IN;
  sdp.conn.addrType = AT_V4;
  if (zero_connection) sdp.conn.address = zero_ip;
  else sdp.conn.address = session->advertisedIP();

  // possible params:
  //  - use 0.0.0.0 connection address or sendonly stream
  // create hold request based on current streams
  AmLock lock(mutex);

  try {
    if (audio.empty()) {
      // create one dummy stream to create valid SDP
      AudioStreamPair pair(a, b, 0);
      audio.push_back(pair);
    }

    for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
      // TODO: put disabled media stream for non-audio media? (we would need to
      // remember what type of media was it etc.)

      TRACE("generating SDP offer from stream %d\n", i->media_idx);
      sdp.media.push_back(SdpMedia());
      SdpMedia &m = sdp.media.back();
      m.type = MT_AUDIO;
      if (a_leg) i->a.getSdpOffer(i->media_idx, m);
      else i->b.getSdpOffer(i->media_idx, m);

      m.send = true; // always? (what if there is no 'hold music' to play?
      if (sendonly) m.recv = false;
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
    if (!processing_started) {
      // try to start it
      onSdpUpdate();
    }
  }
  else ERROR("BUG: can't set %s leg's first stream input, no streams\n", a_leg ? "A": "B");
  // FIXME: start processing if not started and streams in this leg are fully initialized ?
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

void AudioStreamData::debug()
{
  if(stream) {
    if(stream->hasLocalSocket() > 0)
      DBG("\t<%i> <-> <%s:%i>", stream->getLocalPort(),
	  stream->getRHost().c_str(), stream->getRPort());
    else
      DBG("\t<unbound> <-> <%s:%i>",
	  stream->getRHost().c_str(),
	  stream->getLocalPort());
  }
  else
    DBG("\t<null> <-> <null>");
}

// print debug info
void AmB2BMedia::debug()
{
  // walk through all the streams
  DBG("B2B media session ('%s' <-> '%s'):",
      a->getLocalTag().c_str(),
      b->getLocalTag().c_str());

  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    DBG("relay stream:\n");
    i->a.debug();
    i->b.debug();
  }

  for (RelayStreamIterator j = relay_streams.begin(); 
       j != relay_streams.end(); ++j) {

    DBG("relay stream:\n");
    if((*j)->a.hasLocalSocket() > 0)
      DBG("\t<%i> <-> <%s:%i>", (*j)->a.getLocalPort(),
	  (*j)->a.getRHost().c_str(), (*j)->a.getRPort());
    else
      DBG("\t<unbound> <-> <%s:%i>",
	  (*j)->a.getRHost().c_str(),
	  (*j)->a.getRPort());

    if((*j)->b.hasLocalSocket() > 0)
      DBG("\t<%i> <-> <%s:%i>", (*j)->b.getLocalPort(),
	  (*j)->b.getRHost().c_str(), (*j)->b.getRPort());
    else
      DBG("\t<unbound> <-> <%s:%i>",
	  (*j)->b.getRHost().c_str(),
	  (*j)->b.getRPort());
  }
}
