/*
 * $Id$
 *
 * Copyright (C) 2007 Raphael Coeffic
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

#include "trans_layer.h"
#include "udp_trsp.h"

#include "sip_parser.h"
#include "parse_header.h"
#include "hash_table.h"

#include "log.h"

#include "SipCtrlInterface.h"

#include "AmSipMsg.h"
#include "AmUtils.h"

#include <signal.h>

#define SERVER

static void sig_usr(int signo)
{
  WARN("signal %d received\n", signo);
    
  dumps_transactions();
  exit(0);

  return;
}

int main()
{
    log_level  = 3;
    log_stderr = 1;

    //udp_trsp* udp_server = new udp_trsp(tl);
    SipCtrlInterface* ctrl = new SipCtrlInterface("127.0.0.1",5060);
    trans_layer::instance()->register_ua(ctrl);
    
#ifndef SERVER
    char* buf = 
	"REGISTER sip:192.168.0.22 SIP/2.0\r\n"
	"Via: SIP/2.0/UDP 192.168.0.24:5060;branch=z9hG4bKf3f8ddeb9512414252418e7c18c2f0e;rport\r\n"
	"From: \"Raphael\" <sip:raf@192.168.0.22>;tag=2239770325\r\n"
	"To: \"Raphael\" <sip:raf@192.168.0.22>\r\n"
	"Call-ID: 1199294025@192_168_0_24\r\n"
	"CSeq: 1 REGISTER\r\n"
	"Contact: <sip:raf@192.168.0.24:5060>\r\n"
	"Max-Forwards: 70\r\n"
	"User-Agent: S450 IP020970000000\r\n"
	"Expires: 180\r\n"
	"Allow: INVITE, ACK, CANCEL, BYE, OPTIONS, INFO, REFER, SUBSCRIBE, NOTIFY\r\n"
	"Content-Length: 0\r\n"
	"\r\n";

    char* hdr = "Route: <sip:10.36.2.24;ftag=qvj9pp5vw7;lr=on>\r\n";

    char *c = hdr;
    
    sip_msg* msg = new sip_msg();
    int err = parse_headers(msg,&c);
    
    if(err){
	ERROR("Route headers parsing failed\n");
	ERROR("Faulty headers were: <%s>\n",hdr);
	return -1;
    }
    

//     char* buf = 
// 	"INVITE sip:bob@biloxi.com;user=phone;tti=13;ttl=12?abc=def SIP/2.0\r\n"
//  	"Via: SIP/2.0/UDP bigbox3.site3.atlanta.com;branch=z9hG4bK77ef4c2312983.1\r\n"
//  	"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8\r\n"
//  	" ;received=192.0.2.1\r\n"
// 	"Max-Forwards: 69\r\n"
// 	"To: Bob <sip:bob@biloxi.com>\r\n"
// 	"From: sip:alice@atlanta.com;tag=1928301774\r\n"
// 	"Call-ID: a84b4c76e66710\r\n"
// 	"CSeq: 314159 INVITE\r\n"
// 	"Contact: <sip:alice@pc33.atlanta.com>\r\n"
// 	"Content-Type: application/sdp\r\n"
// 	"Content-Length: 148\r\n"
// 	"\r\n"
// 	"v=0\r\n"
// 	"o=alice 53655765 2353687637 IN IP4 pc33.atlanta.com\r\n"
// 	"s=-\r\n"
// 	"t=0 0\r\n"
// 	"c=IN IP4 pc33.atlanta.com\r\n"
// 	"m=audio 3456 RTP/AVP 0 1 3 99\r\n"
// 	"a=rtpmap:0 PCMU/8000";

    //int buf_len = strlen(buf);
    //sip_msg* msg = new sip_msg(buf,buf_len);
    
    //trans_layer* tl = trans_layer::instance();
    //tl->register_ua(ctrl);
    //tl->received_msg(msg);

    //delete msg;

#else

    if (signal(SIGINT, sig_usr) == SIG_ERR ) {
	ERROR("no SIGINT signal handler can be installed\n");
	return -1;
    }
    
    ctrl->start();
    
//     sleep(1);

//     AmSipRequest req;
//     req.method   = "INVITE";
//     req.r_uri    = "sip:sipp@tinytop:5080";
//     req.from     = "From: SEMS <sip:sems@tinytop:5060>;tag=" + int2str(getpid());
//     //req.from_tag = "12345";
//     req.to       = "To: SIPP <sip:sipp@tinytop:5070>";
//     req.cseq     = 10;
//     req.callid   = int2str(getpid()) + "@tinytop";
//     req.contact  = "Contact: sip:tinytop";
//     //req.route    = "Route: <sip:localhost:5070;lr=on>;blabla=abc"; 

//     int send_err = ctrl->send(req, req.serKey);
//     if(send_err < 0) {
//       ERROR("ctrl->send() failed with error code %i\n",send_err);
//     }

    //sleep(10);
    ctrl->join();
    
#endif


    return 0;
}
