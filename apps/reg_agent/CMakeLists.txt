set (reg_agent_SRCS
RegistrationAgent.cpp
)

add_library(reg_agent SHARED ${reg_agent_SRCS})
set_target_properties(reg_agent PROPERTIES OUTPUT_NAME "reg_agent")
set_target_properties(reg_agent PROPERTIES PREFIX "")
target_link_libraries(reg_agent ${CMAKE_DL_LIBS} pthread m)

