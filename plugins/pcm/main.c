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
#include <ekg/debug.h>
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
	char *from, *to;	/* form format, to format */
	int ifreq, ofreq;	/* czêstotliwo¶æ */
	int ibps, obps;		/* bps */
	int ich, och;		/* ilo¶æ kana³ów */
} pcm_private_t; 

CODEC_CONTROL(pcm_codec_control) {
	va_list ap;

	if (type == AUDIO_CONTROL_INIT && aco) {
		pcm_private_t *priv = aco->private;
		audio_io_t *inp, *out;
		char **inpque = NULL, **outque = NULL, **tmp;	/* we create array with vals... (XXX, to query only once.) */
		int valid = 1;

		va_start(ap, aco);
		inp     = va_arg(ap, audio_io_t *);
		out	= va_arg(ap, audio_io_t *);
		va_end(ap);
	/* ;) */
		inp->a->control_handler(AUDIO_CONTROL_SET, AUDIO_READ, inp, "__codec", "pcm", NULL);
		out->a->control_handler(AUDIO_CONTROL_SET, AUDIO_WRITE, out, "__codec", "pcm", NULL);

	/* QUERY FOR I/O if we don't have.. */
		/* CACHE QUERY */
	#define QUERY_INPUT_ADD(attr, val) if (!val) { array_add(&inpque, attr); array_add(&inpque, (char *) &val);	}
	#define QUERY_OUTPUT_ADD(attr, val) if (!val) { array_add(&outque, attr); array_add(&outque, (char *) &val);	}

		QUERY_INPUT_ADD("format", priv->from);
		QUERY_INPUT_ADD("freq", priv->ifreq);
		QUERY_INPUT_ADD("sample", priv->ibps);
		QUERY_INPUT_ADD("channels", priv->ich);
	
		QUERY_OUTPUT_ADD("format", priv->to);
		QUERY_OUTPUT_ADD("freq", priv->ofreq);
		QUERY_OUTPUT_ADD("sample", priv->obps);
		QUERY_OUTPUT_ADD("channels", priv->och);
		/* EXECUTE QUERIES */
		if ((tmp = inpque)) {
			while (*tmp) { inp->a->control_handler(AUDIO_CONTROL_GET, AUDIO_READ, inp, tmp[0], tmp[1]); tmp++; tmp++; }
		}
		if ((tmp = outque)) {
			while (*tmp) { out->a->control_handler(AUDIO_CONTROL_GET, AUDIO_WRITE, out, tmp[0], tmp[1]); tmp++; tmp++; }
		}
		xfree(inpque); xfree(outque);
	
	/* CHECK ALL ATTS: */
		debug("[pcm_codec_control] INIT (INP: 0x%x, 0x%x OUT: 0x%x, 0x%x) from: %s to: %s Ifreq: %d Ofreq: %d Ibps: %d Obps: %d Ichannels: %d Ochannels: %d\n",
			inp, inpque, out, outque, 
			priv->from, priv->to, priv->ifreq, priv->ofreq, priv->ibps, priv->obps, priv->ich, priv->och);
	
		if (xstrcmp(priv->from, "pcm") && xstrcmp(priv->from, "raw"))		valid = 0;	/* CHECK INPUT FORMAT */
		if (xstrcmp(priv->to, "pcm") && xstrcmp(priv->to, "raw"))		valid = 0;	/* CHECK OUTPUT FORMAT */

		if (!priv->ifreq || !priv->ofreq) 							valid = 0;	/* CHECK FREQ */
		if ((priv->ibps != 8 && priv->ibps != 16) || (priv->obps != 8 && priv->obps != 16))	valid = 0;	/* CHECK BPS */
		if (priv->ich < 1 || priv->ich > 2 || priv->och < 1 || priv->och > 2)			valid = 0;	/* CHECK CHANNELS */

	/* return valid 	1 - succ ; 0 - failed*/
		return (void *) valid;

	} else if ((type == AUDIO_CONTROL_SET && !aco) || (type == AUDIO_CONTROL_GET && aco)) {	/* pcm_codec_init()  | _get() */
		const char *from = NULL, *to = NULL;
		char *attr;

		pcm_private_t *priv;

		va_start(ap, aco);

		if (type == AUDIO_CONTROL_SET)	priv = xmalloc(sizeof(pcm_private_t));
		else				if (!(priv = aco->private)) return NULL;

		while ((attr = va_arg(ap, char *))) {
			if (type == AUDIO_CONTROL_SET) {
				char *val = va_arg(ap, char *);
				int v = 0;

				debug("[pcm_codec_control] AUDIO_CONTROL_SET attr: %s value: %s\n", attr, val);
				if (!xstrcmp(attr, "from"))		from	= val;
				else if (!xstrcmp(attr, "to"))		to	= val;

				else if (!xstrcmp(attr, "freq"))	{ v = atoi(val); 	priv->ifreq = v;	priv->ofreq = v;	}
				else if (!xstrcmp(attr, "sample"))	{ v = atoi(val);	priv->ibps = v;		priv->obps = v;		}
				else if (!xstrcmp(attr, "channels"))	{ v = atoi(val);	priv->ich = v;		priv->och = v;		}

				else if (!xstrcmp(attr, "ifreq"))	{ v = atoi(val); 	priv->ifreq = v; 				}
				else if (!xstrcmp(attr, "ibps"))	{ v = atoi(val);	priv->ibps = v;					} 
				else if (!xstrcmp(attr, "ichannels"))	{ v = atoi(val);	priv->ich = v;					} 

				else if (!xstrcmp(attr, "ofreq"))	{ v = atoi(val);  				priv->ofreq = v;	}
				else if (!xstrcmp(attr, "obps"))	{ v = atoi(val);				priv->obps = v;		}
				else if (!xstrcmp(attr, "ochannels"))	{ v = atoi(val);				priv->och = v;		}
			} else if (type == AUDIO_CONTROL_GET) {
				char **val = va_arg(ap, char **);

				debug("[pcm_codec_control] AUDIO_CONTROL_GET attr: %s value: 0x%x\n", attr, val);
				if (!xstrcmp(attr, "format"))		*val = xstrdup("pcm");
				else if (way == AUDIO_READ) {
					if (!xstrcmp(attr, "freq"))		*val = xstrdup(itoa(priv->ifreq));
					else if (!xstrcmp(attr, "sample"))	*val = xstrdup(itoa(priv->ibps));
					else if (!xstrcmp(attr, "channels"))	*val = xstrdup(itoa(priv->ich));
				} else if (way == AUDIO_WRITE) {
					if (!xstrcmp(attr, "freq"))		*val = xstrdup(itoa(priv->ofreq));
					else if (!xstrcmp(attr, "sample"))	*val = xstrdup(itoa(priv->obps));
					else if (!xstrcmp(attr, "channels"))	*val = xstrdup(itoa(priv->och));
				} else					*val = NULL;

			}
		}
		va_end(ap);

		debug("[pcm_codec_control] SET from: %s to: %s Ifreq: %d Ofreq: %d Ibps: %d Obps: %d Ichannels: %d Ochannels: %d\n",
			from, to, priv->ifreq, priv->ofreq, priv->ibps, priv->obps, priv->ich, priv->och);
	
		priv->from	= xstrdup(from);
		priv->to	= xstrdup(to);

		aco		= xmalloc(sizeof(audio_codec_t));
		aco->c		= &pcm_codec;
		aco->way	= 0;
		aco->private 	= priv;
	} else if (type == AUDIO_CONTROL_DEINIT) {	/* pcm_codec_destroy() */
		if (aco) 
			xfree(aco->private);
		aco = NULL;
	} else if (type == AUDIO_CONTROL_HELP) {		/* pcm_codec_capabilities() */ 
		static char *arr[] = { 
			">pcm",	"pcm:ifreq pcm:ibps pcm:ichannels",	/* if INPUT we request for:  ifreq, ibps & ichannels */
			"<pcm", "pcm:ofreq pcm:obps pcm:ochannels",	/* if OUTPUT we request for: ofreq, obps & ochannels */

			"-pcm:freq",		"",
			"-pcm:sample",		"8 16",
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
	return aco;
}

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

int pcm_codec_process(int type, codec_way_t way, stream_buffer_t *input, stream_buffer_t *output, void *data) {
	pcm_private_t *c = data;
	int inchunklen = (c->ibps / 8) * c->ich;
	int outchunklen = (c->obps / 8) * c->och;

	int inchunks = input->len / inchunklen;
	int outchunks = (int) ((double) c->ofreq / (double) c->ifreq * (double) inchunks);
	int i;
	char *out;									/* tymczasowy bufor */
	
//	debug("pcm_codec_process() type=%d input: 0x%x inplen: %d output: 0x%x outlen: %d data: 0x%x\n", 
//		type, input, input ? input->len : 0, output, output ? output->len : 0, data);
	if (type) return 0; 
	if (!input || !output) return -1;

	debug("pcm_codec_process() inchunks: %d inchunklen: %d outchunks: %d outchunklen: %d\n", inchunks, inchunklen, outchunks, outchunklen);

	out = xmalloc(outchunklen);
	for (i = 0; i < outchunks; i++) {
		int j = (int) ((double) i / (double) outchunks * (double) inchunks);

		pcm_recode(input->buf + j * inchunklen, c->ibps, c->ich, out, c->obps, c->och);		/* zrekoduj to co mamy zrekodowac */
		stream_buffer_resize(output, out, outchunklen);						/* dopisz */
	}
	xfree(out);									/* zwolnij bufor */
	stream_buffer_resize(input, NULL, -(inchunks * inchunklen));
	return (inchunks * inchunklen);
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
