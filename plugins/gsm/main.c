/* $Id$ */

/*
 *  (C) Copyright 2002 Wojtek Kaniewski <wojtekka@irc.pl>
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

#include "ekg2-config.h"

#include <string.h>
#include <stdlib.h>

#ifdef HAVE_GSM_H
#  include <gsm.h>
#else
#  ifdef HAVE_LIBGSM_GSM_H
#    include <libgsm/gsm.h>
#  endif
#endif

#include <ekg/audio.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/xmalloc.h>

PLUGIN_DEFINE(gsm, PLUGIN_CODEC, NULL);

static void *gsm_codec_init(const char *, const char *);
static int gsm_codec_process(void *, char *, int, char **, int *);
static void gsm_codec_destroy(void *);

static codec_cap_t gsm_codec_caps[3] = {
	{ "gsm:*", "pcm:8000,16,1" },
	{ "pcm:8000,16,1", "gsm:*" },
	{ NULL, NULL }
};

static codec_t gsm_codec = {
	name: "gsm",
	init: gsm_codec_init,
	process: gsm_codec_process,
	destroy: gsm_codec_destroy,
	caps: gsm_codec_caps
};

typedef struct {
	gsm codec;	/* w³a¶ciwa struktura libgsm */
	int encoder;	/* je¶li kodujemy do gsm to równe 1 */
	int msgsm;	/* > 0 je¶li mamy do czynienia z msgsm */
} gsm_codec_t;

static void *gsm_codec_init(const char *from, const char *to)
{
	gsm_codec_t *c;
	int value = 1;
	gsm codec;

	if (!from || !to)
		return NULL;

	if (!((!xstrncasecmp(from, "gsm:", 3) && !xstrcasecmp(to, "pcm:8000,16,1")) || (!xstrcasecmp(from, "pcm:8000,16,1") && !xstrncasecmp(to, "gsm:", 3))))
		return NULL;

	if (!(codec = gsm_create()))
		return NULL;

	c = xmalloc(sizeof(gsm_codec_t));

	gsm_option(codec, GSM_OPT_FAST, &value);
	gsm_option(codec, GSM_OPT_LTP_CUT, &value);

	c->codec = codec;
	c->encoder = (!xstrncasecmp(from, "pcm:", 3));

	if (!xstrcasecmp(from, "gsm:ms") || !xstrcasecmp(to, "gsm:ms")) {
		gsm_option(codec, GSM_OPT_WAV49, &value);
		c->msgsm = 1;
	}

	return c;
}

static int gsm_codec_process(void *codec, char *in, int inlen, char **p_out, int *p_outlen)
{
	gsm_codec_t *c = codec;
	int inpos = 0, outlen = 0;
	char *out = NULL;

	if (!c || !in || !inlen || !p_out || !p_outlen)
		return -1;
	
	for (;;) {
		int inchunklen, outchunklen;
		
		if (c->encoder) {
			inchunklen = 320;
			outchunklen = (c->msgsm == 1) ? 32 : 33;
		} else {
			inchunklen = (c->msgsm == 2) ? 32 : 33;
			outchunklen = 320;
		}

		if ((inlen - inpos) < inchunklen)
			break;

		out = xrealloc(out, outlen + outchunklen);

		if (c->encoder)
			gsm_encode(c->codec, (gsm_signal*) (in + inpos), out + outlen);
		else
			gsm_decode(c->codec, in + inpos, (gsm_signal*) (out + outlen));

		outlen += outchunklen;

		if (c->msgsm == 1)
			c->msgsm = 2;
		else if (c->msgsm == 2)
			c->msgsm = 1;

		inpos += inchunklen;
	}
	
	/* zwróæ wyniki */
	*p_out = out;
	*p_outlen = outlen;

	return inpos;
}

static void gsm_codec_destroy(void *codec)
{
	gsm_codec_t *c = codec;

	if (c && c->codec)
		gsm_destroy(c->codec);

	xfree(c);
}

int gsm_plugin_init(int prio)
{
	plugin_register(&gsm_plugin, prio);
	codec_register(&gsm_codec);

	return 0;
}

static int gsm_plugin_destroy()
{
	codec_unregister(&gsm_codec);
	plugin_unregister(&gsm_plugin);

	return 0;
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
