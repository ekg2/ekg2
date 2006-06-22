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

#include <ekg/audio.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/xmalloc.h>

PLUGIN_DEFINE(pcm, PLUGIN_CODEC, NULL);
CODEC_DEFINE(pcm);

#ifdef EKG2_WIN32_SHARED_LIB
	EKG2_WIN32_SHARED_LIB_HELPER
#endif

/* prywatna strukturka audio_codec_t */
typedef struct {
	int ifreq, ofreq;	/* czêstotliwo¶æ */
	int ibps, obps;		/* bps */
	int ich, och;		/* ilo¶æ kana³ów */
} pcm_private_t; 


CODEC_CONTROL(pcm_codec_control) {
	audio_codec_t *ac = NULL;
	va_list ap;

	va_start(ap, way);

	if (type == AUDIO_CONTROL_INIT) {	/* pcm_codec_init() */
		char *from, *to;
		char *attr;
		int valid = 1;

		pcm_private_t *priv = xmalloc(sizeof(pcm_private_t));

		while ((attr = va_arg(ap, char *))) {
			char *val = va_arg(ap, char *);
			int v = 0;

			debug("[pcm_codec_control] attr: %s value: %s\n", attr, val);
			if (!xstrcmp(attr, "from"))		from	= xstrdup(val);
			else if (!xstrcmp(attr, "to"))		to	= xstrdup(val);

			else if (!xstrcmp(attr, "freq"))	{ v = atoi(val); 	priv->ifreq = v;	priv->ofreq = v;	}
			else if (!xstrcmp(attr, "bps"))		{ v = atoi(val);	priv->ibps = v;		priv->obps = v;		}
			else if (!xstrcmp(attr, "channels"))	{ v = atoi(val);	priv->ich = v;		priv->och = v;		}

			else if (!xstrcmp(attr, "ifreq"))	{ v = atoi(val); 	priv->ifreq = v; 				}
			else if (!xstrcmp(attr, "ibps"))	{ v = atoi(val);	priv->ibps = v;					} 
			else if (!xstrcmp(attr, "ichannels"))	{ v = atoi(val);	priv->ich = v;					} 

			else if (!xstrcmp(attr, "ofreq"))	{ v = atoi(val);  				priv->ofreq = v;	}
			else if (!xstrcmp(attr, "obps"))	{ v = atoi(val);				priv->obps = v;		}
			else if (!xstrcmp(attr, "ochannels"))	{ v = atoi(val);				priv->och = v;		}
		}

		debug("[pcm_codec_control] TER from: %s to: %s Ifreq: %d Ofreq: %d Ibps: %d Obps: %d Ichannels: %d Ochannels: %d\n",
			from, to, priv->ifreq, priv->ofreq, priv->ibps, priv->obps, priv->ich, priv->och);

	/* CHECK ALL ATTS: */
		if (xstrncasecmp(from, "pcm:", 4) || xstrncasecmp(to, "pcm:", 4)) 			valid = 0;	/* CHECK NAME */
		if (!priv->ifreq || !priv->ofreq) 							valid = 0;	/* CHECK FREQ */
		if ((priv->ibps != 8 && priv->ibps != 16) || (priv->obps != 8 && priv->obps != 16))	valid = 0;	/* CHECK BPS */
		if (priv->ich < 1 || priv->ich > 2 || priv->och < 1 || priv->och > 2)			valid = 0;	/* CHECK CHANNELS */
	/* IF CHECK FAILS: */
		if (!valid) { xfree(priv); goto fail; } 

		ac		= xmalloc(sizeof(audio_codec_t));
		ac->c		= &pcm_codec;
		ac->way		 = 0;
		ac->private 	= priv;
fail:
		xfree(from); xfree(to);
	} else if (type == AUDIO_CONTROL_DEINIT) {	/* pcm_codec_destroy() */
		ac = *(va_arg(ap, audio_codec_t **));
		if (ac) {
			xfree(ac->private);
			xfree(ac);
		}
		ac = NULL;
	} else if (type == AUDIO_CONTROL_HELP) {		/* pcm_codec_capabilities() */ 
		
		static char *arr[] = { 
			">pcm",	"pcm:ifreq pcm:ibps pcm:ichannels",	/* if INPUT we request for:  ifreq, ibps & ichannels */
			"<pcm", "pcm:ofreq pcm:obps pcm:ochannels",	/* if OUTPUT we request for: ofreq, obps & ochannels */

			"-pcm:freq",		"",
			"-pcm:bps",		"8 16",
			"-pcm:channels", 	"1 2",
			
			">pcm:ifreq",		"",
			">pcm:ibps",		"8 16",
			">pcm:ichannels",	"1 2",

			"<pcm:ofreq",		"",
			"<pcm:obps",		"8 16",
			"<pcm:ochannels",	"1 2",
			NULL, };
		return arr;
	} 
	va_end(ap);
	return ac;
}

int pcm_codec_process(int type, codec_way_t way, stream_buffer_t *input, stream_buffer_t *output, void *data) {
#if 0
static void pcm_recode(const char *in, int ibps, int ich, char *out, int obps, int och)
{
	int l, r;

	if (ibps == 8) {
		if (ich == 1)
			l = r = in[0];
		else {
			l = in[0];
			r = in[1];
		}

		l *= 256;
		r *= 256;
	} else {
		if (ich == 1)
			l = r = ((short int*) in)[0];
		else {
			l = ((short int*) in)[0];
			r = ((short int*) in)[1];
		}
	}

	if (obps == 8) {
		if (och == 1)
			out[0] = (l + r) / 512;
		else {
			out[0] = l / 256;
			out[1] = r / 256;
		}
	} else {
		if (och == 1)
			((short int*) out)[0] = (l + r) / 2;
		else {
			((short int*) out)[0] = l;
			((short int*) out)[1] = r;
		}
	}
}

static QUERY(pcm_codec_process)
{
	char **p_cid = va_arg(ap, char**), *cid = *p_cid;
	char **p_in = va_arg(ap, char**), *in = *p_in;
	int *p_inlen = va_arg(ap, int*), inlen = *p_inlen;
	char **p_out = va_arg(ap, char**), *out = *p_out;
	int *p_outlen = va_arg(ap, int*), outlen = *p_outlen;
	pcm_codec_t *c;
	int i, outpos, inchunklen, inchunks, outchunklen, outchunks;
	
	if (!(c = pcm_codec_find(cid)))
		return 0;

	inchunklen = (c->ibps / 8) * c->ich;
	inchunks = inlen / inchunklen;
	outchunklen = (c->obps / 8) * c->och;
	outchunks = (int) ((double) c->ofreq / (double) c->ifreq * (double) inchunks);

	outpos = outlen;
	outlen += inchunks * outchunklen;
	out = xrealloc(out, outlen);

	for (i = 0; i < outchunks; i++) {	
		int j = (int) ((double) i / (double) outchunks * (double) inchunks);

		pcm_recode(in + j * inchunklen, c->ibps, c->ich, out + outpos + i * outchunklen, c->obps, c->och);
	}
	
	/* przesuñ pozosta³o¶æ na pocz±tek bufora i go zmniejsz */
	if (inlen > inchunks * inchunklen) {
		memmove(in, in + inchunks * inchunklen, inlen - inchunks * inchunklen);
		inlen -= inchunks * inchunklen;
		in = xrealloc(in, inlen);
	} else {
		xfree(in);
		in = NULL;
		inlen = 0;
	}

	/* zwróæ wyniki */
	*p_in = in;
	*p_inlen = inlen;
	*p_out = out;
	*p_outlen = outlen;

	return 0;
}
#endif
	return -1;
}

CODEC_RECODE(pcm_codec_code) {
	return pcm_codec_process(type, CODEC_CODE, input, output, data);
}

CODEC_RECODE(pcm_codec_decode) {
	return pcm_codec_process(type, CODEC_DECODE, input, output, data);
}

int pcm_plugin_init(int prio)
{
	plugin_register(&pcm_plugin, prio);
	codec_register(&pcm_codec);
	return 0;
}

static int pcm_plugin_destroy()
{
	codec_unregister(&pcm_codec);
	plugin_unregister(&pcm_plugin);
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
