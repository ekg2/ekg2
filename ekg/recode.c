#include "ekg2-config.h"

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


/* NOTES/THINK/BUGS:
 * 	- do we need any #define? 
 * 	- if we stop using ekg_convert_string_init() in plugins this file could be smaller.
 * 	- don't use gg_*() funcs, always use iconv? lite iconv in compat/ ?
 * 	- create:
 * 		static struct ekg_converter same_enc;
 * 		
 * 		we should know if iconv_open() failed, or we have good console_charset..
 * 		give info to user, if this first happen.
 *
 * 	- we should also reinit encodings, if user changed console_charset.
 * 	- implement ekg_any_to_locale(), ekg_locale_to_any()
 *
 * 	- Check if this code works OK.
 */

#include <errno.h>
#include <string.h>

#ifdef HAVE_ICONV
#	include <iconv.h>
#endif

#include "commands.h"
#include "dynstuff.h"
#include "dynstuff_inline.h"
#include "recode.h"
#include "stuff.h"
#include "windows.h"
#include "xmalloc.h"

#define EKG_ICONV_BAD (void*) -1

/* some code based on libiconv utf8_mbtowc() && utf8_wctomb() from utf8.h under LGPL-2.1 */

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
					size_t ibl1 = xstrlen (*t);
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
				int n = xstrlen (outrepl);
				if (n <= obl)
				{
					memcpy (ob, outrepl, n);
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

/*
 * Convert a string
 * Used in rfc2047.c and rfc2231.c
 *
 * Broken for use within EKG2 (passing iconv_t instead of from/to)
 */

static inline string_t mutt_convert_string (string_t s, iconv_t cd, int is_utf)
{
	string_t ret;
	char *repls[] = { "\357\277\275", "?", 0 };
		/* we can assume that both from and to aren't NULL in EKG2,
		 * and cd is NULL in case of error, not -1 */
	if (cd) {
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

		ib = s->str;
		ibl = s->len + 1;
		obl = 16 * ibl;
		ob = buf = xmalloc (obl + 1);

		mutt_iconv (cd, &ib, &ibl, &ob, &obl, inrepls, outrepl);

		ret = string_init(NULL);
		string_append_raw(ret, buf, ob - buf);

		xfree(buf);

		return ret;
	}
	return NULL;
}

/* End of code taken from mutt. */
#endif /*HAVE_ICONV*/

#ifdef HAVE_ICONV
/**
 * struct ekg_converter
 *
 * Used internally by EKG2, contains information about one initialized character converter.
 */
struct ekg_converter {
	struct ekg_converter *next;

	iconv_t		cd;		/**< Magic thing given to iconv, always not NULL (else we won't alloc struct) */
	iconv_t		rev;		/**< Reverse conversion thing, can be NULL */
	char		*from;		/**< Input encoding (duped), always not NULL (even on console_charset) */
	char		*to;		/**< Output encoding (duped), always not NULL (even on console_charset) */
	int		used;		/**< Use counter - incr on _init(), decr on _destroy(), free if 0 */
	int		rev_used;	/**< Like above, but for rev; if !rev, value undefined */
	int		is_utf;		/**< Used internally for mutt_convert_string() */
};

static struct ekg_converter *ekg_converters = NULL;	/**< list for internal use of ekg_convert_string_*() */

static LIST_FREE_ITEM(list_ekg_converter_free, struct ekg_converter *) { xfree(data->from); xfree(data->to); }
DYNSTUFF_LIST_DECLARE(ekg_converters, struct ekg_converter, list_ekg_converter_free,
	static __DYNSTUFF_LIST_ADD,		/* ekg_converters_add() */
	static __DYNSTUFF_LIST_REMOVE_ITER,	/* ekg_converters_removei() */
	__DYNSTUFF_NODESTROY)			/* XXX? */

#endif

/**
 * ekg_convert_string_init()
 *
 * Initialize string conversion thing for two given charsets.
 *
 * @param from		- input encoding (will be duped; if NULL, console_charset will be assumed).
 * @param to		- output encoding (will be duped; if NULL, console_charset will be assumed).
 * @param rev		- pointer to assign reverse conversion into; if NULL, no reverse converter will be initialized.
 * 
 * @return	Pointer that should be passed to other ekg_convert_string_*(), even if it's NULL.
 *
 * @sa ekg_convert_string_destroy()	- deinits charset conversion.
 * @sa ekg_convert_string_p()		- main charset conversion function.
 */
void *ekg_convert_string_init(const char *from, const char *to, void **rev) {
#ifdef HAVE_ICONV
	struct ekg_converter *p;

	if (!from)
		from	= config_console_charset;
	if (!to)
		to	= config_console_charset;
	if (!xstrcasecmp(from, to)) { /* if they're the same */
		if (rev)
			*rev = NULL;
		return NULL;
	}

		/* maybe we've already got some converter for this charsets */
	for (p = ekg_converters; p; p = p->next) {
		if (!xstrcasecmp(from, p->from) && !xstrcasecmp(to, p->to)) {
			p->used++;
			if (rev) {
				if (!p->rev) { /* init rev */
					p->rev = iconv_open(from, to);
					if (p->rev == (iconv_t)-1) /* we don't want -1 */
						p->rev = NULL;
					else
						p->rev_used = 1;
				} else
					p->rev_used++;
				*rev = p->rev;
			}
			return p->cd;
		} else if (!xstrcasecmp(from, p->to) && !xstrcasecmp(to, p->from)) {
				/* we've got reverse thing */
			if (rev) { /* our rev means its forw */
				p->used++;
				*rev = p->cd;
			}
			if (!p->rev) {
				p->rev = iconv_open(to, from);
				if (p->rev == (iconv_t)-1)
					p->rev = NULL;
				else
					p->rev_used = 1;
			} else
				p->rev_used++;
			return p->rev;
		}
	}

	{
		iconv_t cd, rcd = NULL;

		if ((cd = iconv_open(to, from)) == (iconv_t)-1)
			cd = NULL;
		if (rev) {
			if ((rcd = iconv_open(from, to)) == (iconv_t)-1)
				rcd = NULL;
			*rev = rcd;
		}
			
		if (cd || rcd) { /* don't init struct if both are NULL */
			struct ekg_converter *c	= xmalloc(sizeof(struct ekg_converter));

				/* if cd is NULL, we reverse everything */
			c->cd	= (cd ? cd		: rcd);
			c->rev	= (cd ? rcd		: cd);
			c->from	= (cd ? xstrdup(from)	: xstrdup(to));
			c->to	= (cd ? xstrdup(to)	: xstrdup(from));
			c->used		= 1;
			c->rev_used	= (cd && rcd ? 1 : 0);
				/* for mutt_convert_string() */
			if (!xstrcasecmp(c->to, "UTF-8"))
				c->is_utf = 2;
			else if (!xstrcasecmp(c->from, "UTF-8"))
				c->is_utf = 1;
			ekg_converters_add(c);
		}

		return cd;
	}
#else
	return NULL;
#endif
}

/**
 * ekg_convert_string_destroy()
 *
 * Frees internal data associated with given pointer, and uninitalizes iconv, if it's not needed anymore.
 *
 * @note If 'rev' param was used with ekg_convert_string_init(), this functions must be called two times
 *	- with returned value, and with rev-associated one.
 *
 * @param ptr		- pointer returned by ekg_convert_string_init().
 *
 * @sa ekg_convert_string_init()	- init charset conversion.
 * @sa ekg_convert_string_p()		- main charset conversion function.
 */

void ekg_convert_string_destroy(void *ptr) {
#ifdef HAVE_ICONV
	struct ekg_converter *c;

	if (!ptr) /* we can be called with NULL ptr */
		return;

	for (c = ekg_converters; c; c = c->next) {
		if (c->cd == ptr)
			c->used--;
		else if (c->rev == ptr) /* ptr won't be NULL here */
			c->rev_used--;
		else
			continue; /* we're gonna break */

		if (c->rev && (c->rev_used == 0)) { /* deinit reverse converter */
			iconv_close(c->rev);
			c->rev = NULL;
		}
		if (c->used == 0) { /* deinit forward converter, if not needed */
			iconv_close(c->cd);
			
			if (c->rev) { /* if reverse converter is still used, reverse the struct */
				c->cd	= c->rev;
				c->rev	= NULL; /* rev_used becomes undef */
				c->used = c->rev_used;
				{
					char *tmp	= c->from;
					c->from		= c->to;
					c->to		= tmp;
				}
			} else { /* else, free it */
				(void) ekg_converters_removei(c);
			}
		}
		
		break;
	}
#endif
}

/**
 * ekg_convert_string_p()
 *
 * Converts string to specified encoding, using pointer returned by ekg_convert_string_init().
 * Invalid characters in input will be replaced with question marks.
 *
 * @param ps		- string to be converted (won't be freed).
 * @param ptr		- pointer returned by ekg_convert_string_init().
 *
 * @return	Pointer to allocated result or NULL, if some failure has occured or no conversion
 *			is needed (i.e. resulting string would be same as input).
 *
 * @sa ekg_convert_string_init()	- init charset conversion.
 * @sa ekg_convert_string_destroy()	- deinits charset conversion.
 */

char *ekg_convert_string_p(const char *ps, void *ptr) {
	string_t recod, s = string_init(ps);
	char *r = NULL;

	if ((recod = ekg_convert_string_t_p(s, ptr))) {
		r = xstrndup(recod->str, recod->len);
		string_free(recod, 1);
	}

	return r;
}

/**
 * ekg_convert_string()
 *
 * Converts string to specified encoding, replacing invalid chars with question marks.
 *
 * @note Deprecated, in favour of ekg_convert_string_p(). Should be used only on single
 *	conversions, where charset pair won't be used again.
 *
 * @param ps		- string to be converted (it won't be freed).
 * @param from		- input encoding (if NULL, console_charset will be assumed).
 * @param to		- output encoding (if NULL, console_charset will be assumed).
 *
 * @return	Pointer to allocated result on success, NULL on failure
 *			or when both encodings are equal.
 *
 * @sa ekg_convert_string_p()	- more optimized version.
 */
char *ekg_convert_string(const char *ps, const char *from, const char *to) {
	char *r;
	void *p;

	if (!ps || !*ps) /* don't even init iconv if we've got NULL string */
		return NULL;

	p = ekg_convert_string_init(from, to, NULL);
	r = ekg_convert_string_p(ps, p);
	ekg_convert_string_destroy(p);

	return r;
}

string_t ekg_convert_string_t_p(string_t s, void *ptr) {
#ifdef HAVE_ICONV
	struct ekg_converter *c;
	int is_utf = 0;

	if (!s || !s->len || !ptr)
		return NULL;

		/* XXX, maybe some faster way? any ideas? */
	for (c = ekg_converters; c; c = c->next) {
		if (c->cd == ptr)
			is_utf = c->is_utf;
		else if (c->rev == ptr)
			is_utf = (c->is_utf == 2 ? 1 : (c->is_utf == 1 ? 2 : 0));
		else
			continue;

		break;
	}

	return mutt_convert_string(s, ptr, is_utf);
#else
	return NULL;
#endif
}

string_t ekg_convert_string_t(string_t s, const char *from, const char *to) {
	string_t r;
	void *p;

	if (!s || !s->len) /* don't even init iconv if we've got NULL string */
		return NULL;

	p = ekg_convert_string_init(from, to, NULL);
	r = ekg_convert_string_t_p(s, p);
	ekg_convert_string_destroy(p);
	return r;
}

static char *ekg_convert_string_p_safe(char *ps, void *ptr) {
	char *out = ekg_convert_string_p(ps, ptr);
	if (out)
		xfree(ps);
	else
		out = ps;
	return out;
}

int ekg_converters_display(int quiet) {
#ifdef HAVE_ICONV
	struct ekg_converter *c;

	for (c = ekg_converters; c; c = c->next) {
		/* cd, rev, from, to, used, rev_used, is_utf */

		printq("iconv_list", c->from, c->to, itoa(c->used), itoa(c->rev_used));
//		printq("iconv_list_bad", c->from, c->to, itoa(c->used), itoa(c->rev_used));

	}
	return 0;
#else
	printq("generic_error", "Sorry, no iconv");
	return -1;
#endif
}

static int ekg_utf8_helper(unsigned char *s, int n, unsigned short *ch) {
	unsigned char c = s[0];

	if (c < 0x80) {
		*ch = c;
		return 1;
	}

	if (c < 0xc2) 
		goto invalid;

	if (c < 0xe0) {
		if (n < 2)
			goto invalid;
		if (!((s[1] ^ 0x80) < 0x40))
			goto invalid;
		*ch = ((unsigned short) (c & 0x1f) << 6) | (unsigned short) (s[1] ^ 0x80);
		return 2;
	} 
	
	if (c < 0xf0) {
		if (n < 3)
			goto invalid;
		if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (c >= 0xe1 || s[1] >= 0xa0)))
			goto invalid;
		*ch = ((unsigned short) (c & 0x0f) << 12) | ((unsigned short) (s[1] ^ 0x80) << 6) | (unsigned short) (s[2] ^ 0x80);
		return 3;
	}

invalid:
	*ch = '?';
	return 1;
}

static char *ekg_from_utf8(char *b, const unsigned short *recode_table) {	/* sizeof(recode_table) MUST BE 0x100 ==> 0x80 items */
	unsigned char *buf = (unsigned char *) b;

	char *newbuf;
	int newlen = 0;
	int len;
	int i, j;

	len = strlen(b);

	for (i = 0; i < len; newlen++) {
		unsigned short discard;

		i += ekg_utf8_helper(&buf[i], len - i, &discard);
	}

	newbuf = xmalloc(newlen+1);

	for (i = 0, j = 0; buf[i]; j++) {
		unsigned short znak;
		int k;

		i += ekg_utf8_helper(&buf[i], len - i, &znak);

		if (znak < 0x80) {
			newbuf[j] = znak;
			continue;
		}

		newbuf[j] = '?';

		for (k = 0; k < 0x80; k++) {
			if (recode_table[k] == znak) {
				newbuf[j] = (0x80 | k);
				break;
			}
		}
	}
	newbuf[j] = '\0';

	xfree(b);

	return newbuf;
}

static char *ekg_to_utf8(char *b, const unsigned short *recode_table) {
	unsigned char *buf = (unsigned char *) b;
	char *newbuf;
	int newlen = 0;
	int i, j;

	for (i = 0; buf[i]; i++) {
		unsigned short znak = (buf[i] < 0x80) ? buf[i] : recode_table[buf[i]-0x80];

		if (znak < 0x80)	newlen += 1;
		else if (znak < 0x800)	newlen += 2;
		else			newlen += 3;
	}

	newbuf = xmalloc(newlen+1);

	for (i = 0, j = 0; buf[i]; i++) {
		unsigned short znak = (buf[i] < 0x80) ? buf[i] : recode_table[buf[i]-0x80];
		int count;

		if (znak < 0x80)	count = 1;
		else if (znak < 0x800)	count = 2;
		else			count = 3;

		switch (count) {
			case 3: newbuf[j+2] = 0x80 | (znak & 0x3f); znak = znak >> 6; znak |= 0x800;
			case 2: newbuf[j+1] = 0x80 | (znak & 0x3f); znak = znak >> 6; znak |= 0xc0;
			case 1: newbuf[j] = znak;
		}
		j += count;
	}
	newbuf[j] = '\0';

	xfree(b);
	return newbuf;
}

/* cp1250 <==> any, use ekg_locale_to_cp() and ekg_cp_to_locale() */

#if (USE_UNICODE || HAVE_GTK)
static const unsigned short table_cp1250[] = {
	0x20ac, '?', 0x201a, '?', 0x201e, 0x2026, 0x2020, 0x2021, 		/* 0x80 -      */
	'?', 0x2030, 0x0160, 0x2039, 0x015a, 0x0164, 0x017d, 0x0179,		/*      - 0x8F */
	'?', 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,		/* 0x90 -      */ 
	'?', 0x2122, 0x0161, 0x203a, 0x015b, 0x0165, 0x017e, 0x017a,		/*      - 0x9F */
	0x00a0, 0x02c7, 0x02d8, 0x0141, 0x00a4, 0x0104, 0x00a6, 0x00a7,		/* 0xA0 -      */
	0x00a8, 0x00a9, 0x015e, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x017b, 	/*      - 0xAF */
	0x00b0, 0x00b1, 0x02db, 0x0142, 0x00b4, 0x00b5, 0x00b6, 0x00b7,		/* 0xB0 -      */
	0x00b8, 0x0105, 0x015f, 0x00bb, 0x013d, 0x02dd, 0x013e, 0x017c, 	/*      - 0xBF */
	0x0154, 0x00c1, 0x00c2, 0x0102, 0x00c4, 0x0139, 0x0106, 0x00c7,		/* 0xC0 -      */
	0x010c, 0x00c9, 0x0118, 0x00cb, 0x011a, 0x00cd, 0x00ce, 0x010e, 	/*      - 0xCF */
	0x0110, 0x0143, 0x0147, 0x00d3, 0x00d4, 0x0150, 0x00d6, 0x00d7,		/* 0xD0 -      */
	0x0158, 0x016e, 0x00da, 0x0170, 0x00dc, 0x00dd, 0x0162, 0x00df, 	/*      - 0xDF */
	0x0155, 0x00e1, 0x00e2, 0x0103, 0x00e4, 0x013a, 0x0107, 0x00e7,		/* 0xE0 -      */
	0x010d, 0x00e9, 0x0119, 0x00eb, 0x011b, 0x00ed, 0x00ee, 0x010f, 	/*      - 0xEF */
	0x0111, 0x0144, 0x0148, 0x00f3, 0x00f4, 0x0151, 0x00f6, 0x00f7,		/* 0xF0 -      */
	0x0159, 0x016f, 0x00fa, 0x0171, 0x00fc, 0x00fd, 0x0163, 0x02d9, 	/*      - 0xFF */
};
#endif

/* 80..9F = ?; here is A0..BF, C0..FF is the same */
static const unsigned char iso_to_cp_table[] = {
	0xa0, 0xa5, 0xa2, 0xa3, 0xa4, 0xbc, 0x8c, 0xa7,
	0xa8, 0x8a, 0xaa, 0x8d, 0x8f, 0xad, 0x8e, 0xaf,
	0xb0, 0xb9, 0xb2, 0xb3, 0xb4, 0xbe, 0x9c, 0xa1,
	0xb8, 0x9a, 0xba, 0x9d, 0x9f, 0xbd, 0x9e, 0xbf,
};

static const unsigned char cp_to_iso_table[] = {
	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',
	 '?',  '?', 0xa9,  '?', 0xa6, 0xab, 0xae, 0xac,
	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',
	 '?',  '?', 0xb9,  '?', 0xb6, 0xbb, 0xbe, 0xbc,
	0xa0, 0xb7, 0xa2, 0xa3, 0xa4, 0xa1,  '?', 0xa7,
	0xa8,  '?', 0xaa,  '?',  '?', 0xad,  '?', 0xaf,
	0xb0,  '?', 0xb2, 0xb3, 0xb4,  '?',  '?',  '?',
	0xb8, 0xb1, 0xba,  '?', 0xa5, 0xbd, 0xb5, 0xbf,
};

void *cp_conv_in = EKG_ICONV_BAD;
void *cp_conv_out = EKG_ICONV_BAD;

static char *gg_cp_to_iso(char *b) {
	unsigned char *buf = (unsigned char *) b;

	while (*buf) {
		if (*buf >= 0x80 && *buf < 0xC0)
			*buf = cp_to_iso_table[*buf - 0x80];

		buf++;
	}
	return b;
}

static char *gg_iso_to_cp(char *b) {
	unsigned char *buf = (unsigned char *) b;

	while (*buf) {
		if (*buf >= 0x80 && *buf < 0xA0)
			*buf = '?';
		else if (*buf >= 0xA0 && *buf < 0xC0)
			*buf = iso_to_cp_table[*buf - 0xA0];

		buf++;
	}
	return b;
}

char *ekg_locale_to_cp(char *buf) {
	if (!buf)
		return NULL;
#if (USE_UNICODE || HAVE_GTK)
	if (config_use_unicode)
		return ekg_from_utf8(buf, table_cp1250);
#endif
	if (cp_conv_out != EKG_ICONV_BAD)
		return ekg_convert_string_p_safe(buf, cp_conv_out);
	return gg_iso_to_cp(buf);		/* XXX, assuimg iso, iso bad? */
}

char *ekg_cp_to_locale(char *buf) {
	if (!buf)
		return NULL;
#if (USE_UNICODE || HAVE_GTK)
	if (config_use_unicode)
		return ekg_to_utf8(buf, table_cp1250);
#endif
	if (cp_conv_in != EKG_ICONV_BAD) 
		return ekg_convert_string_p_safe(buf, cp_conv_in);
	return gg_cp_to_iso(buf);		/* XXX, assuimg iso, is bad? */
}

/* ISO-8859-2 <===> any, use ekg_locale_to_latin2() and ekg_latin2_to_locale() */

#if (USE_UNICODE || HAVE_GTK)
static const unsigned short table_iso_8859_2[] = {
	'?', '?', '?', '?', '?', '?', '?', '?',					/* 0x80 -      */
	'?', '?', '?', '?', '?', '?', '?', '?',					/*      - 0x8F */
	'?', '?', '?', '?', '?', '?', '?', '?',					/* 0x90 -      */
	'?', '?', '?', '?', '?', '?', '?', '?',					/*      - 0x9F */
	0x00a0, 0x0104, 0x02d8, 0x0141, 0x00a4, 0x013d, 0x015a, 0x00a7,		/* 0xA0 -      */
	0x00a8, 0x0160, 0x015e, 0x0164, 0x0179, 0x00ad, 0x017d, 0x017b,		/*      - 0xAF */
	0x00b0, 0x0105, 0x02db, 0x0142, 0x00b4, 0x013e, 0x015b, 0x02c7,		/* 0xB0 -      */
	0x00b8, 0x0161, 0x015f, 0x0165, 0x017a, 0x02dd, 0x017e, 0x017c,		/*      - 0xBF */
	0x0154, 0x00c1, 0x00c2, 0x0102, 0x00c4, 0x0139, 0x0106, 0x00c7,		/* 0xC0 -      */
	0x010c, 0x00c9, 0x0118, 0x00cb, 0x011a, 0x00cd, 0x00ce, 0x010e,		/*      - 0xCF */
	0x0110, 0x0143, 0x0147, 0x00d3, 0x00d4, 0x0150, 0x00d6, 0x00d7,		/* 0xD0 -      */
	0x0158, 0x016e, 0x00da, 0x0170, 0x00dc, 0x00dd, 0x0162, 0x00df,		/*      - 0xDF */
	0x0155, 0x00e1, 0x00e2, 0x0103, 0x00e4, 0x013a, 0x0107, 0x00e7,		/* 0xE0 -      */
	0x010d, 0x00e9, 0x0119, 0x00eb, 0x011b, 0x00ed, 0x00ee, 0x010f,		/*      - 0xEF */
	0x0111, 0x0144, 0x0148, 0x00f3, 0x00f4, 0x0151, 0x00f6, 0x00f7,		/* 0xF0 -      */
	0x0159, 0x016f, 0x00fa, 0x0171, 0x00fc, 0x00fd, 0x0163, 0x02d9		/*      - 0xFF */
};
#endif

void *latin2_conv_in = EKG_ICONV_BAD;
void *latin2_conv_out = EKG_ICONV_BAD;

char *ekg_locale_to_latin2(char *buf) {
	if (!buf)
		return NULL;
#if (USE_UNICODE || HAVE_GTK)
	if (config_use_unicode)
		return ekg_from_utf8(buf, table_iso_8859_2);
#endif
	if (latin2_conv_out != EKG_ICONV_BAD)
		return ekg_convert_string_p_safe(buf, latin2_conv_out);
	/* XXX, warn user. */
	return NULL;
}

char *ekg_latin2_to_locale(char *buf) {
	if (!buf)
		return NULL;
#if (USE_UNICODE || HAVE_GTK)
	if (config_use_unicode)
		return ekg_to_utf8(buf, table_iso_8859_2);
#endif
	if (latin2_conv_in != EKG_ICONV_BAD)
		return ekg_convert_string_p_safe(buf, latin2_conv_in);
	/* XXX, warn user. */
	return NULL;
}

/* UTF-8 <===> any, use ekg_locale_to_utf8() and ekg_utf8_to_locale() */

/* XXX, easier recode in/out to latin2? */

void *utf8_conv_in = EKG_ICONV_BAD;
void *utf8_conv_out = EKG_ICONV_BAD;

char *ekg_locale_to_utf8(char *buf) {
	if (!buf)
		return NULL;

	if (utf8_conv_out != EKG_ICONV_BAD)
		return ekg_convert_string_p_safe(buf, utf8_conv_out);
	/* XXX, warn user. */
	return NULL;
}

char *ekg_utf8_to_locale(char *buf) {
	if (!buf)
		return NULL;
	if (utf8_conv_in != EKG_ICONV_BAD)
		return ekg_convert_string_p_safe(buf, utf8_conv_in);
	/* XXX, warn user. */
	return NULL;
}

/* XXX any <===> any, use ekg_any_to_locale() and ekg_locale_to_any() */

char *ekg_any_to_locale(char *buf, char *inp) {


}

char *ekg_locale_to_any(char *buf, char *inp) {


}

