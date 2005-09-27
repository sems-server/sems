/*
 * $Id: AmServer.cpp,v 1.26.2.2 2005/08/25 06:55:12 rco Exp $
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

#include "AmServer.h"
#include "log.h"
#include "AmRequest.h"
#include "AmUtils.h"
#include "AmConfig.h"

#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>

#define READ_PARAMETER_FIFO(p)\
             READ_FIFO_PARAMETER(fifo_stream,p,line_buf,MAX_LINE_SIZE)

#define READ_PARAMETER_UN(p)\
             {\
		 msg_get_param(msg_c,p);\
		 DBG("%s= <%s>\n",#p,p.c_str());\
	     }

AmFifoServer* AmFifoServer::_instance;

AmFifoServer* AmFifoServer::instance()
{
    if(!_instance)
	_instance = new AmFifoServer();
    return _instance;
}

AmFifoServer::AmFifoServer()
    : fifo_stream(NULL)
{
}

AmFifoServer::~AmFifoServer()
{
    if(fifo_stream)
	fclose(fifo_stream);
}

int AmFifoServer::get_line(char* str, size_t len)
{
    return fifo_get_line(fifo_stream,str,len);
}

int AmFifoServer::get_lines(char* str, size_t len)
{
    return fifo_get_lines(fifo_stream,str,len);
}


void AmFifoServer::consume_request()
{
    char line_buf[MAX_LINE_SIZE];
    while( get_line(line_buf,MAX_LINE_SIZE) != 0 ) {
	    ERROR("Consumed: %s\n",line_buf);
    }
}

char* AmFifoServer::get_next_tok(char*& str)
{
    char* start=str;
    while(*start==' ') start++;
    
    char* s=start;
    while(*s){
	if(*s == ' '){
	    *s='\0';
	    str=s+1;
	    return start;
	}
	s++;
    }
    str = s;
    return ( s > start ? start : 0 );
}

int child_pid=0;
int is_main=1;

int AmFifoServer::init(const char * fifo_name)
{
    // create FIFO file
    if( (mkfifo(fifo_name,FIFO_PERM)<0) && (errno!=EEXIST) ) {
	ERROR("while creating fifo `%s': %s \n",fifo_name,strerror(errno));
	return -1;
    }

    // avoid the FIFO reaches EOF...
    if((child_pid=fork())>0){
	FILE* fifo_write=0;
        if(!(fifo_write=fopen(fifo_name, "w"))){
	    ERROR("while opening fifo `%s': %s\n",fifo_name,strerror(errno));
	    kill(child_pid,SIGTERM);
	}
	waitpid(child_pid,0,0);
	if(fifo_write)
	    fclose(fifo_write);
	return -1;
    }
    // the main process is waiting 
    // for that child to terminate
    is_main = 0; 

    if( !(fifo_stream = fopen(fifo_name,"r")) ) {
	ERROR("while opening fifo `%s': %s\n",fifo_name,strerror(errno));
	return -1;
    }
    
    return 0;
}

void AmFifoServer::run()
{
    INFO("FIFO server started\n");

    char line_buf[MAX_LINE_SIZE];
    string version;
    string fct_name;
    string cmd;
    string::size_type pos;

    while(true){

	try {
	    READ_PARAMETER_FIFO(version);
	    if (version == "") {
		    // some odd trailer -- ignore
		    ERROR("odd trailer\n");
		    continue;
	    }
	    if(version != FIFO_VERSION){
		throw string("wrong FIFO Interface version:"+version);
	    }

	    READ_PARAMETER_FIFO(fct_name);

	    if( ((pos = fct_name.find('.')) == string::npos)
		// || (pos+1 >= fct_name.length())
		)
		throw string("bad command: '" + fct_name + "'. "
			     "Syntax: fct_name.{plugin_name,'bye'}");

	    cmd = fct_name.substr(pos+1,string::npos);
	    fct_name = fct_name.substr(0,pos);

	    if(fct_name != "sip_request"){
		map<string,AmFifoServerFct*>::iterator fct_it;
		if( (fct_it = fct_map.find(fct_name)) == fct_map.end() )
		    throw string("unknown server function: '" + fct_name + "'");

		fct_it->second->execute(fifo_stream,cmd);
		continue;
	    }
	    else
		execute(fifo_stream,cmd);
	}
	catch(const string& err_msg){
	    ERROR("%s\n",err_msg.c_str());
	    consume_request();
	    continue;
	}
    }
}

int  AmFifoServer::registerFct(const string& name,AmFifoServerFct* fct)
{
    if(fct_map.find(name) != fct_map.end()){
	ERROR("AmFifoServer::registerFct: function '%s'"
	      " has already been registered.\n",name.c_str());
	return -1;
    }
    
    fct_map[name] = fct;
    DBG("AmFifoServer::registerFct: function '%s'"
	" has been registered.\n",name.c_str());

    return 0;
}

int  AmFifoServer::execute(FILE* stream, const string& cmd_str)
{
    char line_buf[MAX_LINE_SIZE];
    char body_buf[MSG_BODY_SIZE];
    char hdrs_buf[MSG_BODY_SIZE];
    int  body_len=0, hdrs_len=0;

    AmCmd cmd;

    if(cmd_str.empty())
	throw string("AmServer::execute: FIFO parameter plug-in name missing.");

    cmd.cmd = cmd_str;
    READ_PARAMETER_FIFO(cmd.method);
    READ_PARAMETER_FIFO(cmd.user);
    READ_PARAMETER_FIFO(cmd.domain);
    READ_PARAMETER_FIFO(cmd.dstip);    // will be taken for UDP's local peer IP & Contact
    READ_PARAMETER_FIFO(cmd.port);     // will be taken for Contact
    READ_PARAMETER_FIFO(cmd.r_uri);    // ??? will be taken for the answer's Contact header field ???
    READ_PARAMETER_FIFO(cmd.from_uri); // will be taken for subsequent request uri
    READ_PARAMETER_FIFO(cmd.from);
    READ_PARAMETER_FIFO(cmd.to);
    READ_PARAMETER_FIFO(cmd.callid);
    READ_PARAMETER_FIFO(cmd.from_tag);
    READ_PARAMETER_FIFO(cmd.to_tag);

    string cseq_str;
    READ_PARAMETER_FIFO(cseq_str);
    
    if(sscanf(cseq_str.c_str(),"%u", &cmd.cseq) != 1){
	throw string("invalid cseq number (") + cseq_str + string(")\n");
    }
    DBG("cseq= <%s>(%i)\n",cseq_str.c_str(),cmd.cseq);
    
    READ_PARAMETER_FIFO(cmd.key);
    READ_PARAMETER_FIFO(cmd.route);
    READ_PARAMETER_FIFO(cmd.next_hop);
    
    if( (hdrs_len = get_lines(hdrs_buf,MSG_BODY_SIZE)) == -1 ) {
	throw string("len(hdrs) > ") + int2str(MSG_BODY_SIZE) + "\n";
    }
    hdrs_buf[hdrs_len]='\0';
    DBG("hdrs: `%s'\n",hdrs_buf);
    cmd.hdrs = hdrs_buf;
    
    if( (body_len = get_lines(body_buf,MSG_BODY_SIZE)) == -1 ) {
	throw string("len(body) > ") + int2str(MSG_BODY_SIZE) + "\n";
    }
    body_buf[body_len]='\0';
    DBG("body: `%.*s'\n",body_len,body_buf);
    
#define IS_EMPTY(p) if(p.empty()) \
                     throw string("invalid FIFO command: " #p " is empty !!!" );
    
    IS_EMPTY(cmd.cmd);
    IS_EMPTY(cmd.from);
    IS_EMPTY(cmd.to);
    IS_EMPTY(cmd.callid);
    IS_EMPTY(cmd.from_tag);
    
#undef IS_EMPTY

    DBG("everything is OK !\n");
    
    AmRequestUAS req(cmd,body_buf);
    req.execute();
    
    return 0;
}

AmUnServer* AmUnServer::_instance;

AmUnServer* AmUnServer::instance()
{
    if(!_instance)
	_instance = new AmUnServer();
    return _instance;
}

AmUnServer::AmUnServer()
    : fifo_socket(-1)
{
}

AmUnServer::~AmUnServer()
{
    if(fifo_socket != -1){
	close(fifo_socket);
    }
}

void AmUnServer::on_stop()
{
}

int AmUnServer::init(const char * sock_name)
{
    unlink(sock_name);
    fifo_socket = create_unix_socket(sock_name);
    if(fifo_socket == -1){
	ERROR("could not create server socket: exiting\n");
	return -1;
    }
    return 0;
}

void AmUnServer::run()
{
    INFO("Unix socket server started\n");

    char         msg_buf[MSG_BUFFER_SIZE];
    char*        msg_c;
    int          err_cnt = 0;
    int          msg_sz;
    string       version;
    string       fct_name;
    string       cmd;
    string::size_type pos;

    while(true){

	try {

	    msg_sz = recv(fifo_socket,msg_buf,MSG_BUFFER_SIZE,MSG_TRUNC);
	    if(msg_sz == -1){
		ERROR("recv on server socket failed: %s\n",strerror(errno));
		if(++err_cnt >= MAX_MSG_ERR){
		    ERROR("too many consecutive errors: exiting now!\n");
		    return;
		}
		continue;
	    }
	    else
		err_cnt=0;

	    if(msg_sz > MSG_BUFFER_SIZE){
		ERROR("server request is too big (size=%i): discarding\n",msg_sz);
		continue;
	    }
	    msg_buf[msg_sz-1] = '\0';
	    msg_c = msg_buf;

	    DBG("recv-ed: <%s>\n",msg_buf);
	    
	    READ_PARAMETER_UN(version);
	    if(version != FIFO_VERSION){
		throw string("wrong FIFO Interface version.");
	    }

	    READ_PARAMETER_UN(fct_name);

	    if( ((pos = fct_name.find('.')) == string::npos)
		// || (pos+1 >= fct_name.length())
		)
		throw string("bad command: '"+fct_name+"'. "
			     "Syntax: fct_name.{plugin_name,'bye'}");

	    cmd = fct_name.substr(pos+1,string::npos);
	    fct_name = fct_name.substr(0,pos);

	    if(fct_name != "sip_request"){
		map<string,AmUnServerFct*>::iterator fct_it;
		if( (fct_it = fct_map.find(fct_name)) == fct_map.end() )
		    throw string("unknown server function: '" + fct_name + "'");

		fct_it->second->execute(msg_c,cmd);
		continue;
	    }
	    else
		execute(msg_c,cmd);
	}
	catch(const string& err_msg){
	    ERROR("%s\n",err_msg.c_str());
	    continue;
	}
    }
}

int  AmUnServer::registerFct(const string& name,AmUnServerFct* fct)
{
    if(fct_map.find(name) != fct_map.end()){
	ERROR("AmUnServer::registerFct: function '%s'"
	      " has already been registered.\n",name.c_str());
	return -1;
    }
    
    fct_map[name] = fct;
    DBG("AmUnServer::registerFct: function '%s'"
	" has been registered.\n",name.c_str());

    return 0;
}

int  AmUnServer::execute(char* msg_c, const string& cmd_str)
{
    char body_buf[MSG_BODY_SIZE];
    char hdrs_buf[MSG_BODY_SIZE];
    int  body_len=0, hdrs_len=0;

    AmCmd cmd;

    if(cmd_str.empty())
	throw string("AmUnServer::execute: FIFO parameter plug-in name missing.");

    cmd.cmd = cmd_str;
    READ_PARAMETER_UN(cmd.method);
    READ_PARAMETER_UN(cmd.user);
    //READ_PARAMETER_UN(cmd.email);
    READ_PARAMETER_UN(cmd.domain);
    READ_PARAMETER_UN(cmd.dstip);    // will be taken for UDP's local peer IP & Contact
    READ_PARAMETER_UN(cmd.port);     // will be taken for Contact
    READ_PARAMETER_UN(cmd.r_uri);    // ??? will be taken for the answer's Contact header field ???
    READ_PARAMETER_UN(cmd.from_uri); // will be taken for subsequent request uri
    READ_PARAMETER_UN(cmd.from);
    READ_PARAMETER_UN(cmd.to);
    READ_PARAMETER_UN(cmd.callid);
    READ_PARAMETER_UN(cmd.from_tag);
    READ_PARAMETER_UN(cmd.to_tag);

    string cseq_str;
    READ_PARAMETER_UN(cseq_str);
    
    if(sscanf(cseq_str.c_str(),"%u", &cmd.cseq) != 1){
	throw string("invalid cseq number (") + cseq_str + string(")\n");
    }
    DBG("cseq= <%s>(%i)\n",cseq_str.c_str(),cmd.cseq);
    
    READ_PARAMETER_UN(cmd.key);
    READ_PARAMETER_UN(cmd.route);
    READ_PARAMETER_UN(cmd.next_hop);
    
    if( (hdrs_len = msg_get_lines(msg_c,hdrs_buf,MSG_BODY_SIZE)) == -1 ) {
	throw string("len(hdrs) > ") + int2str(MSG_BODY_SIZE) + "\n";
    }
    hdrs_buf[hdrs_len]='\0';
    DBG("hdrs: `%s'\n",hdrs_buf);
    cmd.hdrs = hdrs_buf;
    
    if( (body_len = msg_get_lines(msg_c,body_buf,MSG_BODY_SIZE)) == -1 ) {
	throw string("len(body) > ") + int2str(MSG_BODY_SIZE) + "\n";
    }
    body_buf[body_len]='\0';
    DBG("body: `%.*s'\n",body_len,body_buf);
    
#define IS_EMPTY(p) if(p.empty()) \
                     throw string("invalid FIFO command: " #p " is empty !!!" );
    
    IS_EMPTY(cmd.cmd);
    IS_EMPTY(cmd.from);
    IS_EMPTY(cmd.to);
    IS_EMPTY(cmd.callid);
    IS_EMPTY(cmd.from_tag);

#undef IS_EMPTY

    DBG("everything is OK !\n");
    
    AmRequestUAS req(cmd,body_buf);
    req.execute();
    
    return 0;
}
