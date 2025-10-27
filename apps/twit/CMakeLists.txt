set(twit_audio_files twit_account_msg.wav twit_err_msg.wav twit_ok_msg.wav
                     twit_posting_msg.wav welcome_msg.wav)

set(sems_module_name twit)
include(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/audio.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/python.rules.txt)

python_sources_compile(twit.py ivr)
sems_install_configs(twit)
sems_install_audio("${sems_module_name}" "${${sems_module_name}_audio_files}")
