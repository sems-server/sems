#ifndef AmInterfaceHandler_h
#define AmInterfaceHandler_h

#include "AmThread.h"
#include "AmSipRequest.h"

#include <string>
#include <map>

using std::string;
using std::map;

class AmCtrlInterface;
/** \brief interface of an InterfaceHandler */
class AmInterfaceHandler
{
public:
    virtual ~AmInterfaceHandler();

    /** @return -1 on parsing error, 0 on success. 
     *  Throws string on other error cases.
     */
    virtual int handleRequest(AmCtrlInterface* ctrl)=0;
};

/**
 * \brief interface of a Server function
 *
 * Derive your class from this if you want
 * to implement a request handler.
 */
class AmRequestHandlerFct
{
public:
    virtual int execute(AmCtrlInterface* ctrl, const string& cmd)=0;
    virtual ~AmRequestHandlerFct(){}
};

/**
 * \brief SIP request handler
 *
 * Handles SIP requests that are received by the Server
 */
class AmRequestHandler: public AmInterfaceHandler, 
			public AmRequestHandlerFct
{
    AmMutex                          fct_map_mut;
    map<string,AmRequestHandlerFct*> fct_map;

public:
    static AmRequestHandler* get();

    int  handleRequest(AmCtrlInterface* ctrl);
    int  execute(AmCtrlInterface* ctrl, const string& cmd);
    void dispatch(AmSipRequest& req);
    
    AmRequestHandlerFct* getFct(const string& name);
    void registerFct(const string& name, AmRequestHandlerFct* fct);
};

/**
 * \brief SIP reply handler
 *
 * Handles SIP replys that are received by the Server
 */
class AmReplyHandler: public AmInterfaceHandler
{
    static AmReplyHandler* _instance;
    AmCtrlInterface* m_ctrl;

    AmReplyHandler(AmCtrlInterface* ctrl);
//     void cleanup(AmCtrlInterface* ctrl);

public:
    static AmReplyHandler* get();

    AmCtrlInterface* getCtrl() { return m_ctrl; }

//     AmCtrlInterface* getNewCtrl(const string& callid,
// 				const string& remote_tag,
// 				const string& local_tag,
// 				int           cseq);
    
    int handleRequest(AmCtrlInterface* ctrl);
};

#endif
