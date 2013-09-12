# sst_b2b SBC profile
#
# This implements a transparent B2BUA with
# SIP Session Timers known from the sst_b2b app.
# Also see the description below.

# defaults: transparent
#RURI=$r
#From=$f
#To=$t

## routing
# outbound proxy:
#outbound_proxy=sip:192.168.5.106:5060
# force outbound proxy (in-dialog requests)?
#force_outbound_proxy=yes
# destination IP[:port] for outgoing requests
#next_hop=192.168.5.106

## filters: 
#header_filter=blacklist
#header_list=P-App-Param,P-App-Name
#message_filter=transparent
#message_list=

## authentication:
#enable_auth=yes
#auth_user=$P(u)
#auth_pwd=$P(p)

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
enable_session_timer=yes
# if session_expires is not configured here,
# the values from sbc.conf are used, or the
# default values
#session_expires=120
#minimum_timer=90
#maximum_timer=900
#session_refresh_method=UPDATE_FALLBACK_INVITE
#accept_501_reply=yes

#separate SST configuration for A (caller) leg:
#enable_aleg_session_timer=$H(P-Enable-A-SST)
#aleg_session_expires=$H(P-A-SST-Timer)
#aleg_minimum_timer=90
#aleg_maximum_timer=900
#aleg_session_refresh_method=UPDATE_FALLBACK_INVITE
#aleg_accept_501_reply=yes

#
#This application can be routed through for achieving 
#two things: 
#
#  1. Forcing SIP Session Timers, which prevents 
#     overbilling for calls where BYE is missing,
#     for example in cases where media (RTP) path 
#     does not go through the system, but billing 
#     is still done.
#
#  2. Topology hiding; this application acts as 
#     B2BUA, so on the B leg, no routing info from 
#     the A leg can be seen.
#
#The incoming INVITE for a newly established call is 
#passed in signaling only B2B mode to the B leg, 
#which tries to send it to the request URI.
#
#SIP Session Timers are enabled on both legs. The
#session refresh method may be configured; UPDATE
#or INVITE with last established SDP may be used.
