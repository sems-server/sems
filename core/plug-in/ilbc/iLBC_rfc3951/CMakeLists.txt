set (ilbc_SRCS
FrameClassify.c
LPCdecode.c
LPCencode.c
StateConstructW.c
StateSearchW.c
anaFilter.c
constants.c
createCB.c
doCPLC.c
enhancer.c
filter.c
gainquant.c
getCBvec.c
helpfun.c
hpInput.c
hpOutput.c
iCBConstruct.c
iCBSearch.c
iLBC_decode.c
iLBC_encode.c
lsf.c
packing.c
syntFilter.c
)

SET(CMAKE_VERBOSE_MAKEFILE ON)

add_library(libilbc STATIC ${ilbc_SRCS})
set_target_properties(libilbc PROPERTIES OUTPUT_NAME "ilbc")
set_target_properties(libilbc PROPERTIES PREFIX "")
target_link_libraries(libilbc ${CMAKE_DL_LIBS} pthread m)

