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

ADD_LIBRARY(libilbc STATIC ${ilbc_SRCS})

# FIXME
IF(SEMS_WORKAROUND_FOR_OLD_CMAKE_2_4_FOR_STATIC_LIBS)
ADD_CUSTOM_COMMAND(TARGET libilbc POST_BUILD COMMAND cp ARGS liblibilbc.a libliblibilbc.a)
ENDIF(SEMS_WORKAROUND_FOR_OLD_CMAKE_2_4_FOR_STATIC_LIBS)
