# auth_b2b SBC profile
#
# This implements the identity change and SIP
# authentication known from auth_b2b app.
# For a more detailed description see the 
# explanation below.
#
# A P-App-Param header is expected of the form:
#   P-App-Param: u=<user>;d=<domain>;p=<pwd>
# The INVITE is then sent from <user>@<domain>
# to ruri-user@<domain>
#
# if the user/domain/password should be set here
# in the configuration, replace $P(u), $P(p) and $P(d)
# below.

RURI=sip:$rU@$P(d)
From="\"$P(u)\" <sip:$P(u)@$P(d)>"
To="\"$rU\" <sip:$rU@$P(d)>"

## routing
# outbound proxy:
#outbound_proxy=sip:192.168.5.106:5060
# force outbound proxy (in-dialog requests)?
#force_outbound_proxy=yes
# destination IP[:port] for outgoing requests
#next_hop=192.168.5.106

enable_auth=yes
auth_user=$P(u)
auth_pwd=$P(p)

## authentication for A (caller) leg:
#enable_aleg_auth=yes
#auth_aleg_user=$P(au)
#auth_aleg_pwd=$P(ap)

header_filter=blacklist
header_list=P-App-Param,P-App-Name
message_filter=transparent
#message_list=

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
#session_expires=120
#minimum_timer=90
#session_refresh_method=UPDATE_FALLBACK_INVITE
#accept_501_reply=yes

#################################################################
#This profile implements a pure B2BUA application that does an
#identity change and authenticates on the second leg of the call,
#like this
#
#Caller            SEMS auth_b2b                123@domainb
#  |                     |                        |
#  | INVITE bob@domaina  |                        |
#  | From: alice@domaina |                        |
#  | To: bob@domaina     |                        |
#  | P-App-Param:u=user;d=domainb;p=passwd        |
#  |-------------------->|                        |
#  |                     |INVITE bob@domainb      |
#  |                     |From: user@domainb      |
#  |                     |To: bob@domainb         |
#  |                     |----------------------->|
#  |                     |                        |
#  |                     |  407 auth required     |
#  |                     |<---------------------- |
#  |                     |                        |
#  |                     |                        |
#  |                     | INVITE w/ auth         |
#  |                     |----------------------->|
#  |                     |                        |
#  |                     |  100 trying            |
#  |  100 trying         |<---------------------- |
#  |<--------------------|                        |
#  |                     |                        |
#  |                     |  200 OK                |
#  |  200 OK             |<---------------------- |
#  |<--------------------|                        |
#  |                     |                        |
#  | ACK                 |                        |
#  |-------------------->| ACK                    |
#  |                     |----------------------->|
#
#App-Param:
#  u - user in B leg
#  d - domain in B leg
#  p - password for auth in B leg (auth user=user)
