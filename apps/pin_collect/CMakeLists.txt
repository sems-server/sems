set (audio_files
enter_pin.wav
welcome.wav
)

SET(sems_module_name pin_collect)
SET(ivr_files pin_collect)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/audio.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/ivr.rules.txt)
