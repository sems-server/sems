
#ifndef __UNIXCTRLINTERFACE_H__
#define __UNIXCTRLINTERFACE_H__


#include "AmApi.h"
#include "AmSipDispatcher.h"
#include "UnixSocketAdapter.h"


// The SEMS - SER-0.9.6 unix socket message exchange:
//  - requests(*) are sent from SER to a well known SEMS socket (sems_sock); 
//  SER doesn't expect a reply from SEMS, to ack(**). the good receiving.
//  - requests and replies are sent from SEMS to a well known SER socket
//  (ser_sock). 
//  If processing the request goes fine, no reply is sent back(***), to
//  ack. it; if it goes bad, a "500 Bla" is replied immediately, using the
//  same socket that SER received the request on.
//  The reply is always ack'ed, also using the same socket.
//  - replies (to SEMS requests) are sent from SER to SEMS to another well
//  known socket (sems_rsp_sock). SER doesn't expect an ack.
//
//  (*) in SIP terms; so, these are unix-socket-interface messages describing
//  a SIP message.
//  (**) an "ack" here means a one line string of pattern "200 All went fine",
//  not an SIP ACK.
//  (***) with the notable exception of CANCEL, for which a reply is always
//  sent back.
class UnixCtrlInterface : public AmCtrlInterface
{
  static UnixCtrlInterface* _instance;

 private:
  bool loaded;
    // listeners for requests from SER, for SIP requests and replies
    UnixSocketAdapter reqAdapt, rplAdapt;
    // this adapter is only used for sending replies
    UnixSocketAdapter sndAdapt; 
    // handler of requests (SIP request | reply) received from SER
    AmInterfaceHandler *sipDispatcher;
    // paths to unix sockets where:
    string socket_name; // SEMS listens for SIP requests from SER
    string reply_socket_name; // SEMS listens for SIP replies from SER
    string ser_socket_name; //SER listens for SIP requests|replies from SEMS
    int load();
  protected:
    // AmCtrlInterface:AmThread
    void run();
    void on_stop() {}

  public:
    UnixCtrlInterface(const string &name);
    ~UnixCtrlInterface();

    // AmPluginFactory
    int onLoad();
    // AmCtrlInterface
    int send(const AmSipRequest &, string &);
    int send(const AmSipReply &);
    string localURI(const string &displayName, 
        const string &userName, const string &hostName, 
        const string &uriParams, const string &hdrParams);

    void registerInterfaceHandler(AmInterfaceHandler *handler)
    {
      sipDispatcher = handler;
    }
    AmCtrlInterface* instance() { return _instance; }
};

#endif /* __UNIXCTRLINTERFACE_H__ */
