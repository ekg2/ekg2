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

static char *iso_utf_ent[256] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, "&quot;", 0, 0, 0, "&amp;", "&apos;", 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "&lt;", 0, "&gt;", 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

char *xiconv(const char *from, const char *to, const char *what)
{
	char *dst, *d; //, unikludge[8];
	char *s;
	size_t sl, dl, rdl, delta;
	iconv_t conv; //, unikludge_c;

	if ( (iconv_t)-1 == (conv = iconv_open(to, from)) ) {
		print("jabber_charset_init_error", from, to, strerror(errno));
		return (char *)NULL;
	}

	sl = dl = rdl = xstrlen(what);
	dst = xstrdup(what);

	d = dst;
	s = (char*)what;
	
	while ( sl )
		if ( (size_t)-1 == iconv(conv, &s, &sl, &d, &dl) ) {
			switch ( errno ) {
			case EILSEQ: /* Illegal sequence */
				debug("[xiconv] -EILSEQ: %s (%s)", what, s);

				{ 
					/* Zjadamy jeden znak z wej¶cia przez konwersjê na WCHAR_T
					   i odrzucenie efektu */
					iconv_t kludge_c=0;
					char kludge_s[32];
					size_t kludge_l=sizeof(wchar_t);

					if ( (iconv_t)-1 == 
					     (kludge_c = iconv_open("WCHAR_T", from)) ) {
						print("jabber_charset_init_error",
						      from, "WCHAR_T", strerror(errno));
						return (char *)NULL;
					}

					while ( (size_t)-1 == 
						iconv(kludge_c, &s, &sl, 
						      (char **)&kludge_s, &kludge_l) ) {
						if ( errno == E2BIG)
						{
							debug("[xiconv] This shouldn't happen "
								"- unicode should fit in 32 bytes.");
						}
						else {
							debug("[xiconv] Kludge doesn't work here!");
							s++;
							sl++;
							break;
						}
					}

					iconv_close(kludge_c);
				}
				
				/* Dopisujemy do wyj¶cia znak
				   zapytania, ¿eby user wiedzia³, ¿e
				   co¶ zjedzono. */

				*d = '?';
				d++;
				dl++;
				break;

			case EINVAL: /* garbage at end of buffer */
				debug("[xiconv] -EINVAL: %s (%s)", what, s);
				sl=0;
				break;

			case E2BIG: /* dst too small */
				delta = d-dst;
				dst = xrealloc(dst, rdl*2);
				d = dst+delta;
				dl += rdl;
				rdl *= 2;
				break;

			case EBADF: /* invalid conv */
				debug("[xiconv] -EBADF (can't happen)");
			}
		}
	*d = '\0';

	iconv_close(conv);
	return xrealloc(dst, strlen(dst)+1);
}

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
	const unsigned char *p;
	unsigned char *res, *q, *utftext;
	int len;

	if (!text)
		return NULL;
	if ( !(utftext = xiconv(config_jabber_console_charset, "utf-8", text)) )
		return NULL;
		
	for (p = utftext, len = 0; *p; p++)
		len += iso_utf_ent[*p] ? strlen(iso_utf_ent[*p]) : 1;

	res = xmalloc(len + 1);	
	memset(res, 0, len + 1);

	for (p = utftext, q = res; *p; p++) {
		char *ent = iso_utf_ent[*p];

		if (ent)
			xstrcpy(q, ent);
		else
			*q = *p;

		q += iso_utf_ent[*p] ? strlen(iso_utf_ent[*p]) : 1;
	}

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

	return xiconv("utf-8", config_jabber_console_charset, text);
}

/*
 * jabber_handle_write()
 *
 * obs³uga mo¿liwo¶ci zapisu do socketa. wypluwa z bufora ile siê da
 * i je¶li co¶ jeszcze zostanie, ponawia próbê.
 */
void jabber_handle_write(int type, int fd, int watch, void *data)
{
	jabber_private_t *j = data;
	int res;

#ifdef HAVE_GNUTLS
	if (j->using_ssl) {
		do {
			res = gnutls_record_send(j->ssl_session, j->obuf, j->obuf_len);	
		} while ((res == GNUTLS_E_INTERRUPTED) || (res == GNUTLS_E_AGAIN)); 
		
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
