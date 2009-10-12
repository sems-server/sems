set (audio_files
twit_account_msg.wav
twit_err_msg.wav
twit_ok_msg.wav
twit_posting_msg.wav
welcome_msg.wav
)

SET(sems_module_name twit)
SET(ivr_files twit)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/audio.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/ivr.rules.txt)
