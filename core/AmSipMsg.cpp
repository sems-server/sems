#include <string.h>
#include <stdlib.h>
#include "AmUtils.h"
#include "AmSipMsg.h"


string getHeader(const string& hdrs,const string& hdr_name)
{
  size_t pos1; 
  size_t pos2;
  size_t pos_s;
  if (findHeader(hdrs,hdr_name, pos1, pos2, pos_s)) 
    return hdrs.substr(pos1,pos2-pos1);
  else
    return "";
}

string getHeader(const string& hdrs,const string& hdr_name, 
		 const string& compact_hdr_name)
{
  string res = getHeader(hdrs, hdr_name);
  if (!res.length())
    return getHeader(hdrs, compact_hdr_name);
  return res;
}

bool findHeader(const string& hdrs,const string& hdr_name, 
		size_t& pos1, size_t& pos2, size_t& hdr_start)
{
  unsigned int p;
  char* hdr = strdup(hdr_name.c_str());
  const char* hdrs_c = hdrs.c_str();
  char* hdr_c = hdr;
  const char* hdrs_end = hdrs_c + hdrs.length();
  const char* hdr_end = hdr_c + hdr_name.length(); 

  while(hdr_c != hdr_end){
    if('A' <= *hdr_c && *hdr_c <= 'Z')
      *hdr_c -= 'A' - 'a';
    hdr_c++;
  }

  while(hdrs_c != hdrs_end){

    hdr_c = hdr;

    while((hdrs_c != hdrs_end) && (hdr_c != hdr_end)){

      char c = *hdrs_c;
      if('A' <= *hdrs_c && *hdrs_c <= 'Z')
	c -= 'A' - 'a';

      if(c != *hdr_c)
	break;

      hdr_c++;
      hdrs_c++;
    }

    if(hdr_c == hdr_end)
      break;

    while((hdrs_c != hdrs_end) && (*hdrs_c != '\n'))
      hdrs_c++;

    if(hdrs_c != hdrs_end)
      hdrs_c++;
  }
    
  if(hdr_c == hdr_end){
    hdr_start = hdrs_c - hdrs.c_str();;

    while((hdrs_c != hdrs_end) && (*hdrs_c == ' '))
      hdrs_c++;

    if((hdrs_c != hdrs_end) && (*hdrs_c == ':')){

      hdrs_c++;
      while((hdrs_c != hdrs_end) && (*hdrs_c == ' '))
	hdrs_c++;
	    
      p = hdrs_c - hdrs.c_str();
	    
      string::size_type p_end;
      if( ((p_end = hdrs.find('\r',p)) != string::npos)
	  || ((p_end = hdrs.find('\n',p)) != string::npos) ){
		
	free(hdr);
	// return hdrs.substr(p,p_end-p);
	pos1 = p;
	pos2 = p_end;
	return true;
      }
    }
  }

  free(hdr);
  //    return "";
  return false;
}

/* Print Member */
#define _PM(member, name) \
  do { \
    if (! member.empty()) \
      buf += string(name) + ":" + member + ";"; \
  } while (0)
/* Print Member in Brackets */
#define _PMB(member, name) \
  do { \
    if (! member.empty()) \
      buf += string(name) + ":" + "[" + member + "]" + ";"; \
  } while (0)

string AmSipRequest::print()
{
  string buf;

  _PM(serKey, "serkey");

  _PM(r_uri, "r-uri");
  _PM(callid, "i");
  _PM(int2str(cseq), "cseq");
  _PM(from_tag, "l-tag");
  _PM(to_tag, "r-tag");
  _PM(next_hop, "nhop");
  _PMB(route, "rtset");
  _PM(contact, "m");

  _PMB(hdrs, "hdr");
  _PM(content_type, "c");
  _PMB(body, "body");

  _PM(user, "user");
  _PM(domain, "domain");
  _PM(from_uri, "f-uri");
  _PM(from, "from");
  _PM(to, "to");

  _PM(dstip, "dstip");
  _PM(port, "dstport");


  buf = method + " [" + buf + "]";
  return buf;
}

string AmSipReply::print()
{
  string buf;

  _PM(serKey, "serkey");

  _PM(int2str(code), "code");
  _PMB(reason, "phrase");
  _PM(callid, "i");
  _PM(int2str(cseq), "cseq");
  _PM(local_tag, "l-tag");
  _PM(remote_tag, "r-tag");
  _PM(next_hop, "nhop");
  _PMB(route, "rtset");
  _PM(contact, "m");

  _PMB(hdrs, "hdr");
  _PM(content_type, "c");
  _PMB(body, "body");

  _PM(next_request_uri, "next-r-uri");

  _PM(dstip, "dstip");
  _PM(port, "dstport");

  buf = method + " [" + buf + "]";
  return buf;
}

#undef _PM
#undef _PMB
