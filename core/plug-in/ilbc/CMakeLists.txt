set (ilbc_SRCS
 ilbc.c
)

ADD_SUBDIRECTORY (iLBC_rfc3951)

INCLUDE_DIRECTORIES (iLBC_rfc3951)

set_property(TARGET libilbc PROPERTY IMPORTED_LOCATION ./iLBC_rfc3951/ilbc.a)

add_library(ilbc SHARED ${ilbc_SRCS})
set_target_properties(ilbc PROPERTIES OUTPUT_NAME "ilbc")
set_target_properties(ilbc PROPERTIES PREFIX "")
target_link_libraries(ilbc ${CMAKE_DL_LIBS} pthread m libilbc)

