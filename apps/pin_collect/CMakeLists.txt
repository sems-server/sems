set(pin_collect_audio_files enter_pin.wav welcome.wav)

set(sems_module_name pin_collect)
include(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/audio.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/python.rules.txt)

python_sources_compile(pin_collect.py ivr)
sems_install_configs(pin_collect)
sems_install_audio("${sems_module_name}" "${${sems_module_name}_audio_files}")
