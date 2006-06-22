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
#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/xmalloc.h>

PLUGIN_DEFINE(gsm, PLUGIN_CODEC, NULL);
CODEC_DEFINE(gsm); 

/* prywatna strukturka audio_codec_t */
typedef struct {
	gsm codec;	/* w³a¶ciwa struktura libgsm */
	int msgsm;	/* > 0 je¶li mamy do czynienia z msgsm */
} gsm_private_t;

CODEC_CONTROL(gsm_codec_control) {
	audio_codec_t *ac = NULL;
	va_list ap;
	va_start(ap, way);

	if (type == AUDIO_CONTROL_INIT) {			/* gsm_codec_init() */
		char *attr;
		char *from = NULL, *to = NULL;
		int with_ms = 0;
		codec_way_t cway = CODEC_UNKNOWN;

		gsm_private_t *priv;
		gsm codec;
		int value = 1;

		while ((attr = va_arg(ap, char *))) {
			char *val = va_arg(ap, char *);
			debug("[gsm_codec_control] attr: %s value: %s\n", attr, val);
			if (!xstrcmp(attr, "from"))		from	= xstrdup(val);
			else if (!xstrcmp(attr, "to"))		to	= xstrdup(val);
			else if (!xstrcmp(attr, "with-ms") && atoi(val)) with_ms = 1;
			/* XXX birate, channels */
		}

		if (!((!xstrncasecmp(from, "gsm:", 3) && !xstrcasecmp(to, "pcm:8000,16,1")) || (!xstrcasecmp(from, "pcm:8000,16,1") && !xstrncasecmp(to, "gsm:", 3)))) {
			debug("gsm_codec_control() wrong from/to. from: %s to: %s\n", from, to);
			goto fail;
		
		}
		if (!xstrncasecmp(from, "pcm:", 3))	cway = CODEC_CODE;
		else if (!!xstrncasecmp(to, "pcm:", 3)) cway = CODEC_DECODE;
		else { debug("NEITHER CODEING, NEIHER DECODING ? WHOA THERE...\n"); goto fail; }

		if (!(codec = gsm_create())) {
			debug("gsm_create() fails\n");
			goto fail;
		}

		gsm_option(codec, GSM_OPT_FAST, &value);
		gsm_option(codec, GSM_OPT_LTP_CUT, &value);

		priv		= xmalloc(sizeof(gsm_private_t));
		priv->codec	= codec;
		priv->msgsm	= with_ms;

		if (priv->msgsm) {
			gsm_option(codec, GSM_OPT_WAV49, &value);
		}

		ac		= xmalloc(sizeof(audio_codec_t));
		ac->c		= &gsm_codec;
		ac->way		= cway;
		ac->private	= priv;
fail:
		xfree(from); xfree(to);
	} else if (type == AUDIO_CONTROL_DEINIT) {		/* gsm_codec_destroy() */
		gsm_private_t *priv = NULL;

		ac = *(va_arg(ap, audio_codec_t **));
		if (ac && ac->private) priv = ac->private;

		if (priv && priv->codec) gsm_destroy(priv->codec);
		xfree(priv);
		xfree(ac);
		ac = NULL;
	} else if (type == AUDIO_CONTROL_HELP) {
		static char *arr[] = { 
			"-gsm",			"",
			"-gsm:with-ms",		"0 1",
					/* THINK ABOUT NAMING CONVINECE? */
			"<gsm:__XXXIN",		"pcm:8000,16,1",
			"<gsm:__XXXOUT",	"gsm:*",

			">gsm:__XXXIN",		"gsm:*",
			">GSM:__XXXOUT",	"pcm:8000,16,1",
			NULL, }; 
		return arr;
	} else { debug("[gsm_codec_control] UNIMP\n"); } 
	va_end(ap); 
	return ac;
}

/* way: 0 - code ; 1 - decode */
int gsm_codec_process(int type, codec_way_t way, stream_buffer_t *input, stream_buffer_t *output, void *data) {
	gsm_private_t *c = data;

	if (!c || !input || !output) 
		return -1;

	char *in	= input->buf;
	int inlen	= input->len;

	char **p_out	= &output->buf;
	int *p_outlen	= &output->len;


	if (!input->buf || !input->len) /* we have nothing to code? */
		return 0;

	/* XXX here */
	return -1;

	int inpos = 0, outlen = 0;
	char *out = NULL;

	for (;;) {
		int inchunklen, outchunklen;
		
		if (way == CODEC_CODE) {
			inchunklen = 320;
			outchunklen = (c->msgsm == 1) ? 32 : 33;
		} else if (way == CODEC_DECODE) {
			inchunklen = (c->msgsm == 2) ? 32 : 33;
			outchunklen = 320;
		}
		if ((inlen - inpos) < inchunklen)
			break;

		out = xrealloc(out, outlen + outchunklen);

		if (way == CODEC_CODE)
			gsm_encode(c->codec, (gsm_signal*) (in + inpos), out + outlen);
		else if (way == CODEC_DECODE) 
			gsm_decode(c->codec, in + inpos, (gsm_signal*) (out + outlen));

		outlen += outchunklen;
		if (c->msgsm == 1)	c->msgsm = 2;
		else if (c->msgsm == 2) c->msgsm = 1;

		inpos += inchunklen;
	}
	/* zwróæ wyniki */
	*p_out = out;
	*p_outlen = outlen;

	return inpos;

	return -1;
}

CODEC_RECODE(gsm_codec_code) {
	return gsm_codec_process(type, CODEC_CODE, input, output, data);
}

CODEC_RECODE(gsm_codec_decode) {
	return gsm_codec_process(type, CODEC_DECODE, input, output, data);
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
