#include "AmCtrlInterface.h"
#include "AmUtils.h"
#include "AmConfig.h"
#include "log.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <errno.h>

#include <assert.h>


AmCtrlInterface* AmCtrlInterface::getNewCtrl(AmCtrlUserData* p)
{
//     if(AmConfig::SendMethod == "fifo"){
// 	return new AmFifoCtrlInterface(p);
//     }
//     else if(AmConfig::SendMethod == "unix_socket"){
// 	return new AmUnixCtrlInterface(p);
//     }
//     ERROR("unknown send method\n");
//     return 0;

    return new AmUnixCtrlInterface(p);
}

int AmCtrlInterface::getLine(string& line)
{
    int err = get_line(buffer,CTRL_MSGBUF_SIZE);
    if(err != -1)
	line = buffer;
    return err;
}

int AmCtrlInterface::getLines(string& lines)
{
    int err = get_lines(buffer,CTRL_MSGBUF_SIZE);
    if(err != -1)
	lines = buffer;
    return err;
}

int AmCtrlInterface::getParam(string& param)
{
    return get_param(param,buffer,CTRL_MSGBUF_SIZE);
}


void AmCtrlInterface::consume()
{}

void AmCtrlInterface::close()
{
    if((fd != -1) && close_fd){
	::close(fd);
    }

    fd = -1;
}

int AmFifoCtrlInterface::cacheMsg()
{
    return 0;
}

int AmFifoCtrlInterface::get_line(char* lb, unsigned int lbs)
{
    return fifo_get_line(fp_fifo,lb,lbs);
}

int AmFifoCtrlInterface::get_lines(char* lb, unsigned int lbs)
{
    return fifo_get_lines(fp_fifo,lb,lbs);
}

int AmFifoCtrlInterface::get_param(string& p, char* lb, unsigned int lbs)
{
    return fifo_get_param(fp_fifo,p,lb,lbs);
}


AmFifoCtrlInterface::AmFifoCtrlInterface(AmCtrlUserData* p)
    : AmCtrlInterface(p), fp_fifo(NULL)
{
}

AmFifoCtrlInterface::~AmFifoCtrlInterface()
{
    close();
}

int AmFifoCtrlInterface::createFifo(const string& addr)
{
    const char* fifo_name = addr.c_str();

    if( (mkfifo(fifo_name,FIFO_PERM)<0) && (errno!=EEXIST) ) {
	ERROR("while creating fifo `%s': %s \n",fifo_name,strerror(errno));
	return -1;
    }

    return 0;
}

int AmFifoCtrlInterface::init(const string& addr)
{
    const char* fifo_name = addr.c_str();

    if( (mkfifo(fifo_name,FIFO_PERM)<0) && (errno!=EEXIST) ) {
	ERROR("while creating fifo `%s': %s \n",fifo_name,strerror(errno));
	return -1;
    }

    if( !(fd = ::open(fifo_name, O_RDONLY)) ) {
	ERROR("while opening fifo `%s': %s\n",fifo_name,strerror(errno));
	return -1;
    }
    close_fd = true;
    
    if(!(fp_fifo = fdopen(fd,"r"))){
	ERROR("while opening fifo `%s' (fdopen): %s\n",fifo_name,strerror(errno));
	return -1;
    }

    filename = addr;
    close_fd = false;
    return 0;
}

int AmFifoCtrlInterface::sendto(const string& addr,const char* buf,unsigned int len)
{
    return write_to_fifo(addr,buf,len);
}

void AmFifoCtrlInterface::close()
{
    if((fp_fifo != NULL) && fp_fifo){
	fclose(fp_fifo);
	fp_fifo = NULL;
	AmCtrlInterface::close();
	::unlink(filename.c_str());
    }
}

void AmFifoCtrlInterface::consume()
{
    while( fifo_get_line(fp_fifo,buffer,CTRL_MSGBUF_SIZE) != 0 ) {
	ERROR("consumed from fifo: %s\n",buffer);
    }
}

int AmUnixCtrlInterface::cacheMsg()
{
    int err_cnt=0;

    msg_c = NULL;
    while(true){

	msg_sz = recv(fd,msg_buf,CTRL_MSGBUF_SIZE,MSG_TRUNC|MSG_DONTWAIT);
	if(msg_sz == -1){
	    ERROR("recv on unix socket failed: %s\n",strerror(errno));
	    if(++err_cnt >= MAX_MSG_ERR){
		ERROR("too many consecutive errors...\n");
		return -1;
	    }

	    continue;
	}

	break;
    }

    if(msg_sz > CTRL_MSGBUF_SIZE){
	ERROR("unix socket message is too big (size=%i;max=%i): discarding\n",
	      msg_sz,CTRL_MSGBUF_SIZE);
	return -1;
    }

    msg_buf[msg_sz-1] = '\0';
    msg_c = msg_buf;

    DBG("recv-ed: <%s>\n",msg_buf);

    return 0;
}

int AmUnixCtrlInterface::get_line(char* lb, unsigned int lbs)
{
    assert(msg_c);
    return msg_get_line(msg_c,lb,lbs);
}

int AmUnixCtrlInterface::get_lines(char* lb, unsigned int lbs)
{
    assert(msg_c);
    return msg_get_lines(msg_c,lb,lbs);
}

int AmUnixCtrlInterface::get_param(string& p, char* lb, unsigned int lbs)
{
    assert(msg_c);
    return msg_get_param(msg_c,p,lb,lbs);
}

AmUnixCtrlInterface::AmUnixCtrlInterface(AmCtrlUserData* p)
    : AmCtrlInterface(p),msg_c(NULL),msg_sz(0)
{
    memset(sock_name,0,UNIX_PATH_MAX);
}

AmUnixCtrlInterface::~AmUnixCtrlInterface()
{
    close();
}

int AmUnixCtrlInterface::init(const string& addr)
{
    strncpy(sock_name,addr.c_str(),UNIX_PATH_MAX-1);

    ::unlink(sock_name);
    fd = create_unix_socket(sock_name);
    if(fd == -1){
	ERROR("could not open unix socket '%s'\n",sock_name);
	return -1;
    }

    DBG("AmUnixCtrlInterface::init\n");
    close_fd = true;
    return 0;
}

int AmUnixCtrlInterface::sendto(const string& addr,const char* buf,unsigned int len)
{
    return write_to_socket(fd,addr.c_str(),buf,len);
}

void AmUnixCtrlInterface::close()
{
    AmCtrlInterface::close();

    if(sock_name[0] != '\0')
	::unlink(sock_name);
}

/**
 * Return:
 *    -1 if error.
 *     0 if timeout.
 *     1 if there some datas ready.
 */
int AmCtrlInterface::wait4data(int timeout)
{
    struct pollfd pfd = { fd, POLLIN, 0 };

    int ret = poll(&pfd,1,timeout);
    if(ret < 0){
      ERROR("poll: %s\n",strerror(errno));
      return -1;
    }
    else if(ret == 0){
      WARN("poll timed out\n");
      return -1;
    }

    if(pfd.revents & POLLIN)
      return 1;
    else {
      ERROR("poll: revents & POLLIN == 0\n");
      return -1;
    }
}

