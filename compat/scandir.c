/*
 *  Copyright (c) 2002-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 * 
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/types.h>

#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#define __DIRENT_SIZE(d) (sizeof(struct dirent) - sizeof(((struct dirent *) 0)->d_name) + strlen((d)->d_name) + 1)

int alphasort(const void *__a, const void *__b)
{
	struct dirent **a = (struct dirent**) __a, **b = (struct dirent**) __b;

	if (!a || !b || !*a || !*b || !(*a)->d_name || !(*b)->d_name)
		return 0;

	return strcmp((*a)->d_name, (*b)->d_name);
}

int scandir(const char *path, struct dirent ***namelist, int (*select)(const struct dirent *), int (*compar)(const void *a, const void *b))
{
	int i, count = 0, my_errno = 0;
	struct dirent **res, *tmp;
	DIR *dir;

	if (!(dir = opendir(path)))
		return -1;

	while ((tmp = readdir(dir)))
		if (!select || (*select)(tmp))
			count++;

	rewinddir(dir);

	res = calloc(count, sizeof(struct dirent*));

	if (!res) {
		my_errno = errno;
		goto cleanup;
	}

	memset(res, 0, count * sizeof(struct dirent*));

	for (i = 0; i < count; ) {
		tmp = readdir(dir);

		if (!tmp) {
			my_errno = errno;
			goto cleanup;
		}

		if (select && !(*select)(tmp))
			continue;

		res[i] = malloc(__DIRENT_SIZE(tmp));

		if (!res[i]) {
			my_errno = ENOMEM;
			goto cleanup;
		}

		memcpy(res[i], tmp, __DIRENT_SIZE(tmp));

		i++;
	}

	closedir(dir);

	if (compar)
		qsort(res, count, sizeof(struct dirent*), compar);

	*namelist = res;

	return count;

cleanup:
	for (i = 0; res && res[i] && i < count; i++)
		free(res[i]);

	if (res)
		free(res);

	closedir(dir);

	errno = my_errno;

	return -1;
}
