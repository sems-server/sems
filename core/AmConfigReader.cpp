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
#include "AmConfigReader.h"
#include "AmConfig.h"
#include "log.h"
#include "AmUtils.h"
#include "md5.h"

#include <errno.h>
#include <fstream>

#define IS_SPACE(c) ((c == ' ') || (c == '\t'))

#define IS_EOL(c) ((c == '\0')||(c == '#'))

#define TRIM(s) \
          do{ \
              while( IS_SPACE(*s) ) s++; \
          }while(false)

static int fifo_get_line(FILE* fifo_stream, char* str, size_t len)
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


int  AmConfigReader::loadFile(const string& path)
{
  FILE* fp = fopen(path.c_str(),"r");
  if(!fp){
      WARN("could not open configuration file '%s': %s\n",
	   path.c_str(),strerror(errno));
      return -1;
  }
  
  int  lc = 0;
  int  ls = 0;
  char lb[MAX_CONFIG_LINE] = {'\0'};

  char *c,*key_beg,*key_end,*val_beg,*val_end,*inc_beg,*inc_end;

  c=key_beg=key_end=val_beg=val_end=inc_beg=inc_end=0;
  while(!feof(fp) && ((ls = fifo_get_line(fp, lb, MAX_CONFIG_LINE)) != -1)){
	
    c=key_beg=key_end=val_beg=val_end=0;
    lc++;

    c = lb;
    TRIM(c);

    if(IS_EOL(*c)) continue;

    if (*c == '@') { /* process included config file */
	c++;
	TRIM(c);
	inc_beg = c++;
	while( !IS_EOL(*c) && !IS_SPACE(*c) ) c++;
	inc_end = c;
	string fname = string(inc_beg,inc_end-inc_beg);
	if (fname.length() && fname[0] != '/')
	  fname = AmConfig::ModConfigPath + fname;
	if(loadFile(fname))
	    goto error;
	continue;
    }

    key_beg = c;
    while( (*c != '=') && !IS_SPACE(*c) ) c++;
    
    key_end = c;
    if(IS_SPACE(*c))
      TRIM(c);
    else if( !(c - key_beg) )
      goto syntax_error;

    if(*c != '=')
      goto syntax_error;

    c++;
    TRIM(c);

    if(*c == '"'){
      char last_c = ' ';
      val_beg = ++c;

      while( ((*c != '"') || (last_c == '\\')) && (*c != '\0') ) {
	last_c = *c;
	c++;
      }

      if(*c == '\0')
	goto syntax_error;

      val_end = c;
    }
    else {
      val_beg = c;

      while( !IS_EOL(*c) && !IS_SPACE(*c) ) c++;

      val_end = c;
    }

    if((key_beg < key_end) && (val_beg <= val_end)) {
      string keyname = string(key_beg,key_end-key_beg);
      string val = string(val_beg,val_end-val_beg);
      if (hasParameter(keyname)) {
	WARN("while loading '%s': overwriting configuration "
	     "'%s' value '%s' with  '%s'\n",
	     path.c_str(), keyname.c_str(), 
	     getParameter(keyname).c_str(), val.c_str());
      }

      keys[keyname] = val;

      // small hack to make include work with right path
      if (keyname == "plugin_config_path")
	AmConfig::ModConfigPath = val;

    } else
      goto syntax_error;
  }

  fclose(fp);
  return 0;

 syntax_error:
  ERROR("syntax error line %i in %s\n",lc,path.c_str());
 error:
  fclose(fp);
  return -1;
}

int  AmConfigReader::loadPluginConf(const string& mod_name)
{
  return loadFile(add2path(AmConfig::ModConfigPath,1,
			   string(mod_name + CONFIG_FILE_SUFFIX).c_str()));
}

static int str_get_line(const char** c, const char* end, char* line_buf, size_t line_buf_len)
{
  enum {
    SGL_LINE=0,
    SGL_COMMENT,
    SGL_CR,
    SGL_LF
  };

  char* out = line_buf;
  int st = SGL_LINE;

  while((*c < end)  && line_buf_len && (st == SGL_LINE)){

    switch(**c) {
    case '\r': st = SGL_CR; break;
    case '\n': st = SGL_LF; break;
    case '#':  st = SGL_COMMENT; break;
      
    default:
      *out = **c;
      out++;
      line_buf_len--;
      break;
    }

    (*c)++;
  }

  if(st == SGL_COMMENT) {
    while((*c < end) && (st == SGL_COMMENT)){
      
      switch(*((*c)++)) {
      case '\r': st = SGL_CR; break;
      case '\n': st = SGL_LF; break;
      default: break;
      }
      
      (*c)++;
    }
  }

  if(st == SGL_CR) {
    if(**c == '\n') {
      st = SGL_LF;
      (*c)++;
    }
    else {
      DBG("strange line ending with CR only\n");
    }
  }

  if(line_buf_len > 0){
    // We need one more character
    // for trailing '\0'.
    *out='\0';

    return int(out-line_buf);
  }

  // buffer overran.
  return -1;
}

int AmConfigReader::loadString(const char* cfg_lines, size_t cfg_len)
{
  int  lc = 0;
  int  ls = 0;
  char lb[MAX_CONFIG_LINE] = {'\0'};

  char *c,*key_beg,*key_end,*val_beg,*val_end,*inc_beg,*inc_end;

  const char* cursor = cfg_lines;
  const char* cfg_end = cursor + cfg_len;

  c=key_beg=key_end=val_beg=val_end=inc_beg=inc_end=0;
  while((cursor < cfg_end) && 
	((ls = str_get_line(&cursor, cfg_end, lb, MAX_CONFIG_LINE)) != -1)){
	
    c=key_beg=key_end=val_beg=val_end=0;
    lc++;

    c = lb;
    TRIM(c);

    if(IS_EOL(*c)) continue;

    key_beg = c;
    while( (*c != '=') && !IS_SPACE(*c) ) c++;
    
    key_end = c;
    if(IS_SPACE(*c))
      TRIM(c);
    else if( !(c - key_beg) )
      goto syntax_error;

    if(*c != '=')
      goto syntax_error;

    c++;
    TRIM(c);

    if(*c == '"'){
      char last_c = ' ';
      val_beg = ++c;

      while( ((*c != '"') || (last_c == '\\')) && (*c != '\0') ) {
	last_c = *c;
	c++;
      }

      if(*c == '\0')
	goto syntax_error;

      val_end = c;
    }
    else {
      val_beg = c;

      while( !IS_EOL(*c) && !IS_SPACE(*c) ) c++;

      val_end = c;
    }

    if((key_beg < key_end) && (val_beg <= val_end)) {
      string keyname = string(key_beg,key_end-key_beg);
      string val = string(val_beg,val_end-val_beg);
      if (hasParameter(keyname)) {
	WARN("while loading string: overwriting configuration "
	     "'%s' value '%s' with  '%s'\n",
	     keyname.c_str(), getParameter(keyname).c_str(), 
	     val.c_str());
      }

      keys[keyname] = val;
    } else
      goto syntax_error;
  }

  return 0;

 syntax_error:
  ERROR("syntax error line %i\n",lc);
  return -1;
}

bool AmConfigReader::getMD5(const string& path, string& md5hash, bool lowercase) {
    std::ifstream data_file(path.c_str(), std::ios::in | std::ios::binary);
    if (!data_file) {
      DBG("could not read file '%s'\n", path.c_str());
      return false;
    }
    // that one is clever...
    // (see http://www.gamedev.net/community/forums/topic.asp?topic_id=353162 )
    string file_data((std::istreambuf_iterator<char>(data_file)),
		     std::istreambuf_iterator<char>());

    if (file_data.empty()) {
      return false;
    }

    MD5_CTX md5ctx;
    MD5Init(&md5ctx);
    MD5Update(&md5ctx, (unsigned char*)file_data.c_str(), file_data.length());
    unsigned char _md5hash[16];
    MD5Final(_md5hash, &md5ctx);
    md5hash = "";
    for (size_t i=0;i<16;i++) {
      md5hash+=char2hex(_md5hash[i], lowercase);
    }
    return true;
}

void AmConfigReader::setParameter(const string& param, const string& val) {
  keys[param] = val;
}

void AmConfigReader::eraseParameter(const string& param) {
  keys.erase(param);
}

bool AmConfigReader::hasParameter(const string& param) const
{
  return (keys.find(param) != keys.end());
}

string __empty_string("");

const string& AmConfigReader::getParameter(const string& param) const
{
  return getParameter(param,__empty_string);
}

const string& AmConfigReader::getParameter(const string& param, const string& defval) const
{
  map<string,string>::const_iterator it = keys.find(param);
  if(it == keys.end())
    return defval;
  else
    return it->second;
}

unsigned int AmConfigReader::getParameterInt(const string& param, unsigned int defval) const
{
  unsigned int result=0;
  if(!hasParameter(param) || str2i(getParameter(param),result))
    return defval;
  else
    return result;
}

void AmConfigReader::dump()
{
  for(map<string,string>::iterator it = keys.begin();
      it != keys.end(); it++) {
    
    DBG("\t%s = %s",it->first.c_str(),it->second.c_str());
  }
}
