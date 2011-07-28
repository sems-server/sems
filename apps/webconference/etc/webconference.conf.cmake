
# configure the various announcements here
first_participant=${SEMS_AUDIO_PREFIX}/sems/audio/webconference/first_participant.wav
join_sound=${SEMS_AUDIO_PREFIX}/sems/audio/webconference/beep.wav
drop_sound=${SEMS_AUDIO_PREFIX}/sems/audio/webconference/beep.wav
enter_pin=${SEMS_AUDIO_PREFIX}/sems/audio/webconference/pin_prompt.wav
wrong_pin=${SEMS_AUDIO_PREFIX}/sems/audio/webconference/wrong_pin.wav
wrong_pin_bye=${SEMS_AUDIO_PREFIX}/sems/audio/webconference/wrong_pin_bye.wav
entering_conference=${SEMS_AUDIO_PREFIX}/sems/audio/webconference/entering_conference.wav

#
# Say the PIN after entry?
# Possible values: 'yes'/'no'
#
tell_pin=yes

# digits to play the room number 
# must contain the files 0.wav, 1.wav, ...,  9.wav
digits_dir=${SEMS_AUDIO_PREFIX}/sems/audio/webconference/

#
# Use * key instead of # to finish PIN entry?
#
star_finishes_pin=yes

#
# direct_room_re specifies a regexp which, if matched on 
# the user of an incoming call, connects the caller directly to
# the conference room, without asking for the PIN. 
# 
direct_room_re=000777.*
#
# direct_room_strip specifies the number of digits to strip 
# from the user part of the request uri to get the conference
# room number on direct room dial in.
# e.g. direct_room_strip=5, called 000777123 => room 123
direct_room_strip=6

#
# master_password sets optionally a master password which can be used to
# retreive a room's password with the getRoomPassword function
#
# master_password=verysecret

# ignore_pin sets whether room admin PINs are checked when 
# accessing rooms. If you are for example trust anyone who
# can access the interface, this could be set to "yes" to allow
# all to look into all conference rooms.
# 
# default: no
#
# ignore_pin=yes

#
#
# feedback and statistics
feedback_file=/var/log/sems-webconference-feedback.txt

stats_dir=/var/log/sems-webconference/

#
# if set, this URL will be added to SDP (u line), so clever user agents 
# can display the conference control page right away.
#
# (room number and admin pin is embedded into the URL)
# 
#webconference_urlbase=https://webconference.iptel.org/openRoom.py?action=openRoom

# private_rooms = [yes |no]
#
# In private rooms mode, participants can join rooms only if they have been 
# opened before.
#
# Default: no
#
# private_rooms=yes

# support_rooms_timeout = [yes |no]
#
# Support timeout for rooms? If rooms are timed out, they will be closed and
# all participants disconnected.
#
# Default: no
#
# support_rooms_timeout=yes

#
# participants_expire = [yes | no]
# 
# if set to no, participants do not expire from conference rooms
#
# default: 
# participants_expire=yes

# delay after which a participant whose call has ended is removed from the 
# conference room view. Only active if participants_expire=yes
#
# default: 
# participants_expire_delay = 10
#

#
# rooms_expire = [yes | no]
# 
# if set to no, empty rooms do not expire
#
# default: 
# rooms_expire=yes

# delay after which an empty room vanishes, in seconds.
# Only active if rooms_expire=yes
#
# default: 
# rooms_expire_delay=7200 
#

# Only active if rooms_expire=yes
#
# default: 
# rooms_expire_delay=7200 
#

# look for expired conference rooms every n times, createRoom 
# or dialout is called
#
# room_sweep_interval=10

# predefined_rooms
#
# list of rooms that are openend at server startup
#
# predefined_rooms=discussion:some_pwd;support:other_pwd;


# Participant ID (default: X-ParticipantID header)
#
# get participant ID from P-App-Param:
#participant_id_param=participantid
#
# or, get participant ID from header:
#participant_id_header=x-participantid


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
