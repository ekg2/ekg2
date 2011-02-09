/*
 *  (C) Copyright 2009-2010 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *			Wies³aw Ochmiñski <wiechu@wiechu.com>
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

#include "ekg2-config.h"

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
	string_t s = string_init(ps);
	string_t recod;

	if ((recod = ekg_convert_string_t_p(s, ptr)))
		return string_free(recod, 0);

	string_free(recod, 1);
	return NULL;
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

struct ekg_recoder {
	int count;
	char *(*recode_from_locale)(struct ekg_recoder *, int, char *);
	char *(*recode_to_locale)(struct ekg_recoder *, int, char *);
	void *conv_in, *conv_out;
	int is_utf;
};

void changed_console_charset(const char *name) {
	if (1) {
		/* XXX: replace with some recoding test? */
	} else if (!in_autoexec && xstrcasecmp(console_charset, config_console_charset)) 
		print("console_charset_bad", console_charset, config_console_charset);
}

int ekg_converters_display(int quiet) {
#ifdef HAVE_ICONV
	struct ekg_converter *c;

	for (c = ekg_converters; c; c = c->next) {
		/* cd, rev, from, to, used, rev_used, is_utf */

		printq("iconv_list", c->from, c->to, ekg_itoa(c->used), ekg_itoa(c->rev_used));
//		printq("iconv_list_bad", c->from, c->to, ekg_itoa(c->used), ekg_itoa(c->rev_used));

	}
	return 0;
#else
	printq("generic_error", "Sorry, no iconv");
	return -1;
#endif
}

void ekg_recode_inc_ref(const gchar *enc) {
}

void ekg_recode_dec_ref(const gchar *enc) {
}

char *ekg_recode_from_locale(const gchar *enc, char *buf) {
	gchar *res = ekg_recode_from_locale_use(enc, buf);
	if (res != buf)
		g_free(buf);
	return res;
}

char *ekg_recode_to_locale(const gchar *enc, char *buf) {
	gchar *res = ekg_recode_to_locale_use(enc, buf);
	if (res != buf)
		g_free(buf);
	return res;
}

char *ekg_recode_from_locale_dup(const gchar *enc, const char *buf) {
	gchar *res = ekg_recode_from_locale_use(enc, buf);
	return res == buf ? g_strdup(res) : res;
}

char *ekg_recode_to_locale_dup(const gchar *enc, const char *buf) {
	gchar *res = ekg_recode_to_locale_use(enc, buf);
	return res == buf ? g_strdup(res) : res;
}

const char *ekg_recode_from_locale_use(const gchar *enc, const char *buf) {
	gsize written;
	gchar *res;

	if (!buf)
		return NULL;

	res = g_convert_with_fallback(buf, -1, enc, config_console_charset,
			NULL, NULL, &written, NULL);
	return res ? res : g_strdup(buf);
}

const char *ekg_recode_to_locale_use(const gchar *enc, const char *buf) {
	gsize written;
	gchar *res;

	if (!buf)
		return NULL;

	res = g_convert_with_fallback(buf, -1, config_console_charset, enc,
			NULL, NULL, &written, NULL);
	return res ? res : buf;
}
