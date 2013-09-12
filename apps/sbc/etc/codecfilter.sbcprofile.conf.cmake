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
#next_hop=192.168.5.106

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

# anonymize SDP or not (u, s, o lines)
#
#sdp_anonymize=yes
