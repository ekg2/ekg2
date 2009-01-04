/*
 *  (C) Copyright XXX
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2-remote-config.h"

#include <errno.h>
#include <string.h>

#ifdef HAVE_ICONV
#	include <iconv.h>
#endif

#include "debug.h"
#include "recode.h"
#include "stuff.h"
#include "xmalloc.h"

#define EKG_ICONV_BAD (void*) -1

#ifdef HAVE_ICONV
/* Following two functions shamelessly ripped from mutt-1.4.2i 
 * (http://www.mutt.org, license: GPL)
 * 
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@guug.de>
 * Modified 2004 by Maciek Pasternacki <maciekp@japhy.fnord.org>
 */

/*
 * Like iconv, but keeps going even when the input is invalid
 * If you're supplying inrepls, the source charset should be stateless;
 * if you're supplying an outrepl, the target charset should be.
 */
static inline size_t mutt_iconv (iconv_t cd, char **inbuf, size_t *inbytesleft,
		char **outbuf, size_t *outbytesleft,
		char **inrepls, const char *outrepl)
{
	size_t ret = 0, ret1;
	char *ib = *inbuf;
	size_t ibl = *inbytesleft;
	char *ob = *outbuf;
	size_t obl = *outbytesleft;

	for (;;) {
		ret1 = iconv (cd, &ib, &ibl, &ob, &obl);
		if (ret1 != (size_t)-1)
			ret += ret1;
		if (ibl && obl && errno == EILSEQ) {
			if (inrepls) {
				/* Try replacing the input */
				char **t;
				for (t = inrepls; *t; t++)
				{
					char *ib1 = *t;
					size_t ibl1 = strlen (*t);
					char *ob1 = ob;
					size_t obl1 = obl;
					iconv (cd, &ib1, &ibl1, &ob1, &obl1);
					if (!ibl1) {
						++ib, --ibl;
						ob = ob1, obl = obl1;
						++ret;
						break;
					}
				}
				if (*t)
					continue;
			}
			if (outrepl) {
				/* Try replacing the output */
				int n = strlen(outrepl);
				if (n <= obl)
				{
					memcpy(ob, outrepl, n);
					++ib, --ibl;
					ob += n, obl -= n;
					++ret;
					continue;
				}
			}
		}
		*inbuf = ib, *inbytesleft = ibl;
		*outbuf = ob, *outbytesleft = obl;
		return ret;
	}
}
#endif

static char *mutt_convert_string(const char *ps, iconv_t cd, int is_utf)
{
#ifdef HAVE_ICONV
	char *repls[] = { "\357\277\275", "?", 0 };

	char *ib;
	char *buf, *ob;
	size_t ibl, obl;
	char **inrepls = 0;
	char *outrepl = 0;

	if ( is_utf == 2 ) /* to utf */
		outrepl = repls[0]; /* this would be more evil */
	else if ( is_utf == 1 ) /* from utf */
		inrepls = repls;
	else
		outrepl = "?";

	ib = (char *) ps;
	ibl = strlen(ps) + 1;
	obl = 16 * ibl;
	ob = buf = xmalloc (obl + 1);

	mutt_iconv (cd, &ib, &ibl, &ob, &obl, inrepls, outrepl);

	*ob = '\0';

	buf = (char*)xrealloc((void*)buf, strlen(buf)+1);
	return buf;
/* End of code taken from mutt. */
#else
	return NULL;
#endif /*HAVE_ICONV*/
}

static void *ekg_convert_string_init(const char *from, const char *to, int *utf) {
#ifdef HAVE_ICONV
	iconv_t cd;

	cd = iconv_open(to, from);
	*utf = 0;

	if (cd == EKG_ICONV_BAD)
		return EKG_ICONV_BAD;

	/* for mutt_convert_string() */
	if (!xstrcasecmp(to, "UTF-8"))
		*utf = 2;
	else if (!xstrcasecmp(from, "UTF-8"))
		*utf = 1;

	return cd;
#else
	return EKG_ICONV_BAD;
#endif
}

static inline void ekg_convert_string_destroy(void *ptr) {
#ifdef HAVE_ICONV
	iconv_close(ptr);
#endif
}

static void *remote_conv_in = EKG_ICONV_BAD;
static void *remote_conv_out = EKG_ICONV_BAD;

static int remote_conv_iutf, remote_conv_outf;

EXPORTNOT void remote_recode_destroy(void) {
	if (remote_conv_in != EKG_ICONV_BAD)
		ekg_convert_string_destroy(remote_conv_in);
	if (remote_conv_out != EKG_ICONV_BAD)
		ekg_convert_string_destroy(remote_conv_out);

	remote_conv_in = remote_conv_out = EKG_ICONV_BAD;
}

/* XXX, dorobic reinit */
/* (gdy sie zmienia kodowanie po stronie serwera? swoja droga to i tak raczej nie bedzie dzialac... */
EXPORTNOT void remote_recode_reinit(void) {
	remote_recode_destroy();

	debug("remote_recode_reinit(%s, %s)\n", server_console_charset ? server_console_charset : "-", config_console_charset ? config_console_charset : "-");

	if (server_console_charset && config_console_charset && xstrcasecmp(server_console_charset, config_console_charset)) {
		remote_conv_in = ekg_convert_string_init(server_console_charset, config_console_charset, &remote_conv_iutf);
		remote_conv_out = ekg_convert_string_init(config_console_charset, server_console_charset, &remote_conv_outf);
	}
}

EXPORTNOT char *remote_recode_from(char *buf) {
	if (!buf || !(*buf))
		return buf;

	if (remote_conv_in == EKG_ICONV_BAD)
		return buf;
	else {
		char *newbuf = mutt_convert_string(buf, remote_conv_in, remote_conv_iutf);

		if (!newbuf)
			return buf;

		xfree(buf);
		return newbuf;
	}
}

EXPORTNOT char *remote_recode_to(char *buf) {
	if (!buf || !(*buf))
		return buf;

	if (remote_conv_out == EKG_ICONV_BAD)
		return buf;
	else {
		char *newbuf = mutt_convert_string(buf, remote_conv_out, remote_conv_outf);

		if (!newbuf)
			return buf;

		xfree(buf);
		return newbuf;
	}
}

