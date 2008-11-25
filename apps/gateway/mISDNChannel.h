#ifndef _mISDNCHANNEL_H_
#define _mISDNCHANNEL_H_

#include "AmApi.h"
#include "GWSession.h"
//#include "GWCall.h"
#include "globals.h"
#include "mISDNStack.h"

extern char flip_table[256];
void init_flip_bits(void);
class mISDNChannel: public AmAudio {

public:
	/* AmAudio interface */
	int read(unsigned int user_ts, unsigned int size);
	int write(unsigned int user_ts, unsigned int size);
	/* buffers for audio data */
	//this buffer is feeded by isdn in 128byte chunks, while media processor eats 160bytes each time.
	std::string fromISDN_buffer;
//	pthread_mutex_t fromISDN_lock;
//	std::string toISDN_buffer;
//	pthread_mutex_t toISDN_lock;

	int m_CR;  //call reference (dinfo)
	int m_BC;  //b-channel (addr)
	mISDNport *m_port;
	char m_bchannel;
	
	char m_last_msg[MAX_MSG_SIZE]; 	//we copy here packet from kernel
        int  m_last_msg_s;
        mISDN::iframe_t* m_frame;		//there are pointers to places in m_last_msg buffer
        mISDN::Q931_info_t* m_qi;		//
        char* m_ie_data; 			//

	std::string m_caller;			// caller number
	int m_TON_r,m_NPI_r,m_Screening_r,m_Presentation_r;     //caler number attributes
	std::string m_called;			//calee number
	int m_TON_d,m_NPI_d;     		//calee number attributes

	mISDNChannel(); /* constructor */
	mISDNChannel(mISDNport *port);
	~mISDNChannel();
	void init();
	void setSession(GWSession*);
	GWSession* getSession();
	void unregister_CR();
	void unregister_BC();


	int processMsg(char *msg_buf,int msg_buf_s);
	int bchan_event(char *msg_buf,int msg_buf_s);
	int placeCall(const AmSipRequest &req, std::string tonumber, std::string fromnumber); /* place a call */
	int call();
	int accept(); /* accept a call */
	int hangup(); /* hangup a call */

//ie processing functions
	int GetIEchannel_id();
	int GetCallerNum();
	int GetCalledNum();
	
	private:
	GWSession* m_session;

	int bchan_create();
	int bchan_activate();
	int bchan_deactivate();
	int bchan_destroy();
	void bchan_receive(char *msg_buf,int msg_buf_s);
//	void bchan_send();

//	int id;
};
#endif /*header*/
