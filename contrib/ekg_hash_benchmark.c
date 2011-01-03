#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>

typedef int hash_t;

hash_t no_prompt_cache_hash = 0x139dcbd6;	/* hash value of "no_promp_cache" 2261954 it's default one. i hope good one.. for 32 bit x86 sure. */

hash_t ekg_hash(const char *name);

struct list {
	void *data;
	/*struct list *prev;*/
	struct list *next;
};

typedef struct list *list_t;

int hashes[256];

void ekg_oom_handler() { printf("braklo pamieci\n"); exit(1); }
void *xmalloc(size_t size) { void *tmp = malloc(size); if (!tmp) ekg_oom_handler(); memset(tmp, 0, size); return tmp; }
#define fix(s) ((s) ? (s) : "")
int xstrcmp(const char *s1, const char *s2) { return strcmp(fix(s1), fix(s2)); }
char *xstrdup(const char *s) { char *tmp; if (!s) return NULL; if (!(tmp = (char *) strdup(s))) ekg_oom_handler(); return tmp; }
void xfree(void *ptr) { if (ptr) free(ptr); }

void *list_add_beginning(list_t *list, void *data) {
	list_t new;

	if (!list) {
		errno = EFAULT;
		return NULL;
	}

	new = xmalloc(sizeof(struct list));
	new->next = *list;
	new->data = data;
	*list	  = new;

	return new->data;
}

struct format {
	char *name;
	hash_t name_hash;
	char *value;
};
list_t formats = NULL;

void format_add(const char *name, const char *value, int replace) {
	struct format *f;
	list_t l;
	hash_t hash;

	if (!name || !value)
		return;

	hash = ekg_hash(name);

	if (hash == no_prompt_cache_hash) {
		if (!xstrcmp(name, "no_prompt_cache")) {
			no_prompt_cache_hash = no_prompt_cache_hash;
			return;
		}
		printf("nothit_add0: %s vs no_prompt_cache\n", name);
	}

	for (l = formats; l; l = l->next) {
		struct format *f = l->data;
		if (hash == f->name_hash) {
			if (!xstrcmp(name, f->name)) {
				if (replace) {
					xfree(f->value);
					f->value = xstrdup(value);
				}
				return;
			}
			printf("nothit_add: %s vs %s | %08x\n", name, f->name, hash);
		}
	}

	f = xmalloc(sizeof(struct format));
	f->name		= xstrdup(name);
	f->name_hash	= hash;
	f->value	= xstrdup(value);

	hashes[hash & 0xff]++;

	list_add_beginning(&formats, f);
	return;
}


#define ROL(x) (((x>>25)&0x7f)|((x<<7)&0xffffff80))
hash_t ekg_hash(const char *name) {	/* ekg_hash() from stuff.c (rev: 1.1 to 1.203, and later) */
	hash_t hash = 0;

	for (; *name; name++) {
		hash ^= *name;
		hash = ROL(hash);
	}

	return hash;
}

int i = 0;

const char *format_find(const char *name) {
	const char *tmp;
	hash_t hash;
	list_t l;

	if (!name)
		return "";

	/* speech app */
	if (!strchr(name, ',')) {
		static char buf[1024];
		const char *tmp;

		snprintf(buf, sizeof(buf), "%s,speech", name);
		tmp = format_find(buf);
		if (tmp[0] != '\0')
			return tmp;
	}

	hash = ekg_hash(name);

	for (l = formats; l; l = l->next) {
		struct format *f = l->data;

		if (hash == f->name_hash) {
			if (!xstrcmp(f->name, name))
				return f->value;

			printf("nothit_find: %s vs %s\n", name, f->name);
		}
	}
	return "";
}

int main() {
	no_prompt_cache_hash = ekg_hash("no_prompt_cache");
	fprintf(stderr, "no_prompt_cache %08x\n", no_prompt_cache_hash);

	/* first of all we add all formats to list */

#define _(x) x

// abuse the preprocessor ;p
#include "ekg_hash_benchmark.inc"

	for (i = 0; i < 1; i++) {
		list_t l;

		for (l = formats; l; l = l->next) {
			struct format *f = l->data;
			
			format_find(f->name);
		}
	}

	{
		int totalhash = 0;

		for (i = 0; i < 0x100; i++)
			totalhash += hashes[i];

		printf("-- %d\n", totalhash);

		for (i = 0; i < 0x100; i++)
			printf("%d %.2f\n", hashes[i], (float) ( ((float) hashes[i] / (float) totalhash) * 100));
	}
	return 0;
}

