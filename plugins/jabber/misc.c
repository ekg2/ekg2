/* $Id$ */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <iconv.h>

#include "ekg2-config.h"

#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/themes.h>
#include <ekg/stuff.h>
#include <ekg/xmalloc.h>
#include <ekg/log.h>

#include "jabber.h"
#include "jabber-ssl.h"

int JABBER_COMMIT_DATA(watch_t *w) {
	if (w) { 
		w->transfer_limit = 0;
		return watch_handle_write(w); 
	}
	return -1;
}

char *jabber_attr(char **atts, const char *att)
{
	int i;

	if (!atts)
		return NULL;

	for (i = 0; atts[i]; i += 2)
		if (!xstrcmp(atts[i], att))
			return atts[i + 1];
		
	return NULL;
}

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
static size_t mutt_iconv (iconv_t cd, char **inbuf, size_t *inbytesleft,
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
 */

char *mutt_convert_string (char *ps, const char *from, const char *to)
{
	iconv_t cd;
	char *repls[] = { "\357\277\275", "?", 0 };
	char *s = ps;

	if (!s || !*s)
		return NULL;

	if (to && from && (cd = iconv_open (to, from)) != (iconv_t)-1) {
		int len;
		char *ib;
		char *buf, *ob;
		size_t ibl, obl;
		char **inrepls = 0;
		char *outrepl = 0;

		if ( !xstrcasecmp(to, "utf-8") )
			outrepl = "\357\277\275";
		else if ( !xstrcasecmp(from, "utf-8"))
			inrepls = repls;
		else
			outrepl = "?";

		len = xstrlen (s);
		ib = s, ibl = len + 1;
		obl = 16 * ibl;
		ob = buf = xmalloc (obl + 1);

		mutt_iconv (cd, &ib, &ibl, &ob, &obl, inrepls, outrepl);
		iconv_close (cd);

		*ob = '\0';

		buf = (char*)xrealloc((void*)buf, xstrlen(buf)+1);
		return buf;
	}
	return NULL;
}

/* End of code taken from mutt. */


/*
 * jabber_escape()
 *
 * zamienia tekst w iso-8859-2 na tekst w utf-8 z eskejpniêtymi znakami,
 * które bêd± przeszkadza³y XML-owi: ' " & < >.
 *
 *  - text
 *
 * zaalokowany bufor
 */

char *jabber_escape(const char *text)
{
	unsigned char *utftext;
	char *res;
	if (config_use_unicode)
		return xml_escape(text);
	if (!text)
		return NULL;
	if ( !(utftext = mutt_convert_string((char *)text, config_console_charset, "utf-8")) )
		return NULL;
	res = xml_escape(utftext);
        xfree(utftext);
	return res;
}

/*
 * jabber_unescape()
 *
 * zamienia tekst w utf-8 na iso-8859-2. xmlowe znaczki s± ju¿ zamieniane
 * przez expat, wiêc nimi siê nie zajmujemy.
 *
 *  - text
 *
 * zaalokowany bufor
 */
char *jabber_unescape(const char *text)
{
	if (!text)
		return NULL;
	if (config_use_unicode)
		return xstrdup(text);

	return mutt_convert_string((char *)text, "utf-8", config_console_charset);
}

/* tlen_encode() & tlen_decode() ripped from libtlen. XXX, try to rewrite some code */

/* tlen_encode() - Koduje tekst przy pomocy urlencode + rekoduje charset na iso-8859-2 */
char *tlen_encode(const char *what) {
	const unsigned char *s;
	unsigned char *ptr, *str;
	char *text = NULL;

	if (!what) return NULL;

	if (xstrcmp(config_console_charset, "ISO-8859-2"))
		s = text = mutt_convert_string((char *) what, config_console_charset, "ISO-8859-2");
	else	s = what;

	str = ptr = (unsigned char *) xcalloc(3 * xstrlen(s) + 1, 1);
	while (*s) {
		if (*s == ' ')
			*ptr++ = '+';
		else if ((*s < '0' && *s != '-' && *s != '.')
			 || (*s < 'A' && *s > '9') || (*s > 'Z' && *s < 'a' && *s != '_')
			 || (*s > 'z')) {
			sprintf(ptr, "%%%02X", *s);
			ptr += 3;
		} else
			*ptr++ = *s;
		s++;
	}
	xfree(text);
	return str;
}

/* tlen_decode() - Dekoduje tekst przy pomocy urldecode + rekoduje charset na aktualny.. */
char *tlen_decode(const char *what) {
	unsigned char *dest, *data, *retval;
	char *text;

	if (!what) return NULL;
	dest = data = retval = xstrdup(what);
	while (*data) {
		if (*data == '+')
			*dest++ = ' ';
		else if ((*data == '%') && isxdigit((int)data[1]) && isxdigit((int)data[2])) {
			int     code;
			sscanf(data + 1, "%2x", &code);
			if (code != '\r')
				*dest++ = (unsigned char)code;
			data += 2;
		} else
			*dest++ = *data;
		data++;
	}
	*dest = '\0';
	if (!xstrcmp(config_console_charset, "ISO-8859-2")) return retval;

	text = mutt_convert_string((char *) retval, "ISO-8859-2", config_console_charset);
	xfree(retval);
	return text;
}

/*
 * jabber_handle_write()
 *
 * obs³uga mo¿liwo¶ci zapisu do socketa. wypluwa z bufora ile siê da
 * i je¶li co¶ jeszcze zostanie, ponawia próbê.
 */
#ifdef JABBER_HAVE_SSL
WATCHER_LINE(jabber_handle_write) /* tylko dla ssla. dla zwyklych polaczen jest watch_handle_write() */
{
	jabber_private_t *j = data;
	int res;

	if (type) {
		/* XXX, do we need to make jabber_handle_disconnect() or smth simillar? */
		j->send_watch = NULL;
		return 0;
	}
	res = SSL_SEND(j->ssl_session, watch);

#ifdef JABBER_HAVE_OPENSSL		/* OpenSSL */
	if ((res == 0 && SSL_get_error(j->ssl_session, res) == SSL_ERROR_ZERO_RETURN)); /* connection shut down cleanly */
	else if (res < 0) 
		res = SSL_get_error(j->ssl_session, res);
/* XXX, When an SSL_write() operation has to be repeated because of SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE, it must be repeated with the same arguments. */
#endif

	if (SSL_E_AGAIN(res)) {
		ekg_yield_cpu();
		return 0;
	}

	if (res < 0) {
		print("generic_error", SSL_ERROR(res));
	}

	return res;
}
#endif

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
