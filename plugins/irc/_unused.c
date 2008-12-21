#if 0
/*
 * Unused stuff.
 *
 * For archaeological and sentimental purposes only
 *
 */


/**
 * irc_toupper_int(char *buf, int casemapping)
 *
 * Converts buffer pointed at buf to upper case using one of casmapping's:
 * IRC_CASEMAPPING_ASCII, IRC_CASEMAPPING_RFC1459, IRC_CASEMAPPING_RFC1459_STRICT
 *
 * DO NOT pass strings that can be in unicode;
 *
 * @return	pointer to beginning of a string
 */
static char *irc_toupper_int(char *buf, int casemapping)
{
	char *p = buf;
	int upper_bound;
	/* please, do not change this code, to something like:
	 * 122 + (!!casemapping * (5-casemapping))
	 */
	switch (casemapping)
	{
		case IRC_CASEMAPPING_ASCII:		upper_bound = 'z'; break;
		case IRC_CASEMAPPING_RFC1459_STRICT:	upper_bound = '}'; break;
		case IRC_CASEMAPPING_RFC1459:		upper_bound = '~'; break;
		default: debug_error ("bad value in call to irc_tolower_int: %d\n", casemapping); return 0;
	}
	while (*p)
	{
		if (*p >= 'a' && *p <= upper_bound)
			*p -= 32; /* substract 32, convesion 'a' -> 'A' */
		p++;
	}
	return buf;
}

static void resolver_child_handler(child_t *c, int pid, const char *name, int status, void *priv) {
	debug("(%s) resolver [%d] exited with %d\n", name, pid, status);
}
#endif
