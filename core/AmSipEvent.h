#ifndef AmSipEvent_h
#define AmSipEvent_h

#include "AmEvent.h"
#include "AmSipReply.h"
#include "AmSipRequest.h"

/** \brief SIP event */
class AmSipEvent: public AmEvent
{
public:
    AmSipEvent(int id = -1)
	: AmEvent(id)
    {}
};

/** \brief SIP request event */
class AmSipRequestEvent: public AmSipEvent
{
public:
    AmSipRequest req;
    
    AmSipRequestEvent(const AmSipRequest& r)
	: AmSipEvent(-1), req(r)
    {}
};

/** \brief SIP reply event */
class AmSipReplyEvent: public AmSipEvent
{
public:
    AmSipReply reply;

    AmSipReplyEvent(const AmSipReply& r) 
	: AmSipEvent(),reply(r) {}
};


#endif
