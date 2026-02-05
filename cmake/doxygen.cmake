# Doxygen documentation targets

find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/doc/doxygen_proj
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )

    add_custom_target(fulldoc
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/doc/doxygen_fulldoc_proj
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc
        COMMENT "Generating full documentation with Doxygen"
        VERBATIM
    )

    message(STATUS "Doxygen found - 'doc' and 'fulldoc' targets available")
else()
    message(STATUS "Doxygen not found - documentation targets disabled")
endif()
