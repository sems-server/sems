/*
 * $Id: SerClient.cpp,v 1.2.2.1 2005/04/13 10:57:09 rco Exp $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "SerClient.h"
#include "AmConfig.h"
#include "AmSession.h"
#include "AmUtils.h"
#include "log.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

/** @return NULL on failure. */
static int open_reply_fifo(const string& filename);

static int write_to_fifo(int reply_fd, const char * buf, unsigned int len);
static int write_to_socket(int reply_fd, const char * buf, unsigned int len);

static int read_from_fifo(int fd,char* buffer,int buf_size,int timeout);
static int read_from_socket(int fd,char* buffer,int buf_size,int timeout);


extern "C" {
    
    struct ser_client_cb_t fifo_cbs = {
	open_reply_fifo,
	write_to_fifo,
	read_from_fifo
    };

    struct ser_client_cb_t socket_cbs = {
	create_unix_socket,
	write_to_socket,
	read_from_socket
    };
}

SerClientData::SerClientData(int id,const string& addr)
    : id(id), addr(addr)
{
    msg_buf[0] = '\n';
}

SerClient* SerClient::_client=0;

SerClient* SerClient::getInstance()
{
    if(_client)
	return _client;

    SerClient* c = new SerClient();
    c->cbs=0;

    if(AmConfig::SendMethod == "fifo")
	c->cbs = &fifo_cbs;
    else if(AmConfig::SendMethod == "unix_socket")
 	c->cbs = &socket_cbs;
    else {
	ERROR("unknown send method\n");
	delete c;
	return 0;
    }

    return _client = c;
}

SerClientData* SerClient::id2scd(int id)
{
    map<int,SerClientData*>::iterator scd_it;
    if((scd_it = datas.find(id)) == datas.end()){
	ERROR("SerClient <%i> does not exist.\n",id);
	return 0;
    }

    return scd_it->second;
}
    
int SerClient::open()
{
    string addr = AmSession::getNewId();
    string res_fifo = "/tmp/" + addr;

    int reply_fd = cbs->open_reply(res_fifo);
    if (reply_fd == -1){
	ERROR("could not open reply fifo\n");
 	return -1;
    }

    datas[reply_fd] = new SerClientData(reply_fd,addr);

    return reply_fd;
}

int SerClient::send(int id, const string& cmd, 
		    const string& msg, int timeout)
{
    SerClientData* scd = id2scd(id);
    if(!scd) return -1;

    string buf = ":" + cmd + ":"
	+ scd->addr + "\n" +  msg;
    
    if(cbs->write(scd->id,buf.c_str(),buf.length()) == -1){
	ERROR("while sending request to Ser.\n");
	return -1;
    }

    if(cbs->read(scd->id,scd->msg_buf,
		 MSG_BUFFER_SIZE,timeout) == -1){
	ERROR("while reading Ser's response.\n");
	return -1;
    }

    return 0;
}

char* SerClient::getResponseBuffer(int id)
{
    SerClientData* scd = id2scd(id);
    if(!scd) return NULL;
    return scd->msg_buf;
}

void SerClient::close(int id)
{
    SerClientData* scd = id2scd(id);
    if(scd){
	::close(scd->id);
	::unlink(("/tmp/"+scd->addr).c_str());
    }
    delete scd;
}

/*
 * Creates a reply fifo, opens it nonblocking, 
 * then sets flags to blocking returns -1 on failure 
 */
static int open_reply_fifo(const string& reply_fifo_filename) 
{

    if( (mkfifo(reply_fifo_filename.c_str(),FIFO_PERM)<0) && (errno!=EEXIST) ) {
	ERROR("while creating fifo `%s': %s \n",
	      reply_fifo_filename.c_str(),strerror(errno));
	return 0;
    }
    
    struct stat filestat;
    if (stat(reply_fifo_filename.c_str(), &filestat)==-1) { /* FIFO doesn't exist yet ... */
	return 0;
    } else { /* file can be stat-ed, check if it is really a FIFO */
	if (!(S_ISFIFO(filestat.st_mode))) {
	    ERROR("ERROR: the file is not a FIFO: %s\n", reply_fifo_filename.c_str() );
	    return 0;
	}
    }
    
    /* filehandle for open, being used by fdopen and therefor 
     * does not have to be closed if stream gets closed  */
    int reply_filehandle;
    
    reply_filehandle = open (reply_fifo_filename.c_str(), O_RDONLY | O_NONBLOCK , 0);
    if (reply_filehandle == -1) {
	ERROR("could not open reply_fifo, reason %s\n", strerror(errno));
	return 0;
    }	
    
    /* now set blocking mode to make shure answer is read */
    int flags;
    if ( (flags=fcntl(reply_filehandle, F_GETFL, 0))<0) {
	ERROR("getfl failed: %s\n", strerror(errno));
	goto error;
    }
    flags&=~O_NONBLOCK;
    if (fcntl(reply_filehandle, F_SETFL, flags)<0) {
	ERROR("setfl cntl failed: %s\n", strerror(errno));
	goto error;
    }
    
    return reply_filehandle;

 error:
    close(reply_filehandle);
    unlink(reply_fifo_filename.c_str());
    return 0;
}

static int write_to_fifo(int reply_fd, const char * buf, unsigned int len)
{
    return write_to_fifo(AmConfig::SerFIFO,buf,len);
}

int write_to_fifo(const string& fifo, const char * buf, unsigned int len)
{
    int fd_fifo;
    int retry = SER_WRITE_TIMEOUT / SER_WRITE_INTERVAL;

    for(;retry>0; retry--){
	
	if((fd_fifo = open(fifo.c_str(),
			   O_WRONLY | O_NONBLOCK)) == -1) {
	    ERROR("while opening %s: %s\n",
		  fifo.c_str(),strerror(errno));

	    if(retry)
		sleep_us(50000);
	}
	else {
	    break;
	}
    }

    if(!retry)
	return -1;

    DBG("write_to_fifo: <%s>\n",buf);
    int l = write(fd_fifo,buf,len);
    close(fd_fifo);

    if(l==-1)
	ERROR("while writing: %s\n",strerror(errno));
    else
	DBG("Write to fifo: completed\n");

    return l;
}


int read_from_fifo(int fd, char* buffer, int buf_size, int timeout)
{
    int retry = timeout / SER_READ_INTERVAL;
    int len;

    for(;retry>0;retry--) {
	
	len = read(fd,buffer,buf_size-1);
	if(len > 0){

	    int l,rest_len=0;
	    while((l=read(fd,buffer,buf_size))>0)
		rest_len += l;

	    if(rest_len){
		ERROR("Ser's response is too long (size=%i, max=%i)\n",
		      rest_len+len,MSG_BUFFER_SIZE);
		return -1;
	    }
	    else {
		break;
	    }
	}

	if(retry){
	    /* fifo not ready */
	    sleep_us(SER_READ_INTERVAL);
	}
    }

    if(!retry){
	ERROR("no more retries!\n");
	ERROR("last error: %s\n",strerror(errno));
	return -1;
    }

    buffer[len]='\0';
    //DBG("received: <%s>\n",buffer);
    return len;
}

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

int write_to_socket(int reply_fd, const char * buf, unsigned int len)
{
    int retry = SER_WRITE_TIMEOUT / SER_WRITE_INTERVAL;

    int sd = reply_fd;
//     int sd = socket(PF_UNIX,SOCK_DGRAM,0);
//     if(sd == -1){
// 	ERROR("could not create unix socket: %s\n",strerror(errno));
// 	return -1;
//     }
    
    int ret=-1;
    if(AmConfig::SerSocketName.empty()){
	ERROR("config parameter 'ser_socket_name' has not been configured !!!\n");
	goto error;
    }

    struct sockaddr_un ser_addr;
    ser_addr.sun_family = AF_UNIX;
    strncpy(ser_addr.sun_path,AmConfig::SerSocketName.c_str(),
	    UNIX_PATH_MAX);

    for(;retry>0;retry--){
	
	if( (sendto(sd,buf,len,MSG_DONTWAIT, 
		   (struct sockaddr*)&ser_addr,
		   sizeof(struct sockaddr_un)) == -1) ) {

	    if(errno == EAGAIN){
		if(retry)
		    sleep_us(SER_WRITE_INTERVAL);
		continue;
	    }

	    ERROR("while sending request to %s: %s\n",
		  ser_addr.sun_path,strerror(errno));
	    goto error;
	}
	break;
    }

    if(!retry){
	ERROR("timeout while sending request to %s\n",ser_addr.sun_path);
	goto error;
    }

    DBG("Write to unix socket: completed\n");
    ret = 0;

 error:
//     close(sd);
    return (ret == -1 ? ret : len);
}

int read_from_socket(int fd,char* buffer,int buf_size,int timeout)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd,&fds);

    struct timeval to;
    to.tv_sec  = 0;
    to.tv_usec = timeout;
    
    int err = select(fd+1,&fds,NULL,NULL,&to);
    switch(err){
	case 1:// normal case!
	    break;
	case 0:
	    ERROR("timeout while waiting for Ser to respond\n");
	    return -1;
	case -1:
	    ERROR("select failed: %s\n",strerror(errno));
	    return -1;
	default:
	    ERROR(" unexcepted return value for select: %i\n",err);
	    return -1;
    }
    
    int msg_sz = recv(fd,buffer,buf_size,MSG_TRUNC);
    if(msg_sz == -1){
	ERROR("while receiving return code from Ser (recv failed): %s\n",strerror(errno));
	return -1;
    }
    
    if(msg_sz >= (buf_size-1)){
	ERROR("Ser's response is too long (size=%i, max=%i)\n",msg_sz,buf_size-1);
	return -1;
    }

    buffer[msg_sz-1] = '\0';
    return msg_sz;
}
