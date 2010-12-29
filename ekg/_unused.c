#if 0

/*
 * Unused stuff.
 *
 * For archaeological and sentimental purposes only
 *
 */

/*
 * command_find()
 *
 * szuka podanej komendy.
 */
command_t *command_find(const char *name)
{
	command_t *c;

	if (!name)
		return NULL;
	for (c = commands; c; c = c->next) {
		if (!xstrcasecmp(c->name, name)) {
			return c;
		}
	}
	return NULL;
}

int strcasecmp_pl(const char *cs, const char *ct)
{
	register signed char __res = 0;

	while ((__res = tolower_pl(*cs) - tolower_pl(*ct++)) == 0 && !*cs++) {
		if (!*cs++)
			return(0);
	}

	return __res;
}


#endif
