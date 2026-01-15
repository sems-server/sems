cmake_minimum_required(VERSION 3.17)

cmake_policy(SET CMP0005 NEW)

# Add path for additional FindXXX.cmake scenarios
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

project(SEMS)

# Read the version from the VERSION file
execute_process(
  COMMAND cat "${CMAKE_SOURCE_DIR}/VERSION"
  OUTPUT_VARIABLE FILE_VERSION_RAW
  OUTPUT_STRIP_TRAILING_WHITESPACE)

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
add_definitions(-DARCH="${CMAKE_SYSTEM_PROCESSOR}")
add_definitions(-DOS="${CMAKE_SYSTEM_NAME}")

add_definitions(-DSYSTEM_SAMPLECLOCK_RATE=32000LL)
add_definitions(-DSEMS_APP_NAME="sems")

message(STATUS "Configuring ${CMAKE_PROJECT_NAME} v. ${FILE_VERSION}")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT MSVC_IDE)
  message(
    FATAL_ERROR
      "In-source builds are not allowed.
CMake would overwrite all makefiles distributed with SEMS.
Please create a directory and run cmake from there, passing the path
to this source directory as the last argument.
This process created the file `CMakeCache.txt' and the directory `CMakeFiles'.
Please delete them.")
endif(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT MSVC_IDE)

option(SEMS_USE_OPUS "Build with Opus" OFF)
option(SEMS_USE_SPANDSP "Build with spandsp" OFF)
option(SEMS_USE_LIBSAMPLERATE "Build with libsamplerate" OFF)
option(SEMS_USE_ZRTP "Build with ZRTP" OFF)
option(SEMS_USE_MP3 "Build with MP3" OFF)
option(SEMS_USE_ILBC "Build with iLBC library (fallback to bundled)" ON)
option(SEMS_USE_G729 "Build with bcg729 library" OFF)
option(SEMS_USE_CODEC2 "Build with codec2 library" OFF)
option(SEMS_USE_TTS "Build with Text-to-speech support (requires Flite)" OFF)
option(SEMS_USE_OPENSSL "Build with OpenSSL" OFF)
option(SEMS_USE_MONITORING "Build with monitoring support" ON)
option(SEMS_USE_IPV6 "Build with IPv6 support" OFF)
option(SEMS_USE_PYTHON "Build with Python modules" ON)
option(SEMS_USE_ASAN "Build with AddressSanitizer (memory error detector)" OFF)

# AddressSanitizer configuration
if(SEMS_USE_ASAN)
  message(STATUS "AddressSanitizer enabled")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
endif()

# add -lm and -lpthread / -lthr to all targets

set(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} m)
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  set(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} thr)
else(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  set(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} pthread)
endif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")

if(NOT SEMS_CFG_PREFIX)
  set(SEMS_CFG_PREFIX "")
endif(NOT SEMS_CFG_PREFIX)
message(STATUS "SEMS cfg prefix was set to: ${SEMS_CFG_PREFIX}")

if(NOT SEMS_AUDIO_PREFIX)
  set(SEMS_AUDIO_PREFIX "/usr/local/lib")
endif(NOT SEMS_AUDIO_PREFIX)
message(STATUS "SEMS audio prefix was set to: ${SEMS_AUDIO_PREFIX}")

if(NOT SEMS_EXEC_PREFIX)
  set(SEMS_EXEC_PREFIX "/usr/local")
endif(NOT SEMS_EXEC_PREFIX)
message(STATUS "SEMS exec prefix was set to: ${SEMS_EXEC_PREFIX}")

if(NOT SEMS_RUNDIR_PREFIX)
  set(SEMS_RUNDIR_PREFIX "/var/local/run")
endif(NOT SEMS_RUNDIR_PREFIX)
message(STATUS "SEMS run dir prefix was set to: ${SEMS_RUNDIR_PREFIX}")

if(NOT SEMS_LIBDIR)
  set(SEMS_LIBDIR "lib")
endif(NOT SEMS_LIBDIR)
message(STATUS "SEMS libdir was set to: ${SEMS_LIBDIR}")

if(NOT SEMS_DOC_PREFIX)
  set(SEMS_DOC_PREFIX "/usr/share/doc")
endif(NOT SEMS_DOC_PREFIX)
message(STATUS "SEMS doc prefix was set to: ${SEMS_DOC_PREFIX}")

set(CMAKE_VERBOSE_MAKEFILE ON)

find_package(Libevent2 REQUIRED)

# compile with spandsp DTMF detection? see soft-switch.org this needs a fairly
# new version of spandsp - tested with 0.0.4pre11 will not work with spandsp
# 0.0.2 . (which means that current debian and gentoo packages don't work)
if(SEMS_USE_SPANDSP)
  find_package(Spandsp REQUIRED)
  add_definitions(-DUSE_SPANDSP -D__STDC_LIMIT_MACROS)
  message(STATUS "Using spandsp: YES")
else(SEMS_USE_SPANDSP)
  message(STATUS "Using spandsp: NO (default)")
endif(SEMS_USE_SPANDSP)

# compile with Opus codec?
if(SEMS_USE_OPUS)
  find_package(Opus REQUIRED)
  message(STATUS "Using Opus: YES")
else(SEMS_USE_OPUS)
  message(STATUS "Using Opus: NO (default)")
endif(SEMS_USE_OPUS)

# compile with sample rate conversion from secret rabbit code? (see
# http://www.mega-nerd.com/SRC/)
if(SEMS_USE_LIBSAMPLERATE)
  find_package(Libsamplerate REQUIRED)
  add_definitions(-DUSE_LIBSAMPLERATE)
  message(STATUS "Using libsamplerate: YES")
else(SEMS_USE_LIBSAMPLERATE)
  message(STATUS "Using libsamplerate: NO (default)")
  add_definitions(-DUSE_INTERNAL_RESAMPLER)
endif(SEMS_USE_LIBSAMPLERATE)

# ZRTP support? (see zfoneproject.com)
if(SEMS_USE_ZRTP)
  find_package(Libzrtp REQUIRED)
  message(STATUS "Using libzrtp: YES")
  add_definitions(-DWITH_ZRTP -DBUILD_DEFAULT_CACHE -DBUILD_DEFAULT_TIMER
                  -DUNIX -DBUILD_ZRTP_MUTEXES)
else(SEMS_USE_ZRTP)
  message(STATUS "Using libzrtp: NO (default)")
endif(SEMS_USE_ZRTP)

# Lame MP3 support
if(SEMS_USE_MP3)
  find_package(Lame REQUIRED)
  if(LAME_FOUND)
    message(STATUS "Using mp3 encoding (LAME library): YES")
    find_package(Mpg123)
    if(MPG123_FOUND)
      add_definitions(-DWITH_MPG123DECODER)
      message(STATUS "Using mp3 decoding (mpg123 library): YES")
    else(MPG123_FOUND)
      message(STATUS "Using mp3 decoding (mpg123 library): NO")
    endif(MPG123_FOUND)
  endif(LAME_FOUND)
else(SEMS_USE_MP3)
  message(STATUS "Using mp3: NO (default)")
endif(SEMS_USE_MP3)

if(SEMS_USE_ILBC)
  find_package(Ilbc)
  if(ILBC_FOUND)
    message(STATUS "Using iLBC library: YES (external)")
  else(ILBC_FOUND)
    message(STATUS "Using iLBC library: YES (bundled, default)")
  endif(ILBC_FOUND)
else(SEMS_USE_ILBC)
  message(STATUS "Using iLBC library: NO")
endif(SEMS_USE_ILBC)

if(SEMS_USE_G729)
  find_package(Bcg729 REQUIRED)
  message(STATUS "Using bcg729 library: YES")
else(SEMS_USE_G729)
  message(STATUS "Using bcg729 library: NO (default)")
endif(SEMS_USE_G729)

if(SEMS_USE_CODEC2)
  find_package(Codec2 REQUIRED)
  message(STATUS "Using codec2 library: YES")
else(SEMS_USE_CODEC2)
  message(STATUS "Using codec2 library: NO (default)")
endif(SEMS_USE_CODEC2)

# build in support for monitoring?
if(SEMS_USE_MONITORING)
  message(STATUS "Enable monitoring: YES (default)")
  add_definitions(-DUSE_MONITORING)
else(SEMS_USE_MONITORING)
  message(STATUS "Enable monitoring: NO")
endif(SEMS_USE_MONITORING)

if(SEMS_USE_IPV6)
  add_definitions(-DSUPPORT_IPV6)
  message(STATUS "Enable IPv6 support: YES")
else(SEMS_USE_IPV6)
  message(STATUS "Enable IPv6 support: NO (default)")
endif(SEMS_USE_IPV6)

# Let's try to find GSM library and header files
find_package(Gsm)
if(GSM_FOUND)
  message(STATUS "Using gsm: YES")
else(GSM_FOUND)
  message(STATUS "Using gsm: NO")
endif(GSM_FOUND)

# Let's try to find SPEEX library and header files
find_package(Speex)
if(SPEEX_FOUND)
  message(STATUS "Using speex: YES")
else(SPEEX_FOUND)
  message(STATUS "Using speex: NO")
endif(SPEEX_FOUND)

if(SEMS_USE_OPENSSL)
  find_package(OpenSSL REQUIRED QUIET)
  message(STATUS "Found OpenSSL includes:	${OPENSSL_INCLUDE_DIR}/openssl/ssl.h")
  message(STATUS "Found OpenSSL library: ${OPENSSL_LIBRARIES}")
  message(STATUS "Using OpenSSL: YES")
else(SEMS_USE_OPENSSL)
  message(STATUS "Using OpenSSL: NO (default)")
endif(SEMS_USE_OPENSSL)

# Let's try to find Python development files
if(SEMS_USE_PYTHON)
  find_package(Python3 COMPONENTS Interpreter Development)
  if(Python3_FOUND)
    message(STATUS "Found Python includes: ${Python3_INCLUDE_DIRS}")
    message(STATUS "Found Python library: ${Python3_LIBRARIES}")
    message(STATUS "Found Python version: ${Python3_VERSION}")
    message(STATUS "Using Python: YES")
  else(Python3_FOUND)
    message(STATUS "Using Python: NO")
  endif(Python3_FOUND)

  # Let's try to find SIP (Python bindings generator) development files
  find_package(SIP)
  if(SIP_FOUND)
    message(STATUS "Using SIP: YES")
  else(SIP_FOUND)
    message(STATUS "Using SIP: NO")
  endif(SIP_FOUND)
else(SEMS_USE_PYTHON)
  message(STATUS "Using Python modules: NO")
endif(SEMS_USE_PYTHON)

# Let's try to find MySQL++ development files
find_package(MySQLpp)
if(MYSQLPP_FOUND)
  message(STATUS "Using MySQL++: YES")
else(MYSQLPP_FOUND)
  message(STATUS "Using MySQL++: NO")
endif(MYSQLPP_FOUND)

# Let's try to find MySQL-connector-C++ development files
find_package(MySQLcppconn)
if(MYSQLCPPCONN_FOUND)
  message(STATUS "Using MySQL-connector-C++: YES")
else(MYSQLCPPCONN_FOUND)
  message(STATUS "Using MySQL-connector-C++: NO")
endif(MYSQLCPPCONN_FOUND)

# Let's try to find Flite library and header files
if(SEMS_USE_TTS)
  find_package(Flite REQUIRED)
  message(STATUS "Using text-to-speech: YES")
else(SEMS_USE_TTS)
  set(FLITE_FOUND FALSE)
  message(STATUS "Using text-to-speech: NO (default)")
endif(SEMS_USE_TTS)

# Let's try to find mISDN library and header files
find_package(MISDN)
if(MISDN_FOUND)
  message(STATUS "Using mISDN: YES")
else(MISDN_FOUND)
  message(STATUS "Using mISDN: NO")
endif(MISDN_FOUND)

# Let's try to find libev development files
find_package(Libev)
if(LIBEV_FOUND)
  message(STATUS "Using libev: YES")
else(LIBEV_FOUND)
  message(STATUS "Using libev: NO")
endif(LIBEV_FOUND)

# Let's try to find librtmp development files
find_package(Librtmp)
if(RTMP_FOUND)
  message(STATUS "Using librtmp: YES")
else(RTMP_FOUND)
  message(STATUS "Using librtmp: NO")
endif(RTMP_FOUND)

# Redis support
find_package(Hiredis)
if(HIREDIS_FOUND)
  message(STATUS "Using hiredis: YES")
else(HIREDIS_FOUND)
  message(STATUS "Using hiredis: NO")
endif(HIREDIS_FOUND)

if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)
  # Check for res_search in -lresolv
  include(CheckSymbolExists)
  set(CMAKE_REQUIRED_LIBRARIES resolv)
  check_symbol_exists(res_search "netinet/in.h;resolv.h" HAVE_RES_SEARCH)
  message(STATUS "Have res_search: ${HAVE_RES_SEARCH}")
endif(NOT ${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)

include_directories(${SEMS_SOURCE_DIR}/core/ampi)
include_directories(${SEMS_SOURCE_DIR}/core/amci)
include_directories(${SEMS_SOURCE_DIR}/core)

if(${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)
  include_directories(/usr/local/include)
  link_directories(/usr/local/lib)
endif(${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)

if(${CMAKE_SYSTEM_NAME} STREQUAL SunOS)
  set(CMAKE_C_FLAGS
      "${CMAKE_C_FLAGS} -DHAVE_SYS_SOCKIO_H -DBSD_COMP -fPIC -include compat/solaris.h"
  )
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -DHAVE_SYS_SOCKIO_H -DBSD_COMP -fPIC -include compat/solaris.h"
  )
  # For nanosleep.
  set(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} rt)
  # For inet_aton.
  set(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} resolv)
  set(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} dl socket nsl)
endif(${CMAKE_SYSTEM_NAME} STREQUAL SunOS)

if(${CMAKE_SYSTEM_NAME} STREQUAL Darwin)
  include_directories(/opt/local/include)
  link_directories(/opt/local/lib)
  set(CMAKE_C_FLAGS
      "${CMAKE_C_FLAGS} -fno-common -DBSD44SOCKETS -D__DARWIN_UNIX03")
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -fno-common -DBSD44SOCKETS -D__DARWIN_UNIX03")
  set(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS
      "${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} -flat_namespace -undefined suppress -bundle"
  )
  set(CMAKE_SHARED_MODULE_CREATE_C_FLAGS
      "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS} -flat_namespace -undefined suppress -bundle"
  )
endif(${CMAKE_SYSTEM_NAME} STREQUAL Darwin)

if(${CMAKE_SYSTEM_NAME} STREQUAL iOS)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-common -DBSD44SOCKETS")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-common -DBSD44SOCKETS")
  set(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS
      "${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} -flat_namespace -undefined suppress -bundle"
  )
  set(CMAKE_SHARED_MODULE_CREATE_C_FLAGS
      "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS} -flat_namespace -undefined suppress -bundle"
  )
  set(CMAKE_DL_LIBS ${CMAKE_DL_LIBS} gcc_eh)
endif(${CMAKE_SYSTEM_NAME} STREQUAL iOS)

if(CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_THREAD_SAFE -D_REENTRANT -Wall -O2")
endif(CMAKE_COMPILER_IS_GNUCC)

if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -D_THREAD_SAFE -D_REENTRANT -Wall -std=c++11 -O2")
endif(CMAKE_COMPILER_IS_GNUCXX)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -D_THREAD_SAFE -D_REENTRANT -Wall -std=c++11 -O2")
endif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_THREAD_SAFE -D_REENTRANT -Wall -O2")
endif("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}   -D_DEBUG")

# compile with session thread pool support? use this for very high concurrent
# call count applications (e.g. for signaling only) if compiled with thread
# pool, there will be a thread pool of configurable size processing the
# signaling and application logic of the calls. if compiled without thread pool
# support, every session will have its own thread.
#
# ADD_DEFINITIONS(-DSESSION_THREADPOOL)

# ADD_DEFINITIONS(-DNO_THREADID_LOG) ADD_DEFINITIONS(-DLOG_LOC_DATA_ATEND)

# Support for long debug messages? (useful for debugging SIP messages' contents)
#
# disable for slight performance gain
add_definitions(-DLOG_BUFFER_LEN=2048)

# ADD_DEFINITIONS(-DDEBUG_PLAYOUTBUF)

# Propagate all unparsed SIP headers into AmSipReply. Disabled by default
# because only special applications might need this and it brings higher memory
# demands and affects performance.
# ADD_DEFINITIONS(-DPROPAGATE_UNPARSED_REPLY_HEADERS)

# Disable the ability to run sems as daemon
# ADD_DEFINITIONS(-DDISABLE_DAEMON_MODE)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  add_definitions(-DPROPAGATE_COREDUMP_SETTINGS)
endif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

set(CMAKE_SHARED_LIBRARY_PREFIX "")

enable_testing()

add_subdirectory(apps)
add_subdirectory(core)
add_subdirectory(tools)

# SET_DIRECTORY_PROPERTIES(ADDITIONAL_MAKE_CLEAN_FILES *~)
