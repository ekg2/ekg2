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

	if (d->type == DCC_VOICE) {
		close(audiofds[0]);
		close(audiofds[1]);
		audiofds[0] = -1;
		audiofds[1] = -1;
	}

	gg_dcc_free(g);
}

