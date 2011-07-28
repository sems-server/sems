# script_path - *.py from this path is loaded as applications
script_path=${SEMS_EXEC_PREFIX}/${SEMS_LIBDIR}/sems/ivr/


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

