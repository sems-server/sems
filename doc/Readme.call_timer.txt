call_timer application
----------------------

-------------------------------------------------
This application has been obsoleted by the sbc
module and will be discontinued in the next version.
Please use the sbc module with the call_timer call
profile for the same functionality.
-------------------------------------------------

call_timer is a simple back-to-back user agent application 
that ends the call after a call timer expired. 

If use_app_param is configured to "yes", then the call timer value 
is taken from App-Param "t" or "Timer" value. If it is set to "no" 
or that is not present, the configured default value is used.

The default value is configured with default_call_time config option. 
If that is not present, 1200 seconds are used.

Examples (ser script): 
 remove_hf("P-App-Param");
 append_hf("P-App-Param: t=120\r\n");
 t_relay_to_udp("10.0.0.3","5070");

 remove_hf("P-App-Param");
 append_hf("P-App-Param: Timer=120\r\n");
 t_relay_to_udp("10.0.0.3","5070");


