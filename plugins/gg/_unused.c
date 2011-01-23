#if 0
/*
 * Unused stuff.
 *
 * For archaeological and sentimental purposes only
 *
 */

/* never used? wtf? */
static void gg_dcc_close_handler(dcc_t *d)
{
	struct gg_dcc *g;
	
	if (!d || !(g = d->priv))
		return;

	gg_dcc_free(g);
}
#endif
