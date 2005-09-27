/*
 * $Id: AmUtils.cpp,v 1.20.2.2 2005/08/31 13:54:29 rco Exp $
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

#include "AmUtils.h"
#include "AmThread.h"
#include "AmConfig.h"
#include "log.h"

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include <sys/socket.h>
#include <sys/un.h>

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

static char _int2str_lookup[] = { '0', '1', '2', '3', '4', '5', '6' , '7', '8', '9' };

string int2str(int val)
{
    char buffer[64] = {0};
    int i=62;
    div_t d;

    d.quot = val;
    do{
	d = div(d.quot,10);
	buffer[i] = _int2str_lookup[d.rem];
    }while(--i && d.quot);

    return string((char*)(buffer+i+1));
}

static char _int2hex_lookup[] = { '0', '1', '2', '3', '4', '5', '6' , '7', '8', '9','A','B','C','D','E','F' };

string int2hex(int val)
{
    unsigned int uval = val;
    unsigned int digit=0;

    char buffer[2*sizeof(int)+1] = {0};
    int i,j=0;

    for(i=0; i<int(2*sizeof(int)); i++){
	digit = uval >> 4*(2*sizeof(int)-1);
	uval = uval << 4;
	buffer[j++] = _int2hex_lookup[(unsigned char)digit];
    }

    return string((char*)buffer);
}

/**
 * Convert a reversed hex string to uint.
 * @param str    [in]  string to convert.
 * @param result [out] result integer.
 * @return true if failed. 
 */
bool reverse_hex2int(const string& str, unsigned int& result)
{
  result=0;
  char mychar;

  for (string::const_reverse_iterator pc = str.rbegin();
       pc != str.rend(); ++pc) {

    result <<= 4;
    mychar=*pc;

    if ( mychar >='0' && mychar <='9') 
        result += mychar -'0';
    else if (mychar >='a' && mychar <='f') 
        result += mychar -'a'+10;
    else if (mychar  >='A' && mychar <='F') 
        result += mychar -'A'+10;
    else 
        return true;
  }

  return false;
}

bool str2i(const string& str, unsigned int& result)
{
    char* s = (char*)str.c_str();
    return str2i(s,result);
}

bool str2i(char*& str, unsigned int& result, char sep)
{
	unsigned int ret=0;
	int i=0;
	char* init = str;

	for(; (*str != '\0') && (*str == ' '); str++);

	for(; *str != '\0';str++){
	    if ( (*str <= '9' ) && (*str >= '0') ){
		ret=ret*10+*str-'0';
		i++;
		if (i>10) goto error_digits;
	    } else if( *str == sep )
		    break;
	    else
		goto error_char;
	}

	result = ret;
	return false;

error_digits:
	ERROR("str2i: too many letters in [%s]\n", init);
	return true;
error_char:
	ERROR("str2i: unexpected char %c in %s\n", *str, init);
	return true;
}

int parse_return_code(const char* lbuf, char* res_code_str, unsigned int& res_code, string& res_msg )
{
    const char* cur=lbuf;

    // parse xxx code
    *((int*)res_code_str) = 0;
    for( int i=0; i<3; i++ ){
	if( (*cur >= '0') && (*cur <= '9') )
	    res_code_str[i] = *cur;
	else
	    goto error;
	cur++;
    }

    if( (*cur != ' ') && (*cur != '\t') && (*cur !='-') ){
	ERROR("expected 0x%x or 0x%x or 0x%x, found 0x%x\n",' ','\t','-',*cur);
	goto error;
    }

    if(sscanf(res_code_str,"%u",&res_code) != 1){
	ERROR("wrong code (%s)\n",res_code_str);
	goto error;
    }

    // wrap spaces and tabs
    while( (*cur == ' ') || (*cur == '\t') || (*cur =='-')) 
	cur++;

    res_msg = cur;
    return 0;

 error:
    ERROR("while parsing response\n");
    return -1;
}

int fifo_get_line(FILE* fifo_stream, char* str, size_t len)
{
    char   c;
    size_t l;
    char*  s=str;

    if(!len)
	return 0;
    
    l=len; 

    while( l && (c=fgetc(fifo_stream)) && !ferror(fifo_stream) && c!=EOF && c!='\n' ){
	if(c!='\r'){
	    *(s++) = c;
	    l--;
	}
    }


    if(l>0){
	// We need one more character
	// for trailing '\0'.
	*s='\0';

	return int(s-str);
    }
    else
	// buffer overran.
	return -1;
}

int fifo_get_lines(FILE* fifo_stream, char* str, size_t len)
{
    int l=0,max=len;
    char* s=str;

    if(!len) 
	return 0;

    while( max>0 && (l=fifo_get_line(fifo_stream,s,max)) && l!=-1 ) {
	if(!strcmp(".",s)) 
	    break;
	s+=l;
	*(s++)='\n';
	max-=l+1;
    }

    return (l!=-1 ? s-str : -1);
}

int msg_get_line(char*& msg_c, char* str, size_t len)
{
    size_t l;
    char*  s=str;

    if(!len)
	return 0;
    
    for(l=len; l && (*msg_c) && (*msg_c !='\n'); msg_c++ ){
	if(*msg_c!='\r'){
	    *(s++) = *msg_c;
	    l--;
	}
    }

    if(*msg_c)
	msg_c++;

    if(l>0){
	// We need one more character
	// for trailing '\0'.
	*s='\0';

	return int(s-str);
    }
    else
	// buffer overran.
	return -1;
}

int msg_get_lines(char*& msg_c, char* str, size_t len)
{
    int l=0,max=len;
    char* s=str;

    if(!len) 
	return 0;

    while( max>0 && (l=msg_get_line(msg_c,s,max)) && l!=-1 ) {
	if(!strcmp(".",s)) 
	    break;
	s+=l;
	*(s++)='\n';
	max-=l+1;
    }

    return (l!=-1 ? s-str : -1);
}

void msg_get_param(char*& msg_c, string& p)
{
    char line_buf[MSG_LINE_SIZE];

    if( msg_get_line(msg_c,line_buf,MSG_LINE_SIZE) != -1 ){

	if(!strcmp(".",line_buf))
	    line_buf[0]='\0';

	p = line_buf;
    }
    else {
	throw string("could not read from FIFO: ") + string(strerror(errno));
    }
}

bool file_exists(const string& name)
{
    FILE* test_fp = fopen(name.c_str(),"r");
    if(test_fp){
	fclose(test_fp);
	return true;
    }
    return false;
}

string filename_from_fullpath(const string& path)
{
    string::size_type pos = path.rfind('/');
    if(pos != string::npos)
	return path.substr(pos+1);
    return "";
}

AmMutex inet_ntoa_mut;
string get_addr_str(struct in_addr in)
{
    inet_ntoa_mut.lock();
    string addr = inet_ntoa(in);
    inet_ntoa_mut.unlock();
    return addr;
}

AmMutex inet_gethostbyname;
string get_ip_from_name(const string& name)
{
    inet_gethostbyname.lock();
    struct hostent *he = gethostbyname(name.c_str());
    if(!he){
	inet_gethostbyname.unlock();
	return "";
    }
    struct in_addr a;
    bcopy(he->h_addr, (char *) &a, sizeof(a));
    inet_gethostbyname.unlock();
    return get_addr_str(a);
}

string uri_from_name_addr(const string& name_addr)
{
    string uri = name_addr;
    string::size_type pos = uri.find('<');
    
    if(pos != string::npos)
	uri.erase(0,pos+1);
    
    pos = uri.find('>');
    if(pos != string::npos)
	uri.erase(pos,uri.length()-pos);
    
    return uri;
}

#ifdef SUPPORT_IPV6
#include <netdb.h>

int inet_aton_v6(const char* name, struct sockaddr_storage* ss)
{
    int error;
    //struct sockaddr *sa;
    struct addrinfo hints;
    struct addrinfo *res;

    memset(&hints, 0, sizeof(hints));
    /* set-up hints structure */
    hints.ai_family = PF_UNSPEC;
    error = getaddrinfo(name, NULL, &hints, &res);
    if (error)
	ERROR("%s\n",gai_strerror(error));
    else if (res) {
	assert( (res->ai_family == PF_INET) || 
		(res->ai_family == PF_INET6) );
	memset(ss,0,sizeof(struct sockaddr_storage));
	memcpy(ss,res->ai_addr,res->ai_addrlen);
	freeaddrinfo(res);
	return 1;
    }

    return 0;
}

void set_port_v6(struct sockaddr_storage* ss, short port)
{
    switch(ss->ss_family){
	case PF_INET:
	    ((struct sockaddr_in*)ss)->sin_port = htons(port);
	    break;
	case PF_INET6:
	    ((struct sockaddr_in6*)ss)->sin6_port = htons(port);
	    break;
	default:
	    ERROR("unknown address family\n");
	    assert(0);
	    break;
    }
}

short get_port_v6(struct sockaddr_storage* ss)
{
    switch(ss->ss_family){
	case PF_INET:
	    return ntohs(((struct sockaddr_in*)ss)->sin_port);
	case PF_INET6:
	    return ntohs(((struct sockaddr_in6*)ss)->sin6_port);
	default:
	    ERROR("unknown address family\n");
	    assert(0);
	    break;
    }
}

#endif

int create_unix_socket(const string& path)
{
    if(path.empty()){
	ERROR("parameter path is empty\n");
	return -1;
    }

    int sd = socket(PF_UNIX,SOCK_DGRAM,0);
    if(sd == -1){
	ERROR("could not create unix socket: %s\n",strerror(errno));
	return -1;
    }

    if(path.size() > UNIX_PATH_MAX-1){
	ERROR("could not create unix socket: unix socket path is too long\n");
	close(sd);
	return -1;
    }
	
    struct sockaddr_un sock_addr;
    sock_addr.sun_family = AF_UNIX;
    strcpy(sock_addr.sun_path,path.c_str());
	
    if(bind(sd,(struct sockaddr *)&sock_addr,
	    sizeof(struct sockaddr_un)) == -1) {
	ERROR("could not bind unix socket (path=%s): %s\n",path.c_str(),strerror(errno));
	close(sd);
	return -1;
    }

    return sd;
}


string file_extension(const string& path)
{
    string::size_type pos = path.rfind('.');
    if(pos == string::npos)
	return "";

    return path.substr(pos+1,string::npos);
}
