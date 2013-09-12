# transparent SBC profile
#
# This implements a transparent B2BUA - all possible options are commented

# defaults: transparent
#RURI=$r
#From=$f
#To=$t

#Contact=<sip:$Ri>

#Call-ID
#Call-ID=$ci_leg2

## routing
# outbound proxy:
#outbound_proxy=sip:192.168.5.106:5060
# force outbound proxy (in-dialog requests)?
#force_outbound_proxy=yes
# destination IP[:port] for outgoing requests
#next_hop=192.168.5.106:5060
# set RURI as calculated next_hop
#patch_ruri_next_hop=yes
# outbound interface to use (interface ID)
#outbound_interface=extern

## RTP relay
# enable RTP relaying (bridging):
#enable_rtprelay=yes
# force symmetric RTP (start with passive mode):
#rtprelay_force_symmetric_rtp=yes
# use symmetric RTP indication from P-MsgFlags flag 2
#rtprelay_msgflags_symmetric_rtp=yes
# RTP interface to use for A leg
#aleg_rtprelay_interface=intern
# RTP interface to use for B leg
#rtprelay_interface=default
# use transparent RTP seqno? [yes]
#rtprelay_transparent_seqno=no
# use transparent RTP SSRC? [yes]
#rtprelay_transparent_ssrc=no

## filters: 
#header_filter=blacklist
#header_list=P-App-Param,P-App-Name
#message_filter=transparent
#message_list=
#sdp_filter=whitelist
#sdpfilter_list=g729,g723,ilbc,speex,gsm,amr
# Filter A-Lines: Either black or whitelist
#sdp_alines_filter=whitelist
# Lines to be filtered, separated by ","
#sdp_alinesfilter_list=crypto,x-cap
#sdp_anonymize=yes

## append extra headers
#append_headers="P-Source-IP: $si\r\nP-Source-Port: $sp\r\n"

## reply translations
# translate some 6xx class replies to 4xx class:
#reply_translations="603=>488 Not acceptable here|600=>406 Not Acceptable"

## authentication:
#enable_auth=yes
#auth_user=$P(u)
#auth_pwd=$P(p)

## authentication for A (caller) leg:
#enable_aleg_auth=yes
#auth_aleg_user=$P(au)
#auth_aleg_pwd=$P(ap)

## call timer
#enable_call_timer=yes
#call_timer=60
# or, e.g.: call_timer=$P(t)

## prepaid
#enable_prepaid=yes
#prepaid_accmodule=cc_acc
#prepaid_uuid=$H(P-Caller-Uuid)
#prepaid_acc_dest=$H(P-Acc-Dest)

## session timer:
#enable_session_timer=yes
# if session_expires is not configured here,
# the values from sbc.conf are used, or the
# default values
#session_expires=120
#minimum_timer=90
#session_refresh_method=UPDATE_FALLBACK_INVITE
#accept_501_reply=yes

##separate SST configuration for A (caller) leg, optional:
#enable_aleg_session_timer=yes
#aleg_session_expires=120
#aleg_minimum_timer=90
#aleg_maximum_timer=900
#aleg_session_refresh_method=UPDATE_FALLBACK_INVITE
#aleg_accept_501_reply=yes

## refuse call
# refuse all calls with <code> <reason>
#refuse_with="404 Not Found"
