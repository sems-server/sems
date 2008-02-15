#ifndef __SOLARIS_H__
#define __SOLARIS_H__

#ifdef SOLARIS
/*
 * New compatibility code for Solaris.
 */

#ifndef timeradd 
#define timeradd(a, b, result)                                                \
  do {                                                                        \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;                             \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;                          \
    if ((result)->tv_usec >= 1000000)                                         \
      {                                                                       \
        ++(result)->tv_sec;                                                   \
        (result)->tv_usec -= 1000000;                                         \
      }                                                                       \
  } while (0) 
#endif 
#ifndef timersub 
#define timersub(a, b, result)                                                \
  do {                                                                        \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                             \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                          \
    if ((result)->tv_usec < 0) {                                              \
      --(result)->tv_sec;                                                     \
      (result)->tv_usec += 1000000;                                           \
    }                                                                         \
  } while (0) 
#endif 

// Work around use of deprecated definitions in Solaris system headers.
#include <sys/sockio.h>
#include <sys/types.h>
#define u_int16_t u_short
#define u_int32_t u_int

// For FIONBIO.
// Apparently it's better form to use
//   fcntl( sd, F_SETFL, FNONBLOCK | FASYNC )
// instead of
//   ioctl( sd, FIONBIO, ... )
// but I didn't write the code...
#include <sys/filio.h>

// Solaris doesn't have bcopy/bcmp/bzero. Reimplement them with more common routines.
// Use memmove rather than memcpy to behave correctly with overlapping sequences.
#define bzero(b,n) memset(b,0,n)
#define bcopy(b1,b2,n) memmove(b2,b1,n)
#define bcmp(b1,b2,n) memmove(b1,b2,n)

// Assume that we're going to be running on Solaris 10, which *does* have
// setenv, even though we might be compiling on Solaris 9 (which does not).
// extern "C" int setenv(const char *name, const char *value, int overwrite);

// Solaris doesn't define AF_LOCAL, PF_LOCAL, etc.
#define AF_LOCAL AF_UNIX
#define PF_LOCAL PF_UNIX
#define MSG_NOSIGNAL 0

// No u_int64_t on Solaris.
#ifndef u_int64_t
typedef unsigned long long int u_int64_t;
#endif


#if defined(sun)
#include <sys/byteorder.h>
#if defined(_BIG_ENDIAN)
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif
#else
#include <endian.h>
#endif

#endif /* SOLARIS */
#endif
