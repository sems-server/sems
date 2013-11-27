#ifndef _tcp_trsp_h_
#define _tcp_trsp_h_

#include "transport.h"
#include "sip_parser_async.h"

#include <vector>
using std::vector;

/**
 * Maximum message length for TCP
 * not including terminating '\0'
 */
#define MAX_TCP_MSGLEN 65535

#include <sys/socket.h>
#include <event2/event.h>

#include <map>
#include <deque>
#include <string>
using std::map;
using std::deque;
using std::string;

class tcp_server_worker;
class tcp_server_socket;

class tcp_trsp_socket: public trsp_socket
{
  tcp_server_socket* server_sock;
  tcp_server_worker* server_worker;
  
  bool             closed;
  bool             connected;
  sockaddr_storage peer_addr;
  string           peer_ip;
  unsigned short   peer_port;
  bool             peer_addr_valid;
  
  parser_state     pst;
  unsigned char    input_buf[MAX_TCP_MSGLEN];
  int              input_len;

  struct event_base* evbase;
  struct event*      read_ev;
  struct event*      write_ev;

  struct msg_buf {
    sockaddr_storage addr;
    char*            msg;
    int              msg_len;
    char*            cursor;
    
    msg_buf(const sockaddr_storage* sa, const char* msg, 
	    const int msg_len);
    ~msg_buf();

    int bytes_left() { return msg_len - (cursor - msg); }
  };

  deque<msg_buf*> send_q;
  
  AmMutex sock_mut;

  unsigned char*   get_input() { return input_buf + input_len; }
  int              get_input_free_space() {
    if(input_len > MAX_TCP_MSGLEN) return 0;
    return MAX_TCP_MSGLEN - input_len;
  }

  void reset_input() {
    input_len = 0;
  }

  int parse_input();

  /** fake implementation: we will never bind a connection socket */
  int bind(const string& address, unsigned short port) {
    return 0;
  }

  /**
   * Instantiates read_ev & write_ev
   * Warning: call only ONCE!!!
   */
  void create_events();

  /* 
   * Connects the socket to the destination
   * given in constructor.
   */
  int connect();

  /**
   * Checks whether or not the socket is already connected.
   * If not, a new connection will be tried.
   */
  int check_connection();

  /**
   * Closes the connection/socket
   *
   * Warning: never do anything with the object
   *          after close as it could have been 
   *          destroyed.
   */
  void close();

  /**
   * Generates a transport error for each request
   * left in send queue.
   */
  void generate_transport_errors();

  /**
   * Adds persistent read-event to event base.
   */
  void add_read_event();

  /**
   * Adds one-shot write-event to event base.
   */
  void add_write_event(struct timeval* timeout=NULL);

  /**
   * Same as add_read_event() but unlock before
   * calling event_add().
   */
  void add_read_event_ul();

  /**
   * Same as add_write_event() but unlock before
   * calling event_add().
   */
  void add_write_event_ul(struct timeval* timeout=NULL);

  int  on_connect(short ev);
  void on_write(short ev);
  void on_read(short ev);

  static void on_sock_read(int fd, short ev, void* arg);
  static void on_sock_write(int fd, short ev, void* arg);

  tcp_trsp_socket(tcp_server_socket* server_sock,
		  tcp_server_worker* server_worker,
		  int sd, const sockaddr_storage* sa,
		  struct event_base* evbase);

public:
  static void create_connected(tcp_server_socket* server_sock,
			       tcp_server_worker* server_worker,
			       int sd, const sockaddr_storage* sa,
			       struct event_base* evbase);

  static tcp_trsp_socket* new_connection(tcp_server_socket* server_sock,
					 tcp_server_worker* server_worker,
					 const sockaddr_storage* sa,
					 struct event_base* evbase);
  ~tcp_trsp_socket();

  const char* get_transport() const { return "tcp"; }
  bool        is_reliable() const   { return true; }

  void copy_peer_addr(sockaddr_storage* sa);

  const string& get_peer_ip() { 
    return peer_ip; 
  }

  unsigned short get_peer_port() { 
    return peer_port;
  }

  bool is_connected() {
    return connected;
  }

  /**
   * Sends a message (push it to send-queue).
   * @return -1 if error(s) occured.
   */
  int send(const sockaddr_storage* sa, const char* msg,
	   const int msg_len, unsigned int flags);
};

class tcp_server_worker
  : public AmThread
{
  struct event_base* evbase;
  tcp_server_socket* server_sock;

  AmMutex                      connections_mut;
  map<string,tcp_trsp_socket*> connections;

protected:
  void run();
  void on_stop();

public:
  tcp_server_worker(tcp_server_socket* server_sock);
  ~tcp_server_worker();

  int send(const sockaddr_storage* sa, const char* msg,
	   const int msg_len, unsigned int flags);

  void add_connection(tcp_trsp_socket* client_sock);
  void remove_connection(tcp_trsp_socket* client_sock);
};

class tcp_server_socket: public trsp_socket
{
  struct event_base* evbase;
  struct event*      ev_accept;

  vector<tcp_server_worker*> workers;

  /**
   * Timeout while connecting to a remote peer.
   */
  struct timeval connect_timeout;

  /**
   * Idle Timeout before closing a connection.
   */
  struct timeval idle_timeout;

  /* callback on new connection */
  void on_accept(int sd, short ev);

  /* libevent callback on new connection */
  static void on_accept(int sd, short ev, void* arg);

  static uint32_t hash_addr(const sockaddr_storage* addr);

public:
  tcp_server_socket(unsigned short if_num);
  ~tcp_server_socket() {}

  void add_threads(unsigned int n);
  void start_threads();
  void stop_threads();

  const char* get_transport() const { return "tcp"; }
  bool        is_reliable() const   { return true; }

  /* activates libevent on_accept callback */
  void add_event(struct event_base *evbase);

  int bind(const string& address, unsigned short port);
  int send(const sockaddr_storage* sa, const char* msg,
	   const int msg_len, unsigned int flags);

  /**
   * Set timeout in milliseconds for the connection
   * establishement handshake.
   */
  void set_connect_timeout(unsigned int ms);

  /**
   * Set idle timeout in milliseconds for news connections.
   * If during this period of time no packet is received,
   * the connection will be closed.
   */
  void set_idle_timeout(unsigned int ms);

  struct timeval* get_connect_timeout();
  struct timeval* get_idle_timeout();
};

class tcp_trsp: public transport
{
  struct event_base *evbase;

protected:
  /** @see AmThread */
  void run();
  /** @see AmThread */
  void on_stop();
    
public:
  /** @see transport */
  tcp_trsp(tcp_server_socket* sock);
  ~tcp_trsp();
};

#endif
