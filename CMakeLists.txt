CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

IF(COMMAND cmake_policy)
       CMAKE_POLICY(SET CMP0005 NEW)
ENDIF(COMMAND cmake_policy)

# Add path for additional FindXXX.cmake scenarios
set( CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

PROJECT (SEMS)

# Read the version from the VERSION file
execute_process(
    COMMAND cat "${CMAKE_SOURCE_DIR}/VERSION"
    OUTPUT_VARIABLE FILE_VERSION_RAW
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# If the file is missing or empty, fail with an error
if(NOT FILE_VERSION_RAW)
    message(FATAL_ERROR "VERSION file is missing or empty! Please create it.")
endif()

# Clean up version string: replace dashes with dots and remove 'v' prefix
string(REGEX REPLACE "^v" "" FILE_VERSION "${FILE_VERSION_RAW}")
string(REGEX REPLACE "-" "." FILE_VERSION "${FILE_VERSION}")

# Print version for debugging
message(STATUS "SEMS_VERSION: ${FILE_VERSION}")

# Pass version as a compile definition
add_definitions(-DSEMS_VERSION="${FILE_VERSION}")
ADD_DEFINITIONS(-DARCH="${CMAKE_SYSTEM_PROCESSOR}")
ADD_DEFINITIONS(-DOS="${CMAKE_SYSTEM_NAME}")

ADD_DEFINITIONS(-DSYSTEM_SAMPLECLOCK_RATE=32000LL)
ADD_DEFINITIONS(-DSEMS_APP_NAME="sems")

MESSAGE(STATUS "Configuring ${CMAKE_PROJECT_NAME} v. ${FILE_VERSION}")

IF(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT MSVC_IDE)
	message(FATAL_ERROR "In-source builds are not allowed.
CMake would overwrite all makefiles distributed with SEMS.
Please create a directory and run cmake from there, passing the path
to this source directory as the last argument.
This process created the file `CMakeCache.txt' and the directory `CMakeFiles'.
Please delete them.")
ENDIF(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT MSVC_IDE)

OPTION(SEMS_USE_OPUS          "Build with Opus" OFF)
OPTION(SEMS_USE_SPANDSP       "Build with spandsp" OFF)
OPTION(SEMS_USE_LIBSAMPLERATE "Build with libsamplerate" OFF)
OPTION(SEMS_USE_ZRTP          "Build with ZRTP" OFF)
OPTION(SEMS_USE_MP3           "Build with MP3" OFF)
OPTION(SEMS_USE_ILBC          "Build with iLBC library (fallback to bundled)" ON)
OPTION(SEMS_USE_G729          "Build with bcg729 library" OFF)
OPTION(SEMS_USE_CODEC2        "Build with codec2 library" OFF)
OPTION(SEMS_USE_TTS           "Build with Text-to-speech support (requires Flite)" OFF)
OPTION(SEMS_USE_OPENSSL       "Build with OpenSSL" OFF)
OPTION(SEMS_USE_MONITORING    "Build with monitoring support" ON)
OPTION(SEMS_USE_IPV6          "Build with IPv6 support" OFF)
OPTION(SEMS_USE_PYTHON        "Build with Python modules" ON)

# add -lm and -lpthread / -lthr to all targets

SET(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} m)
IF(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
	SET(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} thr)
ELSE(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
	SET(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} pthread)
ENDIF(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")

IF (NOT SEMS_CFG_PREFIX)
	SET(SEMS_CFG_PREFIX "")
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

IF (NOT SEMS_RUNDIR_PREFIX)
	SET(SEMS_RUNDIR_PREFIX "/var/local/run")
ENDIF (NOT SEMS_RUNDIR_PREFIX)
MESSAGE(STATUS "SEMS run dir prefix was set to: ${SEMS_RUNDIR_PREFIX}")

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

# compile with Opus codec?
IF(SEMS_USE_OPUS)
	FIND_PACKAGE(Opus REQUIRED)
	MESSAGE(STATUS "Using Opus: YES")
ELSE(SEMS_USE_OPUS)
	MESSAGE(STATUS "Using Opus: NO (default)")
ENDIF(SEMS_USE_OPUS)

# compile with sample rate conversion from secret rabbit code?
# (see http://www.mega-nerd.com/SRC/)
IF(SEMS_USE_LIBSAMPLERATE)
	FIND_PACKAGE(Libsamplerate REQUIRED)
	ADD_DEFINITIONS(-DUSE_LIBSAMPLERATE)
	MESSAGE(STATUS "Using libsamplerate: YES")
ELSE(SEMS_USE_LIBSAMPLERATE)
	MESSAGE(STATUS "Using libsamplerate: NO (default)")
	ADD_DEFINITIONS(-DUSE_INTERNAL_RESAMPLER)
ENDIF(SEMS_USE_LIBSAMPLERATE)

# ZRTP support? (see zfoneproject.com)
IF(SEMS_USE_ZRTP)
	FIND_PACKAGE(Libzrtp REQUIRED)
	MESSAGE(STATUS "Using libzrtp: YES")
	ADD_DEFINITIONS(-DWITH_ZRTP -DBUILD_DEFAULT_CACHE -DBUILD_DEFAULT_TIMER -DUNIX -DBUILD_ZRTP_MUTEXES)
ELSE(SEMS_USE_ZRTP)
	MESSAGE(STATUS "Using libzrtp: NO (default)")
ENDIF(SEMS_USE_ZRTP)

# Lame MP3 support
IF(SEMS_USE_MP3)
	FIND_PACKAGE(Lame REQUIRED)
	IF(LAME_FOUND)
		MESSAGE(STATUS "Using mp3 encoding (LAME library): YES")
		FIND_PACKAGE(Mpg123)
		IF(MPG123_FOUND)
			ADD_DEFINITIONS(-DWITH_MPG123DECODER)
			MESSAGE(STATUS "Using mp3 decoding (mpg123 library): YES")
		ELSE(MPG123_FOUND)
			MESSAGE(STATUS "Using mp3 decoding (mpg123 library): NO")
		ENDIF(MPG123_FOUND)
	ENDIF(LAME_FOUND)
ELSE(SEMS_USE_MP3)
	MESSAGE(STATUS "Using mp3: NO (default)")
ENDIF(SEMS_USE_MP3)

IF(SEMS_USE_ILBC)
	FIND_PACKAGE(Ilbc)
	IF(ILBC_FOUND)
		MESSAGE(STATUS "Using iLBC library: YES (external)")
	ELSE(ILBC_FOUND)
		MESSAGE(STATUS "Using iLBC library: YES (bundled, default)")
	ENDIF(ILBC_FOUND)
ELSE(SEMS_USE_ILBC)
	MESSAGE(STATUS "Using iLBC library: NO")
ENDIF(SEMS_USE_ILBC)

IF(SEMS_USE_G729)
	FIND_PACKAGE(Bcg729 REQUIRED)
	MESSAGE(STATUS "Using bcg729 library: YES")
ELSE(SEMS_USE_G729)
	MESSAGE(STATUS "Using bcg729 library: NO (default)")
ENDIF(SEMS_USE_G729)

IF(SEMS_USE_CODEC2)
	FIND_PACKAGE(Codec2 REQUIRED)
	MESSAGE(STATUS "Using codec2 library: YES")
ELSE(SEMS_USE_CODEC2)
	MESSAGE(STATUS "Using codec2 library: NO (default)")
ENDIF(SEMS_USE_CODEC2)

# build in support for monitoring?
IF(SEMS_USE_MONITORING)
	MESSAGE(STATUS "Enable monitoring: YES (default)")
	ADD_DEFINITIONS(-DUSE_MONITORING)
ELSE(SEMS_USE_MONITORING)
	MESSAGE(STATUS "Enable monitoring: NO")
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
	FIND_PACKAGE(OpenSSL REQUIRED QUIET)
	MESSAGE(STATUS "Found OpenSSL includes:	${OPENSSL_INCLUDE_DIR}/openssl/ssl.h")
	MESSAGE(STATUS "Found OpenSSL library: ${OPENSSL_LIBRARIES}")
	MESSAGE(STATUS "Using OpenSSL: YES")
ELSE(SEMS_USE_OPENSSL)
	MESSAGE(STATUS "Using OpenSSL: NO (default)")
ENDIF(SEMS_USE_OPENSSL)

# Let's try to find Python development files
IF(SEMS_USE_PYTHON)
	FIND_PACKAGE(PythonLibs)
	IF(PYTHONLIBS_FOUND OR PYTHON_LIBRARIES)
		SET(PYTHONLIBS_FOUND TRUE)
		MESSAGE(STATUS "Found Python includes:	${PYTHON_INCLUDE_PATH}/Python.h")
		MESSAGE(STATUS "Found Python library: ${PYTHON_LIBRARIES}")
		MESSAGE(STATUS "Using Python: YES ${PYTHONLIBS_FOUND} ${PYTHON_LIBRARIES}")
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
ELSE(SEMS_USE_PYTHON)
	MESSAGE(STATUS "Using Python modules: NO")
ENDIF(SEMS_USE_PYTHON)

# Let's try to find MySQL++ development files
FIND_PACKAGE(MySQLpp)
IF(MYSQLPP_FOUND)
	MESSAGE(STATUS "Using MySQL++: YES")
ELSE(MYSQLPP_FOUND)
	MESSAGE(STATUS "Using MySQL++: NO")
ENDIF(MYSQLPP_FOUND)

# Let's try to find MySQL-connector-C++ development files
FIND_PACKAGE(MySQLcppconn)
IF(MYSQLCPPCONN_FOUND)
	MESSAGE(STATUS "Using MySQL-connector-C++: YES")
ELSE(MYSQLCPPCONN_FOUND)
	MESSAGE(STATUS "Using MySQL-connector-C++: NO")
ENDIF(MYSQLCPPCONN_FOUND)

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

# Redis support
FIND_PACKAGE(Hiredis)
IF(HIREDIS_FOUND)
	MESSAGE(STATUS "Using hiredis: YES")
ELSE(HIREDIS_FOUND)
	MESSAGE(STATUS "Using hiredis: NO")
ENDIF(HIREDIS_FOUND)

IF(NOT ${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)
	# Check for res_search in -lresolv
	include(CheckSymbolExists)
	SET(CMAKE_REQUIRED_LIBRARIES resolv)
	CHECK_SYMBOL_EXISTS(res_search "netinet/in.h;resolv.h" HAVE_RES_SEARCH)
	MESSAGE(STATUS "Have res_search: ${HAVE_RES_SEARCH}")
ENDIF(NOT ${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)

INCLUDE_DIRECTORIES (${SEMS_SOURCE_DIR}/core/ampi)
INCLUDE_DIRECTORIES (${SEMS_SOURCE_DIR}/core/amci)
INCLUDE_DIRECTORIES (${SEMS_SOURCE_DIR}/core)

IF(${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)
	INCLUDE_DIRECTORIES (/usr/local/include)
	LINK_DIRECTORIES(/usr/local/lib)
ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)

IF(${CMAKE_SYSTEM_NAME} STREQUAL SunOS)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_SYS_SOCKIO_H -DBSD_COMP -fPIC -include compat/solaris.h")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_SYS_SOCKIO_H -DBSD_COMP -fPIC -include compat/solaris.h")
	# For nanosleep.
	SET(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} rt)
	# For inet_aton.
	SET(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} resolv)
	SET(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} dl socket nsl)
ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL SunOS)

IF(${CMAKE_SYSTEM_NAME} STREQUAL Darwin)
	INCLUDE_DIRECTORIES (/opt/local/include)
	LINK_DIRECTORIES(/opt/local/lib)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-common -DBSD44SOCKETS -D__DARWIN_UNIX03")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-common -DBSD44SOCKETS -D__DARWIN_UNIX03")
	SET(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS "${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} -flat_namespace -undefined suppress -bundle")
	SET(CMAKE_SHARED_MODULE_CREATE_C_FLAGS "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS} -flat_namespace -undefined suppress -bundle")
ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL Darwin)

IF(${CMAKE_SYSTEM_NAME} STREQUAL iOS)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-common -DBSD44SOCKETS")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-common -DBSD44SOCKETS")
	SET(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS "${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} -flat_namespace -undefined suppress -bundle")
	SET(CMAKE_SHARED_MODULE_CREATE_C_FLAGS "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS} -flat_namespace -undefined suppress -bundle")
	SET(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} gcc_eh)
ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL iOS)

IF(CMAKE_COMPILER_IS_GNUCC)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_THREAD_SAFE -D_REENTRANT -Wall -O2")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_COMPILER_IS_GNUCXX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_THREAD_SAFE -D_REENTRANT -Wall -std=c++11 -O2")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

IF("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_THREAD_SAFE -D_REENTRANT -Wall -std=c++11 -O2")
ENDIF("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

IF("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_THREAD_SAFE -D_REENTRANT -Wall -O2")
ENDIF("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")

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

# Propagate all unparsed SIP headers into AmSipReply. Disabled by default
# because only special applications might need this and it brings higher memory
# demands and affects performance.
#ADD_DEFINITIONS(-DPROPAGATE_UNPARSED_REPLY_HEADERS)

# Disable the ability to run sems as daemon
#ADD_DEFINITIONS(-DDISABLE_DAEMON_MODE)

IF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	ADD_DEFINITIONS(-DPROPAGATE_COREDUMP_SETTINGS)
ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

SET(CMAKE_SHARED_LIBRARY_PREFIX "")

enable_testing()

ADD_SUBDIRECTORY (apps)
ADD_SUBDIRECTORY (core)
ADD_SUBDIRECTORY (tools)

#SET_DIRECTORY_PROPERTIES(ADDITIONAL_MAKE_CLEAN_FILES *~)
