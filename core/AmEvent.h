#ifndef AmEvent_h
#define AmEvent_h

#include "AmArg.h"

#include <string>
using std::string;

#define E_PLUGIN 100

struct AmEvent
{
    int event_id;
    bool processed;

    AmEvent(int event_id);
    virtual ~AmEvent();
};

struct AmPluginEvent: public AmEvent
{
    string      name;
    AmArgArray  data;

    AmPluginEvent(const string& n)
	: AmEvent(E_PLUGIN), name(n), data() {}

    AmPluginEvent(const string& n, const AmArgArray& d)
	: AmEvent(E_PLUGIN), name(n), data(d) {}
};



class AmEventHandler
{
public:
    virtual void process(AmEvent*)=0;
};

#endif
