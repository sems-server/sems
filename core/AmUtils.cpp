/*
 * $Id$
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

/** @file AmUtils.cpp */

#include "AmUtils.h"
#include "AmThread.h"
#include "AmConfig.h"
#include "log.h"
#include "AmServer.h"
#include "AmSipMsg.h"

#include <stdarg.h>
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

#include <regex.h>

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 104
#endif

// timeout in us (ms/1000)
#define SER_WRITE_TIMEOUT  250000 // 250 ms
// write retry interval in us
#define SER_WRITE_INTERVAL 50000  //  50 ms

// timeout in us (ms/1000)
#define SER_SIPREQ_TIMEOUT 5*60*1000*1000 // 5 minutes
#define SER_DBREQ_TIMEOUT  250000 // 250 ms
// read retry interval in us
#define SER_READ_INTERVAL  50000  // 50 ms


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

string int2hex(unsigned int val)
{
  unsigned int digit=0;

  char buffer[2*sizeof(int)+1] = {0};
  int i,j=0;

  for(i=0; i<int(2*sizeof(int)); i++){
    digit = val >> 4*(2*sizeof(int)-1);
    val = val << 4;
    buffer[j++] = _int2hex_lookup[(unsigned char)digit];
  }

  return string((char*)buffer);
}

string long2hex(unsigned long val)
{
  unsigned int digit=0;

  char buffer[2*sizeof(long)+1] = {0};
  int i,j=0;

  for(i=0; i<int(2*sizeof(long)); i++){
    digit = val >> 4*(2*sizeof(long)-1);
    val = val << 4;
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
  DBG("str2i: too many letters in [%s]\n", init);
  return true;
 error_char:
  DBG("str2i: unexpected char %c in %s\n", *str, init);
  return true;
}

int parse_return_code(const char* lbuf, unsigned int& res_code, string& res_msg )
{
  char res_code_str[4] = {'\0'};
  const char* cur=lbuf;

  // parse xxx code
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

  s[0]='\0';

  return (l!=-1 ? s-str : -1);
}

int fifo_get_param(FILE* fp, string& p, char* line_buf, unsigned int size)
{
  if( fifo_get_line(fp,line_buf,size) !=-1 ){
    if(!strcmp(".",line_buf))
      line_buf[0]='\0';

    p = line_buf;
  }
  else {
    ERROR("could not read from FIFO: %s\n",strerror(errno));
    return -1;
  } 

  return 0;
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
  else {
    ERROR("buffer too small (size=%u)\n",(unsigned int)len);
    // buffer overran.
    return -1;
  }
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

  s[0]='\0';

  return (l!=-1 ? s-str : -1);
}

int msg_get_param(char*& msg_c, string& p, char* line_buf, unsigned int size)
{
  if( msg_get_line(msg_c,line_buf,size) != -1 ){

    if(!strcmp(".",line_buf))
      line_buf[0]='\0';

    p = line_buf;
  }
  else {
    ERROR("msg_get_line failed\n");
    return -1;
  }

  return 0;
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

string add2path( const string& path, int n_suffix, ...)
{
  va_list ap;
  string outpath = path;
    
  va_start(ap,n_suffix);

  for(int i=0; i<n_suffix; i++){

    const char* s = va_arg(ap,const char*);

    if(!outpath.empty() && (outpath[outpath.length()-1] != '/'))
      outpath += '/';

    outpath += s;
  }

  va_end(ap);

  return outpath;
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


int write_to_socket(int sd, const char* to_addr, const char * buf, unsigned int len)
{
  int retry = SER_WRITE_TIMEOUT / SER_WRITE_INTERVAL;
  int ret=-1;

  struct sockaddr_un ser_addr;
  memset (&ser_addr, 0, sizeof (ser_addr));
  ser_addr.sun_family = AF_UNIX;
  strncpy(ser_addr.sun_path,to_addr,UNIX_PATH_MAX);

  DBG("sending: <%.*s>\n",len,buf);

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

  DBG("write to unix socket: completed\n");
  ret = 0;

 error:
  //     close(sd);
  //    return (ret == -1 ? ret : len);
  return ret;
}


string extract_tag(const string& addr)
{
  string::size_type p = addr.find(";tag=");
  if(p == string::npos)
    return "";

  p += 5/*sizeof(";tag=")*/;
  string::size_type p_end = p;
  while(p_end < addr.length()){
    if( addr[p_end] == '>'
	|| addr[p_end] == ';' )
      break;
    p_end++;
  }
  return addr.substr(p,p_end-p);
}

bool key_in_list(const string& s_list, const string& key, 
		 char list_delim) 
{
  size_t pos = 0;
  size_t pos2 = 0;
  size_t pos_n = 0;
  while (pos < s_list.length()) {
    pos_n = pos2 = s_list.find(list_delim, pos);
    if (pos2==string::npos)
      pos2 = s_list.length()-1;
    while ((pos2>0)&&
	   ((s_list[pos2] == ' ')||(s_list[pos2] == list_delim)
	    ||(s_list[pos2] == '\n')))
      pos2--;
    if (s_list.substr(pos, pos2-pos+1)==key)
      return true;
    if (pos_n == string::npos)
      return false;
    while ((pos_n<s_list.length()) && 
	   ((s_list[pos_n] == ' ')||(s_list[pos_n] == list_delim)||
	    (s_list[pos_n] == '\n')))
      pos_n++;
    if (pos_n == s_list.length())
      return false;
    pos = pos_n;
  }
  return false;
}

string strip_header_params(const string& hdr_string) 
{
  size_t val_begin = 0; // skip trailing ' '
  for (;(val_begin<hdr_string.length()) && 
	 hdr_string[val_begin]==' ';val_begin++);
  // strip parameters
  size_t val_end = hdr_string.find(';', val_begin);
  if (val_end == string::npos) 
    val_end=hdr_string.length();
  return hdr_string.substr(val_begin, val_end-val_begin);
}

string get_header_param(const string& hdr_string, 
			const string& param_name) 
{
  size_t pos = 0;
  while (pos<hdr_string.length()) {
    pos = hdr_string.find(';',pos);
    if (pos == string::npos) 
      return "";
    if ((hdr_string.length()>pos+param_name.length()+1)
	&& hdr_string.substr(++pos, param_name.length())==param_name 
	&& hdr_string[pos+param_name.length()] == '=') {
      size_t pos2 = pos+param_name.length()+1;
      while(pos2<hdr_string.length() && hdr_string[pos2] != ';'
	    && hdr_string[pos2] != '\n')
	pos2++;
      return hdr_string.substr(pos + param_name.length()+1, 
			       pos2 - pos + - param_name.length() -1);
    }
    pos +=param_name.length();
  }
  return "";
}

/** 
 * get value from parameter header with the name @param name 
 * while skipping escaped values
 */
string get_header_keyvalue(const string& param_hdr, const string& name) {
  // ugly, but we need escaping
#define ST_FINDKEY  0
#define ST_FK_ESC   1
#define ST_CMPKEY   2
#define ST_SRCHEND  3
#define ST_SE_VAL   4
#define ST_SE_ESC   5

  size_t p=0, s_begin=0, corr=0, 
    v_begin=0, v_end=0;

  unsigned int st = ST_FINDKEY;
  
  while (p<param_hdr.length()) {
    char curr = param_hdr[p];

    switch(st) {
    default:
    case ST_FINDKEY: {
      if (curr=='"') {
	st = ST_FK_ESC;
      } else if (curr==name[0]) {
	st = ST_CMPKEY;
	s_begin = p;
	corr = 1;
      }
      p++;
    }; break;

    case ST_FK_ESC: {
      if (curr=='"')
	st = ST_FINDKEY;
      p++;
    }; break;

    case ST_CMPKEY: {
      if (corr==name.length()) {
	if (curr=='=') {
	  st = ST_SRCHEND;
	  v_begin=++p;
	} else {
	  p=s_begin+1;
	  st = ST_FINDKEY;
	  corr=0;
	}
      } else {
	if (curr==name[corr]) {
	  p++;
	  corr++;
	} else {
	  st = ST_FINDKEY;
	  corr=0;
	  p=s_begin+1;	  
	}
      }
    }; break;

    case ST_SRCHEND: {
      if (curr=='"') {
	v_begin++;
	st = ST_SE_ESC;
      } else 
	st = ST_SE_VAL;
      p++;
      v_end = p;
    }; break;

    case ST_SE_VAL: {
      if (curr==';')
	p = param_hdr.length();
      else {
	v_end = p;
	p++;
      }
    }; break;

    case ST_SE_ESC: {
      if (curr=='"')
	p = param_hdr.length();
      else {
	v_end = p;
	p++;
      }
    }; break;

    }
  }

  if (v_begin && v_end)
    return param_hdr.substr(v_begin, v_end-v_begin+1);
  else 
    return "";
}

/** get the value of key @param name from \ref PARAM_HDR header in hdrs */
string get_session_param(const string& hdrs, const string& name) {
  string iptel_app_param = getHeader(hdrs, PARAM_HDR);
  if (!iptel_app_param.length()) {
    //      DBG("call parameters header PARAM_HDR not found "
    // 	 "(need to configure ser's tw_append?).\n");
    return "";
  }

  return get_header_keyvalue(iptel_app_param, name);
}


// support for thread-safe pseudo-random numbers
static unsigned int _s_rand=0;
static AmMutex _s_rand_mut;

void init_random()
{
  int seed=0;
  FILE* fp_rand = fopen("/dev/urandom","r");
  if(fp_rand){
    fread(&seed,sizeof(int),1,fp_rand);
    fclose(fp_rand);
  }
  seed += getpid();
  seed += time(0);
  _s_rand = seed;
}

unsigned int get_random()
{
  _s_rand_mut.lock();
  unsigned int r = rand_r(&_s_rand);
  _s_rand_mut.unlock();
    
  return r;
}
// Explode string by a separator to a vector
vector <string> explode(string s, string e) {
  vector <string> ret;
  int iPos = s.find(e, 0);
  int iLen = e.length();
  while (iPos > -1) {
    if (iPos != 0)
      ret.push_back(s.substr(0, iPos));
    s.erase(0, iPos+iLen);
    iPos = s.find(e, 0);
  }
  if (s != "")
    ret.push_back(s);
  return ret;
}


// Warning: static var is not mutexed
// Call this func only in init code.
//
void add_env_path(const char* name, const string& path)
{
  string var(path);
  char*  old_path=0;

  regex_t path_reg;

  assert(name);
  if((old_path = getenv(name)) != 0) {
    if(strlen(old_path)){
	    
      if(regcomp(&path_reg,("[:|^]" + path + "[:|$]").c_str(),REG_NOSUB)){
	ERROR("could not compile regex\n");
	return;
      }
	    
      if(!regexec(&path_reg,old_path,0,0,0)) { // match

	return; // do nothing
      }

      var += ":" + string(old_path);
    }
  }

  DBG("setting %s to: '%s'\n",name,var.c_str());
  setenv("PYTHONPATH",var.c_str(),1);
}
