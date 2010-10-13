set (adpcm_SRCS
adpcm.c
g711.c
g721.c
g723_16.c
g723_24.c
g723_40.c
g72x.c
)

SET(sems_module_name adpcm)
SET(adpcm_doc_files README_G711)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
