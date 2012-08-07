#ifndef __B2BMEDIA_H
#define __B2BMEDIA_H

#include "AmAudio.h"
#include "AmRtpStream.h"
#include "AmRtpAudio.h"
#include "AmMediaProcessor.h"
#include "AmDtmfDetector.h"

#include <map>

class AmB2BSession;

class B2BMediaStatistics
{
  private:
    std::map<string, int> codec_write_usage;
    std::map<string, int> codec_read_usage;
    AmMutex mutex;

  public:
    void reportCodecWriteUsage(string &dst);
    void reportCodecReadUsage(string &dst);
    void getReport(const AmArg &args, AmArg &ret);

    static B2BMediaStatistics *instance();
    void incCodecWriteUsage(const string &codec_name);
    void decCodecWriteUsage(const string &codec_name);
    void incCodecReadUsage(const string &codec_name);
    void decCodecReadUsage(const string &codec_name);
};

/** \brief Class for computing mask of payloads to relay
 *
 * */
class RelayController {
  public:
    virtual void computeRelayMask(const SdpMedia &m, bool &enable, PayloadMask &mask) = 0;
    virtual ~RelayController() { }
};

/** \brief Storage for several data items required to be held with one RTP
 * stream for B2B media processing.
 *
 * It has shown that there are more items to be remembered with one RTP stream
 * so this class is an attempt to encapsulate them together and make the code
 * better manageable (less duplicates - the same things were often done for
 * A leg stuff and then for B leg stuff). */

class AudioStreamData {
  private:
    /** The RTP stream itself.
     *
     * Audio only for now. */
    AmRtpAudio *stream;

    /** Non-stream input (required for music on hold for example). */
    AmAudio *in;

    /** Flag set when streams in A/B leg are correctly initialized (for
     * transcoding purposes). */
    bool initialized;

    /** remembered value of the option from AmB2BSession */
    bool force_symmetric_rtp;

    /** Enables inband dtmf detection */
    bool enable_dtmf_transcoding;

    /** Low fidelity payloads for which inband DTMF transcoding should be used */
    vector<SdpPayload> lowfi_payloads;
  
    /** DTMF detector used by dtmf_queue */
    AmDtmfDetector *dtmf_detector;
    
    /** Queue for handling raw DTMF events. 
     *
     * It is rather quick hack to make B2B media working with current code.
     * Each stream can use different sampling rate and thus DTMF detection need
     * to be done independently for each stream. */
    AmDtmfEventQueue *dtmf_queue;

    // for performance monitoring
    int outgoing_payload;
    int incoming_payload;
    string outgoing_payload_name;
    string incoming_payload_name;
    void updateSendStats();
    void updateRecvStats(AmRtpStream *s);
    void resetStats();

  public:
    /** Creates data based on associated signaling leg data. */
    AudioStreamData(AmB2BSession *session);

    /** Frees all allocated data. 
     *
     * Stream and its peer (relay stream) must be removed from processing before
     * calling this method! This method doesn't call stopStreamProcessing()
     * itself because of the stream here is used as relay stream in the other
     * leg. Before freeing current stream the other one has to be removed from
     * processing as well. 
     *
     * Please note that the "in" member is freed - this need not to be the right
     * thing but this will show once it will be really used. */
    void clear();

    /** Removes stream from processing by AmRtpReceiver. */
    void stopStreamProcessing();
    
    /** Returns stream from processing by AmRtpReceiver if it was already there. */
    void resumeStreamProcessing();

    /** Set relay stream and payload IDs to be relayed.
     *
     * Removes the stream from AmRtpReceiver before updating and returns it back
     * once done. */
    void setStreamRelay(const SdpMedia &m, AmRtpStream *other, RelayController &rc);

    /** Initializes RTP stream with local and remote media (needed for
     * transcoding). 
     *
     * Returns false if the initialization failed. */
    bool initStream(AmDtmfSink* dtmf_sink, PlayoutType playout_type, AmSdp &local_sdp, AmSdp &remote_sdp, int media_idx);

    /** Discards initialization flag and DTMF related members if the stream was
     * already initialized. In such case true is returned. If the stream wasn't
     * initialized yet this method returns false.
     * 
     * Each time SDP is changed the stream has to be reinitialized with new one
     * (needed to have SDP of both sides to that). */
    bool resetInitializedStream();

    /** Processes raw DTMF events in own queue. */
    void processDtmfEvents() { if (dtmf_queue) dtmf_queue->processEvents(); }

    /** Sends DTMF */
    void sendDtmf(int event, unsigned int duration_ms);

    /** Writes data to won stream. Data are read either from local alternative
     * input (in) or from stream given by src parameter. 
     *
     * Buffer is just space used to read data before writing them,
     * AmMediaProcessor buffer should be propagated here (see AmMediaSession) */
    int writeStream(unsigned long long ts, unsigned char *buffer, AudioStreamData &src);

    // --- helper methods propagating our private member to outside world ---

    void clearRTPTimeout() { if (stream) stream->clearRTPTimeout(); }
    int getLocalPort() { if (stream) return stream->getLocalPort(); else return 0; }
    AmRtpAudio *getStream() { return stream; }
    bool isInitialized() { return initialized; }
};

/** \brief Class for control over media relaying and transcoding in a B2B session.
 *
 * This class manages RTP streams of both call legs, configures AmRtpStream
 * relaying functionality and in case media needs to be transcoded its
 * AmMediaSession interface implementation reads data from RTP streams in one
 * leg and writes them to appropriate RTP streams of the other leg.
 *
 * From the signaling part of the session (AmB2BSession instance for caller and
 * for callee) it needs to be informed about local and remote SDP in each leg
 * via updateLocalSdp() and updateRemoteSdp() methods.
 *
 * Signaling parts of the session (caller and callee) needs to update outgoing
 * SDP bodies by local address and ports of RTP streams using
 * replaceConnectionAddress() method.
 *
 * Because generating B2B SDP is no more based on AmSession's offer/answer
 * mechanism but we relay remote's SDP with just slight changes (some payloads
 * filtered out, some payloads added before forwarding) we don't need to
 * remember payload ID mapping any more (local to remote). Payload IDs should be
 * generated correctly by the remote party and we don't need to change it when
 * relaying RTP packets.
 *
 * TODO:
 *  - handle "on hold" streams - probably should be controlled by signaling
 *    (AmB2BSession) - either we should not send audio or we should send hold
 *    music
 *
 *    Currently problematic, setting AmRtpStream::active to false in
 *    AmRtpStream::init doesn't help always - if some RTP packets arrive later
 *    than media session is updated the stream remains 'active' (verified with
 *    SPA 942 and twinkle)
 *
 *  - non-audio streams - list of AmRtpStream pairs which can be just relayed
 *
 *  - reference counting using atomic variables instead of locking
 *
 *  - RTCP
 *
 *  - independent clear of one call leg (to be able to connect another callleg)
 *
 *  - correct sampling periods when relaying/transcoding according to values
 *    advertised in local SDP (i.e. the relayed one)
 *
 *  - Is non-transparent SSRC & seq. no needed if some payloads can be transcoded and
 *    some relayed? Couldn't be confusing to have transparent ones for relayed but our
 *    own SSRC & seq. no for transcoded payloads? [wireshark seems to be
 *    confused] => disable transparent SSRC/seq.no if there are payloads for transcoding?
 *
 *    Note that forcing our own SSRC can break things if the incomming RTP stream
 *    comes from a source mixing audio from different sources - in that case we should
 *    prefer to propagate SSRC (i.e. use transparent SSRC)!
 *
 *  - we should use our seq. numbers if transcoding is possible but propagate
 *    lost packets (i.e. remember the difference between received seq. numbers and
 *    sent ones and for the transcoding purpose use seq. number = max. already
 *    used number + 1)
 *
 *  - configurable playout buffer type (from a test with transcoding PCMA -> PCMU
 *    between SPA 942 and 941 it seems that at simulated 20% packet loss is the
 *    audio quality better with ADAPTIVE_PLAYOUT in comparison with SIMPLE_PLAYOUT
 *    but can't say it is really big differece)
 *
 *  - In-band DTMF detection within relayed payloads not supported yet. Do we
 *    need it?
 */

class AmB2BMedia: public AmMediaSession
{
  private:
    /* remembered both legs of the B2B call
     * currently required for DTMF processing and used for reading RTP relay
     * parameters (rtp_relay_transparent_seqno, rtp_relay_transparent_ssrc,
     * rtp_interface) */
    AmB2BSession *a, *b;

    /** Pair of audio streams with the possibility to use given audio as input
     * instead of the other stream. */
    struct AudioStreamPair {
      AudioStreamData a, b;
      AudioStreamPair(AmB2BSession *_a, AmB2BSession *_b): a(_a), b(_b) { }
    };

    typedef std::vector<AudioStreamPair>::iterator AudioStreamIterator;
    typedef std::vector<SdpMedia>::iterator SdpMediaIterator;

    /** Callgroup reqired by AmMediaProcessor to distinguish
     * AmMediaProcessorThread which should take care about media session.
     *
     * It might be handy to use own generated callgroup independent on caller's
     * and callee's one. (FIXME: not sure if it is worth consumed additional
     * resources). */
    string callgroup;
      
    // needed for updating relayed payloads
    AmSdp a_leg_local_sdp, a_leg_remote_sdp;
    AmSdp b_leg_local_sdp, b_leg_remote_sdp;

    AmMutex mutex;
    int ref_cnt;

    /** Playout type describes what kind of buffering will be used for audio
     * streams. Please note that ADAPTIVE_PLAYOUT requires some kind of
     * detection if there is really data to read from the buffer because the get
     * function always return something regardless if something was written into
     * or not. 
     */
    PlayoutType playout_type;

    std::vector<AudioStreamPair> audio;

    /** Starts media processing if have all required information. */
    void updateProcessingState();

    /** Clears a_initialized/b_initialized flag if already initialized. Returns
     * true if something was really cleared. */
    bool resetInitializedStreams(bool a_leg);
    bool resetInitializedStream(AudioStreamData &data);

    /** Mark streams in given leg as uninitialized (needed for in-dialog media
     * updates) */
    void clearStreamInitialization(bool a_leg);

    /** Updates streams in given leg. 
     *
     * Creates them if they don't exist and initializes if they need to be
     * initialized. 
     *
     * Method initializes relay & transcoding settings if told to do so. 
     *
     * Returns false if update failed. */
    bool updateStreams(bool a_leg, bool init_relay, bool init_transcoding, RelayController &rc);

    /** initialize given stream (prepares for transcoding) */
    void initStream(AudioStreamData &data, AmSession *session, AmSdp &local_sdp, AmSdp &remote_sdp, int media_idx);

  public:
    AmB2BMedia(AmB2BSession *_a, AmB2BSession *_b);

    //void updateRelayPayloads(bool a_leg, const AmSdp &local_sdp, const AmSdp &remote_sdp);

    /** Adds a reference.
     *
     * Instance of this object is created with reference counter set to zero.
     * Thus if somebody wants to hold a reference it must call addReference()
     * explicitly after construction! */
    void addReference() { mutex.lock(); ref_cnt++; mutex.unlock(); }

    /** Releases reference.
     *
     * Returns true if this was the last reference and the object should be
     * destroyed (call "delete this" here?) */
    bool releaseReference() { mutex.lock(); int r = --ref_cnt; mutex.unlock(); return (r == 0); }

    // ----------------- SDP manipulation & updates -------------------

    /** Replace connection address and ports within SDP.
     *
     * Throws an exception (string) in case of error. (FIXME?) */
    void replaceConnectionAddress(AmSdp &parser_sdp, bool a_leg, const string &relay_address);

    /** Store remote SDP for given leg and update media session appropriately. 
     * Returns false if update failed. */
    bool updateRemoteSdp(bool a_leg, const AmSdp &remote_sdp, RelayController *ctrl = NULL);
    
    /** Store local SDP for given leg and update media session appropriately. 
     * Returns false if update failed. */
    bool updateLocalSdp(bool a_leg, const AmSdp &local_sdp);

    /** Clear audio and stop processing. 
     *
     * Releases all RTP streams and removes itself from media processor if still
     * there. */
    void stop();

    // ---- AmMediaSession interface for processing audio in a standard way ----

    /** Should read from all streams before writing to the other streams. 
     * 
     * Because processing is driven by destination stream (i.e. we don't read
     * anything unless the destination stream is ready to send something - see
     * sendIntReached()) all processing is done in writeStreams */
    virtual int readStreams(unsigned long long ts, unsigned char *buffer) { return 0; }
    
    /** Read and write all RTP streams if data are to be written (see
     * readStreams()). */
    virtual int writeStreams(unsigned long long ts, unsigned char *buffer);

    /** Calls processDtmfEvent on both AmB2BSessions for which this AmB2BMedia
     * instance manages media. */
    virtual void processDtmfEvents();

    /** Sends DTMF using the given call leg */
    void sendDtmf(bool a_leg, int event, unsigned int duration_ms);

    /** Release all RTP streams of both legs and both AmB2BSessions as well. 
     *
     * Though readStreams(), writeStreams() or processDtmfEvents() can be called
     * after call to clearAudio, they will do nothing because all relevant
     * information will be rlready eleased. */
    virtual void clearAudio();

    /** Clear RTP timeout of all streams in both call legs. */
    virtual void clearRTPTimeout();
    
    /** Callback function called once media processor releases this instance
     * from processing loop.
     * 
     * Deletes itself if there are no other references! FIXME: might be
     * returning something like "release me" and calling delete from media
     * processor would be better? */
    virtual void onMediaProcessingTerminated();

};

#endif
