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
	char *from, *to;
	int msgsm;	/* > 0 je¶li mamy do czynienia z msgsm */

	gsm codec;	/* w³a¶ciwa struktura libgsm */
} gsm_private_t;

CODEC_CONTROL(gsm_codec_control) {
	va_list ap;

	if (type == AUDIO_CONTROL_INIT && aco) {
		gsm_private_t *priv = aco->private;
		char **inpque = NULL, **outque = NULL, **tmp;	/* we create array with vals... (XXX, to query only once.) */
		audio_io_t *inp, *out;
		codec_way_t cway = -1;

		int value = 1;
		gsm codec;

		va_start(ap, aco);
		inp     = va_arg(ap, audio_io_t *);
		out	= va_arg(ap, audio_io_t *);
		va_end(ap);
	/* ;) */
		inp->a->control_handler(AUDIO_CONTROL_SET, AUDIO_READ, inp, "__codec", "gsm", NULL);
		out->a->control_handler(AUDIO_CONTROL_SET, AUDIO_WRITE, out, "__codec", "gsm", NULL);

	/* QUERY FOR I/O if we don't have.. */
		/* CACHE QUERY */
	#define QUERY_INPUT_ADD(attr, val) if (!val) { array_add(&inpque, attr); array_add(&inpque, (char *) &val);	}
	#define QUERY_OUTPUT_ADD(attr, val) if (!val) { array_add(&outque, attr); array_add(&outque, (char *) &val);	}

		QUERY_INPUT_ADD("format", priv->from);
		QUERY_OUTPUT_ADD("format", priv->to);

		/* EXECUTE QUERIES */
		if ((tmp = inpque)) {
			while (*tmp) { inp->a->control_handler(AUDIO_CONTROL_GET, AUDIO_READ, inp, tmp[0], tmp[1]); tmp++; tmp++; }
		}
		if ((tmp = outque)) {
			while (*tmp) { out->a->control_handler(AUDIO_CONTROL_GET, AUDIO_WRITE, out, tmp[0], tmp[1]); tmp++; tmp++; }
		}
		xfree(inpque); xfree(outque);
	
		debug("[gsm_codec_control] INIT (INP: 0x%x, 0x%x OUT: 0x%x, 0x%x) \n", inp, inpque, out, outque, 0);
	/* CHECK ALL ATTS: */
		if ((!xstrcmp(priv->from, "pcm") || !xstrcmp(priv->from, "raw")) && !xstrcmp(priv->to, "gsm"))	cway = CODEC_CODE;
		if ((!xstrcmp(priv->from, "gsm")) && (!xstrcmp(priv->to, "pcm") || !xstrcmp(priv->to, "raw")))	cway = CODEC_DECODE;

		if (cway == -1) {
			debug("NEITHER CODEING, NEIHER DECODING ? WHOA THERE... (from: %s to:%s)\n", priv->from, priv->to);
			return NULL;
		}
	/* INIT CODEC */
		if (!(codec = gsm_create())) {
			debug("gsm_create() fails\n");
			return NULL;
		}
		gsm_option(codec, GSM_OPT_FAST, &value);
		if (way == CODEC_DECODE) gsm_option(codec, GSM_OPT_LTP_CUT, &value);

		if (priv->msgsm) 
			gsm_option(codec, GSM_OPT_WAV49, &value);

		priv->codec	= codec;
		aco->way	= cway;

	/* return 1 - succ ; 0 - failed*/
		return (void *) 1;
	} else if (type == AUDIO_CONTROL_SET && !aco) {			/* gsm_codec_init() */
		char *attr;
		const char *from = NULL, *to = NULL;
		int with_ms = 0;

		gsm_private_t *priv;

		va_start(ap, aco);
		while ((attr = va_arg(ap, char *))) {
			char *val = va_arg(ap, char *);
			debug("[gsm_codec_control] attr: %s value: %s\n", attr, val);
			if (!xstrcmp(attr, "from"))				from	= val;
			else if (!xstrcmp(attr, "to"))				to	= val;
			else if (!xstrcmp(attr, "with-ms") && atoi(val)) 	with_ms = 1;
			/* XXX birate, channels */
		}
		va_end(ap); 

		priv		= xmalloc(sizeof(gsm_private_t));
		priv->msgsm	= with_ms;
		priv->from	= xstrdup(from);
		priv->to	= xstrdup(to);

		aco		= xmalloc(sizeof(audio_codec_t));
		aco->c		= &gsm_codec;
		aco->private	= priv;
	} else if (type == AUDIO_CONTROL_DEINIT && aco) {		/* gsm_codec_destroy() */
		gsm_private_t *priv = priv = aco->private;

		if (priv && priv->codec) gsm_destroy(priv->codec);
		xfree(priv);
		aco = NULL;
	} else if (type == AUDIO_CONTROL_HELP) {
		static char *arr[] = { 
			"-gsm",			"",
			"-gsm:with-ms",		"0 1",

			"-gsm:birate",		"8000",
			"-gsm:sample",		"16",
			"-gsm:channels",	"1",

			"<gsm:from",		"pcm raw",
			"<gsm:to",		"gsm",

			">gsm:from",		"gsm",
			">gsm:to",		"pcm raw",
			NULL, }; 
		return arr;
	}
	return aco;
}

/* way: 0 - code ; 1 - decode */
int gsm_codec_process(int type, codec_way_t way, string_t input, string_t output, void *data) {
	gsm_private_t *c = data;
	int inpos = 0;

	if (type)			return 0;
	if (!c || !input || !output) 	return -1;
	if (!input->str || !input->len)	return 0;	 /* we have nothing to code? */

	for (;;) {
		int inchunklen, outchunklen;
		char *out;
		
		if (way == CODEC_CODE) {
			inchunklen = 320;
			outchunklen = (c->msgsm == 1) ? 32 : 33;
		} else if (way == CODEC_DECODE) {
			inchunklen = (c->msgsm == 2) ? 32 : 33;
			outchunklen = 320;
		} else	return -1;	/* neither code neither decode? wtf? */

		if ((input->len - inpos) < inchunklen)
			break;

		out = xmalloc(outchunklen);

		if (way == CODEC_CODE)		gsm_encode(c->codec, (gsm_signal *) (input->str + inpos), out);
		else if (way == CODEC_DECODE) 	gsm_decode(c->codec, input->str + inpos, (gsm_signal *) out);

		string_append_raw(output, out, outchunklen);
		xfree(out);

		if (c->msgsm == 1)	c->msgsm = 2;
		else if (c->msgsm == 2) c->msgsm = 1;

		inpos += inchunklen;
	}
	return inpos;
}

CODEC_RECODE(gsm_codec_code) {
	return gsm_codec_process(type, CODEC_CODE, input, output, data);
}

CODEC_RECODE(gsm_codec_decode) {
	return gsm_codec_process(type, CODEC_DECODE, input, output, data);
}

EXPORT int gsm_plugin_init(int prio)
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
