#include <stdio.h>

/* 
 * CPU detection (http://predef.sourceforge.net/prearch.html)
 */
#if defined(__i386__)
# define ARCH "x86"

#elif (defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64))
# define ARCH "x86_64"

#elif defined(__arm__)
# if defined(__thumb__)
#  define ARCH "arm-thumb"
# else
#  define ARCH "arm"
# endif

#elif (defined(__ia64__) || defined(__IA64) || defined(__IA64__))
# define ARCH "ia64"

#elif defined(__m68k__)
# define ARCH "m68k"

#elif (defined(__mips__) || defined(mips))
# define ARCH "mips"

#elif (defined(__powerpc) || defined(__powerpc__) || defined(__POWERPC__) || defined(__ppc__))
# define ARCH "ppc"

#else
#define ARCH "unknown"
#warning "Could not detect CPU Architecture"

#endif

int main()
{
  printf(ARCH "\n");
}
