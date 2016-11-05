
#if (defined(WIN32) || defined(_WIN32))
#include <windows.h>
typedef HANDLE library_t;
static inline void closeLib(library_t lib) { FreeLibrary(lib); }
static inline library_t openLib(const char *path) { return LoadLibraryA(path); }
static inline library_t findSym(library_t lib, const char *name) { return GetProcAddress(lib, name); }
#elif (defined(__linux__))
#include <dlfcn.h>
typedef void *library_t;
static inline void closeLib(library_t lib) { dlclose(lib); }
static inline library_t openLib(const char *path) { return dlopen(path, RTLD_NOW); }
static inline library_t findSym(library_t lib, const char *name) { return dlsym(lib, name); }
#else
typedef void *library_t;
static inline void closeLib(library_t lib) { (void)lib;}
static inline library_t openLib(const char *path) { return 0; }
static inline library_t findSym(library_t lib, const char *name) { return 0; }
#endif

#include "internal.h"

static const char *pluginLibInstall = "ccvmInit";

static const char *pluginLibDestroy = "ccvmDone";

static struct pluginLib *pluginLibs = NULL;

struct pluginLib {
	struct pluginLib *next;		// next plugin
	void (*onClose)();
	library_t *handle;
};

void closeLibs() {
	while (pluginLibs != NULL) {
		struct pluginLib *lib = pluginLibs;
		pluginLibs = lib->next;

		if (lib->onClose) {
			lib->onClose();
		}
		if (lib->handle) {
			closeLib(lib->handle);
		}
		free(lib);
	}
}

int importLib(rtContext rt, const char *path) {
	int result = -1;
	library_t library = openLib(path);
	if (library != NULL) {
		int (*install)(rtContext) = findSym(library, pluginLibInstall);
		if (install != NULL) {
			struct pluginLib *lib = malloc(sizeof(struct pluginLib));
			if (lib != NULL) {
				lib->onClose = findSym(library, pluginLibDestroy);
				lib->handle = library;
				lib->next = pluginLibs;
				result = install(rt);
				pluginLibs = lib;
			}
		}
		else {
			result = -2;
		}
	}
	return result;
}