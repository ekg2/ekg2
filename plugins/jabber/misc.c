/* $Id$ */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <iconv.h>

#include <ekg2-config.h>

#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/xmalloc.h>
#include <ekg/log.h>

#include "jabber.h"

char *jabber_attr(char **atts, const char *att)
{
	int i;

	for (i = 0; atts[i]; i += 2)
		if (!xstrcmp(atts[i], att))
			return atts[i + 1];
		
	return NULL;
}

char *config_jabber_console_charset = NULL;

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
size_t mutt_iconv (iconv_t cd, char **inbuf, size_t *inbytesleft,
		   char **outbuf, size_t *outbytesleft,
		   char **inrepls, const char *outrepl)
{
  size_t ret = 0, ret1;
  char *ib = *inbuf;
  size_t ibl = *inbytesleft;
  char *ob = *outbuf;
  size_t obl = *outbytesleft;

  for (;;)
  {
    ret1 = iconv (cd, &ib, &ibl, &ob, &obl);
    if (ret1 != (size_t)-1)
      ret += ret1;
    if (ibl && obl && errno == EILSEQ)
    {
      if (inrepls)
      {
	/* Try replacing the input */
	char **t;
	for (t = inrepls; *t; t++)
	{
	  char *ib1 = *t;
	  size_t ibl1 = strlen (*t);
	  char *ob1 = ob;
	  size_t obl1 = obl;
	  iconv (cd, &ib1, &ibl1, &ob1, &obl1);
	  if (!ibl1)
	  {
	    ++ib, --ibl;
	    ob = ob1, obl = obl1;
	    ++ret;
	    break;
	  }
	}
	if (*t)
	  continue;
      }
      if (outrepl)
      {
	/* Try replacing the output */
	int n = strlen (outrepl);
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
    return 0;

  if (to && from && (cd = iconv_open (to, from)) != (iconv_t)-1)
  {
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
      
    len = strlen (s);
    ib = s, ibl = len + 1;
    obl = 16 * ibl;
    ob = buf = xmalloc (obl + 1);
    
    mutt_iconv (cd, &ib, &ibl, &ob, &obl, inrepls, outrepl);
    iconv_close (cd);

    *ob = '\0';

    buf = (char*)xrealloc((void*)buf, strlen(buf)+1);
    return buf;
  }
  else
    return 0;
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
	unsigned char *res, *utftext;

	if (!text)
		return NULL;
	if ( !(utftext = mutt_convert_string((char *)text, config_jabber_console_charset, "utf-8")) )
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

	return mutt_convert_string((char *)text, "utf-8", config_jabber_console_charset);
}

/*
 * jabber_handle_write()
 *
 * obs³uga mo¿liwo¶ci zapisu do socketa. wypluwa z bufora ile siê da
 * i je¶li co¶ jeszcze zostanie, ponawia próbê.
 */
WATCHER(jabber_handle_write)
{
	jabber_private_t *j = data;
	int res;

#ifdef HAVE_GNUTLS
	if (j->using_ssl && j->ssl_session) {

		res = gnutls_record_send(j->ssl_session, j->obuf, j->obuf_len);	

		if ((res == GNUTLS_E_INTERRUPTED) || (res == GNUTLS_E_AGAIN)) {
			ekg_yield_cpu();

			goto notyet;
		}

		if (res < 0) {
			print("generic_error", gnutls_strerror(res));
			return;
		}
			
	} else
#endif
		res = write(j->fd, j->obuf, j->obuf_len);

	if (res == -1) {
		debug("[jabber] write() failed: %s\n", strerror(errno));
		xfree(j->obuf);
		j->obuf = NULL;
		j->obuf_len = 0;
		return;
	}

	if (res == j->obuf_len) {
		debug("[jabber] output buffer empty\n");
		xfree(j->obuf);
		j->obuf = NULL;
		j->obuf_len = 0;
		return;
	}
	
	memmove(j->obuf, j->obuf + res, j->obuf_len - res);
	j->obuf_len -= res;

notyet:
	watch_add(&jabber_plugin, j->fd, WATCH_WRITE, 0, jabber_handle_write, j);
}

/*
 * jabber_write()
 *
 * wysy³a tekst do serwera, a je¶li nie da siê ca³ego wys³aæ, zapisuje
 * resztê do bufora i wy¶le przy najbli¿szej okazji.
 *
 *  - j
 *  - text
 *  - freetext - czy zwolniæ tekst po wys³aniu?
 */
int jabber_write(jabber_private_t *j, const char *format, ...)
{
	const char *buf;
	char *text;
	int len;
	va_list ap;

	if (!j || !format)
		return -1;

	va_start(ap, format);
	text = vsaprintf(format, ap);
	va_end(ap);

	debug("[jabber] send %s\n", text);

	if (!j->obuf) {
		int res;

		len = xstrlen(text);
#ifdef HAVE_GNUTLS
		if (j->using_ssl)
			res = gnutls_record_send(j->ssl_session, text, len);
		else
#endif
			res = write(j->fd, text, len);

		if (res == len) {
			xfree(text);
			return 0;
		}

		if (res == -1) {
			xfree(text);
			return -1;
		}

		buf = text + res;
	} else
		buf = text;

	len = xstrlen(buf);

	if (!j->obuf)
		watch_add(&jabber_plugin, j->fd, WATCH_WRITE, 0, jabber_handle_write, j);

	j->obuf = xrealloc(j->obuf, j->obuf_len + len);
	memcpy(j->obuf + j->obuf_len, buf, len);
	j->obuf_len += len;

	xfree(text);

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
