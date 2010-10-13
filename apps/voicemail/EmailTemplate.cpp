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

#include "EmailTemplate.h"
#include "AmMail.h"
#include "log.h"


#include <string.h>
#include <errno.h>
#include <stdio.h>

int EmailTemplate::load(const string& filename)
{
  tmpl_file = filename;
  FILE* fp = fopen(tmpl_file.c_str(),"r");
  if(!fp){
    ERROR("EmailTemplate: could not open mail template '%s': %s\n",
	  tmpl_file.c_str(),strerror(errno));
    return -1;
  }

  unsigned int file_size = 0;
  fseek(fp,0L,SEEK_END);
  file_size = ftell(fp);
  fseek(fp,0L,SEEK_SET);
  file_size -= ftell(fp);

  char* buffer = new char[file_size+1];
  if(!buffer){
    fclose(fp);
    ERROR("EmailTemplate: not enough memory to load template\n");
    ERROR("(file=%s,size=%u)\n",tmpl_file.c_str(),file_size);
    return -1;
  }

  size_t f_rd = fread(buffer,1,file_size,fp);
  fclose(fp);
  if (f_rd != file_size) {
    WARN("short read on file %s (expected %u, got %zd)\n",
	 filename.c_str(), file_size, f_rd);
  }
  buffer[f_rd] = '\0';

  int ret = parse(buffer);
  delete [] buffer;

  return ret;
}

#define P_SUBJECT 1
#define P_TO      2
#define P_FROM    3
#define P_HEADER  4

int EmailTemplate::parse(char* buffer)
{
  char* s=buffer;
  int state=0;
    
  while(1){
    while( (*s==' ') || (*s=='\r') ) s++;

    if(!(*s)){
      ERROR("EmailTemplate: parsing failed: end of file reached\n");
      return -1;
    }
	
    if(*s=='\n'){ // empty line -> EOH
      s++;
      break;
    }

    char* begin = s;
    while( (*s!=':') && (*s!='\0') && (*s!='\n') ) s++;
	
    if(!strncmp(begin,"subject",7)){
      state = P_SUBJECT;
    }
    else if(!strncmp(begin,"to",2)){
      state = P_TO;
    }
    else if(!strncmp(begin,"from",4)){
      state = P_FROM;
    }
    else if(!strncmp(begin,"header",4)){
      state = P_HEADER;
    }
    else
      state = 0;

    if(!state){
      ERROR("EmailTemplate: parsing failed: unknown token: '%s'\n",begin);
      return -1;
    }

    begin = ++s;
    while( *s && (*s != '\n') ) s++;
    *s = '\0';

    switch(state){
    case P_SUBJECT:
      subject = begin;
      break;
    case P_TO:
      to = begin;
      break;
    case P_FROM:
      from = begin;
      break;
    case P_HEADER:
      header = begin;
      break;
    }
    begin = ++s;
  }

  if(subject.empty()){
    ERROR("EmailTemplate: invalid template: empty or no 'subject' line\n");
    return -1;
  }
  if(to.empty()){
    ERROR("EmailTemplate: invalid template: empty or no 'to' line\n");
    return -1;
  }
  if(from.empty()){
    ERROR("EmailTemplate: invalid template: empty or no 'from' line\n");
    return -1;
  }
    
  if(*s)
    body = s;

  if(body.empty()){
    ERROR("EmailTemplate: invalid template: empty body\n");
    return -1;
  }

  // multiple headers  
  while (header.find("\\n") != string::npos) 
    header.replace(header.find("\\n"), 2, "\n");  

  return 0;
}

string EmailTemplate::replaceVars(const string& str,const EmailTmplDict& dict) const
{
  const char* s = str.c_str();
  const char* last = s;
  string res;

  while(1){

    last = s;
    while( *s && (*s != '%') ) s++;

    if(!(*s)) break;

    if(*(s+1) == '%'){
      res.append(last,(++s)-last);
      last = ++s;
      continue;
    }

    res.append(last,s-last);
    last = ++s;

    while( *s && (*s != '%') ) s++;
    if(!(*s)) break; // should throw a parser error...
	
    string var = string(last,s-last);
    // 	if(var == "user") // local user
    // 	    res.append(cmd.user);
    // 	else if(var == "email")  // local user's email 
    // 	    res.append(cmd.getHeader("P-Email-Address"));
    // 	else if(var == "domain") // local user's domain 
    // 	    res.append(cmd.domain);
    // 	else if(var == "from") // remote SIP address
    // 	    res.append(cmd.from);
    // 	else if(var == "to") // local SIP address
    // 	    res.append(cmd.to);
    // 	else if(var == "from_user"){
    // 	    string from_user;
    // 	    string::size_type pos1 = cmd.from.rfind("<sip:");
    // 	    string::size_type pos2 = cmd.from.find("@",pos1);
    // 	    if(pos1 != string::npos && pos2 != string::npos)
    // 		res.append(cmd.from.substr(pos1+5,pos2-pos1-5));
    // 	}
    // 	else if(var == "from_domain"){
    // 	    string from_domain;
    // 	    string::size_type pos1 = cmd.from.rfind("@");
    // 	    string::size_type pos2 = cmd.from.find(">",pos1);
    // 	    if(pos1 != string::npos && pos2 != string::npos)
    // 		res.append(cmd.from.substr(pos1+1,pos2-pos1-1));
    // 	}

    EmailTmplDict::const_iterator it = dict.find(var);
    if(it != dict.end()){
      res.append(it->second);
    }
    else {
      string err = "unknown variable: '";
      err += var + "'";
      throw err;
    }

    s++;
  }

  res.append(last,s-last);
  return res;
}

AmMail EmailTemplate::getEmail(const EmailTmplDict& dict) const
{
  try {
    return AmMail(replaceVars(from,dict),
		  replaceVars(subject,dict),
		  replaceVars(to,dict),
		  replaceVars(body,dict),
		  replaceVars(header,dict));
  }
  catch(const string& err){
    throw string("EmailTemplate: error in template '" + tmpl_file + "': " + err);
  }
}

