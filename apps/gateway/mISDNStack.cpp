#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "log.h"
#include "AmSession.h"

#include "globals.h"
#include "mISDNStack.h"
#include "mISDNChannel.h"
#include "mISDNNames.h"

using namespace mISDN;

mISDNStack* mISDNStack::_instance=NULL;

mISDNStack* mISDNStack::instance()
{
    if(!_instance) {
    DBG("mISDNStack::instance spawning new\n");
	_instance = new mISDNStack();
	if(_instance->init() != OK){
		delete _instance;
                _instance = 0;
        } else {
	        DBG("mISDNStack::instance start\n");
    		_instance->start();
    		init_flip_bits();
        }
    }
    return _instance;
}
mISDNChannel* mISDNStack::NewCR(mISDNport *port,mISDN::iframe_t *frame) {
    std::map<int,mISDNChannel*>::iterator iter=CR_map.find(frame->dinfo);;
    if(iter==CR_map.end()) {
        mISDNChannel* chan=NULL;
        DBG("This is new CR, spawning new object\n");
        chan = new mISDNChannel(port);
        CR_map[frame->dinfo]=chan;
        chan->m_CR=frame->dinfo;
        DBG("pointer to chan is %p\n",chan);
        return chan;
    } else {
        DBG("got previous CR porinter is %p\n",iter->second);
        return iter->second;
    }
}
                                                                                                                                                                                        
mISDNChannel* mISDNStack::FindCR(mISDN::iframe_t *frame) {
    std::map<int,mISDNChannel*>::iterator iter=CR_map.find(frame->dinfo);;
    if(iter!=CR_map.end()) {
//      DBG("got previous CR porinter is %p\n",iter->second);
        return iter->second;
    } else {
        ERROR("CR 0x%08x not found in CR_map\n",frame->dinfo);
        return NULL;
    }
}
mISDNChannel* mISDNStack::FindBC(mISDN::iframe_t *frame) {
    std::map<int,mISDNChannel*>::iterator iter=BC_map.find(frame->addr&STACK_ID_MASK);;
    if(iter!=BC_map.end()) {
//      DBG("got previous BC porinter is %p\n",iter->second);
        return iter->second;
    } else {
        ERROR("BC address 0x%08x not found in BC_map\n",frame->addr);
        return NULL;
    }
}
                                                                                                                                                                                
int mISDNStack::placeCall(const AmSipRequest &req, GWSession *session, const std::string &tonumber, const std::string &fromnumber) {
    //we will have code to choose right port here.
    mISDNChannel *chan = new mISDNChannel(); //(device);
    if(chan==NULL) {
	ERROR("Cant allocate new mISDNChannel\n");
	return FAIL;
    }
    session->setOtherLeg(chan);
    chan->setSession(session);
    DBG("calling ((mISDNChannel*)m_pstndevice)->placeCall(m_req, tonumber, fromnumber);\n");
    return chan->placeCall(req, tonumber, fromnumber);
}
                                            

int mISDNStack::GetPortInfo() {
	int err;
        int i, num_cards, p;
        int useable, nt, pri;
        unsigned char buff[1025];
        iframe_t *frm = (iframe_t *)buff;
        stack_info_t *stinf;
        int device;
    
	if ((device = mISDN_open()) < 0)  {
                ERROR("mISDNStack::mISDNStack: mISDN_open() failed: ret=%d errno=%d (%s) Check for mISDN modules and device.\n", device, errno, strerror(errno));
                return FAIL;
        }
        DBG("mISDNStack::mISDNStack: mISDN_open %d\n",device);
        /* get number of stacks */
        i = 1;
        num_cards = mISDN_get_stack_count(device);
        if (num_cards <= 0) {
                ERROR("Found no card. Please be sure to load card drivers.\n");
                return FAIL;
        }
                                                                                
        /* loop the number of cards and get their info */
        while(i <= num_cards) {
                err = mISDN_get_stack_info(device, i, buff, sizeof(buff));
                if (err <= 0) { ERROR("mISDN_get_stack_info() failed: port=%d err=%d\n", i, err); break; }
                stinf = (stack_info_t *)&frm->data.p;
                nt = pri = 0;
                useable = 1;
                switch(stinf->pid.protocol[0] & ~ISDN_PID_FEATURE_MASK) {
                        case ISDN_PID_L0_TE_S0:			INFO("Port %2d: TE-mode BRI S/T interface line (for phone lines)\n",i); break;
                        case ISDN_PID_L0_NT_S0: nt = 1; 	INFO("Port %2d: NT-mode BRI S/T interface port (for phones)\n",i); break;
                        case ISDN_PID_L0_TE_E1: pri = 1; 	INFO("Port %2d: TE-mode PRI E1  interface line (for phone lines)\n",i); break;
                        case ISDN_PID_L0_NT_E1: nt = 1;pri = 1;	INFO("Port %2d: NT-mode PRI E1  interface port (for phones)\n",i); break;
                        default: useable = 0; ERROR("unknown type 0x%08x\n",stinf->pid.protocol[0]);
                }
                if (nt) {
                        if (stinf->pid.protocol[1] == 0) { useable = 0;  INFO(" -> Missing layer 1 NT-mode protocol.\n");  }
                        p = 2; while(p <= MAX_LAYER_NR) { if (stinf->pid.protocol[p]) {useable = 0; INFO(" -> Layer %d protocol 0x%08x is detected, but not allowed for NT.\n", p, stinf->pid.protocol[p]); } p++; }
                        if (useable) {
                                if (pri) INFO(" -> Interface is Point-To-Point (PRI).\n");
                                else     INFO(" -> Interface can be Poin-To-Point/Multipoint.\n");
                        }
                } else  {
                        if (stinf->pid.protocol[1] == 0) { useable = 0; INFO(" -> Missing layer 1 protocol.\n"); }
                        if (stinf->pid.protocol[2] == 0) { useable = 0; INFO(" -> Missing layer 2 protocol.\n"); }
                        if (stinf->pid.protocol[2] & ISDN_PID_L2_DF_PTP) { INFO(" -> Interface is Poin-To-Point.\n"); }
                        if (stinf->pid.protocol[3] == 0) { useable = 0; INFO(" -> Missing layer 3 protocol.\n"); } 
                        else {	switch(stinf->pid.protocol[3] & ~ISDN_PID_FEATURE_MASK) {
                                        case ISDN_PID_L3_DSS1USER: INFO(" -> Protocol: DSS1 (Euro ISDN)\n"); break;
                                        default: useable = 0; INFO(" -> Protocol: unknown protocol 0x%08x\n",stinf->pid.protocol[3]);
                                }
                        }
                        p = 4; while(p <= MAX_LAYER_NR) { if (stinf->pid.protocol[p]) { useable = 0; INFO(" -> Layer %d protocol 0x%08x is detected, but not allowed for TE.\n", p, stinf->pid.protocol[p]); } p++; }
                        INFO(" -> childcnt: %d\n",stinf->childcnt);
                }
                if (!useable)  ERROR(" * Port %2d  NOT useable. (maybe somethind is using it already?)\n",i);
                i++;
        }
        /* close mISDN */
        if ((err = mISDN_close(device))) { 
    		ERROR("mISDN_close() failed: err=%d '%s'\n", err, strerror(err));
                return FAIL;
        }
	return OK;
}
mISDNStack::mISDNStack() : m_mISDNdevice(0),m_entity(0) {
    mISDNport_first=NULL;
}

mISDNStack::~mISDNStack() {
}

int mISDNStack::init() {
        unsigned char buff[1025];
        iframe_t *frm = (iframe_t *)buff;
        int ret;
        struct mISDNport *mISDNport, **mISDNportp;
        int i, cnt,port;
        int pri, ports;
        int nt, ptp;
        //net_stack_t *nst;
        //manager_t *mgr;
        layer_info_t li;
        stack_info_t *stinf;
                                

	if ((m_mISDNdevice = mISDN_open()) < 0)  { ERROR("mISDNStack::init: mISDN_open() failed: ret=%d errno=%d (%s) Check for mISDN modules and device.\n", m_mISDNdevice, errno, strerror(errno)); return FAIL; }
        DBG("mISDNStack::init: mISDN_opened %d\n",m_mISDNdevice);
        /* create entity for layer 3 TE-mode */
        mISDN_write_frame(m_mISDNdevice, buff, 0, MGR_NEWENTITY | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
        ret = mISDN_read_frame(m_mISDNdevice, frm, sizeof(iframe_t), 0, MGR_NEWENTITY | CONFIRM, TIMEOUT_1SEC);
        if (ret < (int)mISDN_HEADER_LEN) {  	ERROR("Cannot request MGR_NEWENTITY from mISDN. header too small.\n");return FAIL; }
        m_entity = frm->dinfo & 0xffff;
        if (!m_entity) {			ERROR("Cannot request MGR_NEWENTITY from mISDN. Exitting due to software bug.\n"); return FAIL; }
	DBG("our entity for l3-processes is 0x%08x.\n", m_entity);
	m_crcount=1; //entity and crcount is used to generate uniqe addr for outgoing calls
	
	cnt = mISDN_get_stack_count(m_mISDNdevice);
	if(cnt<=0) { 				ERROR("no devices\n"); return FAIL;}
	for(port=1;port<=cnt;port++) {	
		pri = ports = nt = 0;
		ret = mISDN_get_stack_info(m_mISDNdevice, port, buff, sizeof(buff));
	        if (ret < 0) {	ERROR("Cannot get stack info for port %d (ret=%d)\n", port, ret); }
		stinf = (stack_info_t *)&frm->data.p;
	        switch(stinf->pid.protocol[0] & ~ISDN_PID_FEATURE_MASK)  {
                    case ISDN_PID_L0_TE_S0: DBG("TE-mode BRI S/T interface line\n"); pri = 0;	nt = 0;	break;
	    	    case ISDN_PID_L0_NT_S0: DBG("NT-mode BRI S/T interface port\n"); pri = 0;	nt = 1;	break;
	            case ISDN_PID_L0_TE_E1: DBG("TE-mode PRI E1  interface line\n"); pri = 1;	nt = 0;	break;
	            case ISDN_PID_L0_NT_E1: DBG("LT-mode PRI E1  interface port\n"); pri = 1;	nt = 1;	break;
	            default:
	        	ERROR("unknown port(%d) type 0x%08x\n", port, stinf->pid.protocol[0]);
	        }
	        if (nt) {
	    	    DBG("Port %d (nt)  proto1=0x%08x proto2=0x%08x\n",port, stinf->pid.protocol[1],stinf->pid.protocol[2]);
                    if (stinf->pid.protocol[1] == 0) 	{ ERROR("Given port %d: Missing layer 1 NT-mode protocol.\n", port); }
                    if (stinf->pid.protocol[2])		{ ERROR("Given port %d: Layer 2 protocol 0x%08x is detected, but not allowed for NT.\n", port, stinf->pid.protocol[2]); }
                    ERROR("NT mode not supported yet\n");
                    return FAIL;
		} else { //(te)
	    	    DBG("Port %d (te)  proto1=0x%08x proto2=0x%08x proto3=0x%08x proto4=0x%08x (nul proto4 is good here)\n",port, stinf->pid.protocol[1],stinf->pid.protocol[2],stinf->pid.protocol[3],stinf->pid.protocol[4]);
                    if (stinf->pid.protocol[1] == 0) { ERROR("Given port %d: Missing layer 1 protocol.\n", port);}
	            if (stinf->pid.protocol[2] == 0) { ERROR("Given port %d: Missing layer 2 protocol.\n", port);}
	            if (stinf->pid.protocol[2] & ISDN_PID_L2_DF_PTP) { ptp=1;DBG("Port %d is point-to-point.\n",port); } else { ptp=0;} 
    	            if (stinf->pid.protocol[3] == 0) { ERROR("Given port %d: Missing layer 3 protocol.\n", port);} 
    	            else {
                	switch(stinf->pid.protocol[3] & ~ISDN_PID_FEATURE_MASK) {
                    	    case ISDN_PID_L3_DSS1USER: break;
                    	    default:
                        	ERROR("Given port %d: own protocol 0x%08x", port,stinf->pid.protocol[3]);
                	}
            	    }
            	    if (stinf->pid.protocol[4]) { ERROR("Given port %d: Layer 4 protocol not allowed.\n", port); }
    		}
	        /* add mISDNport structure */
                mISDNportp = &mISDNport_first;
                while(*mISDNportp)
                        mISDNportp = &((*mISDNportp)->next);
                mISDNport = (struct mISDNport *)malloc(sizeof(struct mISDNport));
                *mISDNportp = mISDNport;

		/* allocate ressources of port */
		mISDNport->d_stid = stinf->id;
		DBG("d_stid = 0x%x.\n", mISDNport->d_stid);
	        /* create layer intance */
	        memset(&li, 0, sizeof(li));
	        strcpy(&li.name[0], "te l4");
	        li.object_id = -1;
	        li.extentions = 0;
//	        li.pid.protocol[4] = ISDN_PID_L4_CAPI20;
	        li.pid.protocol[4] = ISDN_PID_L4_B_USER;
	        li.pid.layermask = ISDN_LAYER(4);
	        li.st = mISDNport->d_stid;
	        DBG("setting mISDN_new_layer on port %d, li.st=0x%08x \n",port,li.st);
	        ret = mISDN_new_layer(m_mISDNdevice, &li);
	        if (ret)  { ERROR("Cannot add layer4 of port %d (ret %d)\n", port, ret);
//	                mISDNport_close(mISDNport);
	                return FAIL;
	        }
	        mISDNport->upper_id = li.id;
	        ret = mISDN_register_layer(m_mISDNdevice, mISDNport->d_stid, mISDNport->upper_id);
	        if (ret) { ERROR("Cannot register layer4 of port %d\n", port);
//	                mISDNport_close(mISDNport);
			return FAIL;
		}
	        mISDNport->lower_id = mISDN_get_layerid(m_mISDNdevice, mISDNport->d_stid, 3);
	        if (mISDNport->lower_id < 0) { ERROR("Cannot get layer(%d) id of port %d\n", nt?1:3, port);
//	                mISDNport_close(mISDNport);
			return FAIL;
	        }
	        mISDNport->upper_id = mISDN_get_layerid(m_mISDNdevice, mISDNport->d_stid, 4);
	        if (mISDNport->upper_id < 0) { ERROR("Cannot get layer4 id of port %d\n", port);
//	                mISDNport_close(mISDNport);
			return FAIL;
	        }
	        DBG("Layer 4 of port %d added (0x%08x) lower_id=0x%08x.\n", port,mISDNport->upper_id,mISDNport->lower_id);

	        mISDNport->b_num = stinf->childcnt;
	        mISDNport->portnum = port;
	        mISDNport->ntmode = nt;
	        mISDNport->pri = pri;
	        mISDNport->ptp = ptp;
	        DBG("Port has %d b-channels.\n", mISDNport->b_num);
	        i = 0;
	        while(i < mISDNport->b_num) {
	                mISDNport->b_stid[i] = stinf->child[i];
	                DBG("b_stid[%d] = 0x%x.\n", i, mISDNport->b_stid[i]);
	                i++;
	        }
	        /* if te-mode, query state link */
//	        if (!mISDNport->ntmode) {
	                iframe_t act;
	                /* L2 */
	                act.prim = MGR_SHORTSTATUS | REQUEST;
	                act.addr = mISDNport->upper_id | MSG_BROADCAST;
	                act.dinfo = SSTATUS_BROADCAST_BIT | SSTATUS_ALL;
	                act.len = 0;
	                DBG("sending MGR_SHORTSTATUS request for port %d addr=0x%08x.\n", port,act.addr);
	                mISDN_write(m_mISDNdevice, &act, mISDN_HEADER_LEN+act.len, TIMEOUT_1SEC);
//	        }
			act.prim = PH_ACTIVATE| REQUEST;
			act.addr = mISDNport->upper_id | FLG_MSG_DOWN;
			act.dinfo = 0;
			act.len = 0;
			DBG("sending PH_ACTIVATE request (l1 up) for port %d addr=0x%08x.\n", port,act.addr);
			mISDN_write(m_mISDNdevice, &act, mISDN_HEADER_LEN+act.len, TIMEOUT_1SEC);
	        /* if ptp AND te-mode, pull up the link */
//	        if (mISDNport->ptp && !mISDNport->ntmode) {
//	                iframe_t act;
	                /* L2 */
	                act.prim = DL_ESTABLISH | REQUEST;
//	                act.addr = (mISDNport->upper_id & ~LAYER_ID_MASK) | 4 | FLG_MSG_DOWN;
	                act.addr = mISDNport->upper_id | FLG_MSG_DOWN;
	                act.dinfo = 0;
	                act.len = 0;
	                DBG("sending DL_ESTABLISH request (l2 up) for port %d addr=0x%08x.\n", port,act.addr);
	                mISDN_write(m_mISDNdevice, &act, mISDN_HEADER_LEN+act.len, TIMEOUT_1SEC);
//	        }
	        /* initially, we assume that the link is down, exept for nt-ptmp */
	        mISDNport->l2link = (mISDNport->ntmode && !mISDNport->ptp)?1:0;
	}
	return OK;
}

void mISDNStack::on_stop(void) {
        unsigned char buff[1025];
        DBG("mISDNStack::on_stop\n");
        if (m_mISDNdevice >= 0) {
                mISDN_write_frame(m_mISDNdevice, buff, 0, MGR_DELENTITY | REQUEST, m_entity, 0, NULL, TIMEOUT_1SEC);
                mISDN_close(m_mISDNdevice); m_mISDNdevice = -1;
                DBG("mISDN device closed.\n");
        }
}

int mISDNStack::GenerateCR() {
    int cr;
//llock()
    if (m_crcount++ > 0x7fff)
            m_crcount = 0x0001;
    cr=(m_entity<<16) | m_crcount;
//unlock();
    return cr;
}


                                                                                                                                        
void mISDNStack::run()
{
	DBG("running mISDNStack::run...\n");
	char msg_buf[MAX_MSG_SIZE];
	int  msg_buf_s;
	iframe_t* frame;
	mISDNport *port;
	mISDNChannel* channel;

	while(true){
//		DBG("tick\n");
		msg_buf_s = mISDN_read(m_mISDNdevice,&msg_buf,MAX_MSG_SIZE, TIMEOUT_10SEC);;
		if(msg_buf_s == -1){
			switch(errno){
		    		case EINTR:
		    		case EAGAIN:
			    		continue;
				default: break;
			};
			ERROR("running mISDNStack::run Error in mISDN_read %s\n",strerror(errno));
			break;
		}
		if(msg_buf_s == 0){/*DBG("got 0, cotinuing\n");*/	continue; }

		frame=(iframe_t*)msg_buf;
                if (frame->dinfo==(signed long)0xffffffff && frame->prim==(PH_DATA|CONFIRM)) {
                        ERROR("SERIOUS BUG, dinfo == 0xffffffff, prim == PH_DATA | CONFIRM !!!!\n");
                }
//		DBG("Got something msg_buf_s=%d prim=0x%08x addr=0x%08x dinfo=0x%08x\n",msg_buf_s,frame->prim,frame->addr,frame->dinfo);
	        port = mISDNport_first;
                while(port) {	
            	    if ((frame->addr&MASTER_ID_MASK) == (unsigned int)(port->upper_id&MASTER_ID_MASK)) 
            		    break; 
            	    port = port->next; 
            	}
		if (!port) {	
		    ERROR("message belongs to no mISDNport: prim(0x%x) addr(0x%x) msg->len(%d)\n", frame->prim, frame->addr, msg_buf_s); 
		    continue; 
		}
	        if (frame->addr&FLG_CHILD_STACK) { 		/* child stack */
                /* b-channel data and messages */
//                    DBG("processing child stack for %s (%d) prim(0x%x) addr(0x%x) dinfo=0x%x msg->len(%d) \n",port->name, port->portnum, frame->prim, frame->addr,frame->dinfo, msg_buf_s);
		    channel=FindBC(frame);
		    if(channel==NULL) {
			DBG("b-channel is not associated to an ISDNPort (address 0x%x), ignoring.\n", frame->addr);
			continue;
		    }
		    if(channel->bchan_event(msg_buf,msg_buf_s)!=OK)
			ERROR("Error processing bchan_event in channel object\n");
		    continue;
    	        } else {   /* d-message */
		    //next two are debug packet dumps uncomment if needed
		    l1l2l3_trace_header(port, frame->addr, frame->prim, 1);
		    if(msg_buf_s>16) DBG("IE: %s",mISDNStack::dumpIE(msg_buf,msg_buf_s).c_str());
		    /* general messages not(yet) related to CR */
        	    switch(frame->prim)   {
                        case CC_NEW_CR | INDICATION:
                    		DBG("CC_NEW_CR | INDICATION for %s (%d) \n",port->name, port->portnum);
                    		channel=NewCR(port,frame);
                    		continue;              		break;
                    	case CC_NEW_CR | CONFIRM:
                    		DBG("CC_NEW_CR | CONFIRM for %s (%d) Is this possible?\n",port->name, port->portnum);
                    		continue;             		break;
                        case CC_RELEASE_CR | INDICATION:
                    		DBG("CC_RELEASE_CR | INDICATION for %s (%d) \n",port->name, port->portnum);
                    		channel=FindCR(frame);
                    		if(channel!=NULL) {
                    			DBG("should delete channel=%p but we will leave it to have media procesor happy just unregister CR from map\n",channel);
					channel->unregister_CR();
//	                   		delete(channel);
				} else  ERROR("Channel not found for CC_RELEASE_CR | INDICATION %s (%d) prim(0x%x) addr(0x%x) msg->len(%d) \n",port->name, port->portnum, frame->prim, frame->addr, msg_buf_s);
                    		continue;              		break;
                    	case CC_RELEASE_CR | CONFIRM:
                    		DBG("CC_RELEASE_CR | CONFIRM for %s (%d) Is this possible?\n",port->name, port->portnum);
                    		continue;            		break;
                        case MGR_SHORTSTATUS | INDICATION:
                        case MGR_SHORTSTATUS | CONFIRM:
                    		DBG("MGR_SHORTSTATUS ind or confirm for %s (%d) prim(0x%x) addr(0x%x) msg->len(%d) \n",port->name, port->portnum, frame->prim, frame->addr, msg_buf_s);
	            		switch(frame->dinfo) {
    	            			case SSTATUS_L1_ACTIVATED:	port->l1link = 1;DBG("MGR_SHORTSTATUS->SSTATUS_L1_ACTIVATED for %s (%d) prim(0x%x) addr(0x%x) msg->len(%d) \n",port->name, port->portnum, frame->prim, frame->addr, msg_buf_s);break;
            	    			case SSTATUS_L1_DEACTIVATED:	port->l1link = 0;DBG("MGR_SHORTSTATUS->SSTATUS_L1_DEACTIVATED for %s (%d) prim(0x%x) addr(0x%x) msg->len(%d) \n",port->name, port->portnum, frame->prim, frame->addr, msg_buf_s);break;
            	    			case SSTATUS_L2_ESTABLISHED:	port->l2link = 1;DBG("MGR_SHORTSTATUS->SSTATUS_L2_ESTABLISHED for %s (%d) prim(0x%x) addr(0x%x) msg->len(%d) \n",port->name, port->portnum, frame->prim, frame->addr, msg_buf_s);break;
            	    			case SSTATUS_L2_RELEASED:	port->l2link = 0;DBG("MGR_SHORTSTATUS->SSTATUS_L2_RELEASED for %s (%d) prim(0x%x) addr(0x%x) msg->len(%d) \n",port->name, port->portnum, frame->prim, frame->addr, msg_buf_s);break;
            	    		}
            	    		continue;			break;
                        case PH_ACTIVATE | CONFIRM:
                        case PH_ACTIVATE | INDICATION:
                            DBG("PH_ACTIVATE ind or confirm for %s (%d) prim(0x%x) addr(0x%x) msg->len(%d) \n",port->name, port->portnum, frame->prim, frame->addr, msg_buf_s);
                            port->l1link = 1;
                    	    continue;				break;
                        case PH_DEACTIVATE | CONFIRM:
                        case PH_DEACTIVATE | INDICATION:
	                    DBG("PH_DEACTIVATE ind or confirm for %s (%d) prim(0x%x) addr(0x%x) msg->len(%d) \n",port->name, port->portnum, frame->prim, frame->addr, msg_buf_s);
			    port->l1link = 0;
    	                    continue;				break;
                        case PH_CONTROL | CONFIRM:
                        case PH_CONTROL | INDICATION:
        	            DBG("Received PH_CONTROL for port %d (%s).\n", port->portnum, port->name);
        	            continue;				break;
                        case DL_ESTABLISH | INDICATION:
                        case DL_ESTABLISH | CONFIRM:
                	    DBG("DL_ESTABLISH ind or confirm for %s (%d) prim(0x%x) addr(0x%x) msg->len(%d) \n",port->name, port->portnum, frame->prim, frame->addr, msg_buf_s);
                	    port->l2link = 1;
			    continue;				break;
                        case DL_RELEASE | INDICATION:
                        case DL_RELEASE | CONFIRM:
                            DBG("DL_RELEASE ind or confirm for %s (%d) prim(0x%x) addr(0x%x) msg->len(%d) \n",port->name, port->portnum, frame->prim, frame->addr, msg_buf_s);
                            port->l2link = 0;
                            continue;				break;
                        default:
                        DBG("GOT d-msg from %s port %d prim 0x%x dinfo 0x%x addr 0x%x\n", (port->ntmode)?"NT":"TE", port->portnum, frame->prim, frame->dinfo, frame->addr);
            	    }
                    /* d-message */
                    if (port->ntmode) {
			ERROR("NT mode not supported yet\n");
			continue;
            	    } else {
                	/* l3-data is sent to channel object for processing */
                	channel=FindCR(frame);
                	if(channel==NULL) {
                		ERROR("Cant find channel for message\n"); 
                		continue;
                	}
                	if(channel->processMsg(msg_buf,msg_buf_s)!=OK) 	
                		ERROR("Error processing msg in channel object\n");
                	continue;
            	    }
    		}

	}
}
////////////////////////////////////////////////////////////////
void mISDNStack::l1l2l3_trace_header(struct mISDNport *mISDNport, int port, unsigned long prim, int direction)
{

        string msgtext;
	msgtext.assign(mISDNNames::Message(prim&0xffffff00));
	msgtext.append(mISDNNames::isdn_prim[prim&0x00000003]);
	
        /* add direction */
        if (direction && (prim&0xffffff00)!=CC_NEW_CR && (prim&0xffffff00)!=CC_RELEASE_CR) {
                if (mISDNport) {
                        if (mISDNport->ntmode)	{ if (direction == DIRECTION_OUT) msgtext.append(" N->U"); else msgtext.append(" N<-U"); } 
                        else 			{ if (direction == DIRECTION_OUT) msgtext.append(" U->N"); else  msgtext.append(" U<-N"); }
                }
        }
        DBG("prim=0x%08lx port=0x%08x %s\n",prim,port, msgtext.c_str());
}

#define QI_DUMP(_x_,_y_) sprintf(x," %25s off=0x%04x ridx=0x%04x res1=0x%04x cs_flg=0x%04x",_y_,qi->_x_.off,qi->_x_.ridx,qi->_x_.res1,qi->_x_.cs_flg);ret.append(x);\
if(qi->_x_.off>0) {sprintf(x," 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx\n",\
y[qi->_x_.off],\
y[qi->_x_.off+1],\
y[qi->_x_.off+2],\
y[qi->_x_.off+3]\
);ret.append(x);} else ret.append(" \n");

std::string mISDNStack::dumpIE(char *buf,int len) {
	Q931_info_t* qi;
	ie_info_t *ie;
	char buf2[MAX_MSG_SIZE];
	u16  *arr;
	char *p,*x,*y;
	std::string ret,tmp_a,tmp_b;
	int j;
	size_t i;

	qi = (Q931_info_t*)(buf + mISDN_HEADER_LEN);
	x =(char*)&buf2;
	arr=(u16 *)qi;
	ie=&(qi->bearer_capability);
	p=(char*)qi;
	y=p;
	y+=L3_EXTRA_SIZE;
	sprintf(x,"type=0x%02hhx crlen=0x%02hhx cr=0x%04x\n",qi->type,qi->crlen,qi->cr);ret.append(x);
	for(i=0;i<37;i++) {
	    if(ie[i].off>0) {
		sprintf(x," %25s off=0x%04x ridx=0x%04x res1=0x%04x cs_flg=0x%04x",mISDNNames::IE_Names[i],ie[i].off,ie[i].ridx,ie[i].res1,ie[i].cs_flg);ret.append(x);
		tmp_a.assign("");tmp_b.assign(" ");
		for(j=0;j<y[ie[i].off+1];j++) {
		    sprintf(x," 0x%02hhx",y[ie[i].off+2+j]);tmp_a.append(x);
		    sprintf(x,"%c",(y[ie[i].off+2+j]>32)?y[ie[i].off+2+j]:'.');tmp_b.append(x);
		}
		ret.append(tmp_a);ret.append(tmp_b);ret.append("\n");
		if((ie[i].repeated>0) || (ie[i].ridx>0)) {
		    QI_DUMP(ext[i].ie,"extinfo                  ")
		    sprintf(x," extinfo[%d]:               cs.codeset=0x%04x cs.locked=0x%04x cs.res1=0x%04x cs.len=0x%04x | v.codeset=0x%04x v.res1=0x%04x v.val=0x%04x\n",
			i,qi->ext[i].cs.codeset,qi->ext[i].cs.locked,qi->ext[i].cs.res1,qi->ext[i].cs.len, 
			qi->ext[i].v.codeset,qi->ext[i].v.res1,qi->ext[i].v.val);
		    ret.append(x);	
		}
	    }
	}
	ret.append("=========================\n");
	for(i=0;i<=(len-mISDN_HEADER_LEN)/2;i++) {
	    sprintf(x," 0x%04x (%c %c),",arr[i], 
	    (p[2*i]>=32)?p[2*i]:'.',
	     ((p[2*i+1])>=32)?p[2*i+1]:'.');
	    ret.append(x);
	}
	ret.append("\n");
	if(len>(mISDN_HEADER_LEN+L3_EXTRA_SIZE)) {
		ret.append("tail:");
		for(i=0;i<=(len-(mISDN_HEADER_LEN+L3_EXTRA_SIZE));i++) {
			sprintf(x," 0x%02hhx (%c),",y[i],(y[i]>32)?y[i]:'.');ret.append(x);
		}
		ret.append("\n");
	} else ret.append("no tail\n");
	                                

	return ret;
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
