#include "AmB2BMedia.h"
#include "AmAudio.h"
#include "amci/codecs.h"
#include <string.h>
#include <strings.h>
#include "AmB2BSession.h"
#include "AmRtpReceiver.h"

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
  enable = false;

  // walk through the media line and add all payload IDs to the bit mask
  for (std::vector<SdpPayload>::const_iterator i = m.payloads.begin();
      i != m.payloads.end(); ++i)
  {
    // do not mark telephone-event payload for relay
    if(strcasecmp("telephone-event",i->encoding_name.c_str()) != 0){
      mask.set(i->payload_type);
      enable = true;
      TRACE("marking payload %d for relay\n", i->payload_type);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////

void B2BMediaStatistics::incCodecWriteUsage(const string &codec_name)
{
  if (codec_name.empty()) return;

  mutex.lock();
  map<string, int>::iterator i = codec_write_usage.find(codec_name);
  if (i != codec_write_usage.end()) i->second++;
  else codec_write_usage[codec_name] = 1;
  mutex.unlock();
}

void B2BMediaStatistics::decCodecWriteUsage(const string &codec_name)
{
  if (codec_name.empty()) return;

  mutex.lock();
  map<string, int>::iterator i = codec_write_usage.find(codec_name);
  if (i != codec_write_usage.end()) {
    if (i->second > 0) i->second--;
  }
  mutex.unlock();
}

void B2BMediaStatistics::incCodecReadUsage(const string &codec_name)
{
  if (codec_name.empty()) return;

  mutex.lock();
  map<string, int>::iterator i = codec_read_usage.find(codec_name);
  if (i != codec_read_usage.end()) i->second++;
  else codec_read_usage[codec_name] = 1;
  mutex.unlock();
}

void B2BMediaStatistics::decCodecReadUsage(const string &codec_name)
{
  if (codec_name.empty()) return;

  mutex.lock();
  map<string, int>::iterator i = codec_read_usage.find(codec_name);
  if (i != codec_read_usage.end()) {
    if (i->second > 0) i->second--;
  }
  mutex.unlock();
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
  mutex.lock();
  for (map<string, int>::iterator i = codec_write_usage.begin();
      i != codec_write_usage.end(); ++i) 
  {
    if (first) first = false;
    else dst += ",";
    dst += i->first;
    dst += "=";
    dst += int2str(i->second);
  }
  mutex.unlock();
}

void B2BMediaStatistics::reportCodecReadUsage(string &dst)
{
  if (codec_read_usage.empty()) {
    dst = "pcma=0"; // to be not empty
    return;
  }

  bool first = true;
  dst.clear();
  mutex.lock();
  for (map<string, int>::iterator i = codec_read_usage.begin();
      i != codec_read_usage.end(); ++i) 
  {
    if (first) first = false;
    else dst += ",";
    dst += i->first;
    dst += "=";
    dst += int2str(i->second);
  }
  mutex.unlock();
}
    
void B2BMediaStatistics::getReport(const AmArg &args, AmArg &ret)
{
  AmArg write_usage;
  mutex.lock();
  for (map<string, int>::iterator i = codec_write_usage.begin();
      i != codec_write_usage.end(); ++i) 
  {
    AmArg avp;
    avp["codec"] = i->first;
    avp["count"] = i->second;
    write_usage.push(avp);
  }
  
  AmArg read_usage;
  for (map<string, int>::iterator i = codec_read_usage.begin();
      i != codec_read_usage.end(); ++i) 
  {
    AmArg avp;
    avp["codec"] = i->first;
    avp["count"] = i->second;
    read_usage.push(avp);
  }
  mutex.unlock();

  ret["write"] = write_usage;
  ret["read"] = read_usage;
}

//////////////////////////////////////////////////////////////////////////////////

void AudioStreamData::initialize(AmB2BSession *session)
{
  stream = new AmRtpAudio(session, session->getRtpRelayInterface());
  stream->setRtpRelayTransparentSeqno(session->getRtpRelayTransparentSeqno());
  stream->setRtpRelayTransparentSSRC(session->getRtpRelayTransparentSSRC());
  force_symmetric_rtp = session->getRtpRelayForceSymmetricRtp();
  enable_dtmf_transcoding = session->getEnableDtmfTranscoding();
  session->getLowFiPLs(lowfi_payloads);
}

AudioStreamData::AudioStreamData(AmB2BSession *session):
  in(NULL), initialized(false),
  dtmf_detector(NULL), dtmf_queue(NULL),
  outgoing_payload(UNDEFINED_PAYLOAD),
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
  if (stream && stream->hasLocalSocket()){
    DBG("remove stream [%p] from RTP receiver\n", stream);
    AmRtpReceiver::instance()->removeStream(stream->getLocalSocket());
  }
}

void AudioStreamData::resumeStreamProcessing()
{
  if (stream && stream->hasLocalSocket()){
    DBG("resume stream [%p] into RTP receiver\n",stream);
    AmRtpReceiver::instance()->addStream(stream->getLocalSocket(), stream);
  }
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

  bool ok = true;

  // TODO: try to init only in case there are some payloads which can't be relayed
  stream->forceSdpMediaIndex(media_idx);
  if (stream->init(local_sdp, remote_sdp, force_symmetric_rtp) == 0) {
    stream->setPlayoutType(playout_type);
    initialized = true;
  } else {
    initialized = false;
    ERROR("stream initialization failed\n");
    // there still can be payloads to be relayed (if all possible payloads are
    // to be relayed this needs not to be an error)
    ok = false;
  }
  stream->setOnHold(muted);

  return ok;
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
  a_leg_on_hold(false), b_leg_on_hold(false)
{ 
}
 
void AmB2BMedia::changeSession(bool a_leg, AmB2BSession *new_session)
{
  mutex.lock();

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
      i->a.setRelayStream(i->b.getStream());
      i->b.setRelayStream(i->a.getStream());

      // needed to reinitialize audio processing in case the stream itself has
      // changed (FIXME: ugly again - see above and local/remote SDP might
      // already change since previous initialization!)
      if (a_leg) {
        i->a.initStream(playout_type, a_leg_local_sdp, a_leg_remote_sdp, i->media_idx);
        i->a.setDtmfSink(b);
        i->b.setDtmfSink(new_session);
      }
      else {
        i->b.initStream(playout_type, b_leg_local_sdp, b_leg_remote_sdp, i->media_idx);
        i->b.setDtmfSink(a);
        i->a.setDtmfSink(new_session);
      }
    }

    // return back for processing if needed
    if (processing_started) {
      i->a.resumeStreamProcessing();
      i->b.resumeStreamProcessing();
    }
  }

  if (a_leg) a = new_session;
  else b = new_session;

  mutex.unlock();
}

int AmB2BMedia::writeStreams(unsigned long long ts, unsigned char *buffer)
{
  int res = 0;
  mutex.lock();
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    if (i->a.writeStream(ts, buffer, i->b) < 0) { res = -1; break; }
    if (i->b.writeStream(ts, buffer, i->a) < 0) { res = -1; break; }
  }
  mutex.unlock();
  return res;
}

void AmB2BMedia::processDtmfEvents()
{
  mutex.lock();
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    i->a.processDtmfEvents();
    i->b.processDtmfEvents();
  }

  if (a) a->processDtmfEvents();
  if (b) b->processDtmfEvents();
  mutex.unlock();
}

void AmB2BMedia::sendDtmf(bool a_leg, int event, unsigned int duration_ms)
{
  mutex.lock();
  if(!audio.size())
    return;

  // send the DTMFs using the first available stream
  if(a_leg) {
    audio[0].a.sendDtmf(event,duration_ms);
  }
  else {
    audio[0].b.sendDtmf(event,duration_ms);
  }
  mutex.unlock();
}

void AmB2BMedia::clearAudio()
{
  mutex.lock();

  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    // remove streams from AmRtpReceiver first!
    i->a.stopStreamProcessing();
    i->b.stopStreamProcessing();
    i->a.clear();
    i->b.clear();
  }
  audio.clear();
  processing_started = false;

  // forget sessions to avoid using them once clearAudio is called
  a = NULL;
  b = NULL;

  mutex.unlock();
}

void AmB2BMedia::clearRTPTimeout()
{
  mutex.lock();

  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    i->a.clearRTPTimeout();
    i->b.clearRTPTimeout();
  }
  
  mutex.unlock();
}

void AmB2BMedia::createStreams(const AmSdp &sdp)
{
  AudioStreamIterator streams = audio.begin();
  vector<SdpMedia>::const_iterator m = sdp.media.begin();
  int idx = 0;

  // check already existing streams
  for (; (m != sdp.media.end()) && (streams != audio.end()); ++m, ++idx) {
    if (m->type != MT_AUDIO) continue;
    ++streams;
  }

  // create all the missing streams
  for (; m != sdp.media.end(); ++m, ++idx) {
    if (m->type != MT_AUDIO) continue;

    AudioStreamPair pair(a, b, idx);
    audio.push_back(pair);
  }
}


void AmB2BMedia::replaceConnectionAddress(AmSdp &parser_sdp, bool a_leg, const string &relay_address) 
{
  static const string void_addr("0.0.0.0");
  mutex.lock();

  SdpConnection orig_conn = parser_sdp.conn; // needed for the 'quick workaround' for non-audio media

  // place relay_address in connection address
  if (!parser_sdp.conn.address.empty() && (parser_sdp.conn.address != void_addr)) {
    parser_sdp.conn.address = relay_address;
    DBG("new connection address: %s",parser_sdp.conn.address.c_str());
  }

  // we need to create streams if they are not already created
  createStreams(parser_sdp);

  string replaced_ports;

  AudioStreamIterator streams = audio.begin();

  std::vector<SdpMedia>::iterator it = parser_sdp.media.begin();
  for (; (it != parser_sdp.media.end()) && (streams != audio.end()) ; ++it) {
  
    // FIXME: only audio streams are handled for now
    if (it->type != MT_AUDIO) {
      // quick workaround to allow direct connection of non-audio streams (i.e.
      // those which are not relayed or transcoded): propagate connection
      // address - might work but need not (to be tested with real clients
      // instead of simulators)
      if (it->conn.address.empty()) it->conn = orig_conn;
      continue;
    }

    if(it->port) { // if stream active
      if (!it->conn.address.empty() && (parser_sdp.conn.address != void_addr)) {
        it->conn.address = relay_address;
        DBG("new stream connection address: %s",it->conn.address.c_str());
      }
      try {
        if (a_leg) it->port = streams->a.getLocalPort();
        else it->port = streams->b.getLocalPort();
        replaced_ports += (streams != audio.begin()) ? int2str(it->port) : "/"+int2str(it->port);
      } catch (const string& s) {
        mutex.unlock();
        ERROR("setting port: '%s'\n", s.c_str());
        throw string("error setting RTP port\n");
      }
    }
    ++streams;
  }

  if (it != parser_sdp.media.end()) {
    // FIXME: create new streams here?
    WARN("trying to relay SDP with more media lines than "
        "relay streams initialized (%lu)\n", audio.size());
  }

  DBG("replaced connection address in SDP with %s:%s.\n",
      relay_address.c_str(), replaced_ports.c_str());
        
  mutex.unlock();
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
  if (!(have_a_leg_local_sdp &&
        have_a_leg_remote_sdp &&
        have_b_leg_local_sdp &&
        have_b_leg_remote_sdp)) return; // have not all info yet

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

    i->a.setDtmfSink(b);
    i->a.setRelayStream(i->b.getStream());
    i->a.initStream(playout_type, a_leg_local_sdp, a_leg_remote_sdp, i->media_idx);

    i->b.setDtmfSink(a);
    i->b.setRelayStream(i->a.getStream());
    i->b.initStream(playout_type, b_leg_local_sdp, b_leg_remote_sdp, i->media_idx);

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

bool AmB2BMedia::updateRemoteSdp(bool a_leg, const AmSdp &remote_sdp, RelayController *ctrl)
{
  bool ok = true;
  if (!ctrl) ctrl = &simple_relay_ctrl; // use default controller if none given

  mutex.lock();

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

  AudioStreamIterator streams = audio.begin();
  for (vector<SdpMedia>::const_iterator m = remote_sdp.media.begin(); m != remote_sdp.media.end(); ++m) {
    if (m->type != MT_AUDIO) continue;

    // initialize relay mask in the other(!) leg
    TRACE("relay payloads in direction %s\n", a_leg ? "B -> A" : "A -> B");
    if (a_leg) streams->b.setRelayPayloads(*m, ctrl);
    else streams->a.setRelayPayloads(*m, ctrl);
    ++streams;
  }

  onSdpUpdate();

  mutex.unlock();
  return ok;
}
    
bool AmB2BMedia::updateLocalSdp(bool a_leg, const AmSdp &local_sdp)
{
  bool ok = true;
  mutex.lock();
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

  mutex.unlock();
  return ok;
}

void AmB2BMedia::stop()
{
  clearAudio();
  if (isProcessingMedia()) 
    AmMediaProcessor::instance()->removeSession(this);
}

void AmB2BMedia::onMediaProcessingTerminated() 
{ 
  AmMediaSession::onMediaProcessingTerminated();
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
  mutex.lock();

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

  mutex.unlock();

  TRACE("hold SDP offer generated\n");

  return true;
}

void AmB2BMedia::setMuteFlag(bool a_leg, bool set)
{
  mutex.lock();
  if (a_leg) a_leg_muted = set;
  else b_leg_muted = set;
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    if (a_leg) i->a.mute(set);
    else i->b.mute(set);
  }
  mutex.unlock();
}

void AmB2BMedia::setFirstStreamInput(bool a_leg, AmAudio *in)
{
  mutex.lock();
  //for ( i != audio.end(); ++i) {
  if (!audio.empty()) {
    AudioStreamIterator i = audio.begin();
    if (a_leg) i->a.setInput(in);
    else i->b.setInput(in);
  }
  else ERROR("BUG: can't set %s leg's first stream input, no streams\n", a_leg ? "A": "B");
  // FIXME: start processing if not started and streams in this leg are fully initialized ?
  mutex.unlock();
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

  mutex.lock();

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

  mutex.unlock();
}
