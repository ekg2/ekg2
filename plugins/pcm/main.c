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

#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/xmalloc.h>

PLUGIN_DEFINE(pcm, PLUGIN_CODEC, NULL);

typedef struct {
	int id;			/* numer instancji codeca */

	int ifreq, ofreq;	/* czêstotliwo¶æ */
	int ibps, obps;		/* bps */
	int ich, och;		/* ilo¶æ kana³ów */
} pcm_codec_t;

static int pcm_codec_id = 1;
static list_t pcm_codecs = NULL;

static pcm_codec_t *pcm_codec_find(const char *cid)
{
	list_t l;
	int id;

	if (xstrncasecmp(cid, "pcm:", 4) || !(id = atoi(cid + 4)))
		return NULL;

	for (l = pcm_codecs; l; l = l->next) {
		pcm_codec_t *c = l->data;

		if (c->id == id)
			return c;
	}

	return NULL;
}

static int pcm_codec_capabilities(void *data, va_list ap)
{
	char **p_caps = va_arg(ap, char**);

	xfree(*p_caps);

	*p_caps = xstrdup(
	"pcm:*,8,1-pcm:*,8,2 pcm:*,8,1-pcm:*,16,1 pcm:*,8,1-pcm:*,16,2 "
	"pcm:*,8,2-pcm:*,8,1 pcm:*,8,2-pcm:*,16,1 pcm:*,8,2-pcm:*,16,2 "
	"pcm:*,16,1-pcm:*,16,2 pcm:*,16,1-pcm:*,8,1 pcm:*,16,1-pcm:*,8,2 "
	"pcm:*,16,2-pcm:*,16,1 pcm:*,16,2-pcm:*,8,1 pcm:*,16,2-pcm:*,8,2 ");

	return 0;
}

static int pcm_codec_init(void *data, va_list ap)
{
	char **p_from = va_arg(ap, char**), *from = *p_from;
	char **p_to = va_arg(ap, char**), *to = *p_to;
	char **p_cid = va_arg(ap, char**);
	char **ato, **afrom;
	pcm_codec_t c;

	/* je¶li ju¿ inny codec siê za to zabra³, zapomnij */
	if (*p_cid)
		return 0;

	if (xstrncasecmp(from, "pcm:", 4) || xstrncasecmp(to, "pcm:", 4))
		return 0;

	afrom = array_make(from + 4, ",", 0, 0, 0);
	ato = array_make(to + 4, ",", 0, 0, 0);

	memset(&c, 0, sizeof(c));
	c.id = pcm_codec_id;
	c.ifreq = atoi(afrom[0]);
	c.ibps = atoi(afrom[1]);
	c.ich = atoi(afrom[2]);
	c.ofreq = atoi(ato[0]);
	c.obps = atoi(ato[1]);
	c.och = atoi(ato[2]);

	if ((c.ibps != 8 && c.ibps != 16) || (c.obps != 8 && c.obps != 16))
		goto cleanup;

	if (c.ich < 1 || c.ich > 2 || c.och < 1 || c.och > 2)
		goto cleanup;

	if (!c.ifreq || !c.ofreq)
		goto cleanup;

	list_add(&pcm_codecs, &c, sizeof(c));

	xfree(*p_cid);
	*p_cid = saprintf("pcm:%d", pcm_codec_id++);

cleanup:
	array_free(afrom);
	array_free(ato);

	return 0;
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

static int pcm_codec_process(void *data, va_list ap)
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

static int pcm_codec_destroy(void *data, va_list ap)
{
	char **p_cid = va_arg(ap, char**), *cid = *p_cid;
	pcm_codec_t *c;

	if (!(c = pcm_codec_find(cid)))
		return 0;

	list_remove(&pcm_codecs, c, 1);

	return 0;
}

int pcm_plugin_init(int prio)
{
	plugin_register(&pcm_plugin, prio);

	query_connect(&pcm_plugin, "codec-capabilities", pcm_codec_capabilities, NULL);
	query_connect(&pcm_plugin, "codec-init", pcm_codec_init, NULL);
	query_connect(&pcm_plugin, "codec-destroy", pcm_codec_destroy, NULL);
	query_connect(&pcm_plugin, "codec-process", pcm_codec_process, NULL);

	return 0;
}

static int pcm_plugin_destroy()
{
	plugin_unregister(&pcm_plugin);
	
	list_destroy(pcm_codecs, 1);

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
