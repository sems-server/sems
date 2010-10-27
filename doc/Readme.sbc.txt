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
 o SIP authentication
 o SIP Session Timers

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

  $ai  - P-Asserted-Identity URI
  $au  - P-Asserted-Identity URI
  $aU  - P-Asserted-Identity URI
  ...

  $pi  - P-Preferred-Identity URI
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

Filters
-------
Headers and messages may be filtered. A filter can be set to 
 o transparent - no filtering done

 o whitelist - only let items pass that are in the filter list

 o blacklist - filter out items that are in the filter list

Note that if ACK messages should not be filtered.

Session Timer configuration
---------------------------
If SIP Session Timers are enabled for a profile, the session timers values
(session_refresh, minimum_timer etc) can be configured either in sbc.conf
or in the profile configuration. The profile SST configuration is used if
session_expires is set in the profile configuration file. 

Note that for performance reasons the whole SST configuration is in this
case used from the profile configuration (it is not overwritten value-by-value).

Example profiles
----------------
 transparent   - completely transparent B2BUA
 auth_b2b      - identity change and SIP authentication (obsoletes auth_b2b app)
 sst_b2b       - B2BUA with SIP Session Timers (obsoletes sst_b2b app)

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
- maximum call duration timer
- accounting (MySQL DB, cassandra DB)
- RTP forwarding mode (bridging)
- RTP transcoding mode (bridging)
- overload handling (parallel call to target thresholds)
- call distribution
- select profile on monitoring in-mem DB record
- fallback profile
- add headers
- bridging between interfaces
