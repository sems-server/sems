# symmetricrtp SBC profile
#
# This implements a transparent B2BUA which relays RTP and forces
# symmetric RTP on both sides - thus being able to bridge between
# two NATed clients

# RURI/From/To defaults: transparent
#RURI=$r
#From=$f
#To=$t

## RTP relay
# enable RTP relaying (bridging):
enable_rtprelay=yes
# force symmetric RTP (start with passive mode):
rtprelay_force_symmetric_rtp=yes
# use symmetric RTP indication from P-MsgFlags flag 2
#rtprelay_msgflags_symmetric_rtp=yes
# use transparent RTP seqno? [yes]
#rtprelay_transparent_seqno=no
# use transparent RTP SSRC? [yes]
#rtprelay_transparent_ssrc=no

# RTP interface to use for A leg
#aleg_rtprelay_interface=intern
# RTP interface to use for B leg
#rtprelay_interface=default

# outbound interface to use:
#outbound_interface=extern
