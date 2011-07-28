set (twit_audio_files
twit_account_msg.wav
twit_err_msg.wav
twit_ok_msg.wav
twit_posting_msg.wav
welcome_msg.wav
)

SET(sems_module_name twit)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/audio.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/python.rules.txt)

PYTHON_SOURCES_COMPILE(twit.py ivr)
SEMS_INSTALL_CONFIGS(twit)
SEMS_INSTALL_AUDIO("${sems_module_name}" "${${sems_module_name}_audio_files}")
