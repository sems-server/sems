# call_timer SBC profile
#
# call_timer is a back-to-back user agent application
# that ends the call after a call timer expired.
# The timer value can be configured either statically,
# or it may be taken from e.g. the P-App-Param header
# (e.g. $P(t) for t= parameter of P-App-Param), or some
# other message part (e.g. R-URI user by using $rU).
# This application is known from the call_timer app.

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

## call timer
call_control=call_timer

# module cc_call_timer
call_timer_module=cc_call_timer
# maximum call time in seconds.
# take the timer value from "t" parameter of P-App-Param,
# e.g. P-App-Param: t=120
call_timer_timer=$P(t)

#
# Kamailio/sip-router script: 
#  remove_hf("P-App-Param");
#  append_hf("P-App-Param: t=120\r\n");
#  t_relay_to_udp("10.0.0.3","5070");
#
#For a static value, set it like this
#call_timer=120