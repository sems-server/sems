/*
 * $Id: AmUtils.h,v 1.18.2.1 2005/08/31 13:54:29 rco Exp $
 *
 * Copyright (C) 2006 iptego GmbH
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

#include "ContactInfo.h"
#include "log.h"

#include <strings.h>
#include <iostream>
using namespace std;

bool ContactInfo::isEqual(const ContactInfo& c) const {
  return (uri_user == c.uri_user) &&
    (!strcasecmp(uri_host.c_str(), 
		 c.uri_host.c_str())) &&
    (uri_port == c.uri_port);
}

/*
 * Skip display name part
 */
static inline int skip_name(string& s, unsigned int pos)
{
  size_t i;
  int last_wsp, quoted = 0;
	
  for(i = pos; i < s.length(); i++) {
    char c = s[i];
    if (!quoted) {
      if ((c == ' ') || (c == '\t')) {
	last_wsp = i;
      } else {
	if (c == '<') {
	  return i;
	}

	if (c == '\"') {
	  quoted = 1;
	}
      }
    } else {
      if ((c == '\"') && (s[i-1] != '\\')) quoted = 0;
    }
  }

  if (quoted) {
    ERROR("skip_name(): Closing quote missing in name part of Contact\n");
    return -1;
  } 

  return pos; // no name to skip
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
static inline int skip_uri(string& s, unsigned int pos)
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

#define    uS0 0    // start
#define	   uS1 1    // protocol
#define	   uS2 2    // user / host
#define	   uS3 3    // host
#define    uS3WSP 4 // wsp after host
#define	   uS4 5    // port
#define    uS4WSP 6 // wsp after port
#define    uS5 7    // params 
#define    uS5WSP 8 // wsp after params
#define    uS6 9    // end
/**
 * parse uri into user, host, port, param
 *
 */
bool ContactInfo::parse_uri() {
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
      case '<': { st = uS1; } break;
      default: { 
	if ((eq<=4)&&(toupper(c) ==sip_prot[eq])) 
	  eq++; 
	if (eq==4) { // found sip:
	  st = uS2; p1 = pos;
	};
      } break;
      }; 
    } break;
    case uS1: {
      if (c ==  ':')  { st = uS2; p1 = pos;} 
    } break;
    case uS2: {
      switch(c) {
      case '@':  { 
	uri_user = uri.substr(p1+1, pos-p1-1);
	st = uS3; p1 = pos;
      } ; break;
      case ':': {
	uri_host = uri.substr(p1+1, pos-p1-1);
	st = uS4; p1 = pos;
      }; break;
      case ';': {
	uri_host = uri.substr(p1+1, pos-p1-1);
	st = uS5; p1 = pos;
      }; break;
      case '>': {
	uri_host = uri.substr(p1+1, pos-p1-1);
	st = uS6; p1 = pos;
      }; break;
      }
    } break;
    case uS3: {
      switch (c) {
      case ':': { uri_host = uri.substr(p1+1, pos-p1-1); 
	  st = uS4; p1 = pos; } 
	break;
      case ';': { uri_host = uri.substr(p1+1, pos-p1-1);
	  st = uS5; p1 = pos; } 
	break;
      case '>': { uri_host = uri.substr(p1+1, pos-p1-1); 
	  st = uS6; p1 = pos; } 
	break;
      case ' ': 
      case '\t': { uri_host = uri.substr(p1+1, pos-p1-1); 
	  st = uS3WSP; p1 = pos; } 
	break;
      };
    } break;
    case uS3WSP: {
      switch (c) {
      case ':': { st = uS4; p1 = pos; } 
	break;
      case ';': { st = uS5; p1 = pos; } 
	break;
      case '>': { st = uS6; p1 = pos; } 
    }
    case uS4: {
      switch (c) {
      case ';': { uri_port = uri.substr(p1+1, pos-p1-1); 
	  st = uS5; p1 = pos; } 
	break;
      case '>': { uri_port = uri.substr(p1+1, pos-p1-1); 
	  st = uS6; p1 = pos; } 
	break;
      };
      case ' ': 
      case '\t': 
	{ uri_port = uri.substr(p1+1, pos-p1-1); 
	  st = uS4WSP; p1 = pos; } 
	break;
      };
    } break;
    case uS4WSP: {
      switch (c) {
      case ';': { st = uS5; p1 = pos; } 
	break;
      case '>': { st = uS6; p1 = pos; } 
	break;
      };
    } break;
    case uS5: {
      switch (c) {
      case '>': { uri_param = uri.substr(p1+1, pos-p1-1);
      st = uS6; p1 = pos; } 
	break;
      case ' ': { uri_param = uri.substr(p1+1, pos-p1-1);
      st = uS5WSP; p1 = pos; } 
	break;      };
    } break; 
    case uS5WSP: {
      switch (c) {
      case '>': { st = uS6; p1 = pos; } 
	break;
      };
    } break; 
    };
    //    DBG("(2) c = %c, st = %d\n", c, st);
    pos++;
  }
  switch(st) {
  case uS2:
  case uS3: uri_host = uri.substr(p1+1, pos-p1-1); break;
  case uS4: uri_port = uri.substr(p1+1, pos-p1-1); break;
  case uS5: uri_param = uri.substr(p1+1, pos-p1-1); break;
  case uS0:
  case uS1: { DBG("ERROR while parsing uri\n"); return false; } break;
  };
  return true;
}

#define pS0 0 // start
#define pS1 1 // name
#define pS2 2 // val
/**
 * parse params int param map
 *
 */
bool ContactInfo::parse_params(string& line, int& pos) {
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
	if ((st == pS2) ||(st == pS1)) {
	  params[line.substr(p1, p2-p1)] 
	    = line.substr(p2+1, pos-p2-1);
	  st = pS0;
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


bool ContactInfo::parse_contact(string& line, size_t pos, size_t& end) {
  int p0 = skip_name(line, pos);
  if (p0 < 0) { return false; }
  int p1 = skip_uri(line, p0);
  if (p1 < 0) { return false; }
  //  if (p1 < 0) return false;
  uri = line.substr(p0, p1-p0);
  if (!parse_uri()) { return false; }
  parse_params(line, p1);
  end = p1;
  return true;
}

void ContactInfo::dump() {
  DBG("--- Contact Info --- \n");
  DBG(" uri       '%s'\n", uri.c_str());
  DBG(" uri_user  '%s'\n", uri_user.c_str());
  DBG(" uri_host  '%s'\n", uri_host.c_str());
  DBG(" uri_port  '%s'\n", uri_port.c_str());
  DBG(" uri_param '%s'\n", uri_param.c_str());
  for (map<string, string>::iterator it = params.begin(); 
       it != params.end(); it++) 
    DBG(" param     '%s'='%s'\n", it->first.c_str(), it->second.c_str()) ;
  DBG("-------------------- \n");
}
