# These are needed if you keep audio files in file system
audio_path=${SEMS_AUDIO_PREFIX}/sems/audio/conference
default_announce=first_participant.wav
join_sound=beep.wav
drop_sound=beep.wav

# These are needed if you keep audio files in MySQL
#mysql_host=localhost
#mysql_user=sems
#mysql_passwd=sems
#mysql_db=sems

dialout_suffix=@iptel.org

# playout_type : select playout mechanism
#  adaptive_playout : Adaptive Playout buffer (default, recommended)
#  adaptive_jb      : Adaptive Jitter buffer
#  simple           : simple (fifo) playout buffer
#  
playout_type=adaptive_playout

# Maximum number of participants in a conference
# default = 0 (unlimited)
#max_participants=10

# use_rfc4240_rooms=[yes|no]
#
# RFC4240 specifies for Conference service that the conference 
# room is specified in the user part of the Request URI as:
#  sip:conf=uniqueIdentifier@mediaserver.example.net
# If the conference-id is empty, a 404 is returned.
#
#default:
# use_rfc4240_rooms=no
#

###############################################################
# RFC4028 Session Timer
#

# - enables the session timer ([yes,no]; default: no)
# 
# enable_session_timer=yes

# - set the "Session-Expires" parameter for the session timer.
#
# session_expires=240

# - set the "Min-SE" parameter for the session timer.
#
# minimum_timer=90

# -  maximum Timer value we want to accept
#
#maximum_timer=900

# session refresh (Session Timer, RFC4028) method
#
# INVITE                 - use re-INVITE
# UPDATE                 - use UPDATE
# UPDATE_FALLBACK_INVITE - use UPDATE if indicated in Allow, re-INVITE otherwise
#
# Default: UPDATE_FALLBACK_INVITE
#
# Note: Session Timers are only supported in some applications
#
#session_refresh_method=UPDATE

# accept_501_reply - accept 501 reply as successful refresh? [yes|no]
#
# Default: yes
#
#accept_501_reply=no
