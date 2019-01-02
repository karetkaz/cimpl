#include <dlfcn.h>

static struct pluginLib {
	struct pluginLib *next;		// next plugin
	void (*onClose)(rtContext rt);
	void *handle;
} *pluginLibs = NULL;

int importLib(rtContext rt, const char *path) {

	void *library = dlopen(path, RTLD_NOW);
	if (library == NULL) {
		error(rt, NULL, 0, "Error opening library: %s", dlerror());
		return -2;
	}

	int (*install)(rtContext) = dlsym(library, pluginLibInstall);
	if (install == NULL) {
		error(rt, NULL, 0, "Error opening library: %s", dlerror());
		dlclose(library);
		return -1;
	}
	struct pluginLib *lib = malloc(sizeof(struct pluginLib));
	if (lib == NULL) {
		error(rt, NULL, 0, "Error opening library: %s", "out of memory");
		dlclose(library);
		return -1;
	}

	lib->onClose = dlsym(library, pluginLibDestroy);
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
			dlclose(lib->handle);
		}
		free(lib);
	}
}
