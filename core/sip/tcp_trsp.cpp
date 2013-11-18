#include "tcp_trsp.h"
#include "ip_util.h"
#include "parse_common.h"
#include "sip_parser.h"
#include "trans_layer.h"
#include "hash.h"

#include "AmUtils.h"

#include <netdb.h>
#include <event2/event.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>


void tcp_trsp_socket::on_sock_read(int fd, short ev, void* arg)
{
  if(ev & (EV_READ|EV_TIMEOUT)){
    ((tcp_trsp_socket*)arg)->on_read(ev);
  }
}

void tcp_trsp_socket::on_sock_write(int fd, short ev, void* arg)
{
  if(ev & (EV_WRITE|EV_TIMEOUT)){
    ((tcp_trsp_socket*)arg)->on_write(ev);
  }
}

tcp_trsp_socket::tcp_trsp_socket(tcp_server_socket* server_sock,
				 tcp_server_worker* server_worker,
				 int sd, const sockaddr_storage* sa,
				 struct event_base* evbase)
  : trsp_socket(server_sock->get_if(),0,0,sd),
    server_sock(server_sock), server_worker(server_worker),
    closed(false), connected(false),
    input_len(0), evbase(evbase),
    read_ev(NULL), write_ev(NULL)
{
  // local address
  ip = server_sock->get_ip();
  port = server_sock->get_port();
  server_sock->copy_addr_to(&addr);

  // peer address
  memcpy(&peer_addr,sa,sizeof(sockaddr_storage));

  char host[NI_MAXHOST] = "";
  peer_ip = am_inet_ntop(&peer_addr,host,NI_MAXHOST);
  peer_port = am_get_port(&peer_addr);

  // async parser state
  pst.reset((char*)input_buf);

  if(sd > 0) {
    create_events();
  }
}

void tcp_trsp_socket::create_connected(tcp_server_socket* server_sock,
				       tcp_server_worker* server_worker,
				       int sd, const sockaddr_storage* sa,
				       struct event_base* evbase)
{
  if(sd < 0)
    return;

  tcp_trsp_socket* sock = new tcp_trsp_socket(server_sock,server_worker,
					      sd,sa,evbase);

  inc_ref(sock);
  server_worker->add_connection(sock);

  sock->connected = true;
  sock->add_read_event();
  dec_ref(sock);
}

tcp_trsp_socket* tcp_trsp_socket::new_connection(tcp_server_socket* server_sock,
						 tcp_server_worker* server_worker,
						 const sockaddr_storage* sa,
						 struct event_base* evbase)
{
  return new tcp_trsp_socket(server_sock,server_worker,-1,sa,evbase);
}


tcp_trsp_socket::~tcp_trsp_socket()
{
  DBG("********* connection destructor ***********");
  event_free(read_ev);
  event_free(write_ev);
}

void tcp_trsp_socket::create_events()
{
  read_ev = event_new(evbase, sd, EV_READ|EV_PERSIST,
		      tcp_trsp_socket::on_sock_read,
		      (void *)this);

  write_ev = event_new(evbase, sd, EV_WRITE,
		       tcp_trsp_socket::on_sock_write,
		       (void *)this);
}

void tcp_trsp_socket::add_read_event_ul()
{
  sock_mut.unlock();
  add_read_event();
  sock_mut.lock();  
}

void tcp_trsp_socket::add_read_event()
{
  event_add(read_ev, server_sock->get_idle_timeout());
}

void tcp_trsp_socket::add_write_event_ul(struct timeval* timeout)
{
  sock_mut.unlock();
  add_write_event(timeout);
  sock_mut.lock();
}

void tcp_trsp_socket::add_write_event(struct timeval* timeout)
{
  event_add(write_ev, timeout);
}

void tcp_trsp_socket::copy_peer_addr(sockaddr_storage* sa)
{
  memcpy(sa,&peer_addr,sizeof(sockaddr_storage));
}

tcp_trsp_socket::msg_buf::msg_buf(const sockaddr_storage* sa, const char* msg, 
				  const int msg_len)
  : msg_len(msg_len)
{
  memcpy(&addr,sa,sizeof(sockaddr_storage));
  cursor = this->msg = new char[msg_len];
  memcpy(this->msg,msg,msg_len);
}

tcp_trsp_socket::msg_buf::~msg_buf()
{
  delete [] msg;
}

int tcp_trsp_socket::on_connect(short ev)
{
  DBG("************ on_connect() ***********");

  if(ev & EV_TIMEOUT) {
    DBG("********** connection timeout on sd=%i ************\n",sd);
    close();
    return -1;
  }

  socklen_t len = sizeof(int);
  int error = 0;
  if(getsockopt(sd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
    ERROR("getsockopt: %s",strerror(errno));
    close();
    return -1;
  }

  if(error != 0) {
    DBG("*********** connection error (sd=%i): %s *********",
	sd,strerror(error));
    close();
    return -1;
  }

  connected = true;
  add_read_event();

  return 0;
}

int tcp_trsp_socket::connect()
{
  if(sd > 0) {
    ERROR("pending connection request: close first.");
    return -1;
  }

  if((sd = socket(peer_addr.ss_family,SOCK_STREAM,0)) == -1){
    ERROR("socket: %s\n",strerror(errno));
    return -1;
  } 

  int true_opt = 1;
  if(ioctl(sd, FIONBIO , &true_opt) == -1) {
    ERROR("could not make new connection non-blocking: %s\n",strerror(errno));
    ::close(sd);
    sd = -1;
    return -1;
  }

  DBG("connecting to %s:%i...",
      am_inet_ntop(&peer_addr).c_str(),
      am_get_port(&peer_addr));

  return ::connect(sd, (const struct sockaddr*)&peer_addr, 
		   SA_len(&peer_addr));
}

int tcp_trsp_socket::check_connection()
{
  if(sd < 0){
    int ret = connect();
    if(ret < 0) {
      if(errno != EINPROGRESS && errno != EALREADY) {
	ERROR("could not connect: %s",strerror(errno));
	::close(sd);
	sd = -1;
	return -1;
      }
    }

    // it's time to create the events...
    create_events();

    if(ret < 0) {
      add_write_event_ul(server_sock->get_connect_timeout());
      DBG("connect event added...");

      // because of unlock in ad_write_event_ul,
      // on_connect() might already have been scheduled
      if(closed)
	return -1;
    }
    else {
      // connect succeeded immediatly
      connected = true;
      add_read_event_ul();
    }
  }

  return 0;
}

int tcp_trsp_socket::send(const sockaddr_storage* sa, const char* msg, 
			  const int msg_len, unsigned int flags)
{
  AmLock _l(sock_mut);

  if(closed || (check_connection() < 0))
    return -1;

  send_q.push_back(new msg_buf(sa,msg,msg_len));

  if(connected) {
    add_write_event_ul();
    DBG("write event added...");
  }

  return 0;
}

void tcp_trsp_socket::close()
{
  inc_ref(this);
  server_worker->remove_connection(this);

  closed = true;
  DBG("********* closing connection ***********");

  event_del(read_ev);
  event_del(write_ev);

  if(sd > 0) {
    ::close(sd);
    sd = -1;
  }

  generate_transport_errors();
  dec_ref(this);
}

void tcp_trsp_socket::generate_transport_errors()
{
  while(!send_q.empty()) {

    msg_buf* msg = send_q.front();
    send_q.pop_front();

    sip_msg s_msg(msg->msg,msg->msg_len);
    delete msg;

    copy_peer_addr(&s_msg.remote_ip);
    copy_addr_to(&s_msg.local_ip);

    trans_layer::instance()->transport_error(&s_msg);
  }
}

void tcp_trsp_socket::on_read(short ev)
{
  int bytes = 0;
  char* old_cursor = (char*)get_input();

  {// locked section

    if(ev & EV_TIMEOUT) {
      DBG("************ idle timeout: closing connection **********");
      close();
      return;
    }

    AmLock _l(sock_mut);
    DBG("on_read (connected = %i)",connected);

    bytes = ::read(sd,get_input(),get_input_free_space());
    if(bytes < 0) {
      switch(errno) {
      case EAGAIN:
	return; // nothing to read

      case ECONNRESET:
      case ENOTCONN:
	DBG("connection has been closed (sd=%i)",sd);
	close();
	return;

      case ETIMEDOUT:
	DBG("transmission timeout (sd=%i)",sd);
	close();
	return;

      default:
	DBG("unknown error (%i): %s",errno,strerror(errno));
	close();
	return;
      }
    }
    else if(bytes == 0) {
      // connection closed
      DBG("connection has been closed (sd=%i)",sd);
      close();
      return;
    }
  }// end of - locked section

  input_len += bytes;

  DBG("received: <%.*s>",bytes,old_cursor);

  // ... and parse it
  if(parse_input() < 0) {
    DBG("Error while parsing input: closing connection!");
    sock_mut.lock();
    close();
    sock_mut.unlock();
  }
}

int tcp_trsp_socket::parse_input()
{
  for(;;) {
    int err = skip_sip_msg_async(&pst, (char*)(input_buf+input_len));
    if(err) {

      if(err == UNEXPECTED_EOT) {

	if(pst.orig_buf > (char*)input_buf) {

	  int addr_shift = pst.orig_buf - (char*)input_buf;
	  memmove(input_buf, pst.orig_buf, input_len - addr_shift);

	  pst.orig_buf = (char*)input_buf;
	  pst.c -= addr_shift;
	  if(pst.beg)
	    pst.beg -= addr_shift;
	  input_len -= addr_shift;

	  return 0;
	}
	else if(get_input_free_space()){
	  return 0;
	}

	ERROR("message way too big! drop connection...");
      }
      else {
	ERROR("parsing error %i",err);
      }

      pst.reset((char*)input_buf);
      reset_input();

      return -1;
    }

    int msg_len = pst.get_msg_len();
    DBG("received msg:\n%.*s",msg_len,pst.orig_buf);

    sip_msg* s_msg = new sip_msg((const char*)pst.orig_buf,msg_len);

    copy_peer_addr(&s_msg->remote_ip);
    copy_addr_to(&s_msg->local_ip);

    s_msg->local_socket = this;
    inc_ref(this);

    // pass message to the parser / transaction layer
    trans_layer::instance()->received_msg(s_msg);

    char* msg_end = pst.orig_buf + msg_len;
    char* input_end = (char*)input_buf + input_len;

    if(msg_end < input_end) {
      pst.reset(msg_end);
    }
    else {
      pst.reset((char*)input_buf);
      reset_input();
      return 0;
    }
  }

  // fake:
  //return 0;
}

void tcp_trsp_socket::on_write(short ev)
{
  AmLock _l(sock_mut);

  DBG("on_write (connected = %i)",connected);
  if(!connected) {
    if(on_connect(ev) != 0) {
      return;
    }
  }

  while(!send_q.empty()) {

    msg_buf* msg = send_q.front();
    if(!msg || !msg->bytes_left()) {
      send_q.pop_front();
      delete msg;
      continue;
    }

    // send msg
    int bytes = write(sd,msg->cursor,msg->bytes_left());
    if(bytes < 0) {
      DBG("error on write: %i",bytes);
      switch(errno){
      case EINTR:
      case EAGAIN: // would block
	add_write_event();
	break;

      default: // unforseen error: close connection
	ERROR("unforseen error: close connection (%i/%s)",
	      errno,strerror(errno));
	close();
	break;
      }
      return;
    }

    DBG("bytes written: <%.*s>",bytes,msg->cursor);

    if(bytes < msg->bytes_left()) {
      msg->cursor += bytes;
      add_write_event();
      return;
    }

    send_q.pop_front();
    delete msg;
  }
}

tcp_server_worker::tcp_server_worker(tcp_server_socket* server_sock)
  : server_sock(server_sock)
{
  evbase = event_base_new();
}

tcp_server_worker::~tcp_server_worker()
{
  event_base_free(evbase);
}

void tcp_server_worker::add_connection(tcp_trsp_socket* client_sock)
{
  string conn_id = client_sock->get_peer_ip()
    + ":" + int2str(client_sock->get_peer_port());

  DBG("new TCP connection from %s:%u",
      client_sock->get_peer_ip().c_str(),
      client_sock->get_peer_port());

  connections_mut.lock();
  map<string,tcp_trsp_socket*>::iterator sock_it = connections.find(conn_id);
  if(sock_it != connections.end()) {
    dec_ref(sock_it->second);
    sock_it->second = client_sock;
  }
  else {
    connections[conn_id] = client_sock;
  }
  inc_ref(client_sock);
  connections_mut.unlock();
}

void tcp_server_worker::remove_connection(tcp_trsp_socket* client_sock)
{
  string conn_id = client_sock->get_peer_ip()
    + ":" + int2str(client_sock->get_peer_port());

  DBG("removing TCP connection from %s",conn_id.c_str());

  connections_mut.lock();
  map<string,tcp_trsp_socket*>::iterator sock_it = connections.find(conn_id);
  if(sock_it != connections.end()) {
    dec_ref(sock_it->second);
    connections.erase(sock_it);
    DBG("TCP connection from %s removed",conn_id.c_str());
  }
  connections_mut.unlock();
}

int tcp_server_worker::send(const sockaddr_storage* sa, const char* msg,
			    const int msg_len, unsigned int flags)
{
  char host_buf[NI_MAXHOST];
  string dest = am_inet_ntop(sa,host_buf,NI_MAXHOST);
  dest += ":" + int2str(am_get_port(sa));

  tcp_trsp_socket* sock = NULL;

  bool new_conn=false;
  connections_mut.lock();
  map<string,tcp_trsp_socket*>::iterator sock_it = connections.find(dest);
  if(sock_it != connections.end()) {
    sock = sock_it->second;
    inc_ref(sock);
  }
  else {
    //TODO: add flags to avoid new connections (ex: UAs behind NAT)
    tcp_trsp_socket* new_sock = tcp_trsp_socket::new_connection(server_sock,this,
								sa,evbase);
    connections[dest] = new_sock;
    inc_ref(new_sock);

    sock = new_sock;
    inc_ref(sock);
    new_conn = true;
  }
  connections_mut.unlock();

  // must be done outside from connections_mut
  // to avoid dead-lock with the event base
  int ret = sock->send(sa,msg,msg_len,flags);
  if((ret < 0) && new_conn) {
    remove_connection(sock);
  }
  dec_ref(sock);

  return ret;
}

void tcp_server_worker::run()
{
  // fake event to prevent the event loop from exiting
  int fake_fds[2];
  pipe(fake_fds);
  struct event* ev_default =
    event_new(evbase,fake_fds[0],
	      EV_READ|EV_PERSIST,
	      NULL,NULL);
  event_add(ev_default,NULL);

  /* Start the event loop. */
  int ret = event_base_dispatch(evbase);

  // clean-up fake fds/event
  event_free(ev_default);
  close(fake_fds[0]);
  close(fake_fds[1]);
}

void tcp_server_worker::on_stop()
{
  event_base_loopbreak(evbase);
}

tcp_server_socket::tcp_server_socket(unsigned short if_num)
  : trsp_socket(if_num,0),
    evbase(NULL), ev_accept(NULL)
{
}

int tcp_server_socket::bind(const string& bind_ip, unsigned short bind_port)
{
  if(sd){
    WARN("re-binding socket\n");
    close(sd);
  }

  if(am_inet_pton(bind_ip.c_str(),&addr) == 0){
	
    ERROR("am_inet_pton(%s): %s\n",bind_ip.c_str(),strerror(errno));
    return -1;
  }
    
  if( ((addr.ss_family == AF_INET) && 
       (SAv4(&addr)->sin_addr.s_addr == INADDR_ANY)) ||
      ((addr.ss_family == AF_INET6) && 
       IN6_IS_ADDR_UNSPECIFIED(&SAv6(&addr)->sin6_addr)) ){

    ERROR("Sorry, we cannot bind to 'ANY' address\n");
    return -1;
  }

  am_set_port(&addr,bind_port);

  if((sd = socket(addr.ss_family,SOCK_STREAM,0)) == -1){
    ERROR("socket: %s\n",strerror(errno));
    return -1;
  } 

  int true_opt = 1;
  if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
		(void*)&true_opt, sizeof (true_opt)) == -1) {
    
    ERROR("%s\n",strerror(errno));
    close(sd);
    return -1;
  }

  if(ioctl(sd, FIONBIO , &true_opt) == -1) {
    ERROR("setting non-blocking: %s\n",strerror(errno));
    close(sd);
    return -1;
  }

  if(::bind(sd,(const struct sockaddr*)&addr,SA_len(&addr)) < 0) {

    ERROR("bind: %s\n",strerror(errno));
    close(sd);
    return -1;
  }

  if(::listen(sd, 16) < 0) {
    ERROR("listen: %s\n",strerror(errno));
    close(sd);
    return -1;
  }

  port = bind_port;
  ip   = bind_ip;

  DBG("TCP transport bound to %s/%i\n",ip.c_str(),port);

  return 0;
}

void tcp_server_socket::on_accept(int fd, short ev, void* arg)
{
  tcp_server_socket* trsp = (tcp_server_socket*)arg;
  trsp->on_accept(fd,ev);
}

uint32_t tcp_server_socket::hash_addr(const sockaddr_storage* addr)
{
  unsigned int port = am_get_port(addr);
  uint32_t h=0;
  if(addr->ss_family == AF_INET) {
    h = hashlittle(&SAv4(addr)->sin_addr,sizeof(in_addr),port);
  }
  else {
    h = hashlittle(&SAv6(addr)->sin6_addr,sizeof(in6_addr),port);
  }
  return h;
}

void tcp_server_socket::add_event(struct event_base *evbase)
{
  this->evbase = evbase;

  if(!ev_accept) {
    ev_accept = event_new(evbase, sd, EV_READ|EV_PERSIST,
			  tcp_server_socket::on_accept, (void *)this);
    event_add(ev_accept, NULL); // no timeout
  }
}

void tcp_server_socket::add_threads(unsigned int n)
{
  for(unsigned int i=0; i<n; i++) {
    workers.push_back(new tcp_server_worker(this));
  }
}

void tcp_server_socket::start_threads()
{
  for(unsigned int i=0; i<workers.size(); i++) {
    workers[i]->start();
  }
}

void tcp_server_socket::stop_threads()
{
  for(unsigned int i=0; i<workers.size(); i++) {
    workers[i]->stop();
  }
}

void tcp_server_socket::on_accept(int sd, short ev)
{
  sockaddr_storage src_addr;
  socklen_t        src_addr_len = sizeof(sockaddr_storage);

  int connection_sd = accept(sd,(sockaddr*)&src_addr,&src_addr_len);
  if(connection_sd < 0) {
    WARN("error while accepting connection");
    return;
  }

  int true_opt = 1;
  if(ioctl(connection_sd, FIONBIO , &true_opt) == -1) {
    ERROR("could not make new connection non-blocking: %s\n",strerror(errno));
    close(connection_sd);
    return;
  }

  uint32_t h = hash_addr(&src_addr);
  unsigned int idx = h % workers.size();
  
  // in case of thread pooling, do following in worker thread
  DBG("tcp_trsp_socket::create_connected (idx = %u)",idx);
  tcp_trsp_socket::create_connected(this,workers[idx],connection_sd,
				    &src_addr,evbase);
}

int tcp_server_socket::send(const sockaddr_storage* sa, const char* msg,
			    const int msg_len, unsigned int flags)
{
  uint32_t h = hash_addr(sa);
  unsigned int idx = h % workers.size();
  DBG("tcp_server_socket::send: idx = %u",idx);
  return workers[idx]->send(sa,msg,msg_len,flags);
}

void tcp_server_socket::set_connect_timeout(unsigned int ms)
{
  connect_timeout.tv_sec = ms / 1000;
  connect_timeout.tv_usec = (ms % 1000) * 1000;
}

void tcp_server_socket::set_idle_timeout(unsigned int ms)
{
  idle_timeout.tv_sec = ms / 1000;
  idle_timeout.tv_usec = (ms % 1000) * 1000;
}

struct timeval* tcp_server_socket::get_connect_timeout()
{
  if(connect_timeout.tv_sec || connect_timeout.tv_usec)
    return &connect_timeout;

  return NULL;
}

struct timeval* tcp_server_socket::get_idle_timeout()
{
  if(idle_timeout.tv_sec || idle_timeout.tv_usec)
    return &idle_timeout;

  return NULL;
}

tcp_trsp::tcp_trsp(tcp_server_socket* sock)
    : transport(sock)
{
  evbase = event_base_new();
  sock->add_event(evbase);
}

tcp_trsp::~tcp_trsp()
{
  if(evbase) {
    event_base_free(evbase);
  }
}

/** @see AmThread */
void tcp_trsp::run()
{
  int server_sd = sock->get_sd();
  if(server_sd <= 0){
    ERROR("Transport instance not bound\n");
    return;
  }

  tcp_server_socket* tcp_sock = static_cast<tcp_server_socket*>(sock);
  tcp_sock->start_threads();

  INFO("Started SIP server TCP transport on %s:%i\n",
       sock->get_ip(),sock->get_port());

  /* Start the event loop. */
  int ret = event_base_dispatch(evbase);

  INFO("TCP SIP server on %s:%i finished (%i)",
       sock->get_ip(),sock->get_port(),ret);
}

/** @see AmThread */
void tcp_trsp::on_stop()
{
  event_base_loopbreak(evbase);
  tcp_server_socket* tcp_sock = static_cast<tcp_server_socket*>(sock);
  tcp_sock->stop_threads();
}

