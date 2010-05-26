Announcement then call transfer 

This application plays an announcement and then sends a REFER to
the caller. The Refer-To header field is set to either the uri 
configured in the Refer-To session parameter, or, if it is not set, 
to the request URI of the first INVITE.

Once the REFER request has been replied, a BYE is sent to the caller.

Example ser configuration file for passing parameters to announce_transfer:

	append_hf("P-App-Name: announce_transfer\r\n");
	append_hf("P-App-Param: Refer-To=sip:callme@example.com\r\n");
	# assume that SEMS is running on localhost at port 5080 (default)
       	t_relay_to("udp:localhost:5080");

