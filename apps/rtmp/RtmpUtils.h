/*
 * Copyright (C) 2011 Raphael Coeffic
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

#ifndef _RtmpUtils_h_
#define _RtmpUtils_h_

#include "librtmp/amf.h"

#define INVOKE_PTYPE    0x14
#define CONTROL_CHANNEL 0x03

#define _AVC(s) {(char*)s,sizeof(s)-1}
#define SAVC(x) const AVal av_##x = {(char*)#x,sizeof(#x)-1}
#define SAVC_def(x) extern const AVal av_##x
#define STR2AVAL(av,str) \
   { \
     av.av_val = (char*)str; \
     av.av_len = strlen(av.av_val); \
   }


SAVC_def(app);
SAVC_def(connect);
SAVC_def(flashVer);
SAVC_def(swfUrl);
SAVC_def(pageUrl);
SAVC_def(tcUrl);
SAVC_def(fpad);
SAVC_def(capabilities);
SAVC_def(audioCodecs);
SAVC_def(videoCodecs);
SAVC_def(videoFunction);
SAVC_def(objectEncoding);
SAVC_def(_result);
SAVC_def(_error);
SAVC_def(createStream);
SAVC_def(closeStream);
SAVC_def(deleteStream);
SAVC_def(getStreamLength);
SAVC_def(play);
SAVC_def(fmsVer);
SAVC_def(mode);
SAVC_def(level);
SAVC_def(code);
SAVC_def(description);
SAVC_def(secureToken);
SAVC_def(publish);
SAVC_def(onStatus);
SAVC_def(status);
SAVC_def(error);
SAVC_def(NetStream_Play_Start);
SAVC_def(Started_playing);
SAVC_def(NetStream_Play_Stop);
SAVC_def(Stopped_playing);
SAVC_def(details);
SAVC_def(clientid);
SAVC_def(pause);

// custom methods and params
SAVC_def(dial);
SAVC_def(hangup);
SAVC_def(register);
SAVC_def(accept);
SAVC_def(Sono_Call_Incoming);
SAVC_def(uri);
SAVC_def(Sono_Call_Status);
SAVC_def(status_code);


#endif
