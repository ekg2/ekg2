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

#include "recode_tables.h"

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

	string_free(recode, 1);
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
	char *(*recode_from_locale)(struct ekg_recoder *, char *);
	char *(*recode_to_locale)(struct ekg_recoder *, char *);
	void *conv_in, *conv_out;
	int in_new_buffer;						/*  set if recode_from_locale() and recode_to_locale() return freshly allocated buffer.
									  unset if return old buffer
									 */
	int is_utf;
};

static struct ekg_recoder cp_recoder;
static struct ekg_recoder iso2_recoder;
static struct ekg_recoder utf8_recoder;
static struct ekg_recoder dummy_recoder;

static void ekg_recode_init(struct ekg_recoder *rec);
static void ekg_recode_deinit(struct ekg_recoder *rec);

void changed_console_charset(const char *name) {
	int all_ok = 1;

	/* reinit, and display */
#define REINIT_RECODER(strukt, name)	\
	do {				\
		int oldcount;		\
		if ((oldcount = strukt.count)) {						\
			/* int wasok = (strukt.recode_from_locale && strukt.recode_to_locale); */	\
			ekg_recode_deinit(&strukt);						\
			ekg_recode_init(&strukt);						\
			strukt.count = oldcount;						\
			if (!strukt.recode_from_locale || !strukt.recode_to_locale)		\
				all_ok = 0;							\
		}										\
	} while (0);

	REINIT_RECODER(iso2_recoder, "ISO-8859-2");
	REINIT_RECODER(cp_recoder, "CP-1250");
	REINIT_RECODER(utf8_recoder, "UTF-8");

	if (!all_ok) {
#ifdef HAVE_ICONV
		// XXX, iconv_fail
		print("generic_error", "ekg2 fail to (re)initialize charset conversion between some encodings. Check %Gconsole_charset%n variable, if it won't help inform ekg2 dev team and/or upgrade iconv");
#else
		print("generic_error", "ekg2 fail to (re)initialize charset conversion between some encodings. Check %Gconsole_charset%n variable, %rNOTE: console_charset other than: ISO-8859-2 and UTF-8 "
			"require iconv, go and install some!");
#endif
	} else if (!in_autoexec && xstrcasecmp(console_charset, config_console_charset)) 
		print("console_charset_bad", console_charset, config_console_charset);
}

int ekg_converters_display(int quiet) {
#ifdef HAVE_ICONV
	struct ekg_converter *c;
#endif

#define DISPLAY_RECODER(strukt, name)	\
	do {				\
		if (strukt.count)	\
			printq( (strukt.recode_from_locale && strukt.recode_to_locale) ? 		\
				"iconv_list" : "iconv_list_bad",					\
				config_console_charset, name, itoa(strukt.count), itoa(strukt.count),"");\
	} while(0);

	DISPLAY_RECODER(iso2_recoder, "ISO-8859-2");
	DISPLAY_RECODER(cp_recoder, "CP-1250");
	DISPLAY_RECODER(utf8_recoder, "UTF-8");

#undef DISPLAY_RECODER

#ifdef HAVE_ICONV
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

static char *ekg_from_utf8(char *b, const unsigned short *recode_table) {	/* sizeof(recode_table) = 0x100 ==> 0x80 items */
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
	return newbuf;
}

static char *ekg_to_utf8(char *b, const unsigned short *recode_table) {		/* sizeof(recode_table) = 0x100 ==> 0x80 items */
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
	return newbuf;
}

static char *ekg_change_encoding(char *b, const unsigned char *recode_table) {	/* sizeof(recode_table) = 0x80 ==> 0x80 items */
	unsigned char *buf = (unsigned char *) b;

	while (*buf) {
		if (*buf >= 0x80)
			*buf = recode_table[*buf - 0x80];

		buf++;
	}
	return b;
}

static struct ekg_recoder *ekg_recode_get(enum ekg_recode_name enc) {
	if (enc == EKG_RECODE_CP) 
		return &cp_recoder;
	if (enc == EKG_RECODE_ISO2)
		return &iso2_recoder;
	if (enc == EKG_RECODE_UTF8)
		return &utf8_recoder;
	return &dummy_recoder;
}

static char *recode_ret(struct ekg_recoder *rec, char *buf) { return buf; }	/* locale => locale */

static char *recode_ansi_helper_from(struct ekg_recoder *rec, char *buf) {	/* locale => encoding */
	return ekg_change_encoding(buf, rec->conv_out);
}

static char *recode_ansi_helper_to(struct ekg_recoder *rec, char *buf) {	/* encoding => locale */
	return ekg_change_encoding(buf, rec->conv_in);
}

static char *recode_utf8_helper_from(struct ekg_recoder *rec, char *buf) {	/* locale /utf-8/ => encoding */
	return ekg_from_utf8(buf, rec->conv_out);
}

static char *recode_utf8_helper_to(struct ekg_recoder *rec, char *buf) {	/* encoding => locale /utf-8/ */
	return ekg_to_utf8(buf, rec->conv_in);
}

#ifdef HAVE_ICONV
static inline char *mutt_convert_string2(char *buf, size_t len, iconv_t cd, int is_utf) {
	char *repls[] = { "\357\277\275", "?", 0 };

	char *ib;
	char *res, *ob;
	size_t ibl, obl;
	char **inrepls = 0;
	char *outrepl = 0;

	if ( is_utf == 2 ) /* to utf */
		outrepl = repls[0]; /* this would be more evil */
	else if ( is_utf == 1 ) /* from utf */
		inrepls = repls;
	else
		outrepl = "?";

	ib = buf;
	ibl = len + 1;
	obl = 16 * ibl;
	ob = res = xmalloc (obl + 1);

	mutt_iconv (cd, &ib, &ibl, &ob, &obl, inrepls, outrepl);
	return res;
}

static char *recode_iconv_helper_from(struct ekg_recoder *rec, char *buf) {	/* locale => iconv */
	iconv(rec->conv_out, NULL, NULL, NULL, NULL);	/* reset iconv */
	return mutt_convert_string2(buf, strlen(buf), rec->conv_out, (rec->is_utf == 2 ? 1 : (rec->is_utf == 1 ? 2 : 0)));
}

static char *recode_iconv_helper_to(struct ekg_recoder *rec, char *buf) {	/* iconv => locale */
	iconv(rec->conv_in, NULL, NULL, NULL, NULL);	/* reset iconv */
	return mutt_convert_string2(buf, strlen(buf), rec->conv_in, rec->is_utf);
}
#endif

static void ekg_recode_init_iconv(struct ekg_recoder *rec, const char *encoding) {
#ifdef HAVE_ICONV
	rec->conv_in	= iconv_open(config_console_charset, encoding);
	rec->conv_out	= iconv_open(encoding, config_console_charset);

	if (rec->conv_in != (iconv_t) -1)
		rec->recode_to_locale = recode_iconv_helper_to;
	if (rec->conv_out != (iconv_t) -1)
		rec->recode_from_locale = recode_iconv_helper_from;

	if (!xstrcasecmp(encoding, "UTF-8"))
		rec->is_utf = 2;
	else if (!xstrcasecmp(config_console_charset, "UTF-8"))
		rec->is_utf = 1;

	rec->in_new_buffer = 1;
#endif
}

static void ekg_recode_init(struct ekg_recoder *rec) {
	if (!config_console_charset) {
		/* init not possible. */

	} else if (rec == &cp_recoder) {
		if (!xstrcasecmp("ISO-8859-2", config_console_charset)) {
			rec->recode_from_locale = recode_ansi_helper_from;
			rec->recode_to_locale	= recode_ansi_helper_to;
			rec->conv_in		= (void *) cp_to_iso_table;
			rec->conv_out 		= (void *) iso_to_cp_table;
			rec->in_new_buffer	= 0;
		} else if (!xstrcasecmp("UTF-8", config_console_charset)) {
			rec->recode_from_locale = recode_utf8_helper_from;
			rec->recode_to_locale   = recode_utf8_helper_to;
			rec->conv_in 		= (void *) table_cp1250;
			rec->conv_out 		= (void *) table_cp1250;
			rec->in_new_buffer	= 1;
		} else {
			ekg_recode_init_iconv(rec, "WINDOWS-1250");
		}

	} else if (rec == &iso2_recoder) {
		if (!xstrcasecmp("ISO-8859-2", config_console_charset)) {
			rec->recode_from_locale = rec->recode_to_locale = recode_ret;
			rec->in_new_buffer	= 0;
		} else if (!xstrcasecmp("UTF-8", config_console_charset)) {
			rec->recode_from_locale = recode_utf8_helper_from;
			rec->recode_to_locale	= recode_utf8_helper_to;
			rec->conv_in 		= (void *) table_iso_8859_2;
			rec->conv_out 		= (void *) table_iso_8859_2;
			rec->in_new_buffer	= 1;
		} else {
			ekg_recode_init_iconv(rec, "ISO-8859-2");
		}
	} else if (rec == &utf8_recoder) {
		if (!xstrcasecmp("ISO-8859-2", config_console_charset)) {
			/* note: reversed latin2 <==> utf8 */
			rec->recode_from_locale = recode_utf8_helper_to;
			rec->recode_to_locale	= recode_utf8_helper_from;
			rec->conv_in 		= (void *) table_iso_8859_2;
			rec->conv_out 		= (void *) table_iso_8859_2;
			rec->in_new_buffer	= 1;
		} else if (!xstrcasecmp("UTF-8", config_console_charset)) {
			rec->recode_from_locale = rec->recode_to_locale = recode_ret;
			rec->in_new_buffer	= 0;
		} else {
			ekg_recode_init_iconv(rec, "UTF-8");
		}
	}

	/* if (!rec->recode_from_locale || !rec->recode_to_locale)
	 * 	warn user
	 */
}

static void ekg_recode_deinit(struct ekg_recoder *rec) {
#ifdef HAVE_ICONV
	if (rec->recode_to_locale == recode_iconv_helper_to)
		iconv_close(rec->conv_in);

	if (rec->recode_from_locale == recode_iconv_helper_from)
		iconv_close(rec->conv_out);
#endif
	memset(rec, 0, sizeof(struct ekg_recoder));
}

void ekg_recode_inc_ref(enum ekg_recode_name enc) {
	struct ekg_recoder *rec = ekg_recode_get(enc);

	if (rec->count == 0)
		ekg_recode_init(rec);
	rec->count++;
}

void ekg_recode_dec_ref(enum ekg_recode_name enc) {
	struct ekg_recoder *rec = ekg_recode_get(enc);

	rec->count--;
	if (rec->count == 0)
		ekg_recode_deinit(rec);
}

char *ekg_recode_from_locale(enum ekg_recode_name enc, char *buf) {
	struct ekg_recoder *rec;
	
	if (!buf)
		return NULL;

	rec = ekg_recode_get(enc);
	if (rec->recode_from_locale) {
		char *res = rec->recode_from_locale(rec, buf);
		if (rec->in_new_buffer)
			xfree(buf);
		return res;
	}
	// warn user.
	return buf;
}

char *ekg_recode_to_locale(enum ekg_recode_name enc, char *buf) {
	struct ekg_recoder *rec;

	if (!buf)
		return NULL;

	rec = ekg_recode_get(enc);
	if (rec->recode_to_locale) {
		char *res = rec->recode_to_locale(rec, buf);
		if (rec->in_new_buffer)
			xfree(buf);
		return res;
	}
	// warn user.
	return buf;
}

char *ekg_recode_from_locale_dup(enum ekg_recode_name enc, const char *buf) {
	struct ekg_recoder *rec;
	
	if (!buf)
		return NULL;

	rec = ekg_recode_get(enc);
	if (rec->recode_from_locale) {
		if (!rec->in_new_buffer)
			buf = xstrdup(buf);
		return rec->recode_from_locale(rec, (char *) buf);
	}
	// warn user.
	return xstrdup(buf);
}

char *ekg_recode_to_locale_dup(enum ekg_recode_name enc, const char *buf) {
	struct ekg_recoder *rec;

	if (!buf)
		return NULL;

	rec = ekg_recode_get(enc);
	if (rec->recode_to_locale) {
		if (!rec->in_new_buffer)
			buf = xstrdup(buf);
		return rec->recode_to_locale(rec, (char *) buf);
	}
	// warn user.
	return xstrdup(buf);
}

const char *ekg_recode_from_locale_use(enum ekg_recode_name enc, const char *buf) {
	struct ekg_recoder *rec;

	if (!buf)
		return NULL;

	rec = ekg_recode_get(enc);
	if (rec->recode_from_locale == recode_ret)
		return buf;

	/* almost like ekg_recode_from_locale_dup(), but don't strdup() @ error */
	if (rec->recode_from_locale) {
		if (!rec->in_new_buffer)
			buf = xstrdup(buf);
		return rec->recode_from_locale(rec, (char *) buf);
	}
	// warn user.
	return buf;
}

const char *ekg_recode_to_locale_use(enum ekg_recode_name enc, const char *buf) {
	struct ekg_recoder *rec;

	if (!buf)
		return NULL;

	rec = ekg_recode_get(enc);
	if (rec->recode_to_locale == recode_ret)
		return buf;

	/* almost like ekg_recode_to_locale_dup(), but don't strdup() @ error */
	if (rec->recode_to_locale) {
		if (!rec->in_new_buffer)
			buf = xstrdup(buf);
		return rec->recode_to_locale(rec, (char *) buf);
	}
	// warn user.
	return buf;
}

/*
 * iso_to_ascii()
 *
 * usuwa polskie litery z tekstu.
 *
 *  - c.
 */
void iso_to_ascii(unsigned char *buf) {
#if (USE_UNICODE || HAVE_GTK)
	if (config_use_unicode) return;
#endif
	if (!buf)
		return;

	while (*buf) {
		if (*buf == (unsigned char)'±') *buf = 'a';
		if (*buf == (unsigned char)'ê') *buf = 'e';
		if (*buf == (unsigned char)'æ') *buf = 'c';
		if (*buf == (unsigned char)'³') *buf = 'l';
		if (*buf == (unsigned char)'ñ') *buf = 'n';
		if (*buf == (unsigned char)'ó') *buf = 'o';
		if (*buf == (unsigned char)'¶') *buf = 's';
		if (*buf == (unsigned char)'¿') *buf = 'z';
		if (*buf == (unsigned char)'¼') *buf = 'z';

		if (*buf == (unsigned char)'¡') *buf = 'A';
		if (*buf == (unsigned char)'Ê') *buf = 'E';
		if (*buf == (unsigned char)'Æ') *buf = 'C';
		if (*buf == (unsigned char)'£') *buf = 'L';
		if (*buf == (unsigned char)'Ñ') *buf = 'N';
		if (*buf == (unsigned char)'Ó') *buf = 'O';
		if (*buf == (unsigned char)'¦') *buf = 'S';
		if (*buf == (unsigned char)'¯') *buf = 'Z';
		if (*buf == (unsigned char)'¬') *buf = 'Z';

		buf++;
	}
}

