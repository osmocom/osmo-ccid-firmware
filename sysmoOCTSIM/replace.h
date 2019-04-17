#pragma once
/* whatever talloc 2.1.14 (from Debian talloc-2.1.14-2) required to build it
 * with gcc-arm-none-eabi on a Debian unstable system */

#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#ifndef _PUBLIC_
#define _PUBLIC_ __attribute__((visibility("default")))
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#define TALLOC_BUILD_VERSION_MAJOR 2
#define TALLOC_BUILD_VERSION_MINOR 1
#define TALLOC_BUILD_VERSION_RELEASE 14

#define HAVE_VA_COPY
#define HAVE_CONSTRUCTOR_ATTRIBUTE
