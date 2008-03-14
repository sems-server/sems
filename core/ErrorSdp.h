/*
 *
 */


#ifndef __ErrorSdp__
#define __ErrorSdp__

#include <string>
#include <stdio.h>
#include <iostream>
#include "log.h"
using namespace std;


/*
 *Check if known media type is used
 */
static int media_type(string media)
{
  if(media == "audio")
    return 1;
  else if(media == "video")
    return 2;
  else if(media == "application")
    return 3;
  else if(media == "text")
    return 4;
  else if(media == "message")
    return 5;
  else 
    return -1;
}

static int transport_type(string transport)
{
  if(transport == "RTP/AVP")
    return 1;
  else if(transport == "UDP")
    return 2;
  else if(transport == "RTP/SAVP")
    return 3;
  else 
    return -1;
}

/*
*Check if known attribute name is used
*/
static bool attr_check(string attr)
{
  if(attr == "cat")
    return true;
  else if(attr == "keywds")
    return true;
  else if(attr == "tool")
    return true;
  else if(attr == "ptime")
    return true;
  else if(attr == "maxptime")
    return true;
  else if(attr == "recvonly")
    return true;
  else if(attr == "sendrecv")
    return true;
  else if(attr == "sendonly")
    return true;
  else if(attr == "inactive")
    return true;
  else if(attr == "orient")
    return true;
  else if(attr == "type")
    return true;
  else if(attr == "charset")
    return true;
  else if(attr == "sdplang")
    return true;
  else if(attr == "lang")
    return true;
  else if(attr == "framerate")
    return true;
  else if(attr == "quality")
    return true;
  else
    {
    DBG("sdp_parse_attr: Unknow attribute name used: %s, plz see RFC4566\n", (char*)attr.c_str());
    return false;
    }
}


#endif
