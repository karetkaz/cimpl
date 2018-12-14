/**
 * platform and compiler specific source inclusion.
 */
#ifndef CMPL_PLATFORM_H
#define CMPL_PLATFORM_H

#include "internal.h"

// mingw || gcc || emcc
#if defined(__GNUC__) || defined(__EMSCRIPTEN__)
#ifdef _WIN32
#include "os_win32/time.c"
#include "os_win32/path.c"
#include "os_win32/plugin.c"
#else
#include "os_linux/time.c"
#include "os_linux/path.c"
#include "os_linux/plugin.c"
#endif

// watcom c/c++
#elif defined(__WATCOMC__)
#if defined(__DOS)
msdod not support
#elif defined(_WIN32)
#include "os_win32/time.c"
#include "os_win32/path.c"
#include "os_win32/plugin.c"
#else
// probably linux
#include "os_linux/time.c"
#define __NO_REALPATH
#define __NO_PLUGINS
#endif

/* TODO: other compilers
// visual studio
#elif defined(_MSC_VER)
#include "os_win32/time.c"
#include "os_win32/path.c"
#include "os_win32/plugin.c"

#elif defined(__APPLE__) && defined(__MACH__)
#include "os_linux/time.c"
#include "os_linux/path.c"
#define __NO_PLUGINS
*/
#endif

#ifdef __NO_PLUGINS
int importLib(rtContext rt, const char *path) {
	error(rt, NULL, 0, "Error opening library: %s", "Plugins are not supported");
	return -1;
}
void closeLibs(rtContext rt) {}
#endif //__NO_PLUGINS

#ifdef __NO_REALPATH
// use a fake path to make the tests work.
char *absolutePath(char *path, char *buff, size_t size) {
	buff[0] = '.';
	buff[1] = '/';
	strncpy(buff + 2, path, size - 2);
	return buff;
}
#endif //__NO_REALPATH

#endif //CMPL_PLATFORM_H
