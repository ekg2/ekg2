#include <sys/types.h>
#include <dirent.h>

int alphasort(const void*, const void*);
int scandir(const char*, struct dirent***, int (*select)(const struct dirent*), int (*compar)(const void*, const void*));

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
