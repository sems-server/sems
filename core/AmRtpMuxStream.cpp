#include "AmRtpMuxStream.h"
#include "AmConfig.h"
#include "AmRtpReceiver.h"

#include "log.h"

#include "sip/ip_util.h"
#include "sip/wheeltimer.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>       
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MUX_HDR_SETUP_LENGTH      5
#define MUX_HDR_COMPRESSED_LENGTH 3

#define RTP_MUX_MAX_FRAME_SIZE 256

AmRtpMuxStream::AmRtpMuxStream()
  : AmRtpStream(NULL, 0)
{
  // fixme: init interface _if
}

AmRtpMuxStream::~AmRtpMuxStream()
{
}

u_int16 get_rtp_hdr_len(const rtp_hdr_t* hdr) {
  // fixme: check overflows
  unsigned int hdr_len = sizeof(rtp_hdr_t) + (hdr->cc*4);
  if(hdr->x != 0) {
    //  skip extension header
    hdr_len +=
      ntohs(((rtp_xhdr_t*) (hdr + hdr_len))->len)*4;
  }
  // if ((unsigned char*)(hdr + hdr_len) > (p.getBuffer()+s)) {
  //   ERROR("RTP packet with CC and xtension header too long!\n");
  // }
  return hdr_len;
}

unsigned int calculate_ts_increment(rtp_mux_hdr_setup_t* setup_hdr) {
  return setup_hdr->u ? setup_hdr->ts_inc * RTP_MUX_HDR_TS_MULTIPLIER_HIGH : setup_hdr->ts_inc * RTP_MUX_HDR_TS_MULTIPLIER_LOW;
}

void AmRtpMuxStream::recvPacket(int fd, unsigned char* pkt, size_t len) {
  AmRtpPacket p;
  int s = p.recv(l_sd);

  if (s<0) {
    DBG("ERROR receiving packet on fd %d\n", fd);
    return;
  }

  // DBG("received packet of length %d\n", s);
  if (!s) return;

  unsigned char* frame_ptr = p.getBuffer();
  while ((unsigned char*) frame_ptr < p.getBuffer()+s) {
    rtp_mux_hdr_t* mux_hdr = (rtp_mux_hdr_t*)frame_ptr;

    // fixme: handle situation where no setup frame received previously
    MuxStreamState& state = recv_streamstates[mux_hdr->sid];

    if (mux_hdr->t == RTP_MUX_HDR_TYPE_SETUP) {
      rtp_mux_hdr_setup_t* mux_hdr_setup = (rtp_mux_hdr_setup_t*)frame_ptr;
      // DBG("received setup packed on stream ID %u for port %u, ts_increment %u, len %u hdr_len %u",
      // 	  mux_hdr_setup->sid, ntohs(mux_hdr_setup->dstport), mux_hdr_setup->ts_inc, mux_hdr->len, state.rtp_hdr_len);

      // save params
      state.dstport = ntohs(mux_hdr_setup->dstport);
      state.ts_increment = calculate_ts_increment(mux_hdr_setup);

      // fixme: handle RTCP

      // save RTP header
      rtp_hdr_t* hdr = (rtp_hdr_t*)(frame_ptr + sizeof(rtp_mux_hdr_setup_t));

      state.rtp_hdr_len = get_rtp_hdr_len(hdr);
      memcpy(state.rtp_hdr, hdr, state.rtp_hdr_len);

      // DBG("setup packet for port %u ts_inc %u len %u hdr_len %u\n",
      //  	  state.dstport, state.ts_increment, mux_hdr->len, state.rtp_hdr_len);

      AmRtpReceiver::instance()->recvdPacket(getLocalPort(), state.dstport, (unsigned char*)hdr, mux_hdr->len);

      frame_ptr+= mux_hdr->len + sizeof(rtp_mux_hdr_setup_t) /* skip hdr + frame*/;
    } else {

      // decompress header
      unsigned char rtp_pkt[MAX_RTP_PACKET_LEN];
      decompress((rtp_mux_hdr_compressed_t*)frame_ptr, state.ts_increment, (const rtp_hdr_t*)state.rtp_hdr, rtp_pkt);
      state.rtp_hdr_len = get_rtp_hdr_len((rtp_hdr_t*)rtp_pkt);
      // save header
      memcpy(state.rtp_hdr, rtp_pkt, state.rtp_hdr_len); 
      // copy payload
      memcpy(rtp_pkt + state.rtp_hdr_len, frame_ptr+sizeof(rtp_mux_hdr_compressed_t), mux_hdr->len);

      // DBG("received compressed packet for dstport %u of size %u\n", state.dstport, state.rtp_hdr_len + mux_hdr->len);
      AmRtpReceiver::instance()->recvdPacket(getLocalPort(), state.dstport, rtp_pkt, state.rtp_hdr_len + mux_hdr->len);

      frame_ptr+= mux_hdr->len + sizeof(rtp_mux_hdr_compressed_t) /* skip hdr + frame*/;
    }
  }
}

int _AmRtpMuxSender::send(unsigned char* buffer, unsigned int b_size,
			  const string& remote_ip, unsigned short remote_port, unsigned short rtp_dst_port) {
  if (remote_ip.empty() || !remote_port) {
    ERROR("internal error: need RTP MUX remote IP:port to send to\n");
    return -1;
  }

  send_queues_mut.lock();
  int res = send_queues[remote_ip+":"+int2str(remote_port)].send(buffer, b_size, remote_ip, remote_port, rtp_dst_port);
  send_queues_mut.unlock();
  return res;
}

void _AmRtpMuxSender::close(const string& remote_ip, unsigned short remote_port, unsigned short rtp_dst_port) {
  DBG("RTP MUX: closing queue to %s:%u for port %u\n", remote_ip.c_str(), remote_port, rtp_dst_port);
  send_queues_mut.lock();
  send_queues[remote_ip+":"+int2str(remote_port)].close(remote_ip, remote_port, rtp_dst_port);
  send_queues_mut.unlock();
}

MuxStreamQueue::MuxStreamQueue()
  : l_sd(0), end_ptr(buf), oldest_frame_i(false), mux_packet_id(0), is_setup(false)
{
  memset(&r_saddr,0,sizeof(struct sockaddr_storage));
  memset(&l_saddr,0,sizeof(struct sockaddr_storage));
}

int MuxStreamQueue::init(const string& _remote_ip, unsigned short _remote_port) {
  if (l_sd)
    close();

  remote_ip = _remote_ip;
  remote_port = _remote_port;

  int l_if = 0;
  map<string,unsigned short>::iterator n_it=AmConfig::RTP_If_names.find(AmConfig::RtpMuxOutInterface);
  if (n_it == AmConfig::RTP_If_names.end()) {
    // WARN("rtp_mux_interface '%s' not found. Using default RTP interface instead\n", AmConfig::RtpMuxOutInterface.c_str());
  } else {
    l_if = n_it->second;
  }

  if (!am_inet_pton(AmConfig::RTP_Ifs[l_if].LocalIP.c_str(), &l_saddr)) {
    ERROR("MuxStreamQueue::init: Invalid IP address: %s\n", AmConfig::RTP_Ifs[l_if].LocalIP.c_str());
    return -1;
  }

  int retry = 10;
  unsigned short port = 0;
  for(;retry; --retry){

    int sd=0;
    if((sd = socket(l_saddr.ss_family, SOCK_DGRAM, 0)) == -1) {
      ERROR("%s\n",strerror(errno));
      return -2;
    } 

    int true_opt = 1;
    if(ioctl(sd, FIONBIO , &true_opt) == -1){
      ERROR("%s\n",strerror(errno));
      ::close(l_sd);
      return -2;
    }

    l_sd = sd;

    port = AmConfig::RTP_Ifs[l_if].getNextRtpPort();

    am_set_port(&l_saddr,port);
    if(bind(l_sd,(const struct sockaddr*)&l_saddr,SA_len(&l_saddr))) {
      ::close(l_sd);
      l_sd = 0;
      DBG("bind: %s\n",strerror(errno));		
      continue;
    }

    break;
  }

  if (!retry){
    ERROR("RTP MUX: could not find a free RTP port\n");
    return -1;
  }

  int true_opt = 1;
  // rco: does that make sense after bind() ????
  if(setsockopt(l_sd, SOL_SOCKET, SO_REUSEADDR,
		(void*)&true_opt, sizeof (true_opt)) == -1) {

    ERROR("%s\n",strerror(errno));
    ::close(l_sd);
    l_sd = 0;
    return -1;
  }

  if (!am_inet_pton(remote_ip.c_str(), &r_saddr)) {
    WARN("Address not valid (host: %s).\n", remote_ip.c_str());
    return -1;
  }
  am_set_port(&r_saddr, remote_port);

  DBG("RTP MUX: opened connection <%s>:%u --> %s:%u\n",
      AmConfig::RtpMuxOutInterface.empty() ? "default" : AmConfig::RtpMuxOutInterface.c_str(), port, remote_ip.c_str(), remote_port);
  return 0;
}

void MuxStreamQueue::close() {
  ::close(l_sd);
  l_sd = 0;
}

crc_t calc_crc4(u_int32 ts) {
  crc_t res = crc_init();
  res = crc_update(res, &ts, sizeof(u_int32));
  return crc_finalize(res);
}

int MuxStreamQueue::send(unsigned char* buffer, unsigned int b_size,
			 const string& _remote_ip, unsigned short _remote_port,
			 unsigned short rtp_dst_port) {
  if (b_size > RTP_MUX_MAX_FRAME_SIZE) {
    ERROR("RTP MUX: trying to send packet of too large size (b_size = %u, max size %u)\n",
	  b_size, RTP_MUX_MAX_FRAME_SIZE);
    return -1;
  }

  // initialize UDP connection to MUX IP:port
  if ((remote_ip != _remote_ip) || (remote_port != _remote_port)) {
    if (init(_remote_ip, _remote_port))
      return -1;
  }

  // find stream_id for rtp_dst_port
  unsigned char stream_id = 0;
  map<unsigned short, unsigned char>::iterator mux_id_it = stream_ids.find(rtp_dst_port);
  if (mux_id_it == stream_ids.end()) {
    // set up a new stream_id for port rtp_dst_port
    for (stream_id=0;stream_id<=256;stream_id++) {
      if (streamstates.find(stream_id)==streamstates.end())
	break;
    }
    if (stream_id==256) {
      ERROR("trying to send more than 256 streams on RTP MUX connection to  %s:%u\n", remote_ip.c_str(), remote_port);
      // or: find the oldest stream and overwrite that one
      return -1;
    }
    DBG("RTP MUX: setting up new MUX stream for dst port %u as stream_id %u on RTP MUX %s:%u\n",
	rtp_dst_port, stream_id, remote_ip.c_str(), remote_port);
    stream_ids[rtp_dst_port]=stream_id;
  } else {
    stream_id = mux_id_it->second;
  }

  MuxStreamState& stream_state = streamstates[stream_id];

  bool send_setup_frame = false;
  // RTP header changed -> send setup frame
  if (rtp_hdr_changed((rtp_hdr_t*)stream_state.rtp_hdr, (rtp_hdr_t*)buffer)) {
    // DBG("RTP hdr changed - sending setup frame\n");
    send_setup_frame = true;
    stream_state.setup_frame_ctr = MUX_SETUP_FRAME_REPEAT;
    stream_state.ts_increment = DEFAULT_TS_INCREMENT;
  }

  // still in n initial setup period?
  if (!send_setup_frame &&
      stream_state.setup_frame_ctr && (stream_state.last_mux_packet_id != mux_packet_id)) {
    stream_state.setup_frame_ctr--;
    send_setup_frame = true;
  }

  // periodic update?
  if (!send_setup_frame &&
      (stream_state.last_mux_packet_id != mux_packet_id) && 
      wheeltimer::instance()->interval_elapsed(stream_state.last_setup_frame_ts, MUX_PERIODIC_SETUP_FRAME_MS)) {
    send_setup_frame = true;
  }

  rtp_hdr_t* rtp_hdr = (rtp_hdr_t*)buffer;
  u_int16 rtp_hdr_len = get_rtp_hdr_len(rtp_hdr);
  u_int16 data_len = b_size - rtp_hdr_len;
  rtp_hdr_t* old_rtp_hdr = (rtp_hdr_t*)stream_state.rtp_hdr;

  // length has changed (e.g. CSRC, extension hdr)
  if (rtp_hdr_len != stream_state.rtp_hdr_len) {
    send_setup_frame = true;
  }

  // ts_increment right?
  if (!send_setup_frame &&
      ntohl(rtp_hdr->ts) != (ntohl(old_rtp_hdr->ts) + stream_state.ts_increment)) {
    u_int16 old_ts_increment = stream_state.ts_increment;
    stream_state.ts_increment = ntohl(rtp_hdr->ts) - ntohl(old_rtp_hdr->ts);
    DBG("corrected ts_increment %u -> %u; old_hdr_ts %u, ts = %u (FIXME: TS resync?)\n",
	old_ts_increment, stream_state.ts_increment, ntohl(old_rtp_hdr->ts), ntohl(rtp_hdr->ts));
    send_setup_frame = true;
  }

  if (!send_setup_frame) {
    rtp_mux_hdr_compressed_t* rtp_mux_hdr_compressed = (rtp_mux_hdr_compressed_t*)end_ptr;
    rtp_mux_hdr_compressed->t = RTP_MUX_HDR_TYPE_COMPRESSED;
    rtp_mux_hdr_compressed->sid = stream_id;
    rtp_mux_hdr_compressed->len = data_len;
    rtp_mux_hdr_compressed->m = rtp_hdr->m;
    rtp_mux_hdr_compressed->sn_lsb = ntohs(rtp_hdr->seq) & 7 /* 0b0111 */;
    rtp_mux_hdr_compressed->ts_crc4 = calc_crc4(ntohl(rtp_hdr->ts));

    // try decompressing
    // DBG("test decompress\n");
    unsigned char rtp_restored_hdr[MAX_RTP_HDR_LEN];
    decompress(rtp_mux_hdr_compressed, stream_state.ts_increment, old_rtp_hdr, rtp_restored_hdr);

    if (!memcmp(rtp_restored_hdr, rtp_hdr, rtp_hdr_len)) {
      // compress -> decompress worked, send compressed
      // skip compressed header
      end_ptr+=sizeof(rtp_mux_hdr_compressed_t);
      // copy payload (skipping RTP hdr)
      memcpy(end_ptr, buffer+rtp_hdr_len, data_len);
      end_ptr+= data_len;
      // DBG("decompress matched, sending compressed frame (%lu mux hdr + %u payload bytes).\n",
      // 	  sizeof(rtp_mux_hdr_compressed_t), (unsigned int)(b_size-rtp_hdr_len));
    } else {
      // decompressed frame didn't match, sending setup frame
      // DBG("decompressed frame didn't match, sending setup frame.\n");
      send_setup_frame = true;
    }
  }

  if (send_setup_frame) {
    // setup frame header
    rtp_mux_hdr_setup_t* setup_hdr =  (rtp_mux_hdr_setup_t*)end_ptr;
    setup_hdr->t = RTP_MUX_HDR_TYPE_SETUP;
    setup_hdr->sid = stream_id;
    setup_hdr->len = b_size;
    setup_hdr->dstport = htons(rtp_dst_port);

    // set ts increment
    if (!(stream_state.ts_increment % RTP_MUX_HDR_TS_MULTIPLIER_HIGH)) {
      // it's safe to divide by 160
      setup_hdr->u = RTP_MUX_HDR_TS_MULTIPLIER_160;
      setup_hdr->ts_inc = stream_state.ts_increment / RTP_MUX_HDR_TS_MULTIPLIER_HIGH;
    } else {
      // use div by 40
      setup_hdr->u = RTP_MUX_HDR_TS_MULTIPLIER_40;
      setup_hdr->ts_inc = stream_state.ts_increment / RTP_MUX_HDR_TS_MULTIPLIER_LOW;
    }
    if (stream_state.ts_increment != calculate_ts_increment(setup_hdr)) {
      WARN("ts increment of %u can't be expressed by u %u and ts_inc %u (TS resync?)\n",
	   stream_state.ts_increment, setup_hdr->u, setup_hdr->ts_inc);
    }

    end_ptr += sizeof(rtp_mux_hdr_setup_t);

    // copy complete frame (rtp hdr + payload)
    memcpy(end_ptr, buffer, b_size);
    end_ptr += b_size;

    stream_state.last_setup_frame_ts = wheeltimer::instance()->wall_clock;
  }

  stream_state.last_mux_packet_id = mux_packet_id;

  // save old RTP header
  stream_state.rtp_hdr_len = get_rtp_hdr_len(rtp_hdr);
  memcpy(stream_state.rtp_hdr, rtp_hdr, stream_state.rtp_hdr_len);

  // DBG("queued frame on MUX stream for dst port %u, stream_id %u on RTP MUX %s:%u (hdr_len %u, len %u); send_setup_frame = %s\n",
  //     rtp_dst_port, stream_id, remote_ip.c_str(), remote_port, get_rtp_hdr_len((rtp_hdr_t*)buffer), b_size, send_setup_frame?"true":"false");

  return sendQueue();
}

int MuxStreamQueue::sendQueue(bool force) {
  bool force_send = force;

  if (!oldest_frame_i) {
    // record oldest TS in the queue
    oldest_frame = wheeltimer::instance()->wall_clock;
    oldest_frame_i = true;
  } else if (wheeltimer::instance()->interval_elapsed(oldest_frame, AmConfig::RtpMuxMaxFrameAgeMs)) {
    // enough time elapsed - flush queue
    force_send = true;
  }

  unsigned int len = end_ptr - buf;

  if (len >= AmConfig::RtpMuxMTUThreshold)
    force_send = true;

  if (!force_send) // can buffer more
    return 0;

  // DBG("sending frame on MUX stream \n");

  int err = ::sendto(l_sd, buf, len, 0,
		     (const struct sockaddr *)&r_saddr,
		     SA_len(&r_saddr));

  if(err == -1){
    ERROR("RTP MUX: while sending RTP packet: '%s' trying to send %u bytes to %s:%u\n",
	  strerror(errno), len, am_inet_ntop(&r_saddr).c_str(), am_get_port(&r_saddr));
  }

  // reset queue
  end_ptr = buf;
  oldest_frame_i = false;

  // increase, so send code above can see queue being cleared in between
  mux_packet_id++;

  return err;
}

void MuxStreamQueue::close(const string& _remote_ip, unsigned short _remote_port, unsigned short rtp_dst_port) {
  if ((remote_ip != _remote_ip) || (remote_port != _remote_port) || !rtp_dst_port) {
    return;
  }

  // do we have that rtp_dst_port already?
  map<unsigned short, unsigned char>::iterator mux_id_it = stream_ids.find(rtp_dst_port);
  if (mux_id_it == stream_ids.end()) {
    //we don't have it - all ok
    return;
  }
  unsigned char stream_id = mux_id_it->second;

  MuxStreamState& stream_state = streamstates[stream_id];

  // flush remaining frames - might be including frames of other streams, but this
  // destination might not be periodically handled in the future if this was the only
  // stream to this destination
  DBG("RTP MUX: flushing queue for %s:%u\n", remote_ip.c_str(), remote_port);
  sendQueue(true);

  // fixme: maybe the stream_id closing should be signaled?
  // remove that stream here
  DBG("RTP MUX: freeing stream_id %u to %s:%u\n", stream_id, remote_ip.c_str(), remote_port);
  stream_ids.erase(mux_id_it);
  streamstates.erase(stream_id);
}

void decompress(const rtp_mux_hdr_compressed_t* rtp_mux_hdr_compressed, unsigned int ts_increment,
		const rtp_hdr_t* old_rtp_hdr, unsigned char* rtp_restored_hdr) {
  u_int16 hdr_len = get_rtp_hdr_len(old_rtp_hdr);
  // use old as template
  memcpy(rtp_restored_hdr, old_rtp_hdr, hdr_len);
  rtp_hdr_t* rtp_hdr = (rtp_hdr_t*)rtp_restored_hdr;
  // copy marker bit
  rtp_hdr->m = rtp_mux_hdr_compressed->m;
  // use 3 lsb bits sent in compressed
  u_int16 old_rtp_hdr_seq = ntohs(old_rtp_hdr->seq);
  u_int32 old_rtp_hdr_ts = ntohl(old_rtp_hdr->ts);
  u_int16 rtp_hdr_seq = (ntohs(rtp_hdr->seq) & 0xFFF8/*(~ (u_int16)7)*/) + rtp_mux_hdr_compressed->sn_lsb;
  bool wrap = false;
  // new Seqno smaller?
  if (rtp_hdr_seq < old_rtp_hdr_seq) {
      // next 3-bit
      rtp_hdr_seq+=8;
  }
  // test different possible SNs
  u_int16 test_sns[9] = {0, 8, 1, (u_int16)-8, 16, 32, 40, 48,  (u_int16)-16};

  bool found = false;
  for (size_t i=0; i<9; i++) {
    u_int16 sn_diff_test = test_sns[i];
    u_int16 sn_test = rtp_hdr_seq + sn_diff_test;
    u_int16 sn_diff = sn_test - old_rtp_hdr_seq;
    u_int32 ts_test = ntohl(rtp_hdr->ts) + sn_diff * ts_increment;
    // DBG("testing SN diff %u - sn_test %u, sn_diff %u, ts_test %u, crc4(ts) %u\n",
    // 	sn_diff_test, sn_test, sn_diff, ts_test, calc_crc4(ts_test));
    if (calc_crc4(ts_test) == rtp_mux_hdr_compressed->ts_crc4) {
      // found correct TS -> SeqNo must be correct as well
      rtp_hdr->ts = htonl(ts_test);
      rtp_hdr->seq = htons(sn_test);
      found = true;
      break;
    }
  }
  if (!found) {
    WARN("couldn't reconstruct TS/Seqno: old seq %u, ts %u, ts_inc %u, sn_lsb %u, ts_crc4 %u\n",
	 old_rtp_hdr_seq, old_rtp_hdr->ts, ts_increment, rtp_mux_hdr_compressed->sn_lsb, rtp_mux_hdr_compressed->ts_crc4);
    // using most likely one - or drop?
    u_int16 sn_diff = rtp_hdr_seq - old_rtp_hdr_seq;
    rtp_hdr->ts += htonl(sn_diff * ts_increment);    
  } else {
    // DBG("found\n");
  }
}

// return true if RTP header has changed significantly so will need to be sent a setup frame 
bool rtp_hdr_changed(const rtp_hdr_t* hdr1, const rtp_hdr_t* hdr2) {
  //           PT change           SSRC change                  CSRC count change
  if ((hdr1->pt != hdr2->pt) || (hdr1->ssrc != hdr2->ssrc) ||  (hdr1->cc != hdr2->cc) ||  (hdr1->x != hdr2->x))
    return true;
  // fixme: check CSRCs and extension headers' contents 

  //      TS resync?
  if( ts_less()(ntohl(hdr1->ts), ntohl(hdr2->ts) - RESYNC_MAX_DELAY/2) || 
      !ts_less()(ntohl(hdr1->ts), ntohl(hdr2->ts) + RESYNC_MAX_DELAY) ) {
    DBG("TS resync: hdr1->ts %u, hdr2->ts %u\n", ntohl(hdr1->ts), ntohl (hdr2->ts));
    return true;
  }

  return false;
}
