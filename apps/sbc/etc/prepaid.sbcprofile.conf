# prepaid SBC profile
#
# prepaid is a back-to-back user agent application
# that checks for the user's balance on connecting,
# and ends the call when the balance has run out.
# This application is known from the sw_prepaid_sip app.
# See the explanation below.

RURI=$H(P-Proxy);cc_prepaid
#From=$f
To=<$H(P-R-Uri)>

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

## prepaid
call_control=prepaid
prepaid_module=cc_prepaid
prepaid_uuid=$H(P-Caller-Uuid)
prepaid_acc_dest=$H(P-Acc-Dest)

## call timer
#call_control=prepaid;timer
# maximum call time in seconds.
# take the timer value from "t" parameter of P-App-Param,
# e.g. P-App-Param: t=120
#timer_module=cc_call_timer
#timer_timer=$P(t)
#
# Kamailio/sip-router script: 
#  remove_hf("P-App-Param");
#  append_hf("P-App-Param: t=120\r\n");
#  t_relay_to_udp("10.0.0.3","5070");
#
#For a static value, set it like this
#timer_timeout=120

#######################################################################
#######################################################################
#
# sw_prepaid_sip, the signalling-only prepaid engine
# Copyright (C) 2007 Sipwise GmbH
# Based on mycc, Copyright (C) 2002-2003 Fhg Fokus
#
# Author: Andreas Granig <agranig@sipwise.com>
#
########################################################################
#
# How sw_prepaid_sip works
# ------------------------
# The plugin extracts the headers "P-Caller-Uuid" and "P-Acc-Dest" as
# caller and callee identifiers, respectively. Using these values,
# it calls an accounting backend, which can defined in sw_prepaid_sip.conf,
# and fetches the maximum call duration (per default, cc_acc is used as
# billing backend, which is just a dummy plugin to show the most basic
# concept of a billing backend).
# 
# If the credit is > 0, it uses the Request-Uri passed in the header "P-R-Uri"
# and connects the callee using the outbound proxy passed in the header "P-Proxy".
# 
# When the call duration exceeds the credit, the call is terminated by SEMS.
# 
# 
# A typical call flow would look like this, where (a) is the caller leg and
# (b) the callee leg:
# 
# Caller    Proxy       SEMS      Callee
#   |          |          |          |
#   |  INV(a)  |          |          |
#   |--------->|          |          |
#   |  407(a)  |          |          |
#   |<---------|          |          |
#   |  INV(a)  |          |          |
#   |--------->|          |          |
#   |  100(a)  |          |          |
#   |<---------|          |          |
#   |          |  INV(a)  |          |
#   |          |--------->|          |
#   |          |  101(a)  |          |
#   |          |<---------|          |
#   |          |  INV(b)  |          |
#   |          |<---------|          |
#   |          |  INV(b)  |          |
#   |          |-------------------->|
#   |          |          |  180(b)  |
#   |          |<--------------------|
#   |          |  180(b)  |          |
#   |          |--------->|          |
#   |          |  180(a)  |          |
#   |          |<---------|          |
#   |  180(a)  |          |          |
#   |<---------|          |          |
#   |          |          |  200(b)  |
#   |          |<--------------------|
#   |          |  200(b)  |          |
#   |          |--------->|          |
#   |          |  200(a)  |          |
#   |          |<---------|          |
#   |  200(a)  |          |          |
#   |<---------|          |          |
#   |  ACK(a)  |          |          |
#   |--------->|          |          |
#   |          |  ACK(a)  |          |
#   |          |--------->|          |
#   |          |  ACK(b)  |          |
#   |          |<---------|          |
#   |          |  ACK(b)  |          |
#   |          |-------------------->|
#   |          |          |          |
#   |          |          |          |
# 
# 
# How to configure SEMS' sip proxy
# --------------------------------
# 
# Using SIP router, just add a proper block to pass prepaid calls to SEMS. 
# 
# In this example, we assume that calls prefixed by "pre" are considered prepaid 
# requests:
# 
# if(uri =~ "sip:pre.+@")
# {
# 	strip(3);
# 	# assume that SEMS is running at localhost:5080
# 	t_relay_to("udp:localhost:5080");
# 	break;
# }
# 
# If you need to pass other headers from leg (a) to leg (b), you can add them to "tw_append" 
# and they are copied to the other leg. This is for example necessary to pass the destination
# URI to leg (b), which can then be extracted from the proxy and set accordingly.
# 
# How to configure a proxy to interact with sw_prepaid_sip
# --------------------------------------------------------
# 
# If your proxy runs on IP 192.168.100.10:5060 and your SEMS' proxy on 192.168.100.10:5070, you
# can pass leg (a) to the prepaid engine like this (assuming you use OpenSER >= 1.2.0 as 
# sip proxy):
# 
# if(/* caller is prepaid */)
# {
# 	/* use authentication user as caller uuid in prepaid engine */
# 	append_hf("P-Caller-Uuid: $au\r\n");
# 
# 	/* use simplified R-URI as destination pattern in prepaid engine;
# 	   NOTE that cc_acc doesn't evaluate this field, but more 
# 	   advanced backends might do, so sw_prepaid_sip requires
# 	   this header */
# 	append_hf("P-Acc-Dest: $rU@$rd\r\n");
# 
# 	/* the URI which is to be used by SEMS as outbound proxy for
# 	   call leg (b) - use our own here */
# 	append_hf("P-Proxy: sip:192.168.100.10:5060\r\n");
# 
# 	/* the R-URI to be used to connect call leg (b) */
# 	append_hf("P-R-Uri: $ru\r\n");
# 
# 	if($du != null)
# 	{
# 		/* if there's a D-Uri set, also send it to SEMS to correctly contact
# 		   call leg (b). NOTE you have to add this header in SEMS's sip proxy
# 		   config as "tw_append" value. */
# 		append_hf("P-D-Uri: $du\r\n");
# 	}
# 
# 	/* point the R-Uri to SEMS' proxy and relay; this is just the most simple
# 	   example, but you can also use the LCR module for SEMS load balancing
# 	   and failover. */
# 	rewriteuri("sip:192.168.100.10:5070");
# 	t_relay();
# }
# 
# In the above block, call leg (a) is passed to SEMS. Now we prepare a block to catch 
# call leg (b) coming from SEMS. Add this at the very beginning of your proxy configuration.
# 
# /* usual sanity checks like msg-size and max-fwd, then do something like: */
# 
# if(src_ip == 192.168.100.10 && src_port == 5070 /* check if from SEMS */
# 	&& uri =~ ";sw_prepaid" /* this uri param is added by SEMS to detect prepaid calls */
# 	&& method == "INVITE" && !has_totag()) /* only process initial invites here */
# {
# 	/* recover the R-Uri (maybe check for existence first!) */
# 	$ru = $hdr(P-R-Uri);
# 
# 	/* recover destination Uri */
# 	if(is_present_hf("P-D-Uri"))
# 	{
# 		$du = $hdr(P-D-Uri);
# 		insert_hf("Route: <$hdr(P-D-Uri)>\r\n");
# 	}
# 
# 	/* you might also consider removing the helper headers
# 	   by calling remove_hf(...) */
# 
# 	/* relay to callee */
# 	t_relay();
# } 
# 
# Please note that the above example is a very basic one. For example, if you want to route
# to PSTN gateways, you might want to add an informational header in leg (a) and evaluate
# it in leg (b) to load a set of PSTN gateways instead of relaying to the R-Uri.
# 
# IMPORTANT: make sure to prevent black-hats from spoofing the SEMS proxy address to place
# unauthenticated calls by using a private address for SEMS' sip proxy and by adding 
# proper firewall rules on your ingress router.
