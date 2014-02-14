SBC call control modules
========================

General
=======

Call control (CC) modules for the sbc application implement business logic which controls
how the SBC operates. For example, a CCmodule can implement concurrent call limits, call
limits per user, enforce other policies, or implement routing logic.

Multiple CC modules may be applied for one call. The data that the CC modules get from the
call may be freely configured.

User documentation 
==================

Call control modules may be applied to a call by specifying call_control=... in the SBC profile.

For each item in call_control, the specific information is then read; for example, if
 call_control=cc_pcalls,cc_timer
is configured, "cc_pcalls" and "cc_timer" sections are read.

Each CC module section must at least specify the CC interface name (usually the module name), e.g.:
 cc_pcalls_module=cc_pcalls

Additionally, the data that is passed to the CC module is specified, this can contain the usual
substitution patterns, e.g.:
 cc_pcalls_uuid=$H(P-UUID)
 cc_pcalls_max_calls=$H(P-Call-Limit)
will pass the contents of the Header named "P-UUID" as value "uuid" and the contents of "P-Call-Limit"
as "max_calls".

Alternatively, call control can also be set through message parts by using replacement patterns.
Example:
   call_control=cc_pcalls,$H(P-Call-Control)

The header 'P-Call-Control' is a comma separated list of call control configurations, with key,value pairs
appended, i.e. of the form module_name;param=val;param=val. 
Example: 
  P-Call-Control: cc_prepaid;uuid=joe, cc_pcalls;uuid=joe;max_calls=10

As usual with multi-value headers, several separate headers may be used, so the above example is
equivalent to:
  P-Call-Control: cc_prepaid;uuid=joe
  P-Call-Control: cc_pcalls;uuid=joe;max_calls=10


Several CC modules are implemented
  o cc_pcalls         - parallel calls limiting
  o cc_call_timer     - call timer (maximum call duration)
  o cc_prepaid        - prepaid billing with storing balances in memory
  o cc_prepaid_xmlrpc - prepaid billing querying balances from external server with XMLRPC
  o cc_ctl            - control SBC profile options through headers
  o cc_rest           - query REST/http API and use response for retargeting etc
  o cc_syslog_cdr     - write CDRs to syslog
  o cc_bl_redis       - check blacklist from REDIS (redis.io)
  o cc_registrar      - local registrar (REGISTER handling, lookup on INVITEs) 
  
See their respective documentation in the doc/sbc/ directory for details.

Developer documentation 
=======================

The functions of CC modules are invoked
 - at the beginning of a call: "start" function
 - when the call is connected: "connect" function
 - when the call is ended: "end" function
 - when an out-of-dialog request should be routed: "route" function

In the start function, CC modules can
 - modify values in the used call profile instance (RURI, From, To etc)
 - set timers
 - drop the call
 - refuse the call with a code and reason

SBC CC API
----------

ampi/SBCCallControlAPI.h should be included, which defines necessary constants.
If the call profile is to be modified, SBCCallProfile.h should be included.

The CC modules must implement a DI API, which must implement at least the functions "start",
"connect", "end".


  function: start
  Parameters: string                  cc_namespace     name of call control section
                                                       (as configured, e.g. variable namespace to use)
              string                  ltag             local tag (ID) of the call (A leg)
              SBCCallProfile Object   call_profile     used call profile instance
              Array of int            timestamps       start/connect/end timestamp
              ValueStruct             values           configured values
                                                       (struct of key:string value)
              int                     timer_id         ID of first timer if set with timer
                                                       action
   
  Return values
              Array of actions:
                  int                 action           action to take
                  ...                 parameters       parameters to action
 
      Actions
               SBC_CC_DROP_ACTION                       drop the call (no reply sent)
                  Parameters: none

               SBC_CC_REFUSE_ACTION                     refuse the call with code and reason
                  Parameters: int    code
                              string reason

               SBC_CC_SET_CALL_TIMER_ACTION             set a timer; the id of the first
                                                        timer will be timer_id
                  Parameters: int    timeout

  function: connect
  Parameters: string                  cc_namespace     name of call control section
                                                       (as configured, e.g. variable namespace to use)
              string                  ltag             local tag (ID) of the call (A leg)
              SBCCallProfile Object   call_profile     used call profile instance
              Array of int            timestamps       start/connect/end timestamp
              string                  other ltag       local tag (ID) of B leg


  function: end
  Parameters: string                  cc_namespace     name of call control section
                                                       (as configured, e.g. variable namespace to use)
              string                  ltag             local tag (ID) of the call (A leg)
              SBCCallProfile Object   call_profile     used call profile instance
              Array of int            timestamps       start/connect/end timestamp


  timestamps Array of int
  -----------------------
       0           CC_API_TS_START_SEC                start TS sec (seconds since epoch)
       1           CC_API_TS_START_USEC               start TS usec
       2           CC_API_TS_CONNECT_SEC              connect TS sec (seconds since epoch)
       3           CC_API_TS_CONNECT_USEC             connect TS usec
       4           CC_API_TS_END_SEC                  end TS sec (seconds since epoch)
       5           CC_API_TS_END_USEC                 end TS usec

 
Storing call related information
--------------------------------

The SBC call profile object contains named general purpose variables of variant type (AmArg) for
general purpose CC module use.

Synopsis:

  setting a variable:
    call_profile->cc_vars["mycc::myvar"] = "myvalue";

  checking for presence of a variable:
    SBCVarMapIteratorT vars_it = call_profile->cc_vars.find("mycc::myvar");
    if (vars_it == call_profile->cc_vars.end() || !isArgCStr(vars_it->second)) {
      ERROR("internal: could not find mycc:myvar for call '%s'\n", ltag.c_str());
      return;
    }
    string myvar = vars_it->second.asCStr();

  removing a variable:
    call_profile->cc_vars.erase("mycc::myvar");

If several invocations of the same call control module should be independent from each other,
CC variables should be prefixed by the CC namespace
(e.g. args[CC_API_PARAMS_CC_NAMESPACE + "::" + var_name).


Verifying correct configuration
-------------------------------
A DI function "getMandatoryValues" (CC_INTERFACE_MAND_VALUES_METHOD) can be implemented.

This function should return a list of strings, which will be checked with the sbc profile. If
there are configuration values missing in the sbc profile, the profile is not loaded.

Sample CC module
----------------

A sample CC module "cc_template" can be found in apps/sbc/call_control/template. For implementing
a new call control module, this code should be taken, the class names changed and the code customized
it.
