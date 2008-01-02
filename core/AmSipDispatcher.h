#ifndef __AMSIPDISPATCHER_H__
#define __AMSIPDISPATCHER_H__

#include <vector>

#include "AmSipMsg.h"
#include "AmApi.h"

using std::vector;

class AmInterfaceHandler
{
  public:
    virtual void handleSipMsg(AmSipRequest &) = 0;
    virtual void handleSipMsg(AmSipReply &) = 0;

    virtual ~AmInterfaceHandler(){};
};

class AmSipDispatcher : public AmInterfaceHandler
{
  private:
    static AmSipDispatcher *_instance;
    vector<AmSIPEventHandler*> reply_handlers;

  public:
    void handleSipMsg(AmSipRequest &);
    void handleSipMsg(AmSipReply &);

    /** register a reply handler for incoming replies pertaining 
     * to a dialog without a session (not in SessionContainer) */
    void registerReplyHandler(AmSIPEventHandler* eh) 
    {
      reply_handlers.push_back(eh);
    }

    static AmSipDispatcher* instance();
};

#endif /* __AMDISPATCHER_H__ */
