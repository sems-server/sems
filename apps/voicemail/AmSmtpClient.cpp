/*
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>

#include "AmSmtpClient.h"
#include "AmUtils.h"
#include "log.h"

#include "sip/resolver.h"

#define B64_FRAME_LINE_SIZE    15
#define B64_MAX_BUF_LINES      45

#define SEND_LINE(l) \
    { \
	if(send_line(l)) \
	    return true; \
    }

AmSmtpClient::AmSmtpClient()
  : server_ip(), server_port(0), 
    sd(0)
{
}

AmSmtpClient::~AmSmtpClient()
{
  if(sd){
    close();
  }
}

bool AmSmtpClient::connect(const string& _server_ip, unsigned short _server_port)
{
  if(sd && close())
    return true;

  server_ip = _server_ip;
  server_port = _server_port;
    
  if(server_ip.empty())
    return true;
    
  if(!server_port)
    server_port = 25; /* Not present on FreeBSD IPPORT_SMTP; */

  struct sockaddr_in addr;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(server_port);

  {
    sockaddr_storage _sa;
    dns_handle       _dh;
    
    if(resolver::instance()->resolve_name(server_ip.c_str(),
					&_dh,&_sa,IPv4) < 0) {
      ERROR("address not valid (smtp server: %s)\n",server_ip.c_str());
      return false;
    }

    memcpy(&addr.sin_addr,&((sockaddr_in*)&_sa)->sin_addr,sizeof(in_addr));
  }

  sd = socket(PF_INET, SOCK_STREAM, 0);
  if(::connect(sd,(struct sockaddr *)&addr,sizeof(addr)) == -1) {
    ERROR("%s\n",strerror(errno));
    return false;
  }
    
  INFO("connected to: %s\n",server_ip.c_str());
  bool cont = !get_response(); // server's welcome

  if(cont){
    INFO("%s welcomes us\n",server_ip.c_str());
    return send_command("HELO " + server_ip);
  }
  else
    return true;
}

// returns:  0 if succeded
//          -1 if failed
bool AmSmtpClient::send(const AmMail& mail)
{
  string mail_from = "mail from: <" + mail.from + ">";
  string rcpt_to = "rcpt to: <" + mail.to + ">";

  vector<string> headers;

  if (!mail.header.empty()) headers.push_back(mail.header);
  headers.push_back("From: " + mail.from);
  headers.push_back("To: " + mail.to);
  headers.push_back("Subject: " + mail.subject);

  if ( send_command(mail_from)
       || send_command(rcpt_to)
       || send_body(headers,mail) )
    return true;

  return false;
}

bool AmSmtpClient::disconnect()
{
  return send_command("quit");
}

bool AmSmtpClient::close()
{
  ::close(sd);
  sd = 0;
  INFO("We are now deconnected from server\n");
  return false;
}


bool AmSmtpClient::read_line()
{
  received=0;
  int s = read(sd,lbuf,SMTP_LINE_BUFFER);
  if(s == -1)
    ERROR("AmSmtpClient::read_line(): %s\n",strerror(errno));
  else if(s > 0){
    received = s;
    DBG("RECEIVED: %.*s\n",s,lbuf);
    lbuf[s] = '\0';
  }
  else if(!s)
    DBG("AmSmtpClient::read_line(): EoF reached!\n");
    
  return (s<=0);
}

bool AmSmtpClient::send_line(const string& cmd)
{
  string snd_buf = cmd;
    
  string::size_type pos = 0;
  while( (pos = snd_buf.find('\n',pos)) != string::npos ){
    if( (pos == 0) || ((pos > 0) && (snd_buf[pos-1] != '\r')) ){
      snd_buf.insert(pos,1,'\r');
      pos += 2;
    }
  }

  snd_buf += "\r\n";
  int ssize = write(sd,snd_buf.c_str(),snd_buf.length());
  if(ssize == -1){
    ERROR("AmSmtpClient::send_line(): %s\n",strerror(errno));
    return true;
  }

  DBG("SENT: %.*s",(int)snd_buf.length(),snd_buf.c_str());

  return false;
}

bool AmSmtpClient::get_response()
{
  return (read_line() || parse_response());
}

bool AmSmtpClient::send_command(const string& cmd)
{
  if( send_line(cmd) || get_response()){
    status = st_Error;
    return true;
  }

  if(res_code >= 200 && res_code < 400) {
    status = st_Ok;
  }
  else if(res_code < 600) {
    ERROR("smtp server answered: %i %s (cmd was '%s')\n",
	  res_code,res_msg.c_str(),cmd.c_str());
    status = st_Error;
  }
  else {
    WARN("unknown response from smtp server: %i %s (cmd was '%s')\n",
	 res_code,res_msg.c_str(),cmd.c_str());
    status = st_Unknown;
  }

  return (status != st_Ok);
}



bool AmSmtpClient::parse_response()
{
  if(parse_return_code(lbuf,res_code,res_msg)==-1){

    ERROR("AmSmtpClient::parse_response(): while parsing response\n");
    return true;
  }

  return false;
}

bool AmSmtpClient::send_body(const vector<string>& hdrs, const AmMail& mail)
{
  return send_command("data") 
    || send_data(hdrs,mail)
    || send_command(".");
}

static void base64_encode(unsigned char* in, unsigned char* out, unsigned int in_size);
static int base64_encode_file(FILE* in, int out);

bool AmSmtpClient::send_data(const vector<string>& hdrs, const AmMail& mail)
{
  string part_delim = "----=_NextPart_" 
    + int2str(int(time(NULL))) 
    + "_" + int2str(int(getpid()));

  for( vector<string>::const_iterator hdr_it = hdrs.begin();
       hdr_it != hdrs.end(); ++hdr_it )
    SEND_LINE(*hdr_it);

  SEND_LINE("MIME-Version: 1.0");

  if(!mail.attachements.empty()){
    SEND_LINE("Content-Type: multipart/mixed; ");
    SEND_LINE("      boundary=\"" + part_delim + "\"");
    SEND_LINE(""); // EoH
    SEND_LINE("--" + part_delim);
  }

  if(mail.charset.empty()){
    SEND_LINE("Content-Type: text/plain");
  }
  else {
    SEND_LINE("Content-Type: text/plain; ");
    SEND_LINE("      charset=\"" + mail.charset + "\"");
  }
  SEND_LINE(""); //EoH
  SEND_LINE(mail.body);


  for( Attachements::const_iterator att_it = mail.attachements.begin();
       att_it != mail.attachements.end(); ++att_it	) {
	
    SEND_LINE("--" + part_delim );
    if(!att_it->content_type.empty()){
      SEND_LINE("Content-Type: " + att_it->content_type);
    }
    else {
      SEND_LINE("Content-Type: application/octet-stream");
    }
    SEND_LINE("Content-Transfer-Encoding: base64");
	
    if(att_it->filename.empty()) {
      SEND_LINE("Content-Disposition: inline"); // | "attachement"
    }
    else {
      SEND_LINE("Content-Disposition: inline; "); // | "attachement"
      SEND_LINE("      filename=\"" + att_it->filename + "\"");
    }
    SEND_LINE(""); // EoH

    base64_encode_file(att_it->fp,sd);
    SEND_LINE(""); // base64_encode_file() doesn't generate any EoL
  }

  if(!mail.attachements.empty()){
    SEND_LINE("--" + part_delim + "--");
  }

  return false;
}

// !! do not touch !! configure only B64_FRAME_LINE_SIZE & B64_MAX_BUF_LINES

#define B64_IN_LINE_SIZE       (3 * B64_FRAME_LINE_SIZE)
#define B64_OUT_LINE_SIZE      (4 * B64_FRAME_LINE_SIZE) 

#define B64_INPUT_BUFFER_SIZE  (B64_IN_LINE_SIZE * B64_MAX_BUF_LINES)
#define B64_OUTPUT_BUFFER_SIZE (B64_OUT_LINE_SIZE * B64_MAX_BUF_LINES)


char base64_table[] = {
  'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
  'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
  'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
  'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
};

static void base64_encode(unsigned char* in, unsigned char* out, unsigned int in_size)
{
  unsigned int dw;
	
  switch(in_size){
  case 3:
    dw = (((unsigned int)in[0]) << 16)
      | (((unsigned int)in[1]) << 8)
      | ((unsigned int)in[2]);
    break;
  case 2:
    dw = (((unsigned int)in[0]) << 16)
      | (((unsigned int)in[1]) << 8);
    break;
  case 1:
    dw = (((unsigned int)in[0]) << 16);
    break;
  default:
    return;
  }

  unsigned int i=0;
  for(; i<(in_size+1); i++)
    out[i] = base64_table[(dw >> (18-6*i)) & ((1<<6)-1)];

  for(; i<4; i++)
    out[i] = '=';
}

static int base64_encode_file(FILE* in, int out_fd)
{
  unsigned char ibuf[B64_INPUT_BUFFER_SIZE];
  unsigned char obuf[B64_OUTPUT_BUFFER_SIZE]={' '};
  int s;
    
  FILE* out = fdopen(out_fd,"w");

  if(!out){
    ERROR("base64_encode_file: out file == NULL\n");
    return -1;
  }

  rewind(in);
  //     FILE* in  = fopen(filename,"rb");
  //     if(!in){
  // 	ERROR("%s\n",strerror(errno));
  // 	return -1;
  //     }

  int bytes_written=0;
  while((s = fread(ibuf,1,B64_INPUT_BUFFER_SIZE,in))){

    unsigned int ioff=0;
    unsigned int ooff=0;
    while(s>=3){
      base64_encode(ibuf+ioff,obuf+ooff,3);
      ioff += 3;
      ooff += 4;
      s -= 3;
    }
    if(s){
      base64_encode(ibuf+ioff,obuf+ooff,s);
      ooff += 4;
    }

    unsigned int off=0;
    while(ooff >= 60){
      fprintf(out,"%.*s\r\n",60,obuf + off);
      off  += 60;
      ooff -= 60;
    }

    if(ooff){
      fprintf(out,"%.*s\r\n",int(ooff),obuf + off);
      off += ooff;
    }

    bytes_written += off;
  };
    
  fflush(out);
  //fclose(in);
  DBG("%i bytes written\n",bytes_written);
  return 0;
}

