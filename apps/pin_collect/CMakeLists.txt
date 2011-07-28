set (pin_collect_audio_files
enter_pin.wav
welcome.wav
)

SET(sems_module_name pin_collect)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/audio.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/python.rules.txt)

PYTHON_SOURCES_COMPILE(pin_collect.py ivr)
SEMS_INSTALL_CONFIGS(pin_collect)
SEMS_INSTALL_AUDIO("${sems_module_name}" "${${sems_module_name}_audio_files}")
