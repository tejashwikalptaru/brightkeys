/* Version numbers. Release builds override these from CMake (-DBK_VERSION=x.y.z);
 * the defaults below are used for local development builds. */
#pragma once

#ifndef BK_VERSION_MAJOR
#define BK_VERSION_MAJOR 0
#endif
#ifndef BK_VERSION_MINOR
#define BK_VERSION_MINOR 0
#endif
#ifndef BK_VERSION_PATCH
#define BK_VERSION_PATCH 0
#endif

#define BK_STR2(x) #x
#define BK_STR(x) BK_STR2(x)
#define BK_VERSION_STR BK_STR(BK_VERSION_MAJOR) "." BK_STR(BK_VERSION_MINOR) "." BK_STR(BK_VERSION_PATCH)
