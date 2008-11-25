#ifndef _MISDNSTACK_H_
#define _MISDNSTACK_H_

#include <pthread.h>
#include "globals.h"
namespace mISDN { //this is hack to cover definition of dprint which is defined in mISDN and sems core
extern "C" {
#include <mISDNuser/mISDNlib.h>
#include <mISDNuser/net_l2.h>
#include <mISDNuser/l3dss1.h>
}
}
#include "GWSession.h"

class mISDNChannel; //forward

struct mISDNport {
        mISDN::net_stack_t nst; /* MUST be the first entry, so &nst equals &mISDNlist */
        mISDN::manager_t mgr;
        int upper_id; /* id to transfer data down */
        int lower_id; /* id to transfer data up */
        int d_stid;
//        mISDN::msg_queue_t downqueue;          /* l4->l3 */
        struct mISDNport *next;
        char name[64]; /* name of port, if available */
        int portnum; /* port number 1..n */
        int ptp; /* if ptp is set, we keep track of l2link */
        int l1link; /* if l1 is available */
        int l2link; /* if l2 is available */
        int use; /* counts the number of port that uses this port */
        int ntmode; /* is TRUE if port is nt mode */
        int pri; /* is TRUE if port is a primary rate interface */
        int b_num; /* number of bchannels */
        class mISDNChannel *b_port[128]; /* bchannel assigned to port object */
        int b_stid[128]; /* stack ids */
        unsigned long b_addr[128]; /* addresses on b-channels */
        int b_state[128]; /* statemachine, 0 = IDLE */
};

//forward declaration;
class mISDNChannel;

class mISDNStack: public AmThread
{
	static mISDNStack* _instance;
	int init();
	void run();
	void on_stop();
	    
public:
	mISDNStack();
	~mISDNStack();

	int placeCall(const AmSipRequest &req, GWSession *session, const std::string &tonumber, const std::string &fromnumber);
	int m_mISDNdevice;
	int m_entity;
	int m_crcount;
	mISDNport *mISDNport_first;
	/* here are some maps and functions to lookup CallReference and B-channel number and decide to which channel object pass message */
	std::map<int,mISDNChannel*> CR_map;
	std::map<int,mISDNChannel*> BC_map;
	mISDNChannel* NewCR(mISDNport *port,mISDN::iframe_t *frame);
	mISDNChannel* FindCR(mISDN::iframe_t *frame);
	mISDNChannel* FindBC(mISDN::iframe_t *frame);
	
	static mISDNStack* instance();
	static int GetPortInfo();
	int GenerateCR();
	void l1l2l3_trace_header(struct mISDNport *mISDNport, int port, unsigned long prim, int direction);
	std::string dumpIE(char *buf,int len);

};
#define DIRECTION_NONE  0
#define DIRECTION_OUT   1
#define DIRECTION_IN    2
enum {
        B_STATE_IDLE,           /* not open */
        B_STATE_ACTIVATING,     /* DL_ESTABLISH sent */
        B_STATE_ACTIVE,         /* channel active */
        B_STATE_DEACTIVATING,   /* DL_RELEASE sent */
        B_STATE_EXPORTING,      /* BCHANNEL_ASSIGN sent */
        B_STATE_REMOTE,         /* bchannel assigned to remote application */
        B_STATE_IMPORTING,      /* BCHANNEL_REMOVE sent */
};
#define ISDN_PID_L3_B_USER 0x430000ff
#define ISDN_PID_L4_B_USER 0x440000ff                                                        
#endif


