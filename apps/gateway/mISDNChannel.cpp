#include "AmApi.h"
//#include "GWCall.h"
#include "globals.h"
#include "GatewayFactory.h"
#include "mISDNChannel.h"
#include "mISDNNames.h"
#include "amci/codecs.h"
#include <math.h>

using namespace mISDN;

//we need to swap bits in bytes for isdn audio. this 2 functions are borowed from asterisk
char flip_table[256];
void init_flip_bits(void) {
  int i,k;
  for (i = 0 ; i < 256 ; i++) {
    unsigned char sample = 0 ;
     for (k = 0; k<8; k++) {
       if ( i & 1 << k ) sample |= 0x80 >>  k;
     }
     flip_table[i] = sample;
  }
}
static char * flip_buf_bits ( char * buf , int len) {
  int i;
  char * start = buf;
  for (i = 0 ; i < len; i++) {
    buf[i] = flip_table[(unsigned char)buf[i]];
  }
  return start;
}

/* First AmAudio interface This is short. */
int mISDNChannel::read(unsigned int user_ts, unsigned int size) {
//	DBG("mISDNChannel::read user_ts=%d size=%d buffersize=%d\n",user_ts,size,fromISDN_buffer.size());
	//i know this is not fastest implementation but its short and works
	fromISDN_buffer.copy((char *)((unsigned char*)samples),size);
	fromISDN_buffer.erase(0,size);
	return size;
} 
int mISDNChannel::write(unsigned int user_ts, unsigned int size) {
	char buf[4096+mISDN_HEADER_LEN];
        mISDN::iframe_t *frame = (mISDN::iframe_t *)buf;
        int ret;
//    DBG("mISDNChannel::write user_ts=%d size=%d\n",user_ts,size);
	if(m_BC==0) {
//		DBG("bchannel is already detached or not yet initialised\n"); //we silently discard this to avoid log flooding
		return 0; 
	}
	if(size>=4096) {
	    DBG("truncating output audio (%d)\n",size);
	    size=4096;
	}
	memcpy(buf + (mISDN_HEADER_LEN), (unsigned char*)samples, size);
	flip_buf_bits( buf + mISDN_HEADER_LEN, size); 
	frame->addr = m_BC | FLG_MSG_DOWN;
	frame->prim = DL_DATA | REQUEST;
	frame->dinfo = 0;
	frame->len = size;
	ret =  mISDN::mISDN_write(mISDNStack::instance()->m_mISDNdevice, buf, frame->len+mISDN_HEADER_LEN, 8000);
//	DBG("mISDNChannel::write: sending packet directly to isdn 0x%x 0x%x %d  ret=%d\n",frame->addr, frame->prim,frame->len,ret);
    return ret;
}
void mISDNChannel::bchan_receive(char *msg_buf,int msg_buf_s) {
    int len=msg_buf_s-mISDN_HEADER_LEN;
    std::string tmp;
//    DBG("mISDNChannel::bchannel_receive size=%d buffersize=%d\n",msg_buf_s - mISDN_HEADER_LEN,fromISDN_buffer.size());
    flip_buf_bits( msg_buf + mISDN_HEADER_LEN, len);
    tmp.assign(msg_buf + mISDN_HEADER_LEN,len);
    fromISDN_buffer.append(tmp);
    return;
}
//void mISDNChannel::bchannel_send() {
//	char buf[PLAY_SIZE+mISDN_HEADER_LEN];
//	mISDN::iframe_t *frame = (mISDN::iframe_t *)buf;
//	int l,ret;
//	int i,t;
//	char *s;
//	
//	flip_buf_bits( buf + mISDN_HEADER_LEN, PLAY_SIZE);  
//	frame->addr = m_BC | FLG_MSG_DOWN;
//	frame->prim = DL_DATA | REQUEST;
//	frame->dinfo = 0;
//	frame->len = PLAY_SIZE;
//	ret = mISDN::mISDN_write(mISDNStack::instance()->m_mISDNdevice, buf, PLAY_SIZE+mISDN_HEADER_LEN, 8000);
//	DBG("mISDNChannel::bchannel_send write=%d\n",ret);
//	return;
//}


//some code borrowed from Linux Call Router
static signed char _mISDN_l3_ie2pos[128] = {
                        -1,-1,-1,-1, 0,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,
                         2,-1,-1,-1, 3,-1,-1,-1, 4,-1,-1,-1, 5,-1, 6,-1,
                         7,-1,-1,-1,-1,-1,-1, 8, 9,10,-1,-1,11,-1,-1,-1,
                        -1,-1,-1,-1,12,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                        13,-1,14,15,16,17,18,19,-1,-1,-1,-1,20,21,-1,-1,
                        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,22,23,-1,-1,
                        24,25,-1,-1,26,-1,-1,-1,27,28,-1,-1,29,30,31,-1
};

/*
static unsigned char _mISDN_l3_pos2ie[32] = {
                        0x04, 0x08, 0x10, 0x14, 0x18, 0x1c, 0x1e, 0x20,
                        0x27, 0x28, 0x29, 0x2c, 0x34, 0x40, 0x42, 0x43,
                        0x44, 0x45, 0x46, 0x47, 0x4c, 0x4d, 0x6c, 0x6d,
                        0x70, 0x71, 0x74, 0x78, 0x79, 0x7c, 0x7d, 0x7e
};
*/

int mISDN_get_free_ext_ie(mISDN::Q931_info_t *qi) {
        int     i;
        for (i = 0; i < 8; i++) {
                if (qi->ext[i].ie.off == 0)
                        return(i);
        }
        return (-1);
}

int mISDN_AddIE(mISDN::Q931_info_t *qi, u_char *p, u_char ie, u_char *iep) {
        u_char          *ps;
        mISDN::ie_info_t       *ies;
        int             l;

        if (ie & 0x80) { /* one octett IE */
                if (ie == IE_MORE_DATA)
                        ies = &qi->more_data;
                else if (ie == IE_COMPLETE)
                        ies = &qi->sending_complete;
                else if ((ie & 0xf0) == IE_CONGESTION)
                        ies = &qi->congestion_level;
                else {
                        return(-1);
                }
                l = 0;
        } else {
                if (!iep || !iep[0])
                        return(-3); 
                ies = &qi->bearer_capability;
                if (_mISDN_l3_ie2pos[ie]<0) {
                        return(-2);
                }
                ies += _mISDN_l3_ie2pos[ie];
                if (ies->off) {
                        while (ies->repeated)
                                ies = &qi->ext[ies->ridx].ie;;
                        l = mISDN_get_free_ext_ie(qi);
                        if (l < 0) { // overflow
                                return(-3);
                        }
                        ies->ridx = l;
                        ies->repeated = 1;
                        ies = &qi->ext[l].ie;
                        ies->cs_flg = 0;
                        qi->ext[l].v.codeset = 0;
                        qi->ext[l].v.val = ie;
                }
                l = iep[0] + 1;
        }
        ps = (u_char *) qi;
        ps += L3_EXTRA_SIZE;
        ies->off = (u16)(p - ps);
        *p++ = ie;
        if (l)
                memcpy(p, iep, l);
        return(l+1);
}


mISDNChannel::mISDNChannel() : AmAudio(new AmAudioSimpleFormat(CODEC_ALAW)){
	DBG("this is temporary constructor\n");
	init();
	m_port=mISDNStack::instance()->mISDNport_first;
}

mISDNChannel::mISDNChannel(mISDNport *port) : AmAudio(new AmAudioSimpleFormat(CODEC_ALAW)){
	init();
	m_port=port;
}

mISDNChannel::~mISDNChannel() {
	unregister_CR();
	unregister_BC();
	DBG("mISDNChannel destructor ends\n");
}
void mISDNChannel::init() {

        m_frame=(mISDN::iframe_t*)m_last_msg;
        m_qi = (mISDN::Q931_info_t*)(m_last_msg + mISDN_HEADER_LEN);
	m_ie_data =(char *)m_qi + L3_EXTRA_SIZE;
        /* init the audio buffers for in&out */
//        pthread_mutex_init(&fromISDN_lock, NULL);
//        pthread_mutex_init(&toISDN_lock, NULL);
	fromISDN_buffer.assign("");
}

void mISDNChannel::setSession(GWSession* session) {
        m_session = session;
}
GWSession* mISDNChannel::getSession() {
        return m_session;
}

void mISDNChannel::unregister_CR() {
	mISDNStack* stack=mISDNStack::instance();
        std::map<int,mISDNChannel*>::iterator CR_iter=stack->CR_map.find(m_CR);;
        if(CR_iter==stack->CR_map.end()) {
    		DBG("mISDNChannel::unregister_CR Cant find myself in CR_map this=%p (0x%08x)\n",this,m_CR);
    	} else {
    		DBG("mISDNChannel::unregister_CR removing channel from CR_map this=%p (0x%08x)\n",this,m_CR);
    		stack->CR_map.erase(CR_iter);
    	}
    	m_CR=0;
}
void mISDNChannel::unregister_BC() {
	mISDNStack* stack=mISDNStack::instance();
        if(m_BC!=0) {
		std::map<int,mISDNChannel*>::iterator BC_iter=stack->BC_map.find(m_BC&STACK_ID_MASK);;
		if(BC_iter==stack->BC_map.end()) {
			DBG("mISDNChannel::unregister_BC Cant find myself in BC_map %p (0x%08x)\n",this,m_BC);
		} else {
			DBG("mISDNChannel::unregister_BC is removing channel from BC_map this=%p (0x%08x)\n",this,m_BC);
			stack->BC_map.erase(BC_iter);
		}
		m_BC=0;
	} else DBG("mISDNChannel::unregister_BC BC already removed or not initialized, this=%p (0x%08x)\n",this,m_BC);
}
	
int mISDNChannel::placeCall(const AmSipRequest &req, std::string tonumber, std::string fromnumber) {
        m_called=tonumber;
        m_TON_d=0; //Unknown
        m_NPI_d=1; // ISDN E.164
	if(fromnumber.empty()) {
	    m_caller=gwconf.getParameter("out_msn","");
	} else 
	    m_caller=fromnumber;
        m_TON_r=0; //Unknown
        m_NPI_r=1; // ISDN E.164
        m_Screening_r=0; // user provided
        m_Presentation_r=0; // allowed
        return call(); 
                                                                        
}
int mISDNChannel::accept() {
	mISDNStack* stack=mISDNStack::instance();
	char buf[MAX_MSG_SIZE];
        // mISDN::Q931_info_t *qi;
        mISDN::iframe_t *frame = (mISDN::iframe_t *)buf;
	DBG("mISDNChannel::accept\n");
	frame->prim = CC_CONNECT | REQUEST;
	frame->addr = m_port->upper_id | FLG_MSG_DOWN;
	frame->dinfo= m_CR;
	frame->len= 0;
	DBG("Sending CC_CONNECT | REQUEST for CR=0x%04x \n",m_CR);
    	mISDN::mISDN_write(stack->m_mISDNdevice, buf, mISDN_HEADER_LEN+frame->len, TIMEOUT_1SEC);
	
	return OK;
}

int mISDNChannel::hangup() {
	mISDNStack* stack=mISDNStack::instance();
	char buf[MAX_MSG_SIZE];
        // mISDN::Q931_info_t *qi;
        mISDN::iframe_t *frame = (mISDN::iframe_t *)buf;
                        
	DBG("mISDNChannel::hangup\n");
	frame->prim = CC_DISCONNECT | REQUEST;
	frame->addr = m_port->upper_id | FLG_MSG_DOWN;
	frame->dinfo= m_CR;
	frame->len= 0;
	DBG("Sending CC_DISCONNECT | REQUEST for CR=0x%04x \n",m_CR);
    	mISDN::mISDN_write(stack->m_mISDNdevice, buf, mISDN_HEADER_LEN+frame->len, TIMEOUT_1SEC);
	
	return OK;
}

int mISDNChannel::call() {
	mISDNStack* stack=mISDNStack::instance();
	unsigned char buf[MAX_MSG_SIZE], *p, *msg, ie[64];
	mISDN::Q931_info_t *qi;
	mISDN::iframe_t *frame = (mISDN::iframe_t *)buf;
	int ret;
	size_t i;

	INFO("mISDN is making outbound call from %s to %s\n", m_caller.c_str(), m_called.c_str());
	//making new isdn call ref
	m_CR=stack->GenerateCR();
	frame->prim = CC_NEW_CR | REQUEST;
	frame->addr = m_port->upper_id | FLG_MSG_DOWN;
	frame->dinfo= m_CR;
	frame->len=0;
	DBG("sending CC_NEW_CR | REQUEST to device=%d addr=0x%08x dinfo=0x%08x\n",mISDNStack::instance()->m_mISDNdevice,frame->addr,frame->dinfo);
	ret=mISDN::mISDN_write(mISDNStack::instance()->m_mISDNdevice, buf, mISDN_HEADER_LEN+frame->len, TIMEOUT_1SEC);
	if ( ret<0) {
		ERROR("mISDNChannel::call error on NEW_CR | REQUEST %d\n", ret);
		return FAIL;
	}
	stack->CR_map[m_CR]=this;
        DBG("Adding self (%p) to channel_map my CR=0x%08x \n",this,m_CR);
	p = msg = buf + mISDN_HEADER_LEN;
	qi = (mISDN::Q931_info_t *)p;
	memset(qi, 0, sizeof(mISDN::Q931_info_t));
        qi->type = MT_SETUP;
        p += L3_EXTRA_SIZE;
        p++; /* needed to avoid offset 0 in IE array */
        ret = mISDN_AddIE(qi, p, IE_COMPLETE, NULL);
	if ( ret<0) {
		ERROR("mISDNChannel::call Add IE_COMPLETE error %d\n", ret);
		return FAIL;
	}
	p += ret;
	ret = mISDN_AddIE(qi, p, IE_BEARER, (unsigned char*)"\x3\x90\x90\xa3");	/* Audio */
//	ret = mISDN_AddIE(qi, p, IE_BEARER, (unsigned char*)"\x2\x88\x90"); /* default Datatransmission 64k */
	if (ret<0) { ERROR("mISDNChannel::call Add IE_BEARER error %d\n", ret); return FAIL;}
	p += ret;
        ie[0] = m_caller.size() + 1;
        ie[1] = 0x00 + (m_TON_r << 4) + m_NPI_r;        /* Ext = '1'B, Type = '000'B, Plan = '0001'B. */
        if (m_Presentation_r >= 0)    {
                ie[1] = 0x00 + (m_TON_r<<4) + m_NPI_r;
                ie[2] = 0x80 + (m_Presentation_r<<5) + m_Screening_r;
		for (i=0; i<m_caller.size(); i++)
		    ie[3+i] = m_caller[i] & 0x7f;
		ie[3+m_caller.size()] = 0;
        } else {
                ie[1] = 0x80 + (m_TON_r<<4) + m_NPI_r;
		for (i=0; i<m_caller.size(); i++)
		    ie[2+i] = m_caller[i] & 0x7f;
		ie[2+m_caller.size()] = 0;
        }
        ret = mISDN_AddIE(qi, p, IE_CALLING_PN, ie);
        if (ret<0) { ERROR("mISDNChannel::call Add IE_CALLING_PN error %d\n", ret);return FAIL; }
	p += ret;
        ie[0] =m_called.size() + 1;
        ie[1] = 0x80 + (m_TON_d << 4) + m_NPI_d;        /* Ext = '1'B, Type = '000'B, Plan = '0001'B. */
        for (i=0; i<m_called.size(); i++)
                ie[2+i] = m_called[i] & 0x7f;
	ie[2+m_called.size()] = 0;
        ret = mISDN_AddIE(qi, p, IE_CALLED_PN, ie);
        if (ret<0) { ERROR("mISDNChannel::call Add IE_CALLED_PN error %d\n", ret);return FAIL; }
	p += ret;
	frame->prim = CC_SETUP | REQUEST;
	frame->addr = m_port->upper_id | FLG_MSG_DOWN;
	frame->dinfo= m_CR;
	frame->len= p - msg;
	ret=mISDN::mISDN_write(mISDNStack::instance()->m_mISDNdevice, buf, mISDN_HEADER_LEN+frame->len, TIMEOUT_1SEC);                                                                                                                                                        
	if ( ret<0) {
    		ERROR("mISDNChannel::call error dending CC_SETUP | REQUEST %d\n", ret);
    		return FAIL;
        }
    	return OK;	
}

int mISDNChannel::processMsg(char *msg_buf,int msg_buf_s) {
	mISDNStack* stack=mISDNStack::instance();
	char buf[MAX_MSG_SIZE];
	// mISDN::Q931_info_t *qi;
        mISDN::iframe_t *frame = (mISDN::iframe_t *)buf;
	char *p;

	p=(char *)&m_last_msg;
	memcpy(p, msg_buf, msg_buf_s);
	m_last_msg_s=msg_buf_s;
	memset(p+msg_buf_s,0,MAX_MSG_SIZE-msg_buf_s);
	switch(m_frame->prim)   {
		case CC_SETUP |INDICATION:
			DBG("CC_SETUP | INDICATION for CR=0x%04x \n",m_CR);
			GetIEchannel_id();
			DBG("Creating Bchannel for CR=0x%04x \n",m_CR);
			bchan_create();
			GetCallerNum();
			GetCalledNum();
			m_session=GWSession::CallFromOutside(m_caller, m_called, 0, this);
			if(m_session!=NULL) {
		            unsigned char buf[MAX_MSG_SIZE];
//            		    mISDN::Q931_info_t *qi;
                    	    mISDN::iframe_t *frame = (mISDN::iframe_t *)buf;
                            int ret;
			    frame->prim = CC_PROCEEDING | REQUEST;
		    	    frame->addr = m_port->upper_id | FLG_MSG_DOWN;
	                    frame->dinfo= m_CR;
	                    frame->len= 0;
	            	    ret=mISDN::mISDN_write(mISDNStack::instance()->m_mISDNdevice, buf, mISDN_HEADER_LEN+frame->len, TIMEOUT_1SEC);
			    DBG("m_session=%p Not null sending CC_PROCEEDING |REQUEST\n",m_session);
			}                                       
			break;
		case CC_PROCEEDING |INDICATION:
			DBG("CC_PROCEEDING | INDICATION for CR=0x%04x \n",m_CR);
//			m_session->acceptAudio(m_session->invite_req.body,m_req.hdrs,&sdp_reply);
//			m_session->dlg.reply(m_session->invite_req,180, "Isdn side state is: PROCEEDING", "application/sdp",sdp_reply);
			m_session->dlg.reply(m_session->invite_req,180, "Isdn side state is: PROCEEDING");
			DBG("check 2 CC_PROCEEDING | INDICATION for CR=0x%04x \n",m_CR);
			GetIEchannel_id();
			DBG("Creating Bchannel for CR=0x%04x \n",m_CR);
			bchan_create();
			break;
		case CC_ALERTING | INDICATION:
			DBG("CC_ALERTING | INDICATION for CR=0x%04x \n",m_CR);
//			m_session->acceptAudio(m_session->invite_req.body,m_req.hdrs,&sdp_reply);
//			m_session->dlg.reply(m_session->invite_req,183, "Isdn side state is: ALERTING", "application/sdp",sdp_reply);
                        m_session->dlg.reply(m_session->invite_req,183, "Isdn side state is: ALERTING");
                    	break;
		case CC_CONNECT | INDICATION:
		case CC_CONNECT_ACKNOWLEDGE | INDICATION:
			DBG("CC_CONNECT(_ACKNOWLEDGE) | INDICATION for CR=0x%04x \n",m_CR);
			DBG("Activating Bchannel for CR=0x%04x \n",m_CR);
			bchan_activate();
			m_session->onSessionStart(m_session->invite_req);
			break;
		case CC_DISCONNECT | INDICATION:
		case CC_DISCONNECT | CONFIRM:
			DBG("CC_DISCONNECT | INDICATION or CONFIRMfor CR=0x%04x \n",m_CR);
			m_session->setInOut(NULL,NULL);
                        m_session->setStopped();
                        if(m_session->dlg.getStatus()==AmSipDialog::Pending) {
                    	    DBG("Sip side not yet connected sending reply\n");
                    	    m_session->dlg.reply(m_session->invite_req,487, "Isdn side state is: DISCONNECTING");
                    	    //maybe this would be better
//			    throw AmSession::Exception(487, "call terminated");
                        } else {
                    	    DBG("Sip side already connected sending bye\n");
                    	    m_session->dlg.bye();
                    	}
//			mISDN::mISDN_write_frame(stack->m_mISDNdevice, buf, m_port->upper_id | FLG_MSG_DOWN, CC_RELEASE | REQUEST, m_CR, 0, NULL, TIMEOUT_1SEC);
			frame->prim = CC_RELEASE | REQUEST;
			frame->addr = m_port->upper_id | FLG_MSG_DOWN;
			frame->dinfo= m_CR;
			frame->len= 0;
			DBG("Sending CC_RELEASE | REQUEST for CR=0x%04x \n",m_CR);
		    	mISDN::mISDN_write(stack->m_mISDNdevice, buf, mISDN_HEADER_LEN+frame->len, TIMEOUT_1SEC);
		    	bchan_deactivate();
			break;
		case CC_RELEASE | INDICATION:
		case CC_RELEASE_COMPLETE | INDICATION:	
			DBG("CC_RELEASE(_COMPLETE) | INDICATION for CR=0x%04x \n",m_CR);
//			mISDN::mISDN_write_frame(stack->m_mISDNdevice, buf, m_port->upper_id | FLG_MSG_DOWN, CC_RELEASE_COMPLETE | REQUEST, m_CR, 0, NULL, TIMEOUT_1SEC);
			frame->prim = CC_RELEASE_COMPLETE| REQUEST;
			frame->addr = m_port->upper_id | FLG_MSG_DOWN;
			frame->dinfo= m_CR;
			frame->len= 0;
			DBG("Sending CC_RELEASE_COMPLETE | REQUEST for CR=0x%04x \n",m_CR);
		    	mISDN::mISDN_write(stack->m_mISDNdevice, buf, mISDN_HEADER_LEN+frame->len, TIMEOUT_1SEC);
		    	bchan_destroy();
			break;
                default:
                        ERROR("mISDNChannel::processMsg unhandled: prim(0x%x) addr(0x%x) msg->len(%d)\n", m_frame->prim, m_frame->addr, msg_buf_s);
                        return FAIL;
                        break;
                                                                                
	}
	return OK;
}

int mISDNChannel::GetIEchannel_id(){
	char *p;
	m_bchannel=-1; //no ie or error (for now)
	
	if (m_qi->channel_id.off==0) {	ERROR("No channel_id IE here\n");		return FAIL; }
	p = m_ie_data + m_qi->channel_id.off;
	//p[0] is 0x18 - code for this ie;
	DBG("mISDNChannel::GetIEchannel_id p= 0x%02hhx 0x%02hhx 0x%02hhx\n",p[0],p[1],p[2]);
	if(p[1]<1) {		ERROR("IE Too short\n"); 				return FAIL; }
	if(p[2] & 0x40)	{	ERROR("Channels on other interfaces not supported\n"); 	return FAIL; } // bit 7 - other interface
	if(p[2] & 0x04)	{	ERROR("using d-channel not supported\n");		return FAIL; } // bit 3 - d channel;
	if(m_port->pri) { 
	    switch((p[2]&0x03)) {
		case 0:	m_bchannel=-2; 			return OK; 	break; //no Channel
		case 1: 						break; //channel num in folowing bytes
		case 2: ERROR("Reserved bit set\n"); 	return FAIL;	break; // reserver bit;
		case 3: m_bchannel=-3;			return OK;	break; //ANY channel
	    }
	    if(p[1] < 3) {	ERROR("IE Too short for PRI\n");			return FAIL; }	// we need extended info on channel num for pri
	    if(p[3] & 0x10) {	ERROR("channel map not supported\n");			return FAIL; } // bit 3 - d channel;
	    m_bchannel=p[4]&0x7f;
	    if(m_bchannel<1 || m_bchannel==16) {
		    ERROR("PRI channel out of range (%d)\n",m_bchannel);
		    m_bchannel=-1; 
		    return FAIL;
	    }
	    DBG("mISDNChannel::GetIEchannel_id will use PRI b_channel=%d\n",m_bchannel);
	    return OK;
	} else { //BRI
	    if ((p[2] & 0x20)) { ERROR("req for bri channel on PRI interface\n");	return FAIL; } // bit 6 - 0=BRI, 1-other(PRI);
	    switch((p[2]&0x03)) {
		case 0:	m_bchannel=-2;	 	break; //no Channel
		case 1: m_bchannel=1;		break; //channel 1
		case 2: m_bchannel=2;		break; //channel 2
		case 3: m_bchannel=-3;		break; //ANY channel
	    }
	    DBG("mISDNChannel::GetIEchannel_id will use BRI b_channel=%d\n",m_bchannel);
	    return OK;
	}	    
}
int mISDNChannel::GetCallerNum(){
	char *p;
	int len;
	
	if (m_qi->calling_nr.off==0) {	ERROR("No calling_nr IE here\n");		return FAIL; }
	p = m_ie_data + m_qi->calling_nr.off;
	//p[0] is 0x6C - code for this ie;
	DBG("mISDNChannel::GetCallerNum p= 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx\n",p[0],p[1],p[2],p[3]);
	p++;//now we scan each byte as some are optional
	if(p[0]<1) {		ERROR("IE Too short\n"); 				return FAIL; }
	if(p[0]>=MAX_NUM_LEN-1) {		ERROR("Number too long for MAX_NUM_LEN \n");	return FAIL; }
	len=p[0];
	p++;
	m_TON_r=(p[0]&0x70)>>4;
	m_NPI_r=p[0]&0x0F;
	if(!(p[0]&80)) { //if this is not last byte
	    len--;p++; //warning p is shifted here to next byte (optional Presentation/Screening)
	    m_Presentation_r=(p[0]&0x60)>>5;
	    m_Screening_r=p[0]&0x03;
	} else DBG("mISDNChannel::GetCallerNum no Presentation/Screening byte\n");
	len--;p++;
        DBG("mISDNChannel::GetCallerNum len=%d TON=%d NPI=%d Presentation=%d Screening=%d\n",len,m_TON_r,m_NPI_r,m_Presentation_r,m_Screening_r);
//        memcpy(&m_caller, p, len);
//        m_caller[len]='\0';
	m_caller.assign(p,len);
        DBG("mISDNChannel::GetCallerNum %s %s %s %s %s\n",m_caller.c_str(),mISDNNames::TON(m_TON_r),mISDNNames::NPI(m_NPI_r),mISDNNames::Presentation(m_Presentation_r),mISDNNames::Screening(m_Screening_r));
        return OK;
}
int mISDNChannel::GetCalledNum(){
	char *p;
	int len;
	
	if (m_qi->called_nr.off==0) {	ERROR("No called_nr IE here\n");		return FAIL; }
	p = m_ie_data + m_qi->called_nr.off;
	//p[0] is 0x70 - code for this ie;
	DBG("mISDNChannel::GetCalledNum p= 0x%02hhx 0x%02hhx 0x%02hhx\n",p[0],p[1],p[2]);
	if(p[1]<1) {			ERROR("IE Too short\n"); 				return FAIL; }
	if(p[1]>=MAX_NUM_LEN-1) {	ERROR("Number too long for MAX_NUM_LEN \n");		return FAIL; }
	len=p[1];
        m_TON_d=(p[2]&0x70)>>4;
        m_NPI_d=p[2]&0x0F;
        DBG("mISDNChannel::GetCalledNum len=%d TON=%d NPI=%d\n",len,m_TON_d,m_NPI_d);
//        memcpy(&m_called, p+3, len-1);
//        m_called[len-1]='\0';
	m_called.assign(p+3,len-1);
        DBG("mISDNChannel::GetCalledNum %s %s %s\n",m_called.c_str(),mISDNNames::TON(m_TON_d),mISDNNames::NPI(m_NPI_d));
        return OK;
}
int mISDNChannel::bchan_event(char *msg_buf,int msg_buf_s) {
	char *p;

	p=(char *)&m_last_msg;
	memcpy(p, msg_buf, msg_buf_s);
	m_last_msg_s=msg_buf_s;
	memset(p+msg_buf_s,0,MAX_MSG_SIZE-msg_buf_s);
	switch(m_frame->prim)   {
            case PH_DATA | CONFIRM:
            case DL_DATA | CONFIRM:
//                DBG("PH_DATA or DL_DATA  confirm  prim(0x%x) addr(0x%x) msg->len(%d) \n", m_frame->prim, m_frame->addr, msg_buf_s);
//              bchannel_send();  //we are now transmiting directly from Channel(AmAudio)::write
                break;
            case PH_DATA | INDICATION:
            case DL_DATA | INDICATION:
//                DBG("PH_DATA or DL_DATA  IND  prim(0x%x) addr(0x%x) msg->len(%d) \n", m_frame->prim, m_frame->addr, msg_buf_s);
                bchan_receive(msg_buf,msg_buf_s);
                break;
            case PH_CONTROL | INDICATION:
            case PH_SIGNAL | INDICATION:
                DBG("PH_CONTROL or PH_SIGNAL  IND  prim(0x%x) addr(0x%x) msg->len(%d) \n", m_frame->prim, m_frame->addr, msg_buf_s);
                break;
            case PH_ACTIVATE | INDICATION:
            case DL_ESTABLISH | INDICATION:
            case PH_ACTIVATE | CONFIRM:
            case DL_ESTABLISH | CONFIRM:
                DBG("(PH|DL)_(ESTABLISH|ACTIVATE (IND|CONFIRM): bchannel is now activated (address 0x%x).\n", m_frame->addr);
                break;
            case PH_DEACTIVATE | INDICATION:
            case DL_RELEASE | INDICATION:
            case PH_DEACTIVATE | CONFIRM:
            case DL_RELEASE | CONFIRM:
                DBG("(PH|DL)_(RELEASE|DEACTIVATE (IND|CONFIRM): bchannel is now de-activated (address 0x%x).\n", m_frame->addr);
                bchan_destroy();
                unregister_BC();
                break;
            default:
                ERROR("child message not handled: prim(0x%x) addr(0x%x) msg->len(%d)\n", m_frame->prim, m_frame->addr, msg_buf_s);
                return FAIL;
                break;
        }
	return OK;		
}

int mISDNChannel::bchan_create() {
	layer_info_t li;
	mISDN_pid_t pid;
	int ret;
	mISDNStack* stack=mISDNStack::instance();
	
	if(m_bchannel<=0) {			ERROR("b-channel num not known or invalid (%d)\n",m_bchannel);	return FAIL; }
	if(m_port->b_stid[m_bchannel-1]==0) {	ERROR("No stack for b-channel (%d)\n",m_bchannel);		return FAIL; }
	if(m_port->b_addr[m_bchannel-1]!=0) {	ERROR("Stack already created for b-channel (%d)\n",m_bchannel);	return FAIL; }
	memset(&li, 0, sizeof(li));
	memset(&pid, 0, sizeof(pid));
	li.object_id = -1;
	li.extentions = 0;
	li.st = m_port->b_stid[m_bchannel-1];
	strcpy(li.name, "B L4");
//	li.pid.layermask = ISDN_LAYER((4));
	li.pid.layermask = ISDN_LAYER((3));
//	li.pid.protocol[4] = ISDN_PID_L4_B_USER;
	li.pid.protocol[3] = ISDN_PID_L3_B_TRANS;
	ret = mISDN_new_layer(mISDNStack::instance()->m_mISDNdevice, &li);
        if(ret || !li.id) {		ERROR("mISDN_new_layer() failed to add bchannel %d\n", m_bchannel);	return FAIL; }
	m_BC=m_port->b_addr[m_bchannel-1]=li.id;
	pid.protocol[1] = ISDN_PID_L1_B_64TRANS;
	pid.protocol[2] = ISDN_PID_L2_B_TRANS;
	pid.protocol[3] = ISDN_PID_L3_B_TRANS;
//	pid.protocol[3] = ISDN_PID_L3_B_DSP;
//	pid.protocol[4] = ISDN_PID_L4_B_USER;
	pid.layermask = ISDN_LAYER((1)) | ISDN_LAYER((2)) | ISDN_LAYER((3)) ;
//	pid.layermask = ISDN_LAYER((1)) | ISDN_LAYER((2)) | ISDN_LAYER((3)) | ISDN_LAYER((4));
	ret = mISDN_set_stack(stack->m_mISDNdevice, m_port->b_stid[m_bchannel-1], &pid);
        if(ret) {			ERROR("mISDN_set_stack failed to add bchannel %d\n", m_bchannel);	return FAIL; }
	ret = mISDN_get_setstack_ind(stack->m_mISDNdevice, m_BC);
        if(ret) {			ERROR("mISDN_set_stack_ind failed to add bchannel %d\n", m_bchannel);	return FAIL; }
        m_BC=m_port->b_addr[m_bchannel-1]=mISDN_get_layerid(stack->m_mISDNdevice, m_port->b_stid[m_bchannel-1], 3);
//      m_BC=m_port->b_addr[m_bchannel-1]=mISDN_get_layerid(stack->m_mISDNdevice, m_port->b_stid[m_bchannel-1], 4);
	if(m_BC==0) {			ERROR("mISDN_get_layerid failed to add bchannel %d\n", m_bchannel);	return FAIL; }
	stack->BC_map[m_BC&STACK_ID_MASK]=this;
	m_port->b_port[m_bchannel-1]=this;
	DBG("Successfully created stack for port %d. addr=0x%08x\n",m_bchannel,m_BC);
	return OK;
}
int mISDNChannel::bchan_activate() {
	mISDN::iframe_t frame;
	mISDNStack* stack=mISDNStack::instance();
	int ret;
	
	if(m_BC==0) {	ERROR("bchannel (%d) not created\n",m_bchannel);		return FAIL; }
	DBG("sending DL_ESTABLISH | REQUEST to device=%d for bchannel=%d addr=0x%08x dinfo=0x%08x\n",stack->m_mISDNdevice, m_bchannel,frame.addr, frame.dinfo);
	ret = mISDN::mISDN_write_frame(stack->m_mISDNdevice, &frame, m_BC | FLG_MSG_DOWN, DL_ESTABLISH | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	return OK;
}
int mISDNChannel::bchan_deactivate() {
	mISDN::iframe_t frame;
	mISDNStack* stack=mISDNStack::instance();
	int ret;
	
	DBG("sending DL_RELEASE | REQUEST to device=%d for bchannel=%d addr=0x%08x dinfo=0x%08x\n",stack->m_mISDNdevice, m_bchannel,frame.addr, frame.dinfo);
	ret = mISDN::mISDN_write_frame(stack->m_mISDNdevice, &frame, m_BC | FLG_MSG_DOWN, DL_RELEASE | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	return OK;
}

int mISDNChannel::bchan_destroy() {
	mISDN::iframe_t frame;
	mISDNStack* stack=mISDNStack::instance();
	int ret;
	
	ret = mISDN_clear_stack(stack->m_mISDNdevice, m_port->b_stid[m_bchannel-1]);
	DBG("sending MGR_DELLAYER | REQUEST to device=%d for bchannel=%d addr=0x%08x dinfo=0x%08x\n",stack->m_mISDNdevice, m_bchannel,frame.addr, frame.dinfo);
	ret = mISDN::mISDN_write_frame(stack->m_mISDNdevice, &frame, m_BC | FLG_MSG_DOWN, MGR_DELLAYER | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	unregister_BC();
	m_port->b_port[m_bchannel-1]=NULL;
	m_port->b_addr[m_bchannel-1]=0;
	return OK;
}


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
