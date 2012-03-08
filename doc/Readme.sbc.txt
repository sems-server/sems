SBC module

Copyright (C) 2010-2011 Stefan Sayer

Overview
--------
The SBC application is a highly flexible high-performance Back-to-Back
User Agent (B2BUA). It can be employed for a variety of uses, for example 
topology hiding, From/To modification, enforcing SIP Session Timers, 
identity change, SIP authentication, RTP relaying. Future uses include
accounting, transcoding, call distribution.

Features
--------
 o B2BUA
 o flexible call profile based configuration
 o online reload of call profiles
 o From, To, RURI, Contact, Call-ID update
 o RTP bridging
 o Header and message filter
 o adding arbitrary headers
 o reply code translation
 o SIP authentication
 o SIP Session Timers
 o call timer
 o prepaid accounting
 o CDR generation
 o call teardown from external control through RPC

SBC Profiles
------------
All features are set in an SBC profile, which is configured in a separate
configuration file with the extension .sbcprofile.conf. Several SBC profiles
may be loaded at startup (load_profiles), and can be selected with the 
active_profile configuration option. The active_profile option is a comma-separated
list, the first profile that matches, i.e. is non-empty, will be used.

In this list a profile may be selected

 o statically (active_profile=<profile_name>)

 o depending on user part of INVITE Request URI (active_profile=$(ruri.user))

 o depending on "profile" option in P-App-Param header (active_profile=$(paramhdr))

 o using any replacement pattern (see below), especially regex maps $M(val=>map)

By using the latter options, the SBC profile for the call can also be selected in
the proxy.

Examples:
 active_profile=auth_b2b
 active_profile=$(paramhdr),refuse
 active_profile=$M($si=>ipmap),$(P-SBCProfile),refuse

Example: 
  In order to have all calls coming from source IP 10.0.* going to
  'internal1' profile, all calls coming from source IP 10.1.* going to 'internal2'
  profile, then for calls coming from other IP addresses those to RURI-domain
  iptel.org go to 'iptel' profile, and all other calls being refused, we could set
  ~~~~~~~~~ sbc.conf ~~~~~~~~~
  profiles=internal1,internal2,iptel,refuse
  regex_maps=src_ipmap,rurimap
  active_profile=$M($si=>src_ipmap),$M($rh=>rurimap),refuse
  ~~~~~~~~~~~~~~~~~~ ~~~~~~~~~

  ~~~~~~~~~ src_ipmap.conf ~~~
  ^10\.0\..*=>internal1
  ^10\.1\..*=>internal2
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  ~~~~~~~~~ rurimap.conf ~~~~~
  iptel.org=>iptel
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~

SBC profile reload
------------------
The SBC profiles may be reloaded while the server is running. A set of (python) scripts
is provided and installed to trigger the reload (through XMLRPC):

  sems-sbc-list-profiles                        list loaded profiles
  sems-sbc-reload-profile  <name>               reload a profile (from its .conf file)
  sems-sbc-reload-profiles                      reload all profiles (from .conf files)
  sems-sbc-load-profile <name> <conf_file>      load a profile from a file (e.g. new
                                                profile or file path changed)
  sems-sbc-get-activeprofile                    get active_profile
  sems-sbc-set-activeprofile <active_profile>   set active_profile

  sems-sbc-teardown-call <call_ltag>            tear down call (use e.g. monitoring's
                                                sems-list-active-calls to get the ltag)

The xmlrpc2di module must be loaded and the XMLRPC control server bound to port 8090 for
the scripts to work.

For tracking file revisions and changes, the MD5 hash sum is printed on profile load and
reload, and returned as information by the scripts and the DI management commands. An MD5
hash to compare checksums of profile files can also be generated with the md5sum(1) tool.

Alternatively, the reload functions can be accessed by json-rpc v2 if the jsonrpc module
is loaded. The expected parameters to all functions are in a dictionary with 
   'name' :          profile name
   'path' :          profile conf file path
   'active_profile': active profile (string)
Return code is [200, "OK", <result dictionary>] on success, or 
[<error code>, <error reason>] on failure.

Replacement patterns - active_profile, RURI, From, To, Contact, etc
-------------------------------------------------------------------
In SBC profile the appearance of the outgoing INVITE request can be set,
by setting RURI, From and To parameters. If any of those parameters is not
set, the corresponding value of the incoming request is used.

The values that are set can contain patterns, which are set to values taken
from the incoming INVITE request. The syntax loosely follows sip-router's
pseudo variables. Any of the RURI, From and To values can contain any elements,
e.g. the request-URI can be set to the user part of the P-Asserted-Identity
header combined with the host part of the To.

The patterns which can be used are the following:

  $r (or $r. if something follows) - R-URI
  $f (or $f. if something follows) - From
  $t (or $t. if something follows) - To
  $a (or $a. if something follows) - P-Asserted-Identity
  $p (or $p. if something follows) - P-Preferref-Identity

  $fu  - From URI
  $fU  - From User
  $fd  - From domain (host:port)
  $fh  - From host
  $fp  - From port
  $fH  - From headers
  $fP  - From Params
  $ft  - From tag
  $fn  - From display name

  $tu  - To URI
  $fU  - To User
  ...

  $ru  - R-URI URI
  $rU  - R-URI User
  ...

  $ai  - P-Asserted-Identity URI (alias to $au)
  $au  - P-Asserted-Identity URI
  $aU  - P-Asserted-Identity User
  ...

  $pi  - P-Preferred-Identity URI (alias to $pu)
  $pu  - P-Preferred-Identity URI
  $pU  - P-Preferred-Identity User
  ...


  $ci  - Call-ID

  $si  - source (remote) IP address
  $sp  - source (remote) port

  $Ri  - destination (local/received) IP address
  $Rp  - destination (local/received) port
  $Rf  - local/received interface id (0=default)
  $Rn  - local/received interface name ('default', 'intern', ... as set in sems.conf)
  $RI  - local/received interface public IP (as set in sems.conf)

  $P(paramname) - paramname from P-App-Param
    Example:
      P-App-Param: u=myuser;p=mypwd;d=mydomain
        and
      auth_user=$P(u)
      auth_pwd=$P(p)
      From=sip:$P(u)@$P(d)

  $H(headername) - value of header <headername>
   Examples:
    o P-Caller-Uuid: 0004152379B8
       and
      prepaid_caller_uuid=$H(P-Caller-Uuid)

    o P-NextHop-IP: 10.0.2.15
       and
      next_hop_ip=$H(P-NextHop-IP)

  $HU(headername) - header <headername> (as URI) User
  $Hd(headername) - header <headername> (as URI) domain (host:port)
  ...

   Example:
    o P-SomeNH-URI: sip:user@10.0.2.15:5092
       and
      next_hop_ip=$Hh(P-SomeNH-URI)
      next_hop_port=$Hp(P-SomeNH-URI)


  $M(value=>regexmap) - map a value (any pattern) to a regexmap (see below)
    Example: $M($fU=>usermap)

  $_*(value) - string modifiers: 
   $_u(value)   - value to uppercase (e.g.: $_u($fh) From host in uppercase)
   $_l(value)   - value to lowercase (e.g.: $_l($fh) From host in lowercase)
   $_s(value)   - length of value (e.g.: $_s($fU) string length of From user)
   $_5(value)   - MD5 of value

  \\  -> \
  \$  -> $
  \*  -> *
  \r  -> cr  (e.g. use \r\n to separate different headers in append_headers)
  \n  -> lf
  \t  -> tab

If a quotation mark (") is used, it needs to be escaped with a backslash in
the sbc profile configuration file.
 Example: 
   From="\"Anonymous\" <sip:anonymous@invalid>"

If a space is contained, use quotation at the beginning and end.
 Example:
   To="\"someone\" <$aU@mytodomain.com>"

Regex mappings ($M(key=>map))
-----------------------------

A regex mapping is a (sorted) list of "regular expression" => "string value" pairs.
The regex mapping is executed with a key - any string, replacement pattern or
combination - and the first regular expression that matches returns the "string value".

Regex mappings are read from a text file, where each line corresponds to one
regex=>value pair. The mappings to load on startup are set with the regex_maps
config option, the file name from where it is loaded is "<mapping name>.conf" in
the plugin config path.

Mappings can also loaded into the running server by using the setRegexMap DI function
or the included sems-sbc-*-regex-* scripts:

  sems-sbc-set-regex-map <name> <file>      load a regex map from a file
  sems-sbc-get-regex-map-names              list regex map names

 Example regex map:
   ~~~~~~~ usermap.conf ~~~~~~
   # this is a comment
   ^stefan=>stefansayer
   ^frank=>frankmajer
   ~~~~~~~~~~~~~~~~~~~~~~~~~~~

Setting Call-ID
---------------
For debugging purposes, the call-id of the outgoing leg can be set to depend on
the call-id the first leg, by setting the Call-ID parameter.

Example:
  Call-ID=$ci_leg2

  If the incoming call leg had "Call-ID: 3c2d4b9a6b6f-hb22s7k9n0iv", the outgoing
  leg will have "Call-ID: 3c2d4b9a6b6f-hb22s7k9n0iv_leg2".

If Call-ID is not set, a standard unique ID is generated by SEMS, of the form
UUID@host-ip.

Outbound proxy and next hop
---------------------------

An outbound proxy may be set with the outbound_proxy option. If this is
not set, the outbound_proxy option of sems.conf is used, if that one is set.
Setting an outbound proxy will add a route header.

force_outbound_proxy forces the outbound proxy as first route and request URI
also for in-dialog requests. Note that this is NOT RFC3261 compliant (section
2.2 Requests within a Dialog, 12.2.1 UAC Behavior).

The next hop (destination IP[:port] of outgoing requests) can be set with
the next_hop_ip and next_hop_port options. next_hop_port defaults to 5060
if not set or empty. Usually, replies are sent back to where the request came
from (honoring rport), but if next_hop should be used nevertheless,
next_hop_for_replies profile option can be set to "yes".

These two settings apply only for the UAC side, i.e. the outgoing side of
the initial INVITE.

Filters
-------
Headers and messages may be filtered. A filter can be set to 
 o transparent - no filtering done

 o whitelist - only let items pass that are in the filter list
 o blacklist - filter out items that are in the filter list

Note that ACK messages should not be filtered.

Codec filter
------------
The SDP body of INVITE/200, UPDATE/200 and ACK may be filtered for codecs
with the sdp_filter and sdpfilter_list call profile options. If sdp_filter is 
set to transparent, the SDP is parsed and reconstructed (SDP sanity check).
Codecs may be filtered out by their payload names in whitelist or blacklist
modes. The payload names in the list are case-insensitive (PCMU==pcmu).

The s, u and o-lines of the SDP can be anonymized with the setting sdp_anonymize=yes.

RTP relay
---------
RTP can be bridged through the SBC. Where without rtprelay, A call would go only
with the signaling through the SBC, in rtprelay mode, the connection address in
SDP messages will be replaced to the one of SEMS, such that caller and callee
send RTP media to SEMS. SEMS then relays the RTP packets between the two sides.

RTP relay can be enabled by setting
  enable_rtprelay=yes

The SBC detects if UAs indicate that they are behind NAT by setting a=direction:active
in SDP, and goes into passive mode until it receives the first packet from the NATed
client, from which it learns the remote address. This mechanism is called "symmetric
RTP".

Symmetric RTP (starting in passive mode) can also be forced by setting the
 rtprelay_force_symmetric_rtp=yes
sbc profile option. Symmetric RTP is enabled if rtprelay_force_symmetric_rtp
evaluates to anything other than "" (empty string), "0" or "no".

Some ser/sip-router/kamailio/*ser configurations add flag 2 in a header P-MsgFlags
header to the INVITE to indicate forcing of symmetric RTP. With the sbc profile
option
 rtprelay_msgflags_symmetric_rtp=yes
the SBC honors this and sets symmetric RTP accordingly.

Adding headers
--------------
Additional headers can be added to the outgoing initial INVITE by using the
append_headers call profile option. Here, several headers can be separated with
\r\n. All replacement patterns from above can be used.

Examples:
 append_headers="P-Received-IP: $Ri\r\nP-Received-Port: $Rp"
 append_headers="P-Source-IP: $si\r\nP-Source-Port: $sp\r\n"
 append_headers="P-Original-URI: $r"

Response code translations
-----------------------
Response codes and reasons may be translated, e.g. if some 6xx class replies need
to be changed to 4xx class replies.

Example:
 reply_translations="603=>488 Not acceptable here"

Here, all 603 replies received on one leg will be sent out as 488 reply with
the reason string "Not acceptable here".

Entries are separated in the reply_translations list with a pipe symbol (|).

Example:
 reply_translations="603=>488 Not acceptable here|600=>406 Not acceptable"

Warning: Changing response codes, especially between different response
         code classes, can seriously mess up everything. Use with caution
         and only if you know what you are doing!

Reliable 1xx (PRACK)
--------------------

Reliable 1xx (PRACK) extension (3262) is supported in a transparent mode,
i.e. the RSeq header is relayed and RAck CSeq is translated properly.

SIP authentication
------------------
The SBC can perform SIP digest authentication. To use SIP authentication, the
uac_auth module needs to be loaded.

SIP authentication is enabled by the following parameters, separately for both
call legs:

# Authentication for B leg (second/callee leg):
   enable_auth       "yes" or "no"
   auth_user         authentication user
   auth_pwd          authentication password
# Authentication for A leg (first/caller leg):
   enable_aleg_auth  "yes" or "no"
   auth_aleg_user    authentication user
   auth_aleg_pwd     authentication password


Note: The 'A' leg is always the first leg, the one from the caller. 'B' leg is
the one to callee:
 caller <--- A (first) leg ---> SEMS <--- B (second) leg ---> callee

Example:
  enable_auth=yes
  auth_user=$H(P-Auth-B-User)
  auth_pwd=$H(P-Auth-B-Pwd)
  enable_aleg_auth=yes
  auth_aleg_user=$H(P-Auth-A-User)
  auth_aleg_pwd=$H(P-Auth-A-Pwd)


SIP Session Timer configuration
-------------------------------
If SIP Session Timers are enabled for a profile, the session timers values
(session_refresh, minimum_timer etc) can be configured either in sbc.conf
or in the profile configuration, which overrides the sbc.conf configuration.

SIP Session Timers may be configured for each leg individually.
enable_session_timer overrides enable_aleg_session_timer if that one is not set:
SST may be disabled on the A (caller) leg by setting enable_aleg_session_timer=no.
If enable_session_timer=yes and enable_aleg_session_timer not set, SST is enabled for
both legs. Likewise, if aleg_session_expires etc. is not set, the SST configuration of
the B leg is used (session_expires, minimum_timer etc).

Call control modules
--------------------
Call control (CC) modules for the sbc application implement business logic which controls
how the SBC operates. For example, a CC module can implement concurrent call limits, call
limits per user, enforce other policies, or implement routing logic.

Call control (CC) modules should be loaded using the load_cc_plugins option in sbc.conf,
or loaded later into the server by the sems-sbc-loadcallcontrol-modules script
(loadCallcontrolModules DI function).

Multiple CC modules may be applied for one call. The data that the CC modules get from the
call may be freely configured. Call control modules may also be applied through message parts
(replacement patterns).

Example: 
  Limiting From-User to 5 parallel calls, and 90 seconds maximum call duration:
    call_control=pcalls,call_timer
    pcalls_module=cc_pcalls
    pcalls_uuid=$fU
    pcalls_max_calls=5
    call_timer_module=cc_call_timer
    call_timer_timer=90

Example:
  Applying 90 seconds maximum call duration and other call control from a header:
    call_control=call_timer,$H(P-CallControl)
    call_timer_module=cc_call_timer
    call_timer_timer=90

   SIP message:
    INVITE sip:foo@bar.net SIP/2.0
    From: sip:a@example.com;tag=1234
    To: b@example.com
    P-CallControl: cc_pcalls;uuid=$rU, cc_pcalls;uuid=a_user
    ...

See also Readme.sbc_call_control.txt.

Call control: Prepaid
---------------------
Prepaid accounting can be enabled with using a prepaid call control module.
The credit of an account is fetched when the initial INVITE is processed,
and a timer is set once the call is connected. When the call ends, the credit
is subtracted from the user.

For accounting, a separate module is used. This allows to plug several types
of accounting modules.

 Example:
    call_control=prepaid
    prepaid_module=cc_prepaid
    prepaid_uuid=$H(P-Caller-Uuid)
    prepaid_acc_dest=$H(P-Acc-Dest)

  Here the account UUID is taken from the P-Caller-Uuid header, and the
  accounting destination from the P-Acc-Dest header.

Credit amounts are expected to be calculated in seconds. The timestamps
are presented in unix timestamp value (seconds since epoch). start_ts is
the initial INVITE timestamp, connect_ts the connect (200 OK) timestamp,
end_ts the BYE timestamp.

The cc_prepaid and cc_prepaid_xmlrpc modules may be used for accounting modules, or
as starting points for integration into custom billing systems.

Call control: Parallel calls limit
----------------------------------
Parallel call limits can be enforced by using the parallel calls call control module.

 Example (limit From-User to max 5 calls):
    call_control=pcalls
    pcalls_module=cc_pcalls
    pcalls_uuid=$fU
    pcalls_max_calls=5

Call control: Call Timer
------------------------
A maximum call duration timer can be set with the call timer call control module.

 Example (timer taken from P-Timer header):
    call_control=call_timer
    call_timer_module=cc_call_timer
    call_timer_timer=$H(P-Timer)

 Example (maximum 90 seconds):
    call_control=call_timer
    call_timer_module=cc_call_timer
    call_timer_timer=90

CDR generation
--------------
CDR generation can be enabled with loading a CDR call control module.

The cc_syslog_cdr module writes CDRs to syslog(3) to be processed
by standard syslog utils, e.g. syslog-ng.

 Example:
  call_control=cdr
  cdr_module=cc_syslog_cdr
  cdr_Calling-Station-Id=$fU
  cdr_Called-Station-Id=$tU
  cdr_Sip-From-Tag=$ft

See also cc_syslog_cdr module documentation.

Refusing calls
--------------

In some configurations, if may be necessary to refuse calls with a certain error response
code and reason. If the refuse_with call profile option is set, the call is refused with
the code and reason specified. In this case, all other call profile options are ignored,
only the append_headers option has effect.

Examples:
 refuse_with="403 Invalid Domain $rd"

 refuse_with="606 Not Acceptable"
 append_headers="P-Original-URI: $r\r\nP-Original-To: $t"

Example profiles
----------------
 transparent   - completely transparent B2BUA (contains all options in comments)
 auth_b2b      - identity change and SIP authentication (obsoletes auth_b2b app)
 sst_b2b       - B2BUA with SIP Session Timers (obsoletes sst_b2b app)
 call_timer    - call timer (obsoletes call_timer app)
 prepaid       - prepaid accounting (obsoletes sw_prepaid_sip app)
 codecfilter   - let only some low bitrate codecs pass
 replytranslate - swap 603 and 488 response code in replies
 refuse        - refuse all calls with 403 Forbidden
 symmetricrtp  - RTP relay with symmetric RTP for NAT handling

Dependencies
------------
For SIP authentication: uac_auth module
For SIP Session Timers and call timers: session_timer module

Roadmap
-------
x header filter (whitelist or blacklist)
x message filter (whitelist or blacklist)
x SDP filter (reconstructed SDP)
x remote URI update (host / user / host/user)
x From update (displayname / host / host/user)
x To update (displayname / host / host/user)
x SIP authentication
x session timers
x maximum call duration timer
- accounting (MySQL DB, cassandra DB)
x RTP forwarding mode (bridging)
- RTP transcoding mode (bridging)
- overload handling (parallel call to target thresholds)
- call distribution
- select profile on monitoring in-mem DB record
x fallback profile
x add headers
x bridging between interfaces
- rel1xx in non-transparent mode
