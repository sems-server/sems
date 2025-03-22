set (ivr_test_audio_files
adios.wav
test.wav
quattro_formaggi.wav
)

SET(sems_module_name ivr_test)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/audio.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/python.rules.txt)

PYTHON_SOURCES_COMPILE(ivr_test.py ivr)
SEMS_INSTALL_CONFIGS(ivr_test)
SEMS_INSTALL_AUDIO("${sems_module_name}" "${${sems_module_name}_audio_files}")
