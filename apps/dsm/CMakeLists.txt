file(GLOB dsm_SRCS "*.cpp")

add_subdirectory(lib)
add_subdirectory(mods)

set(sems_module_name dsm)
set(dsm_config_files dsm dsm_in_prompts dsm_out_prompts)
set(dsm_doc_files ../../doc/dsm/Readme.dsm.txt ../../doc/dsm/dsm_syntax.txt
                  ../../doc/dsm/dsm_todo.txt)
set(dsm_doc_dirs ../../doc/dsm)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
