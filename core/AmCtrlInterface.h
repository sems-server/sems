#ifndef AmCtrlInterface_h
#define AmCtrlInterface_h

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 104
#endif

#include <string>
#include <memory>
#include <stdio.h>

using std::string;
using std::auto_ptr;


#define FIFO_VERSION "0.3"

#define CTRL_MSGBUF_SIZE 2048
#define MAX_MSG_ERR         5

/**
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

class AmCtrlUserData
{
public:
    virtual ~AmCtrlUserData(){}
};


class AmCtrlInterface
{
    auto_ptr<AmCtrlUserData> u_data;

protected:
    int  fd;
    bool close_fd;
    char buffer[CTRL_MSGBUF_SIZE];

    /** @return -1 on error, 0 if success */
    virtual int get_line(char* lb, unsigned int lbs)=0;
    virtual int get_lines(char* lb, unsigned int lbs)=0;
    virtual int get_param(string& p, char* lb, unsigned int lbs)=0;

    AmCtrlInterface(AmCtrlUserData* p): u_data(p),fd(-1),close_fd(true) {}
    AmCtrlInterface(int fd,AmCtrlUserData* p): u_data(p),fd(fd),close_fd(true) {}


public:

    static AmCtrlInterface* getNewCtrl(AmCtrlUserData* p);

    virtual ~AmCtrlInterface(){}

    /** 
     * Open a server control interface.
     * This must be called before everything else
     *
     * @param addr local address for server behavior
     * @return -1 on error, 0 if success 
     */
    virtual int init(const string& addr)=0;
    
    /**
     * Send a message.
     *
     * @param addr destination address.
     * @return -1 on error, 0 if success.
     */
    virtual int sendto(const string& addr, 
		       const char* buf, 
		       unsigned int len)=0;

    /**
     * Return:
     *    -1 if error.
     *     0 if timeout.
     *     1 if there some datas ready.
     */
    int wait4data(int timeout);

    int getFd() const 
    { return fd; }

    const AmCtrlUserData* getUserData() const 
    { return u_data.get(); }

    /** @return -1 on error, 0 if success */
    virtual int cacheMsg()=0;
    int getLine(string& line);
    int getLines(string& lines);
    int getParam(string& param);

    virtual void consume();
    virtual void close();
};


class AmFifoCtrlInterface: public AmCtrlInterface
{
    FILE*  fp_fifo;
    string filename;

    int get_line(char* lb, unsigned int lbs);
    int get_lines(char* lb, unsigned int lbs);
    int get_param(string& p, char* lb, unsigned int lbs);

public:
    AmFifoCtrlInterface(AmCtrlUserData* p);
    ~AmFifoCtrlInterface();

    int createFifo(const string& addr);

    int init(const string& addr);
    int sendto(const string& addr, 
	       const char* buf, 
	       unsigned int len);
    
    int  cacheMsg();
    void close();
    void consume();

};

class AmUnixCtrlInterface: public AmCtrlInterface
{
    char   msg_buf[CTRL_MSGBUF_SIZE];
    char*  msg_c;
    int    msg_sz;
    char   sock_name[UNIX_PATH_MAX];

    int get_line(char* lb, unsigned int lbs);
    int get_lines(char* lb, unsigned int lbs);
    int get_param(string& p, char* lb, unsigned int lbs);

public:
    AmUnixCtrlInterface(AmCtrlUserData* p);
    ~AmUnixCtrlInterface();

    int  init(const string& addr);
    int  sendto(const string& addr, 
		const char* buf, 
		unsigned int len);

    void close();
    int cacheMsg();
};

#endif
