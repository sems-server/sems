FILE (GLOB dsm_SRCS "*.cpp")

ADD_SUBDIRECTORY (lib)
ADD_SUBDIRECTORY (mods)

SET(sems_module_name dsm)
SET(dsm_config_files dsm dsm_in_prompts dsm_out_prompts)
SET(dsm_doc_files
	../../doc/dsm/Readme.dsm.txt
	../../doc/dsm/dsm_syntax.txt
	../../doc/dsm/dsm_todo.txt
)
SET(dsm_doc_dirs ../../doc/dsm)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
