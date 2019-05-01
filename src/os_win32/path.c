#include <windows.h>

char *absolutePath(const char *path, char *buff, size_t size) {
	GetFullPathNameA(path, size, buff, NULL);
	return buff;
}
