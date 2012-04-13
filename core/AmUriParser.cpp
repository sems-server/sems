/*
 * Copyright (C) 2006 iptego GmbH
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

#include "AmUriParser.h"
#include "log.h"

// Not on Solaris!
#if !defined (__SVR4) && !defined (__sun)
#include <strings.h>
#endif

#include <iostream>
using namespace std;

bool AmUriParser::isEqual(const AmUriParser& c) const {
  return (uri_user == c.uri_user) &&
    (!strcasecmp(uri_host.c_str(), 
		 c.uri_host.c_str())) &&
    (uri_port == c.uri_port);
}

/*
 * Skip display name part
 */
static inline int skip_name(const string& s, unsigned int pos)
{
  size_t i = pos;
  // skip leading WSP
  while (i<s.length() && (s[i]==' ' || s[i]=='\t'))
    i++;
  if (i==s.length())
    return i;

  if (s[i] == '"') {
    // quoted
    i++;
    int escaped = 0;
    while (i<s.length()) {
      if (escaped) {
	escaped = 0;
      } else {
	if (s[i] == '\\') {
	  escaped = 1;
	} else {
	  if (s[i] == '"') {
	    // name ended
	    i++;
	    while (i<s.length() && (s[i]==' ' || s[i]=='\t'))
	      i++;
	    return i;
	  }
	  if (s[i] == '\\') {
	    escaped = 1;
	  }
	}
      }
      i++;
    }
    DBG("unclosed quoted name in URI '%s'\n", s.substr(pos).c_str());
    return -1;
  } else {
    while (i<s.length()) {
      if (s[i]=='<') {
	// start of URI
	return i;
      }
      // was addr-spec not nameaddr
      if (s[i]=='@') {
	return pos;
      }
      i++;
    }
    DBG("addr-spec not found in URI '%s'\n",  s.substr(pos).c_str());
    return -1;
  }
}

#define ST1 1 /* Basic state */
#define ST2 2 /* Quoted */
#define ST3 3 /* Angle quoted */
#define ST4 4 /* Angle quoted and quoted */
#define ST5 5 /* Escape in quoted */
#define ST6 6 /* Escape in angle quoted and quoted */

/*
 * Skip URI, stops when , (next contact)
 * or ; (parameter) is found
 */
static inline int skip_uri(const string& s, unsigned int pos)
{
  unsigned int len = s.length() - pos;
  unsigned int p = pos;

  register int st = ST1;

  while(len) {
    switch(s[p]) {
    case ',':
    case ';':
      if (st == ST1) return p;
      break;

    case '\"':
      switch(st) {
      case ST1: st = ST2; break;
      case ST2: st = ST1; break;
      case ST3: st = ST4; break;
      case ST4: st = ST3; break;
      case ST5: st = ST2; break;
      case ST6: st = ST4; break;
      }
      break;

    case '<':
      switch(st) {
      case ST1: st = ST3; break;
      case ST3: 
	DBG("ERROR skip_uri(): Second < found\n");
	return -1;
      case ST5: st = ST2; break;
      case ST6: st = ST4; break;
      }
      break;
			
    case '>':
      switch(st) {
      case ST1: 
	DBG("ERROR skip_uri(): > is first\n");
	return -2;

      case ST3: st = ST1; break;
      case ST5: st = ST2; break;
      case ST6: st = ST4; break;
      }
      break;

    case '\\':
      switch(st) {
      case ST2: st = ST5; break;
      case ST4: st = ST6; break;
      case ST5: st = ST2; break;
      case ST6: st = ST4; break;
      }
      break;

    default: break;

    }

    p++;
    len--;
  }

  if (st != ST1) {
    DBG("ERROR skip_uri(): < or \" not closed\n"); 
    return -3;
  }
  return p;
}
enum {
  uS0=       0, // start
  uSPROT,       // protocol
  uSUHOST,      // user / host
  uSHOST,       // host
  uSHOSTWSP,    // wsp after host
  uSPORT,       // port
  uSPORTWSP,    // wsp after port
  uSHDR,        // header
  uSHDRWSP,     // wsp after header
  uSPARAM,      // params 
  uSPARAMWSP,   // wsp after params
  uS6           // end
};
/**
 * parse uri into user, host, port, param
 *
 */
bool AmUriParser::parse_uri() {
  // assuming user@host
  size_t pos = 0; int st = uS0;
  size_t p1 = 0; 
  int eq = 0; const char* sip_prot = "SIP:";
  uri_user = ""; 	uri_host = ""; uri_port = ""; uri_param = ""; 

  if (uri.empty())
    return false;

  while (pos<uri.length()) {
    char c = uri[pos];
    //    DBG("(1) c = %c, st = %d\n", c, st);
    switch(st) {
    case uS0: {
      switch (c) {
      case '<': { st = uSPROT; } break;
      default: { 
	if ((eq<=4)&&(toupper(c) ==sip_prot[eq])) 
	  eq++; 
	if (eq==4) { // found sip:
	  st = uSUHOST; p1 = pos;
	};
      } break;
      }; 
    } break;
    case uSPROT: { 
      if (c ==  ':')  { st = uSUHOST; p1 = pos;} 
    } break;
    case uSUHOST: {
      switch(c) {
      case '@':  { uri_user = uri.substr(p1+1, pos-p1-1);
	  st = uSHOST; p1 = pos; }; break;
      case ':': { uri_host = uri.substr(p1+1, pos-p1-1);
	  st = uSPORT; p1 = pos;  }; break;
      case '?': { uri_host = uri.substr(p1+1, pos-p1-1);
	  st = uSHDR;  p1 = pos; }; break;
      case ';': { uri_host = uri.substr(p1+1, pos-p1-1);
	  st = uSPARAM;p1 = pos; }; break;
      case '>': { uri_host = uri.substr(p1+1, pos-p1-1);
	  st = uS6;    p1 = pos; }; break;
      };
    } break;
    case uSHOST: {
      switch (c) {
      case ':': { uri_host = uri.substr(p1+1, pos-p1-1); 
	  st = uSPORT; p1 = pos; } break;
      case '?': { uri_host = uri.substr(p1+1, pos-p1-1);
	  st = uSHDR; p1 = pos; } break;
      case ';': { uri_host = uri.substr(p1+1, pos-p1-1);
	  st = uSPARAM; p1 = pos; } break;
      case '>': { uri_host = uri.substr(p1+1, pos-p1-1); 
	  st = uS6; p1 = pos; } break;
      case ' ': 
      case '\t': { uri_host = uri.substr(p1+1, pos-p1-1); 
	  st = uSHOSTWSP; p1 = pos; } 
	break;
      };
    } break;
    case uSHOSTWSP: {
      switch (c) {
      case ':': { st = uSPORT;  p1 = pos; } break;
      case '?': { st = uSHDR;   p1 = pos; } break;
      case ';': { st = uSPARAM; p1 = pos; } break;
      case '>': { st = uS6;     p1 = pos; } break;
      }
    }; break;

    case uSPORT: {
      switch (c) {
      case ';': { uri_port = uri.substr(p1+1, pos-p1-1); 
	  st = uSPARAM; p1 = pos; } break;
      case '?': { uri_port = uri.substr(p1+1, pos-p1-1); 
	  st = uSHDR;   p1 = pos; } break;
      case '>': { uri_port = uri.substr(p1+1, pos-p1-1); 
	  st = uS6;     p1 = pos; } break;
      case ' ': 
      case '\t':{ uri_port = uri.substr(p1+1, pos-p1-1); 
	  st = uSPORTWSP; p1 = pos; } break;
      };
    } break;
    case uSPORTWSP: {
      switch (c) {
      case '?': { st = uSHDR;   p1 = pos; } break;
      case ';': { st = uSPARAM; p1 = pos; } break;
      case '>': { st = uS6;     p1 = pos; } break;
      };
    } break;

    case uSHDR: {
      switch (c) {
      case ';': { uri_headers = uri.substr(p1+1, pos-p1-1);
	  st = uSPARAM; p1 = pos; }; break; 
      case '>': { uri_headers = uri.substr(p1+1, pos-p1-1);
      st = uS6; p1 = pos; } break;
      case '\t':
      case ' ': { uri_headers = uri.substr(p1+1, pos-p1-1);
	  st = uSHDRWSP; p1 = pos; } break;      
      } 
    } break; 
    case uSHDRWSP: {
      switch (c) {
      case ';': { st = uSPARAM; p1 = pos; }; break; 
      case '>': { st = uS6;     p1 = pos; } break;
      }
    } break; 

    case uSPARAM: {
      switch (c) {
      case '>': { uri_param = uri.substr(p1+1, pos-p1-1);
      st = uS6; p1 = pos; } break;
      case '\t':
      case ' ': { uri_param = uri.substr(p1+1, pos-p1-1);
      st = uSPARAMWSP; p1 = pos; } break;      
      };
    } break; 
    case uSPARAMWSP: {
      switch (c) {
      case '>': { st = uS6; p1 = pos; } break;
      };
    } break; 
    };
    //    DBG("(2) c = %c, st = %d\n", c, st);
    pos++;
  }
  switch(st) {
  case uSUHOST:
  case uSHOST:  uri_host = uri.substr(p1+1, pos-p1-1); break;
  case uSPORT:  uri_port = uri.substr(p1+1, pos-p1-1); break;
  case uSHDR:   uri_headers = uri.substr(p1+1, pos-p1-1); break;
  case uSPARAM: uri_param = uri.substr(p1+1, pos-p1-1); break;
  case uS0:
  case uSPROT: { DBG("ERROR while parsing uri\n"); return false; } break;
  };
  return true;
}

#define pS0 0 // start
#define pS1 1 // name
#define pS2 2 // val
/**
 * parse params in param map
 *
 */
bool AmUriParser::parse_params(const string& line, int& pos) {
  size_t p1=pos, p2=pos;
  int st = 0; int quoted = false;
  char last_c = ' ';
  bool hit_comma = false;
  params.clear();
  while((size_t)pos < line.length()) {
    char c = line[pos];
    if (!quoted) {
      if (c == ',') {
	hit_comma = true;
	break;
      }
      if (c == '\"') {
	quoted = 1;
      } else if (c == '=') {
	p2 = pos; st = pS2;
      } else if (c == ';') {
	if (st == pS1) {
	  params[line.substr(p1, pos-p1)] = "";
	  st = pS0;
	  p1 = pos;
	} else if (st == pS2) {
	  params[line.substr(p1, p2-p1)] 
	    = line.substr(p2+1, pos-p2-1);
	  st = pS0;
	  p1 = pos;
	}
      } else {
	if (st == pS0) {
	  st = pS1;
	  p1 = pos;
	}
      }

    } else {
      if ((c == '\"') && (last_c != '\\')) quoted = 0;
    }
    last_c = c;
    pos++;
  }
	
  if (st == pS2) {
    if (hit_comma)
      params[line.substr(p1, p2-p1)] = line.substr(p2+1, pos-p2 -1);	
    else 
      params[line.substr(p1, p2-p1)] = line.substr(p2+1, pos-p2);	
  }
  return true;
}


bool AmUriParser::parse_contact(const string& line, size_t pos, size_t& end) {
  int p0 = skip_name(line, pos);
  if (p0 < 0) { return false; }

  if (p0-pos) {
    // fix display name
    int dn_start = pos;
    while (dn_start<p0 && line[dn_start]==' ')
      dn_start++;

    int dn_end = p0;
    while (dn_end>dn_start && line[dn_end-1]==' ')
      dn_end--;
    display_name = line.substr(dn_start, dn_end-dn_start);

    if (display_name.size() &&
	display_name[0]=='"' && display_name[display_name.length()-1] == '"') {
      // unquote
      display_name.erase(0,1);
      display_name.erase(display_name.size()-1,1);
      char last_c = ' ';
      size_t i=0;
      // unescape
      while (i<display_name.size()) {
	if ((last_c=='\\') &&
	    (display_name[i]=='"' ||display_name[i]=='\\')) {
	  display_name.erase(i-1, 1);
	  last_c = ' ';
	} else {
	  last_c = display_name[i];
	  i++;
	}
      }
    }
  }

  int p1 = skip_uri(line, p0);
  if (p1 < 0) { return false; }
  //  if (p1 < 0) return false;
  uri = line.substr(p0, p1-p0);
  if (!parse_uri()) { return false; }
  parse_params(line, p1);
  end = p1;
  return true;
}

void AmUriParser::dump() {
  DBG("--- Uri Info --- \n");
  DBG(" uri           '%s'\n", uri.c_str());
  DBG(" display_name  '%s'\n", display_name.c_str());
  DBG(" uri_user      '%s'\n", uri_user.c_str());
  DBG(" uri_host      '%s'\n", uri_host.c_str());
  DBG(" uri_port      '%s'\n", uri_port.c_str());
  DBG(" uri_hdr       '%s'\n", uri_headers.c_str());
  DBG(" uri_param     '%s'\n", uri_param.c_str());
  for (map<string, string>::iterator it = params.begin(); 
       it != params.end(); it++) {
    if (it->second.empty())
      DBG(" param         '%s'\n", it->first.c_str());
    else
      DBG(" param     '%s'='%s'\n", it->first.c_str(), it->second.c_str());
  }
  DBG("-------------------- \n");
}

string AmUriParser::uri_str()
{
  string res = uri_host;

  if(!uri_user.empty()) {
    res = uri_user + "@" + res;
  }

  if(!uri_port.empty()) {
    res += ":" + uri_port;
  }

  if(!uri_param.empty()) {
    res += ";" + uri_param;
  }

  res = "sip:" + res;

  return res;
}

string AmUriParser::nameaddr_str()
{
  return "\"" + display_name + "\" <" + uri_str() + ">";
}
