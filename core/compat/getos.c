#include <stdio.h>

/*
 * OS detection (see http://predef.sourceforge.net/preos.html)
 */
#if (defined(linux) || defined(__linux))
# define OS "linux"

#elif (defined(sun) || defined(__sun))
# define OS "solaris"

#elif (defined(__APPLE__) && defined(__MACH__))
# define OS "macosx"

#elif defined(__FreeBSD__)
# define OS "freebsd"

#elif defined(__NetBSD__)
# define OS "netbsd"

#elif defined(__OpenBSD__)
# define OS "openbsd"

#elif defined(__bsdi__)
# define OS "bsdi"

#elif defined(__DragonFly__)
# define OS "dragonfly"

#else
# define OS "unknown"
# warning "Could not detect the OS"
#endif

int main()
{
  printf(OS "\n");
}
