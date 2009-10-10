#CFGOPTION_SEMS_ANNOUNCEPATH
announce_path=${SEMS_AUDIO_PREFIX}/sems/audio/
#ENDCFGOPTION

#CFGOPTION_SEMS_ANNOUNCEMENT
default_announce=default_en.wav
#ENDCFGOPTION

#
# continue the call in B2BUA mode ? [yes | no | app-param]
#  if continue_b2b=app-param, continuation is controlled by 
#  'B2B' app param, e.g. P-App-Param: B2B=yes
# 
# default:
# continue_b2b=no
