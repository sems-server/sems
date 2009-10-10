#
#
# Prompt sets are searched under this directory:
#
prompt_base_path=${SEMS_AUDIO_PREFIX}/sems/audio/voicebox/

#
# under the prompt_base_path, all domain/language combinations
# are searched, e.g.
#   ${SEMS_AUDIO_PREFIX}/sems/audio/voicebox/iptel.org/english
#   ${SEMS_AUDIO_PREFIX}/sems/audio/voicebox/iptel.org/german
#   ${SEMS_AUDIO_PREFIX}/sems/audio/voicebox/iptel.org/
#   ${SEMS_AUDIO_PREFIX}/sems/audio/voicebox/otherdomain.org/english
#   ...
# 
# specific prompts for these domains will be supported:
#
domains=iptel.org

# 
# a language may have single digits to follow the tens if spoken 
# (digits=right), or the single digits come first (digits=left). 
# Examples: English is digits=right. German is digits=left.
#
# specific prompts for these languages will be supported:
#
languages=english(digits=right);


#
# default language (if not set in App-Params)
#
default_language=english

#
# keys config (optional)
#
# repeat_key=1
# save_key=2
# delete_key=3
# startover_key=4
