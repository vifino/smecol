// Macros that make my life less painful.

#ifndef __SMECOL_MACROS__
#define __SMECOL_MACROS__

#include <stdio.h>
#define eprintf(...) fprintf(stderr, __VA_ARGS__)

#ifdef DEBUG
#define debug(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

#endif
