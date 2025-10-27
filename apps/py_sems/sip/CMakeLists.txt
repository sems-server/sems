set(sip_SRCS
    sippy_sems_libAmAudioEvent.cpp
    sippy_sems_libAmAudioFile.cpp
    sippy_sems_libAmB2ABCalleeSession.cpp
    sippy_sems_libAmEvent.cpp
    sippy_sems_libAmSessionAudioConnector.cpp
    sippy_sems_libAmSipDialog.cpp
    sippy_sems_libAmSipReply.cpp
    sippy_sems_libAmSipRequest.cpp
    sippy_sems_libPySemsB2ABCalleeDialog.cpp
    sippy_sems_libPySemsB2ABDialog.cpp
    sippy_sems_libPySemsB2ABEvent.cpp
    sippy_sems_libPySemsB2BDialog.cpp
    sippy_sems_libPySemsDialog.cpp
    sippy_sems_libcmodule.cpp
    sippy_sems_libstring.cpp)

execute_process(COMMAND python configure.py
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_library(sip STATIC ${sip_SRCS})
