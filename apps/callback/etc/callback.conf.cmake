# "please type in the number you want to call, followed by the pund key"
welcome_prompt=${SEMS_AUDIO_PREFIX}/sems/audio/webconference/pin_prompt.wav

digits_dir=${SEMS_AUDIO_PREFIX}/sems/audio/webconference/

playout_type=adaptive_playout

# regexp of allowed cb numbers
accept_caller_re=016213*

# n sec wait before calling back
cb_wait=10   

# GW used to call back
gw_user=somegwuser
gw_domain=sparvoip.de
auth_pwd=somesecret
#auth_user=neededifdifferentfromgw_user
