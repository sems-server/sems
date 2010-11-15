SBC module

Copyright (C) 2010 Stefan Sayer

Overview
--------
The SBC application is a highly flexible high-performance Back-to-Back
User Agent (B2BUA). It can be employed for a variety of uses, for example 
topology hiding, From/To modification, enforcing SIP Session Timers, 
identity change, SIP authentication. Future uses include accounting,
call timers, RTP call bridging, transcoding, call distribution.

Features
--------
 o B2BUA
 o flexible call profile based configuration
 o From, To, RURI update
 o Header and message filter
 o reply code translation
 o SIP authentication
 o SIP Session Timers
 o call timer
 o prepaid accounting

SBC Profiles
------------
All features are set in an SBC profile, which is configured in a separate
configuration file with the extension .sbcprofile.conf. Several SBC profiles
may be loaded at startup (load_profiles), and can be selected with the 
active_profile configuration option

 o statically (active_profile=<profile_name>)

 o depending on user part of INVITE Request URI(active_profile=$(ruri.user))

 o depending on "profile" option in P-App-Param header (active_profile=$(paramhdr))

By using the latter two options, the SBC profile for the call can be selected in the
proxy.

RURI, From, To - Replacement patterns
-------------------------------------
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


  \\  -> \
  \$  -> $
  \*  -> *

If a quotation mark (") is used, it needs to be escaped with a backslash in
the sbc profile configuration file.
 Example: 
   From="\"Anonymous\" <sip:anonymous@invalid>"

If a space is contained, use quotation at the beginning and end.
 Example:
   To="\"someone\" <$aU@mytodomain.com>"

Outbound proxy and next hop
---------------------------

An outbound proxy may be set with the outbound_proxy option. If this is
not set, the outbound_proxy option of sems.conf is used, if that one is set.
Setting an outbound proxy will add a route header. force_outbound_proxy forces
the outbound proxy route also for in-dialog requests.

The next hop (destination IP[:port] of outgoing requests) can be set with
the next_hop_ip and next_hop_port options. next_hop_port defaults to 5060
if not set or empty.

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

Reply code translations
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

Session Timer configuration
---------------------------
If SIP Session Timers are enabled for a profile, the session timers values
(session_refresh, minimum_timer etc) can be configured either in sbc.conf
or in the profile configuration. The profile SST configuration is used if
session_expires is set in the profile configuration file. 

Note that for performance reasons the whole SST configuration is in this
case used from the profile configuration (it is not overwritten value-by-value).

Prepaid
-------
Prepaid accounting can be enabled with the enable_prepaid option. The credit
of an account is fetched when the initial INVITE is processed,
and a timer is set once the call is connected. When the call ends, the credit
is subtracted from the user.

For accounting, a separate module is used. This allows to plug several types
of accounting modules. The accounting module is selected with the 
prepaid_accmodule option.

The account which is billed is taken from the prepaid_uuid option. The billing
destination is set with the prepaid_acc_dest option. 

 Example:
    enable_prepaid=yes
    prepaid_accmodule=cc_acc
    prepaid_uuid=$H(P-Caller-Uuid)
    prepaid_acc_dest=$H(P-Acc-Dest)

  Here the account UUID is taken from the P-Caller-Uuid header, and the
  accounting destination from the P-Acc-Dest header.

Credit amounts are expected to be calculated in seconds. The timestamps
are presented in unix timestamp value (seconds since epoch). start_ts is
the initial INVITE timestamp, connect_ts the connect (200 OK) timestamp,
end_ts the BYE timestamp.

Accounting interface:
  getCredit(string uuid, string acc_dest, int start_ts, string call_id, string ltag)
      result: int credit

  connectCall(string uuid, string acc_dest, int start_ts, int connect_ts,
              string call_id, string ltag, string b_ltag)

  subtractCredit(string uuid, int call_duration, string acc_dest, int start_ts,
                 int connect_ts, int end_ts, string call_id, string ltag, string b_ltag)

The cc_acc and cc_acc_xmlrpc modules may be used for accounting modules, or as starting
points for integration into custom billing systems.

Parallel call limits can be implemented by implementing an account specific limit to the
accounting module.


Example profiles
----------------
 transparent   - completely transparent B2BUA
 auth_b2b      - identity change and SIP authentication (obsoletes auth_b2b app)
 sst_b2b       - B2BUA with SIP Session Timers (obsoletes sst_b2b app)
 call_timer    - call timer (obsoletes call_timer app)
 prepaid       - prepaid accounting (obsoletes sw_prepaid_sip app)
 codecfilter   - let only some low bitrate codecs pass

Dependencies
------------
For SIP authentication: uac_auth module
For SIP Session Timers and call timers: session_timer module

Roadmap
-------
x header filter (whitelist or blacklist)
x message filter (whitelist or blacklist)
- SDP filter (reconstructed SDP)
x remote URI update (host / user / host/user)
x From update (displayname / host / host/user)
x To update (displayname / host / host/user)
x SIP authentication
x session timers
x maximum call duration timer
- accounting (MySQL DB, cassandra DB)
- RTP forwarding mode (bridging)
- RTP transcoding mode (bridging)
- overload handling (parallel call to target thresholds)
- call distribution
- select profile on monitoring in-mem DB record
- fallback profile
- add headers
- bridging between interfaces
