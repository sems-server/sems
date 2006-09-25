#ifndef AmEvent_h
#define AmEvent_h

#include "AmArg.h"

#include <string>
using std::string;

#define E_PLUGIN 100

/** \brief base event class */
struct AmEvent
{
    int event_id;
    bool processed;

    AmEvent(int event_id);
    virtual ~AmEvent();
};

/** 
 * \brief named event for inter-plugin-API 
 *
 * Optionally the AmPluginEvent also holds a dynamic argument array.
 */
struct AmPluginEvent: public AmEvent
{
    string      name;
    AmArgArray  data;

    AmPluginEvent(const string& n)
	: AmEvent(E_PLUGIN), name(n), data() {}

    AmPluginEvent(const string& n, const AmArgArray& d)
	: AmEvent(E_PLUGIN), name(n), data(d) {}
};


/** \brief event handler interface */
class AmEventHandler
{
public:
    virtual void process(AmEvent*)=0;
    virtual ~AmEventHandler() { };
};

#endif
