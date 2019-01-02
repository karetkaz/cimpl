#include <Windows.h>

static struct pluginLib {
	struct pluginLib *next;		// next plugin
	void (*onClose)(rtContext rt);
	HANDLE handle;
} *pluginLibs = NULL;

int importLib(rtContext rt, const char *path) {

	HANDLE library = LoadLibraryA(path);
	if (library == NULL) {
		error(rt, NULL, 0, "Error[0x%08x] opening library: %s", GetLastError(), path);
		return -2;
	}

	int (*install)(rtContext) = (void*)GetProcAddress(library, pluginLibInstall);
	if (install == NULL) {
		error(rt, NULL, 0, "Error[0x%08x] opening library: %s", GetLastError(), path);
		FreeLibrary(library);
		return -1;
	}
	struct pluginLib *lib = malloc(sizeof(struct pluginLib));
	if (lib == NULL) {
		error(rt, NULL, 0, "Error opening library: %s", "out of memory");
		FreeLibrary(library);
		return -1;
	}

	lib->onClose = (void*)GetProcAddress(library, pluginLibDestroy);
	lib->handle = library;
	lib->next = pluginLibs;
	pluginLibs = lib;
	return install(rt);
}

void closeLibs(rtContext rt) {
	while (pluginLibs != NULL) {
		struct pluginLib *lib = pluginLibs;
		pluginLibs = lib->next;

		if (lib->onClose != NULL) {
			lib->onClose(rt);
		}
		if (lib->handle) {
			FreeLibrary(lib->handle);
		}
		free(lib);
	}
}
