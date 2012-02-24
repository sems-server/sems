/*
 * Copyright (C) 2010 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "ParamReplacer.h"

#include "log.h"
#include "AmSipHeaders.h"
#include "AmUtils.h"
#include "SBC.h" // for RegexMapper SBCFactory::regex_mappings
#include <algorithm>

void replaceParsedParam(const string& s, size_t p,
			const AmUriParser& parsed, string& res) {
  switch (s[p+1]) {
  case 'u': { // URI
    res+=parsed.uri_user+"@"+parsed.uri_host;
    if (!parsed.uri_port.empty())
      res+=":"+parsed.uri_port;
  } break;
  case 'U': res+=parsed.uri_user; break; // User
  case 'd': { // domain
    res+=parsed.uri_host;
    if (!parsed.uri_port.empty())
      res+=":"+parsed.uri_port;
  } break;
  case 'h': res+=parsed.uri_host; break; // host
  case 'p': res+=parsed.uri_port; break; // port
  case 'H': res+=parsed.uri_headers; break; // Headers
  case 'P': res+=parsed.uri_param; break; // Params
  case 'n': res+=parsed.display_name; break; // Params

  // case 't': { // tag
  //   map<string, string>::const_iterator it = parsed.params.find("tag");
  //   if (it != parsed.params.end())
  //     res+=it->second;
  // } break;
  default: WARN("unknown replace pattern $%c%c\n",
		s[p], s[p+1]); break;
  };
}

string replaceParameters(const string& s,
			 const char* r_type,
			 const AmSipRequest& req,
			 const string& app_param,
			 AmUriParser& ruri_parser, AmUriParser& from_parser,
			 AmUriParser& to_parser) {
  string res;
  bool is_replaced = false;
  size_t p = 0;
  bool is_escaped = false;

  // char last_char=' ';
  
  while (p<s.length()) {
    size_t skip_chars = 1;

    if (is_escaped) {
      switch (s[p]) {
      case 'r': res += '\r'; break;
      case 'n': res += '\n'; break;
      case 't': res += '\t'; break;
      default: res += s[p]; break;
      }
      is_escaped = false;
    } else { // not escaped
      if (s[p]=='\\') {
	if (p==s.length()-1) {
	  res += '\\'; // add single \ at the end
	} else {
	  is_escaped = true;
	  is_replaced = true;
	}
      } else if ((s[p]=='$') && (s.length() >= p+1)) {
	is_replaced = true;
	p++;
	switch (s[p]) {
	case 'f': { // from
	  if ((s.length() == p+1) || (s[p+1] == '.')) {
	    res += req.from;
	    break;
	  }

	  if (s[p+1]=='t') { // $ft - from tag
	    res += req.from_tag;
	    break;
	  }

	  if (from_parser.uri.empty()) {
	    size_t end;
	    if (!from_parser.parse_contact(req.from, 0, end)) {
	      WARN("Error parsing From URI '%s'\n", req.from.c_str());
	      break;
	    }
	    from_parser.dump();
	  }

	  replaceParsedParam(s, p, from_parser, res);

	}; break;

	case 't': { // to
	  if ((s.length() == p+1) || (s[p+1] == '.')) {
	    res += req.to;
	    break;
	  }

	  if (to_parser.uri.empty()) {
	    size_t end;
	    if (!to_parser.parse_contact(req.to, 0, end)) {
	      WARN("Error parsing To URI '%s'\n", req.to.c_str());
	      break;
	    }
	  }

	  replaceParsedParam(s, p, to_parser, res);

	}; break;

	case 'r': { // r-uri
	  if ((s.length() == p+1) || (s[p+1] == '.')) {
	    res += req.r_uri;
	    break;
	  }

	  if (ruri_parser.uri.empty()) {
	    ruri_parser.uri = req.r_uri;
	    if (!ruri_parser.parse_uri()) {
	      WARN("Error parsing R-URI '%s'\n", req.r_uri.c_str());
	      break;
	    }
	  }
	  replaceParsedParam(s, p, ruri_parser, res);
	}; break;

	case 'c': { // call-id
	  if ((s.length() == p+1) || (s[p+1] == 'i')) {
	    res += req.callid;
	    break;
	  }
	  WARN("unknown replacement $c%c\n", s[p+1]);
	}; break;

	case 's': { // source (remote)
	  if (s.length() < p+1) {
	    WARN("unknown replacement $s\n");
	    break;
	  }

	  if (s[p+1] == 'i') { // $si source IP address
	    res += req.remote_ip.c_str();
	    break;
	  } else if (s[p+1] == 'p') { // $sp source port
	    res += int2str(req.remote_port);
	    break;
	  }

	  WARN("unknown replacement $s%c\n", s[p+1]);
	}; break;

	case 'R': { // received (local)
	  if (s.length() < p+1) {
	    WARN("unknown replacement $R\n");
	    break;
	  }

	  if (s[p+1] == 'i') { // $Ri received IP address
	    res += req.local_ip.c_str();
	    break;
	  } else if (s[p+1] == 'p') { // $Rp received port
	    res += int2str(req.local_port);
	    break;
	  } else if (s[p+1] == 'f') { // $Rf received interface id
	    res += int2str(req.local_if);
	  } else if (s[p+1] == 'n') { // $Rn received interface name
	    if ((req.local_if >= 0) && req.local_if < AmConfig::Ifs.size()) {
	      res += AmConfig::Ifs[req.local_if].name;
	    }
	  } else if (s[p+1] == 'I') { // $RI received interface public IP
	    if ((req.local_if >= 0) && req.local_if < AmConfig::Ifs.size()) {
	      res += AmConfig::Ifs[req.local_if].PublicIP;
	    }
	  }
	  WARN("unknown replacement $R%c\n", s[p+1]);
	}; break;


#define case_HDR(pv_char, pv_name, hdr_name)				\
	  case pv_char: {						\
	    AmUriParser uri_parser;					\
	    string m_uri = getHeader(req.hdrs, hdr_name);		\
	    if ((s.length() == p+1) || (s[p+1] == '.')) {		\
	      res += m_uri;						\
	      break;							\
	    }								\
	    size_t end;							\
	    if (!uri_parser.parse_contact(m_uri, 0, end)) {		\
	      WARN("Error parsing " pv_name " URI '%s'\n", m_uri.c_str()); \
	      break;							\
	    }								\
	    if (s[p+1] == 'i') {					\
	      res+=uri_parser.uri_user+"@"+uri_parser.uri_host;		\
	      if (!uri_parser.uri_port.empty())				\
		res+=":"+uri_parser.uri_port;				\
	    } else {							\
	      replaceParsedParam(s, p, uri_parser, res);		\
	    }								\
	  }; break;

	  case_HDR('a', "PAI", SIP_HDR_P_ASSERTED_IDENTITY);  // P-Asserted-Identity
	  case_HDR('p', "PPI", SIP_HDR_P_PREFERRED_IDENTITY); // P-Preferred-Identity

	case 'P': { // app-params
	  if (s[p+1] != '(') {
	    WARN("Error parsing P param replacement (missing '(')\n");
	    break;
	  }
	  if (s.length()<p+3) {
	    WARN("Error parsing P param replacement (short string)\n");
	    break;
	  }

	  size_t skip_p = p+2;
	  for (;skip_p<s.length() && s[skip_p] != ')';skip_p++) { }
	  if (skip_p==s.length()) {
	    WARN("Error parsing P param replacement (unclosed brackets)\n");
	    break;
	  }
	  string param_name = s.substr(p+2, skip_p-p-2);
	  // DBG("param_name = '%s' (skip-p - p = %d)\n", param_name.c_str(), skip_p-p);
	  res += get_header_keyvalue(app_param, param_name);
	  skip_chars = skip_p-p;
	} break;

	case 'H': { // header
	  size_t name_offset = 2;
	  if (s[p+1] != '(') {
	    if (s[p+2] != '(') {
	      WARN("Error parsing H header replacement (missing '(')\n");
	      break;
	    }
	    name_offset = 3;
	  }
	  if (s.length()<name_offset+1) {
	    WARN("Error parsing H header replacement (short string)\n");
	    break;
	  }

	  size_t skip_p = p+name_offset;
	  for (;skip_p<s.length() && s[skip_p] != ')';skip_p++) { }
	  if (skip_p==s.length()) {
	    WARN("Error parsing H header replacement (unclosed brackets)\n");
	    break;
	  }
	  string hdr_name = s.substr(p+name_offset, skip_p-p-name_offset);
	  // DBG("param_name = '%s' (skip-p - p = %d)\n", param_name.c_str(), skip_p-p);
	  if (name_offset == 2) {
	    // full header
	    res += getHeader(req.hdrs, hdr_name);
	  } else {
	    // parse URI and use component
	    AmUriParser uri_parser;
	    string m_uri = getHeader(req.hdrs, hdr_name);
	    if ((s[p+1] == '.')) {
	      res += m_uri;
	      break;
	    }

	    size_t end;
	    if (!uri_parser.parse_contact(m_uri, 0, end)) {
	      WARN("Error parsing header %s URI '%s'\n",
		   hdr_name.c_str(), m_uri.c_str());
	      break;
	    }
	    replaceParsedParam(s, p, uri_parser, res);
	  }
	  skip_chars = skip_p-p;
	} break;

	case 'M': { // regex map
	  if (s[p+1] != '(') {
	    WARN("Error parsing $M regex map replacement (missing '(')\n");
	    break;
	  }
	  if (s.length()<p+3) {
	    WARN("Error parsing $M regex map replacement (short string)\n");
	    break;
	  }

	  size_t skip_p = p+2;
	  skip_p = skip_to_end_of_brackets(s, skip_p);

	  if (skip_p==s.length()) {
	    WARN("Error parsing $M regex map replacement (unclosed brackets)\n");
	    skip_chars = skip_p-p;
	    break;
	  }

	  string map_str = s.substr(p+2, skip_p-p-2);
	  size_t spos = map_str.rfind("=>");
	  if (spos == string::npos) {
	    skip_chars = skip_p-p;
	    WARN("Error parsing $M regex map replacement: no => found in '%s'\n",
		 map_str.c_str());
	    break;
	  }

	  string map_val = map_str.substr(0, spos);
	  string map_val_replaced = replaceParameters(map_val, r_type, req, app_param,
						      ruri_parser, from_parser, to_parser);
	  string mapping_name = map_str.substr(spos+2);

	  string map_res; 
	  if (SBCFactory::regex_mappings.
	      mapRegex(mapping_name, map_val_replaced.c_str(), map_res)) {
	    DBG("matched regex mapping '%s' (orig '%s) in '%s'\n",
		map_val_replaced.c_str(), map_val.c_str(), mapping_name.c_str());
	    res+=map_res;
	  } else {
	    DBG("no match in regex mapping '%s' (orig '%s') in '%s'\n",
		map_val_replaced.c_str(), map_val.c_str(), mapping_name.c_str());
	  }
 
	  skip_chars = skip_p-p;
	} break;

	case '_': { // modify
	  if (s.length()<p+4) { // $_O()
	    WARN("Error parsing $_ modifier replacement (short string)\n");
	    break;
	  }

	  char operation = s[p+1];
	  if (operation != 'U' && operation != 'l'
	      && operation != 's' && operation != '5') {
	    WARN("Error parsing $_%c string modifier: unknown operator '%c'\n",
		 operation, operation);
	  }

	  if (s[p+2] != '(') {
	    WARN("Error parsing $U upcase replacement (missing '(')\n");
	    break;
	  }

	  size_t skip_p = p+3;
	  skip_p = skip_to_end_of_brackets(s, skip_p);

	  if (skip_p==s.length()) {
	    WARN("Error parsing $_ modifier (unclosed brackets)\n");
	    skip_chars = skip_p-p;
	    break;
	  }

	  string br_str = s.substr(p+3, skip_p-p-3);
	  string br_str_replaced = replaceParameters(br_str, "$_*(...)",
						     req, app_param,
						     ruri_parser, from_parser, to_parser);
	  br_str = br_str_replaced;
	  switch(operation) {
	  case 'u': // uppercase
	    transform(br_str_replaced.begin(), br_str_replaced.end(),
		      br_str_replaced.begin(), ::toupper); break;
	  case 'l': // lowercase
	    transform(br_str_replaced.begin(), br_str_replaced.end(),
		      br_str_replaced.begin(), ::tolower); break;

	  case 's': // size (string length)
	    br_str_replaced = int2str((unsigned int)br_str.length());
	    break;

	  case '5': // md5
	    br_str_replaced = calculateMD5(br_str);
	    break;

	  default: break;
	  }
	  DBG("applied operator '%c': '%s' => '%s'\n", operation,
	      br_str.c_str(), br_str_replaced.c_str());
	  res+=br_str_replaced;

	  skip_chars = skip_p-p;
	} break;

	default: {
	  WARN("unknown replace pattern $%c%c\n",
	       s[p], s[p+1]);
	}; break;
	};

	p+=skip_chars; // skip $.X
      } else {
	res += s[p];
      }
    } // end not escaped

    p++;
  }

  if (is_replaced) {
    DBG("%s pattern replace: '%s' -> '%s'\n", r_type, s.c_str(), res.c_str());
  }
  return res;
}
