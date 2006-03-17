#ifndef AmEvent_h
#define AmEvent_h

struct AmEvent
{
    int event_id;
    bool processed;

    AmEvent(int event_id);
    virtual ~AmEvent();
};

class AmEventHandler
{
public:
    virtual void process(AmEvent*)=0;
};

#endif
