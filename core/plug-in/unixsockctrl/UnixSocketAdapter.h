
#ifndef __UNIXSOCKETADAPTER_H__
#define __UNIXSOCKETADAPTER_H__

#include <string.h>

#ifndef UNIX_PATH_MAX
#include <sys/un.h>
#define UNIX_PATH_MAX sizeof(((struct sockaddr_un *)0)->sun_path)
#endif
#define CTRL_MSGBUF_SIZE 2048

using std::string;

/** 
 * \brief UNIX socket control interface
 *
 * The FIFO / Unix socket Server.
 * 
 * Enables Ser to send request throught FIFO file.
 * 
 * Syntax for FIFO commands:<br>
 *   <pre>
 *   version  EOL // 'x.x'
 *   command  EOL // fct_name.{plugin_name,'bye'}
 *   method   EOL
 *   user     EOL 
 *   dstip    EOL // important for the RTP connection IP
 *   from     EOL
 *   to       EOL
 *   call-id  EOL
 *   from-tag EOL
 *   [to-tag] EOL
 *   cseq     EOL
 *   transid  EOL // Ser's tranction ID
 *   [body]   EOL
 *   .EOL
 *   EOL
 *   </pre>
 */
class UnixSocketAdapter
{
  int fd;
  char buffer[CTRL_MSGBUF_SIZE];
  bool close_fd;
  char   msg_buf[CTRL_MSGBUF_SIZE];
  char*  msg_c;
  int    msg_sz;
  char   sock_name[UNIX_PATH_MAX];

  /** @return -1 on error, 0 if success */
  int getLine(string& line);
  int getLines(string& lines);
  int getParam(string& param);

  /** @return -1 on error, 0 if success */
  int cacheMsg();
  int get_line(char* lb, unsigned int lbs);
  int get_lines(char* lb, unsigned int lbs);
  int get_param(string& p, char* lb, unsigned int lbs);

  /* message manipulation methods */
  static bool isComplete(const AmSipReply &rpl);
  static bool isComplete(const AmSipRequest &rpl);
  static string serialize(const AmSipRequest& req, const string &rplAddr);
  static string serialize_cancel(const AmSipRequest &, const string &rplAddr);
  static string serialize(const AmSipReply& reply, const string &rplAddr);

  int send_msg(const string &msg, const string &src, const string &dst, 
      int timeout);

  /**
   * Send a message.
   *
   * @param addr destination address.
   * @return -1 on error, 0 if success.
   */
  int  sendto(const string& addr, 
	      const char* buf, 
	      unsigned int len);

  /**
   * Return:
   *    -1 if error.
   *     0 if timeout.
   *     1 if there some datas ready.
   */
  int wait4data(int timeout);

 public:
  UnixSocketAdapter();
  ~UnixSocketAdapter();

  bool init(const string& addr);
  int getFd() { return fd; }
  void close();

  bool receive(AmSipRequest &);
  bool receive(AmSipReply &);

  int send(const AmSipRequest &req, string &src, string &dst);
  int send(const AmSipReply &reply, const string &dst);
};


#endif /* __UNIXSOCKETADAPTER_H__ */
