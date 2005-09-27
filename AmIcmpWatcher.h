#ifndef _AmIcmpWatcher_h_
#define _AmIcmpWatcher_h_

#include "AmThread.h"

#include <map>
using std::map;

#define ICMP_BUF_SIZE 512

class AmRtpStream;

class AmIcmpWatcher: public AmThread
{
    static AmIcmpWatcher* _instance;

    /* RAW socket descriptor */
    int raw_sd;

    /* RTP Stream map */
    map<int,AmRtpStream*> stream_map;
    AmMutex               stream_map_m;

    /* constructor & destructor are
     * private as we want a singleton.
     */
    AmIcmpWatcher();
    ~AmIcmpWatcher();

    void run();
    void on_stop();

public:
    static AmIcmpWatcher* instance();
    void addStream(int localport, AmRtpStream* str);
    void removeStream(int localport);
};

class IcmpReporter: public AmThread
{
    AmRtpStream* rtp_str;
    void run();
    void on_stop();
public:
    IcmpReporter(AmRtpStream* str);
};

#endif
