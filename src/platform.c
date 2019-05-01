/**
 * platform and compiler specific source inclusion.
 */
#ifndef CMPL_PLATFORM_H
#define CMPL_PLATFORM_H

#include "internal.h"
#include <unistd.h>

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
	(void) path;
	return -1;
}
void closeLibs(rtContext rt) {
	(void) rt;
}
#endif //__NO_PLUGINS

#ifdef __NO_REALPATH
// use a fake path to make the tests work.
char *absolutePath(const char *path, char *buff, size_t size) {
	strncpy(buff, path, size - 2);
	return buff;
}
#endif //__NO_REALPATH


char *relativeToCWD(const char *path) {
	static char *CWD = NULL;
	if (CWD == NULL) {
		// lazy init on first call
		CWD = getcwd(NULL, 0);
		if (CWD == NULL) {
			CWD = "";
		}
		for (char *ptr = CWD; *ptr; ++ptr) {
			// convert windows path names to uri
			if (*ptr == '\\') {
				*ptr = '/';
			}
		}
	}

	for (int i = 0; path[i] != 0; ++i) {
		if (path[i] == 0) {
			// use absolute path
			break;
		}
		if (CWD[i] == 0) {
			if (path[i] == '/') {
				// use relative path
				return (char *) path + i + 1;
			}
			break;
		}
		if (path[i] != CWD[i]) {
			break;
		}
	}
	return (char*) path;
}

#endif //CMPL_PLATFORM_H
