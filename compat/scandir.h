#include <sys/types.h>
#include <dirent.h>

int alphasort(const void*, const void*);
int scandir(const char*, struct dirent***, int (*select)(const struct dirent*), int (*compar)(const void*, const void*));
