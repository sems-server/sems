#include "AmB2BMedia.h"
#include "AmAudio.h"
#include "amci/codecs.h"
#include <string.h>
#include "AmB2BSession.h"
#include "AmRtpReceiver.h"

#include <algorithm>

#define TRACE DBG
#define UNDEFINED_PAYLOAD (-1)

static B2BMediaStatistics b2b_stats;

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

AudioStreamData::AudioStreamData(AmB2BSession *session):
  in(NULL), initialized(false),
  dtmf_detector(NULL), dtmf_queue(NULL),
  outgoing_payload(UNDEFINED_PAYLOAD)
{
  stream = new AmRtpAudio(session, session->getRtpRelayInterface());
  stream->setRtpRelayTransparentSeqno(session->getRtpRelayTransparentSeqno());
  stream->setRtpRelayTransparentSSRC(session->getRtpRelayTransparentSSRC());
  force_symmetric_rtp = session->getRtpRelayForceSymmetricRtp();
}

void AudioStreamData::clear()
{
  resetStats();
  if (in) {
    in->close();
    delete in;
    in = NULL;
  }
  if (stream) {
    delete stream;
    stream = NULL;
  }
  if (dtmf_detector) {
    delete dtmf_detector;
    dtmf_detector = NULL;
  }
  if (dtmf_queue) {
    delete dtmf_queue;
    dtmf_queue = NULL;
  }
}

void AudioStreamData::stopStreamProcessing()
{
  if (stream->hasLocalSocket())
    AmRtpReceiver::instance()->removeStream(stream->getLocalSocket());
}

void AudioStreamData::resumeStreamProcessing()
{
  if (stream->hasLocalSocket())
    AmRtpReceiver::instance()->addStream(stream->getLocalSocket(), stream);
}

void AudioStreamData::setStreamRelay(const SdpMedia &m, AmRtpStream *other)
{
  // We are in locked section, so the stream can not change under our hands
  // remove the stream from processing to avoid changing relay params under the
  // hands of an AmRtpReceiver process.
  // Updating relay information is not done so often so this might be better
  // solution than using additional locking within AmRtpStream.
  stopStreamProcessing();

  if ((m.payloads.size() > 0) && other) {
    PayloadMask mask;

    // walk through the media line and add all payload IDs to the bit mask
    for (std::vector<SdpPayload>::const_iterator i = m.payloads.begin(); 
        i != m.payloads.end(); ++i) 
    {
      mask.set(i->payload_type);
      TRACE("marking payload %d for relay\n", i->payload_type);
    }

    stream->enableRtpRelay(mask, other);
  }
  else {
    // nothing to relay
    stream->disableRtpRelay();
  }

  resumeStreamProcessing();
}
    
bool AudioStreamData::initStream(AmSession *session, 
    PlayoutType playout_type,
    AmSdp &local_sdp, AmSdp &remote_sdp, int media_idx)
{
  bool ok = true;

  // remove from processing to safely update the stream
  stopStreamProcessing();

  // TODO: try to init only in case there are some payloads which can't be relayed
  stream->forceSdpMediaIndex(media_idx);
  if (stream->init(local_sdp, remote_sdp, force_symmetric_rtp) == 0) {
    stream->setPlayoutType(playout_type);
    initialized = true;
    if (session->isDtmfDetectionEnabled()) {
      dtmf_detector = new AmDtmfDetector(session);
      dtmf_queue = new AmDtmfEventQueue(dtmf_detector);
      dtmf_detector->setInbandDetector(AmConfig::DefaultDTMFDetector, stream->getSampleRate());
    }
  } else {
    ERROR("stream initialization failed\n");
    // there still can be payloads to be relayed, so eat the error, just don't
    // set the initialized flag to avoid problems later on
    //??ok = false;
  }

  // return back for processing if needed (if stream->init failed it still can
  // be possible to relay some payloads, so we should return for processing
  // anyway)
  resumeStreamProcessing();

  return ok;
}

bool AudioStreamData::resetInitializedStream()
{
  resetStats();
  if (initialized) {
    initialized = false;

    if (dtmf_detector) {
      delete dtmf_detector;
      dtmf_detector = NULL;
    }
    if (dtmf_queue) {
      delete dtmf_queue;
      dtmf_queue = NULL;
    }
    return true;
  }
  return false;
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
          if (dtmf_queue) dtmf_queue->putDtmfAudio(buffer, got, ts);
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

//////////////////////////////////////////////////////////////////////////////////

AmB2BMedia::AmB2BMedia(AmB2BSession *_a, AmB2BSession *_b): 
  ref_cnt(0), // everybody who wants to use must add one reference itselves
  a(_a), b(_b),
  callgroup(AmSession::getNewId()),
  playout_type(ADAPTIVE_PLAYOUT)
  //playout_type(SIMPLE_PLAYOUT)
{ 
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

void AmB2BMedia::replaceConnectionAddress(AmSdp &parser_sdp, bool a_leg, const string &relay_address) 
{
  static const string void_addr("0.0.0.0");
  mutex.lock();

  // place relay_address in connection address
  if (!parser_sdp.conn.address.empty() && (parser_sdp.conn.address != void_addr)) {
    parser_sdp.conn.address = relay_address;
    DBG("new connection address: %s",parser_sdp.conn.address.c_str());
  }

  string replaced_ports;

  AudioStreamIterator streams = audio.begin();

  std::vector<SdpMedia>::iterator it = parser_sdp.media.begin();
  for (; (it != parser_sdp.media.end()) && (streams != audio.end()) ; ++it) {
  
    // FIXME: only audio streams are handled for now
    if (it->type != MT_AUDIO) continue;

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
      
bool AmB2BMedia::resetInitializedStreams(bool a_leg)
{
  bool res = false;
  for (AudioStreamIterator i = audio.begin(); i != audio.end(); ++i) {
    if (a_leg) res = res || i->a.resetInitializedStream();
    else res = res || i->b.resetInitializedStream();
  }
  return res;
}

bool AmB2BMedia::updateStreams(bool a_leg, bool init_relay, bool init_transcoding)
{
  unsigned stream_idx = 0;
  unsigned media_idx = 0;
  bool ok = true;

  TRACE("update streams for changed %s-leg SDP:%s%s\n", a_leg ? "A" : "B", 
      init_relay ? " relay": "",
      init_transcoding ? " transcoding": "");

  AmSdp *sdp;
  if (a_leg) sdp = &a_leg_remote_sdp;
  else sdp = &b_leg_remote_sdp;

  for (SdpMediaIterator m = sdp->media.begin(); m != sdp->media.end(); ++m, ++media_idx) {
    if (m->type != MT_AUDIO) continue;

    // create pair of Rtp streams if it doesn't exist yet
    if (stream_idx >= audio.size()) {
      AudioStreamPair pair(a, b);
      audio.push_back(pair);
      stream_idx = audio.size() - 1;
    }
    AudioStreamPair &pair = audio[stream_idx];

    if (init_relay) {
      // initialize RTP relay stream and payloads in the other leg (!)
      // Payloads present in this SDP can be relayed directly by the AmRtpStream
      // of the other leg to the AmRtpStream of this leg.
      if (a_leg) pair.b.setStreamRelay(*m, pair.a.getStream());
      else pair.a.setStreamRelay(*m, pair.b.getStream());
    }

    // initialize the stream for current leg if asked to do so
    if (init_transcoding) {
      if (a_leg) {
        ok = ok && pair.a.initStream(a, playout_type, a_leg_local_sdp, a_leg_remote_sdp, media_idx);
      } else {
        ok = ok && pair.b.initStream(b, playout_type, b_leg_local_sdp, b_leg_remote_sdp, media_idx);
      }
    }

    stream_idx++;
  }

  return ok;
}

bool AmB2BMedia::updateRemoteSdp(bool a_leg, const AmSdp &remote_sdp)
{
  bool ok = true;
  mutex.lock();

  bool initialize_streams;

  if (a_leg) a_leg_remote_sdp = remote_sdp;
  else b_leg_remote_sdp = remote_sdp;

  if (resetInitializedStreams(a_leg)) { 
    // needed to reinitialize later 
    // streams were initialized before and the local SDP is still not up-to-date
    // otherwise streams would by alredy reset
    initialize_streams = false;
  }
  else {
    // streams were not initialized, we should initialize them if we have
    // local SDP already
    if (a_leg) initialize_streams = a_leg_local_sdp.media.size() > 0;
    else initialize_streams = b_leg_local_sdp.media.size() > 0;
  }

  ok = ok && updateStreams(a_leg, 
      true /* needed to initialize relay stuff on every remote SDP change */, 
      initialize_streams);

  if (ok) updateProcessingState(); // start media processing if possible

  mutex.unlock();
  return ok;
}
    
bool AmB2BMedia::updateLocalSdp(bool a_leg, const AmSdp &local_sdp)
{
  bool ok = true;
  mutex.lock();
  // streams should be created already (replaceConnectionAddress called
  // before updateLocalSdp uses/assignes their port numbers)

  if (a_leg) a_leg_local_sdp = local_sdp;
  else b_leg_local_sdp = local_sdp;
 
  bool initialize_streams;
  if (resetInitializedStreams(a_leg)) { 
    // needed to reinitialize later 
    // streams were initialized before and the remote SDP is still not up-to-date
    // otherwise streams would by alredy reset
    initialize_streams = false;
  }
  else {
    // streams were not initialized, we should initialize them if we have
    // remote SDP already
    if (a_leg) initialize_streams = a_leg_remote_sdp.media.size() > 0;
    else initialize_streams = b_leg_remote_sdp.media.size() > 0;
  }

  ok = ok && updateStreams(a_leg, 
      false /* local SDP change has no effect on relay */, 
      initialize_streams);

  if (ok) updateProcessingState(); // start media processing if possible

  mutex.unlock();
  return ok;
}

void AmB2BMedia::updateProcessingState()
{
  // once we send local SDP to the other party we have to expect RTP so we
  // should start RTP processing now (though streams in opposite direction need
  // not to be initialized yet) ... FIXME: but what to do with data then? =>
  // wait for having all SDPs ready

  // FIXME: only if there are initialized streams?
  if (a_leg_local_sdp.media.size() &&
      a_leg_remote_sdp.media.size() &&
      b_leg_local_sdp.media.size() &&
      b_leg_remote_sdp.media.size() &&
      !isProcessingMedia()) 
  {
    ref_cnt++; // add reference (hold by AmMediaProcessor)
    AmMediaProcessor::instance()->addSession(this, callgroup);
  }
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
