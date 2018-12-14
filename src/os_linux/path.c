#include <limits.h>

char *absolutePath(char *path, char *buff, size_t size) {
	if (size < PATH_MAX) {
		abort();
	}
	return realpath(path, buff);
}
