SIP Session Timers (SST) enabled B2B application.

-------------------------------------------------
This application has been obsoleted by the sbc
module and will be discontinued in the next version.
Please use the sbc module with the sst_b2b call
profile for the same functionality.
-------------------------------------------------

This application can be routed through for achieving 
two things: 

  1. Forcing SIP Session Timers, which prevents 
     overbilling for calls where BYE is missing,
     for example in cases where media (RTP) path 
     does not go through the system, but billing 
     is still done.

  2. Topology hiding; this application acts as 
     B2BUA, so on the B leg, no routing info from 
     the A leg can be seen.

The incoming INVITE for a newly established call is 
passed in signaling only B2B mode to the B leg, 
which tries to send it to the request URI.

SIP Session Timers are enabled on both legs. The
session refresh method may be configured; UPDATE
or INVITE with last established SDP may be used.

SST expiration is configurable in config file.

Session refresh with last established SDP:

 A                  b2b                  B
 |---INVITE / SDPa-->|                   |
 |                   |---INVITE / SDPa-->|
 |                   |                   |
 |                   |<-- OK/SDPb--------|
 |                   |--- ACK ---------->|
 |<-- OK/SDPb--------|                   |
 |--- ACK ---------->|                   |
 |                   |                   |

          ... SST timer expires :
 |                   |                   |
 |<-- INVITE / SDPb -|                   |
 |- OK/SDPa (offer)->|                   |
 |<----ACK  ---------|                   |
 |                   |                   |
 |                   |---INVITE / SDPa-->|
 |                   |<-- OK/SDPb (answ)-|
 |                   |----ACK----------->|

