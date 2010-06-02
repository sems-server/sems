announce_path=${SEMS_AUDIO_PREFIX}/sems/audio/
default_announce=default_en.wav
beep=${SEMS_AUDIO_PREFIX}/sems/audio/beep.wav

# prompts

#"Welcome to iptel dot org voip service."
welcome=${SEMS_AUDIO_PREFIX}/sems/audio/annrecorder/welcome.wav

# "Your auto attendant greeting sounds like this: -"
your_prompt=${SEMS_AUDIO_PREFIX}/sems/audio/annrecorder/your_prompt.wav

# "To record a new auto attendant greeting, press any key. 
# End recording with any key. -"
to_record=${SEMS_AUDIO_PREFIX}/sems/audio/annrecorder/to_record.wav

# "Press one to keep the new greeting, or two to record a new one. -"
confirm=${SEMS_AUDIO_PREFIX}/sems/audio/annrecorder/confirm.wav

# "Your new auto attendant greeting has been set."
greeting_set=${SEMS_AUDIO_PREFIX}/sems/audio/annrecorder/greeting_set.wav

# "Thank you for using the iptel dot org service. Good Bye. - "
bye=${SEMS_AUDIO_PREFIX}/sems/audio/annrecorder/bye.wav

#
# Simple mode: 
#
#  If the simple mode is activated, the user part 
#  of the From-URI is used as the key to store the 
#  user annoucement. (no domain is used)
#
# Default value: no
#
# simple_mode=yes
