CMAKE_MINIMUM_REQUIRED(VERSION 2.4)

IF(COMMAND cmake_policy)
	# Don't do automatic escaping of variables
	CMAKE_POLICY(SET CMP0005 OLD)
	CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND cmake_policy)


# Add path for additional FindXXX.cmake scenarios
set( CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

PROJECT (SEMS)

SET(SEMS_MAJOR_VERSION 1)
SET(SEMS_MINOR_VERSION 6)
SET(SEMS_BUILD_VERSION 0)
SET(SEMS_EXTRA_VERSION dev)

IF(EXISTS ${CMAKE_SOURCE_DIR}/.git)
	SET(SEMS_VERSION
	    "${SEMS_MAJOR_VERSION}.${SEMS_MINOR_VERSION}.${SEMS_BUILD_VERSION}-${SEMS_EXTRA_VERSION}")
ELSE(EXISTS ${CMAKE_SOURCE_DIR}/.git)
	SET(SEMS_VERSION
	    "${SEMS_MAJOR_VERSION}.${SEMS_MINOR_VERSION}.${SEMS_BUILD_VERSION}")
ENDIF(EXISTS ${CMAKE_SOURCE_DIR}/.git)

MESSAGE(STATUS "Configuring ${CMAKE_PROJECT_NAME} v. ${SEMS_VERSION}")

SET(SEMS_APP_NAME "sems")

IF(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT MSVC_IDE)
	message(FATAL_ERROR "In-source builds are not allowed.
CMake would overwrite all makefiles distributed with SEMS.
Please create a directory and run cmake from there, passing the path
to this source directory as the last argument.
This process created the file `CMakeCache.txt' and the directory `CMakeFiles'.
Please delete them.")
ENDIF(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT MSVC_IDE)

OPTION(SEMS_USE_SPANDSP       "Build with spandsp" OFF)
OPTION(SEMS_USE_LIBSAMPLERATE "Build with libsamplerate" OFF)
OPTION(SEMS_USE_ZRTP          "Build with ZRTP" OFF)
OPTION(SEMS_USE_MP3           "Build with Lame MP3" OFF)
OPTION(SEMS_USE_ILBC          "Build with iLBC library (fallback to bundled)" ON)
OPTION(SEMS_USE_TTS           "Build with Text-to-speech support (requires Flite)" OFF)
OPTION(SEMS_USE_OPENSSL       "Build with OpenSSL" OFF)
OPTION(SEMS_USE_MONITORING    "Build with monitoring support" OFF)
OPTION(SEMS_USE_IPV6          "Build with IPv6 support" OFF)
OPTION(MAX_RTP_SESSIONS:int   "How many rtp sessions SEMS will handle simultaneously? (default 2048)" 2048)

# Fix weird static libs handling in old CMake
IF (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} STREQUAL "2.4")
	SET(SEMS_WORKAROUND_FOR_OLD_CMAKE_2_4_FOR_STATIC_LIBS  ON)
ELSE (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} STREQUAL "2.4")
	SET(SEMS_WORKAROUND_FOR_OLD_CMAKE_2_4_FOR_STATIC_LIBS  OFF)
ENDIF (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} STREQUAL "2.4")

# add -lm and -lpthread to all targets
SET(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} m pthread)

IF (NOT SEMS_CFG_PREFIX)
	SET(SEMS_CFG_PREFIX "/usr/local")
ENDIF (NOT SEMS_CFG_PREFIX)
MESSAGE(STATUS "SEMS cfg prefix was set to: ${SEMS_CFG_PREFIX}")

IF (NOT SEMS_AUDIO_PREFIX)
	SET(SEMS_AUDIO_PREFIX "/usr/local/lib")
ENDIF (NOT SEMS_AUDIO_PREFIX)
MESSAGE(STATUS "SEMS audio prefix was set to: ${SEMS_AUDIO_PREFIX}")

IF (NOT SEMS_EXEC_PREFIX)
	SET(SEMS_EXEC_PREFIX "/usr/local")
ENDIF (NOT SEMS_EXEC_PREFIX)
MESSAGE(STATUS "SEMS exec prefix was set to: ${SEMS_EXEC_PREFIX}")

IF (NOT SEMS_LIBDIR)
	SET(SEMS_LIBDIR "lib")
ENDIF (NOT SEMS_LIBDIR)
MESSAGE(STATUS "SEMS libdir was set to: ${SEMS_LIBDIR}")

IF (NOT SEMS_DOC_PREFIX)
	SET(SEMS_DOC_PREFIX "/usr/share/doc")
ENDIF (NOT SEMS_DOC_PREFIX)
MESSAGE(STATUS "SEMS doc prefix was set to: ${SEMS_DOC_PREFIX}")

SET(CMAKE_VERBOSE_MAKEFILE ON)

FIND_PACKAGE(Libevent2 REQUIRED)

# compile with spandsp DTMF detection? see soft-switch.org
#   this needs a fairly new version of spandsp - tested with 0.0.4pre11
#   will not work with spandsp 0.0.2 .
#   (which means that current debian and gentoo packages don't work)
IF(SEMS_USE_SPANDSP)
	FIND_PACKAGE(Spandsp REQUIRED)
	ADD_DEFINITIONS(-DUSE_SPANDSP -D__STDC_LIMIT_MACROS)
	MESSAGE(STATUS "Using spandsp: YES")
ELSE(SEMS_USE_SPANDSP)
	MESSAGE(STATUS "Using spandsp: NO (default)")
ENDIF(SEMS_USE_SPANDSP)

# compile with sample rate conversion from secret rabbit code?
# (see http://www.mega-nerd.com/SRC/)
IF(SEMS_USE_LIBSAMPLERATE)
	FIND_PACKAGE(Libsamplerate REQUIRED)
	ADD_DEFINITIONS(-DUSE_LIBSAMPLERATE)
	MESSAGE(STATUS "Using libsamplerate: YES")
ELSE(SEMS_USE_LIBSAMPLERATE)
	MESSAGE(STATUS "Using libsamplerate: NO (default)")
ENDIF(SEMS_USE_LIBSAMPLERATE)

# ZRTP support? (see zfoneproject.com)
IF(SEMS_USE_ZRTP)
	FIND_PACKAGE(Libzrtp REQUIRED)
	MESSAGE(STATUS "Using libzrtp: YES")
	ADD_DEFINITIONS(-DWITH_ZRTP -DBUILD_ZRTP_MUTEXES -DBUILD_DEFAULT_CACHE -DBUILD_DEFAULT_TIMER -DUNIX -DBUILD_ZRTP_MUTEXES)
ELSE(SEMS_USE_ZRTP)
	MESSAGE(STATUS "Using libzrtp: NO (default)")
ENDIF(SEMS_USE_ZRTP)

# Lame MP3 support
IF(SEMS_USE_MP3)
	FIND_PACKAGE(Lame REQUIRED)
	MESSAGE(STATUS "Using lame mp3: YES")
ELSE(SEMS_USE_MP3)
	MESSAGE(STATUS "Using lame mp3: NO (default)")
ENDIF(SEMS_USE_MP3)

IF(SEMS_USE_ILBC)
	FIND_PACKAGE(Ilbc)
	IF(ILBC_FOUND)
		MESSAGE(STATUS "Using iLBC library: YES (external)")
	ELSE(ILBC_FOUND)
		MESSAGE(STATUS "Using iLBC library: YES (bundled, default)")
	ENDIF(ILBC_FOUND)
ELSE(SEMS_USE_ILBC)
	MESSAGE(STATUS "Uusing iLBC library: NO")
ENDIF(SEMS_USE_ILBC)

# build in support for monitoring?
IF(SEMS_USE_MONITORING)
	MESSAGE(STATUS "Enable monitoring: YES")
	ADD_DEFINITIONS(-DUSE_MONITORING)
ELSE(SEMS_USE_MONITORING)
	MESSAGE(STATUS "Enable monitoring: NO (default)")
ENDIF(SEMS_USE_MONITORING)

IF(SEMS_USE_IPV6)
	ADD_DEFINITIONS(-DSUPPORT_IPV6)
	MESSAGE(STATUS "Enable IPv6 support: YES")
ELSE(SEMS_USE_IPV6)
	MESSAGE(STATUS "Enable IPv6 support: NO (default)")
ENDIF(SEMS_USE_IPV6)

# Let's try to find GSM library and header files
FIND_PACKAGE(Gsm)
IF(GSM_FOUND)
	MESSAGE(STATUS "Using gsm: YES")
ELSE(GSM_FOUND)
	MESSAGE(STATUS "Using gsm: NO")
ENDIF(GSM_FOUND)

# Let's try to find SPEEX library and header files
FIND_PACKAGE(Speex)
IF(SPEEX_FOUND)
	MESSAGE(STATUS "Using speex: YES")
ELSE(SPEEX_FOUND)
	MESSAGE(STATUS "Using speex: NO")
ENDIF(SPEEX_FOUND)

IF(SEMS_USE_OPENSSL)
	FIND_PACKAGE(OpenSSL REQUIRED QUIETLY)
	MESSAGE(STATUS "Found OpenSSL includes:	${OPENSSL_INCLUDE_DIR}/openssl/ssl.h")
	MESSAGE(STATUS "Found OpenSSL library: ${OPENSSL_LIBRARIES}")
	MESSAGE(STATUS "Using OpenSSL: YES")
ELSE(SEMS_USE_OPENSSL)
	MESSAGE(STATUS "Using OpenSSL: NO (default)")
ENDIF(SEMS_USE_OPENSSL)

# Let's try to find Python development files
FIND_PACKAGE(PythonLibs)
IF(PYTHONLIBS_FOUND OR PYTHON_LIBRARIES)
	SET(PYTHONLIBS_FOUND TRUE)
	MESSAGE(STATUS "Found Python includes:	${PYTHON_INCLUDE_PATH}/Python.h")
	MESSAGE(STATUS "Found Python library: ${PYTHON_LIBRARIES}")
	MESSAGE(STATUS "Using Python: YES")
ELSE(PYTHONLIBS_FOUND OR PYTHON_LIBRARIES)
	MESSAGE(STATUS "Using Python: NO")
ENDIF(PYTHONLIBS_FOUND OR PYTHON_LIBRARIES)

# Let's try to find SIP (Python bindings generator) development files
FIND_PACKAGE(SIP)
IF(SIP_FOUND)
	MESSAGE(STATUS "Using SIP: YES")
ELSE(SIP_FOUND)
	MESSAGE(STATUS "Using SIP: NO")
ENDIF(SIP_FOUND)

# Let's try to find MySQL++ development files
FIND_PACKAGE(MySQLpp)
IF(MYSQLPP_FOUND)
	MESSAGE(STATUS "Using MySQL++: YES")
ELSE(MYSQLPP_FOUND)
	MESSAGE(STATUS "Using MySQL++: NO")
ENDIF(MYSQLPP_FOUND)

# Let's try to find Flite library and header files
IF(SEMS_USE_TTS)
	FIND_PACKAGE(Flite REQUIRED)
	MESSAGE(STATUS "Using text-to-speech: YES")
ELSE(SEMS_USE_TTS)
	SET(FLITE_FOUND FALSE)
	MESSAGE(STATUS "Using text-to-speech: NO (default)")
ENDIF(SEMS_USE_TTS)

# Let's try to find mISDN library and header files
FIND_PACKAGE(MISDN)
IF(MISDN_FOUND)
	MESSAGE(STATUS "Using mISDN: YES")
ELSE(MISDN_FOUND)
	MESSAGE(STATUS "Using mISDN: NO")
ENDIF(MISDN_FOUND)

# Let's try to find libev development files
FIND_PACKAGE(Libev)
IF(LIBEV_FOUND)
	MESSAGE(STATUS "Using libev: YES")
ELSE(LIBEV_FOUND)
	MESSAGE(STATUS "Using libev: NO")
ENDIF(LIBEV_FOUND)

# Let's try to find librtmp development files
FIND_PACKAGE(Librtmp)
IF(RTMP_FOUND)
	MESSAGE(STATUS "Using librtmp: YES")
ELSE(RTMP_FOUND)
	MESSAGE(STATUS "Using librtmp: NO")
ENDIF(RTMP_FOUND)

# Check for res_search in -lresolv
include(${CMAKE_SOURCE_DIR}/cmake/CheckSymbolExists.cmake)
SET(CMAKE_REQUIRED_LIBRARIES resolv)
CHECK_SYMBOL_EXISTS(res_search resolv.h HAVE_RES_SEARCH)
MESSAGE(STATUS "Have res_search: ${HAVE_RES_SEARCH} ")

INCLUDE_DIRECTORIES (${SEMS_SOURCE_DIR}/core/ampi)
INCLUDE_DIRECTORIES (${SEMS_SOURCE_DIR}/core/amci)
INCLUDE_DIRECTORIES (${SEMS_SOURCE_DIR}/core)

CONFIGURE_FILE( ${SEMS_SOURCE_DIR}/core/sems.h.cmake ${SEMS_SOURCE_DIR}/core/sems.h )

IF(CMAKE_COMPILER_IS_GNUCC)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_THREAD_SAFE -D_REENTRANT -Wall -fPIC -g -O2")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_COMPILER_IS_GNUCXX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_THREAD_SAFE -D_REENTRANT -Wall -Wno-reorder -fPIC -g -O2")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -D_DEBUG")

# compile with session thread pool support?
#      use this for very high concurrent call count
#      applications (e.g. for signaling only)
#      if compiled with thread pool, there will be a
#      thread pool of configurable size processing the
#      signaling and application logic of the calls.
#      if compiled without thread pool support, every
#      session will have its own thread.
#
#ADD_DEFINITIONS(-DSESSION_THREADPOOL)

#ADD_DEFINITIONS(-DNO_THREADID_LOG)
#ADD_DEFINITIONS(-DLOG_LOC_DATA_ATEND)

# Support for long debug messages? (useful for debugging SIP messages' contents)
#
# disable for slight performance gain
ADD_DEFINITIONS(-DLOG_BUFFER_LEN=2048)

#ADD_DEFINITIONS(-DDEBUG_PLAYOUTBUF)

SET(CMAKE_SHARED_LIBRARY_PREFIX "")

ADD_SUBDIRECTORY (core)
ADD_SUBDIRECTORY (apps)
ADD_SUBDIRECTORY (tools)

#SET_DIRECTORY_PROPERTIES(ADDITIONAL_MAKE_CLEAN_FILES *~)
