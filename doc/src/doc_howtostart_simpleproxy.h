/*! \page howtostart_simpleproxy How to set up a simple proxy for trying out and using SEMS

  \section Introduction
  
   <p>
   This text describes how one can set up a simple SIP proxy in order to 
   try out services in SEMS.
   </p>
   <p>
    We will use the Kamailio 3.0 default proxy installation, and add a route SERVICES
    which adds the application name, and forwards the call to SEMS. The same configuration 
    can be used with the original SER (iptel.org/ser), sip-router (sip-router.org) or other
    SER derivatives, like OpenSIPS (opensips.org).
   </p>
   
   \section Installing_Kamailio Installing Kamailio
   
   To install Kamailio 3.0, there is excellent documentation on the <a href = "http://www.kamailio.org/dokuwiki/doku.php#setup">
   Kamailio website</a>. In debain lenny, or for example in Ubuntu 9.10, one can install Kamailio with
   \code
    $ wget http://www.kamailio.org/pub/kamailio/latest/packages/debian-lenny/kamailio_3.0.1_i386.deb
    $ dpkg -i kamailio_3.0.1_i386.deb
   \endcode
   
   To activate kamailio, one needs to set <b>RUN_KAMAILIO=yes</b> in <b>/etc/default/kamailio</b>.
   
   \section adding_service_route Adding a service route
   
   Kamailio processes all requests according to the logic that is set in the route section of its configuration file, which is 
   a very flexible one. In order to have services executed when some special numebrs are called (e.g. 200 and 300), we add another 
   route to <b>/etc/kamailio/kamailio.cfg</b>:
   \code   
   route[SERVICES] {
	if ($rU=~"^200.*") {
		remove_hf("P-App-Name");
		append_hf("P-App-Name: echo\r\n"); 
		$ru = "sip:" + $rU + "@" + "127.0.0.1:5070";
		route(RELAY);
		exit;
        }
	if ($rU=~"^300.*") {
		remove_hf("P-App-Name");
		append_hf("P-App-Name: conference\r\n"); 
		$ru = "sip:" + $rU + "@" + "127.0.0.1:5070";
		route(RELAY);
		exit;
        }
   }
   \endcode   
   
   This route block can be added anywhere, for example at the end, or between the PSTN and the SERVICES routes.
   
   Then, in the main route section, which is the one marked with the comment <em># main request routing logic</em>,
   we call our SERVICES-route, preferably before (or after) the PSTN route:
   \code
   ...
	if ($rU==$null) {
		# request with no Username in RURI
		sl_send_reply("484","Address Incomplete");
		exit;
	}

	route(SERVICES);

	route(PSTN);

	# apply DB based aliases (uncomment to enable)
	##alias_db_lookup("dbaliases");

	if (!lookup("location")) {
   ...
    \endcode
    
    Now, if we register a phone to the server, and call the 200 or the 300 number, the INVITE gets sent to 127.0.0.1:5070, with 
    the application that is to be called, added as header to the INVITE.
    
   \section setting_up_sems Setting up SEMS to select the application
   
   If we load several applications in SEMS, we can select which application to execute by the P-App-Name header. In <b>sems.conf</b> 
   we set application=$(apphdr) so that SEMS looks into the P-App-Name header to determine which application to run:
   \code
   application=$(apphdr)
   load_plugin=wav;gsm;ilbc;speex;session_timer;conference;echo
   sip_ip=127.0.0.1
   sip_port=5070
   media_ip=some.public.ip.here
   \endcode
  
   \note in this simple case, we could also have set application=$(mapping) and used regular expression mapping in app_mapping.conf
    
*/