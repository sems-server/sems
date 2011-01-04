
# profiles - comma-separated list of call profiles to load
#
# <name>.sbcprofile.conf is loaded from module config 
# path (the path where this file resides)
profiles=transparent,auth_b2b,sst_b2b

# active call profile
#
# o active_profile=<profile_name>  always use <profile_name>
#
# o active_profile=$(ruri.user)    use user part of INVITE Request URI
#
# o active_profile=$(paramhdr)     use  "profile" option in P-App-Param header
#
active_profile=transparent


## RFC4028 Session Timer
# default configuration - can be overridden by call profiles

# - enables the session timer ([yes,no]; default: no)
#
#enable_session_timer=yes

# - set the "Session-Expires" parameter for the session timer.
#
# session_expires=240

# - set the "Min-SE" parameter for the session timer.
#
# minimum_timer=90

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

