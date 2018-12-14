#include <windows.h>

char *absolutePath(char *path, char *buff, size_t size) {
	GetFullPathNameA(path, size, buff, NULL);
	return buff;
}
