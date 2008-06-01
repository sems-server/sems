#ifndef __AMSIPDISPATCHER_H__
#define __AMSIPDISPATCHER_H__

#include <vector>

#include "AmSipMsg.h"
#include "AmApi.h"

using std::vector;

class AmSipDispatcher
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
