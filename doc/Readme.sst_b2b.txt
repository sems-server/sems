SIP Session Timers (SST) enabled B2B application.

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

SIP Session Timers are enabled on both legs. When the 
timer expires, an empty INVITE is sent, and the resulting 
SDP offer from body of the 200 is relayed into the other 
leg, where it is sent out as INVITE with the offer. The 
answer from B leg is relayed into A leg and sent as body
in ACK message.

SST expiration is configurable in config file.


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
 |<-- INVITE --------|                   |
 |- OK/SDPc (offer)->|                   |
 |                   |---INVITE / SDPc-->|
 |                   |<-- OK/SDPd (answ)-|
 |<----ACK/SDPd------|----ACK----------->|

