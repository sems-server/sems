
# authentication mode:
#    XMLRPC : authenticate against XMLRPC server
#    REFER  : add pin to REFER sent out to be checked at proxy
#    TRANSFER  : add pin to R-URI, transfer call flow (see Readme.pin_collect.txt)

auth_mode=XMLRPC

# XMLRPC url to authenticate against if auth_mode==XMLRPC
auth_xmlrpc_url = http://127.0.0.1:9090/

# messages to play to caller
welcome_msg=${SEMS_AUDIO_PREFIX}/sems/audio/pin_collect/welcome.wav
pin_msg=${SEMS_AUDIO_PREFIX}/sems/audio/pin_collect/enter_pin.wav
fail_msg=${SEMS_AUDIO_PREFIX}/sems/audio/pin_collect/fail.wav
auth_fail_msg=${SEMS_AUDIO_PREFIX}/sems/audio/pin_collect/notcorrect.wav

