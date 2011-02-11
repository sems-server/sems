# codec filter SBC profile
#
# This lets only some low bandwidth codecs pass

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
#next_hop_ip=192.168.5.106
#next_hop_port=5060

## filters: 
#header_filter=blacklist
#header_list=P-App-Param,P-App-Name
#message_filter=transparent
#message_list=

# sdp_filter can be transparent,whitelist or blacklist
# - leave empty (commented) for no touching SDP
# - transparent does SDP reconstruction ('sanity check')
# - whitelist and blacklist filter codec on both sides
sdp_filter=whitelist
sdpfilter_list=g729,g723,ilbc,speex,gsm,amr

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
#enable_session_timer=yes
# if session_expires is not configured here,
# the values from sbc.conf are used, or the
# default values
#session_expires=120
#minimum_timer=90
#session_refresh_method=UPDATE_FALLBACK_INVITE
#accept_501_reply=yes
