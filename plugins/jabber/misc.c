/* $Id$ */

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "ekg2-config.h"

#ifdef HAVE_ZLIB
# include "zlib.h"
#endif

#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/themes.h>
#include <ekg/stuff.h>
#include <ekg/xmalloc.h>
#include <ekg/log.h>

#include <ekg/queries.h>

#include "jabber.h"
#include "jabber-ssl.h"

jabber_userlist_private_t *jabber_userlist_priv_get(userlist_t *u) {
	int func			= EKG_USERLIST_PRIVHANDLER_GET;
	jabber_userlist_private_t *up	= NULL;

	query_emit_id(&jabber_plugin, USERLIST_PRIVHANDLE, &u, &func, &up);

	return up;
}
	
/* XXX, It's the same function from mcjabber, but uses one buffor. */
static char *jabber_gpg_strip_header_footer(char *data) {
	char *p, *q;

	if (!data)
		return NULL;

	if (!(p = xstrstr(data, "\n\n")))
		return data;

	p += 2;

	for (q = p ; *q; q++);
	for (q--; q > p && (*q != '\n' || *(q+1) != '-'); q--) ;

	if (q <= p) {
		debug_error("jabber_gpg_strip_header_footer() assert. shouldn't happen, happen!\n");
		xfree(data);
		return NULL;
	}
	xstrncpy(data, p, q-p);
	data[q-p] = 0;
	return data;
}

char *jabber_openpgp(session_t *s, const char *fromto, enum jabber_opengpg_type_t way, char *message, char *key, char **error) {
	char *err = NULL;
	int ret = -2;
	char *oldkey = key;

	if (!message)	return NULL;
	if (!s) 	return NULL;

	switch (way) {
		case JABBER_OPENGPG_ENCRYPT:
			ret = query_emit_id(NULL, GPG_MESSAGE_ENCRYPT, &fromto, &message, &err); 	break;
		case JABBER_OPENGPG_DECRYPT:
			ret = query_emit_id(NULL, GPG_MESSAGE_DECRYPT, &s->uid, &message, &err);	break; 
		case JABBER_OPENGPG_SIGN:
			ret = query_emit_id(NULL, GPG_SIGN, &s->uid, &message, &err);			break;
		case JABBER_OPENGPG_VERIFY:
			ret = query_emit_id(NULL, GPG_VERIFY, &fromto, &message, &key, &err); 		break;	/* @ KEY retval key-id */
	}

	if (ret == -2)
		err = xstrdup("Load GPG plugin you moron.");

/* if way == JABBER_OPENGPG_VERIFY than message is never NULL */

	if (!message && !err)
		err = xstrdup("Bad password?");

	if (way == JABBER_OPENGPG_VERIFY && !key && !err)
		err = xstrdup("wtf?");

	if (err) 
		debug_error("jabber_openpgp(): %s\n", err);

	if (error) 
		*error = err;
	else
		xfree(err);

	if (err && way == JABBER_OPENGPG_VERIFY) {
		if (oldkey == key) {
			xfree(key);
			return NULL;
		}
	} else if (err) {
		xfree(message);
		return NULL;
	}

	if (way == JABBER_OPENGPG_SIGN || way == JABBER_OPENGPG_ENCRYPT) {
		message = jabber_gpg_strip_header_footer(message);
	}

	return way != JABBER_OPENGPG_VERIFY ? message : key;
}

#ifdef HAVE_ZLIB
char *jabber_zlib_compress(const char *buf, int *len) {
	size_t destlen = (*len) * 1.01 + 12;
	char *compressed = xmalloc(destlen);

	if (compress(compressed, &destlen, buf, *len) != Z_OK) {
		debug_error("jabber_zlib_compress() zlib compress() != Z_OK\n");
		xfree(compressed);
		return NULL;
	} 
	debug_function("jabber_handle_write() compress ok, retlen: %d orglen: %d\n", destlen, *len);
	*len = destlen;
	
	return compressed;
}

char *jabber_zlib_decompress(const char *buf, int *len) {
#define ZLIB_BUF_SIZE 1024
	z_stream zlib_stream;
	int err;
	size_t size = ZLIB_BUF_SIZE+1;
	int rlen = 0;

	char *uncompressed = NULL;

	zlib_stream.zalloc 	= Z_NULL;
	zlib_stream.zfree	= Z_NULL;
	zlib_stream.opaque	= Z_NULL;

	if ((err = inflateInit(&zlib_stream)) != Z_OK) {
		debug_error("[jabber] jabber_handle_stream() inflateInit() %d != Z_OK\n", err);
		return NULL;
	}

	zlib_stream.next_in	= buf;
	zlib_stream.avail_in	= *len;

	do {
		uncompressed = xrealloc(uncompressed, size);
		zlib_stream.next_out = uncompressed + rlen;
		zlib_stream.avail_out= ZLIB_BUF_SIZE;

		err = inflate(&zlib_stream, Z_NO_FLUSH);
		if (err != Z_OK && err != Z_STREAM_END) {
			debug_error("[jabber] jabber_handle_stream() inflate() %d != Z_OK && %d != Z_STREAM_END %s\n", 
					err, err, zlib_stream.msg);
			break;
		}

		rlen += (ZLIB_BUF_SIZE - zlib_stream.avail_out);
		size += (ZLIB_BUF_SIZE - zlib_stream.avail_out);
	} while (err == Z_OK && zlib_stream.avail_out == 0);

	inflateEnd(&zlib_stream);

	uncompressed[rlen] = 0;

	*len = rlen;

	return uncompressed;
}
#endif

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

/**
 * jabber_escape()
 * 
 * Convert charset from config_console_charset to "utf-8"<br>
 * Escape xml chars using xml_escape()
 *
 * @note If config_use_unicode is set, this function return only xml_escape(@a text)
 *
 * @param text - text to reencode+escape
 *
 * @sa jabber_unescape() - For function reconverting charset back to config_console_charset
 *
 * @return Dynamic allocated string, which should be xfree()'d
 */

char *jabber_escape(const char *text) {
	unsigned char *utftext;
	char *res;

	if (!text)
		return NULL;

	if (config_use_unicode)
		return xml_escape(text);

	utftext = ekg_convert_string(text, NULL, "utf-8");

	res = xml_escape(utftext ? utftext : text);
        xfree(utftext);
	return res;
}

/**
 * jabber_unescape()
 *
 * Convert charset from "utf-8" to config_console_charset.<br>
 * xml escaped chars are already changed by expat. so we don't care about them.
 *
 * @note If config_use_unicode is set, this function only xstrdup(@a text) 
 *
 * @param text - text to reencode.
 *
 * @sa jabber_escape() - for function escaping xml chars + reencoding string to utf-8
 *
 * @return Dynamic allocated string, which should be xfree()'d
 */

char *jabber_unescape(const char *text) {
	const char *s;
	if (!text)
		return NULL;
	if (config_use_unicode)
		return xstrdup(text);
	s = ekg_convert_string(text, "utf-8", NULL);

	return (s ? s : xstrdup(text));
}

/**
 * tlen_encode()
 *
 * Convert charset from config_console_charset to ISO-8859-2<br>
 * ,,encode'' string with urlencode
 *
 * @note It was ripped from libtlen. (c) Libtlen developers see: http://libtlen.sourceforge.net/
 *
 * @todo Try to rewrite.
 *
 * @param what - string to encode.
 *
 * @sa tlen_decode() - for urldecode.
 *
 * @return Dynamic allocated string, which should be xfree()'d
 */

char *tlen_encode(const char *what) {
	const unsigned char *s;
	unsigned char *ptr, *str;
	char *text = NULL;

	if (!what) return NULL;

	if (xstrcasecmp(config_console_charset, "ISO-8859-2"))
		s = text = ekg_convert_string(what, NULL, "ISO-8859-2");
	if (!text)
		s = what;

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

/**
 * tlen_decode()
 *
 * Decode string ,,encoded'' with urldecode [in ISO-8859-2] and convert charset to config_console_charset<br>
 *
 * @note It was ripped from libtlen. (c) Libtlen developers see: http://libtlen.sourceforge.net/
 *
 * @todo Try to rewrite
 *
 * @param what - string to decode.
 *
 * @sa tlen_encode() - for urlencode.
 *
 * @return Dynamic allocated string, which should be xfree()'d
 */

char *tlen_decode(const char *what) {
	unsigned char *dest, *data, *retval;
	char *text;

	if (!what) return NULL;
	dest = data = retval = xstrdup(what);
	while (*data) {
		if (*data == '+')
			*dest++ = ' ';
		else if ((*data == '%') && xisxdigit(data[1]) && xisxdigit(data[2])) {
			int code;

			sscanf(data + 1, "%2x", &code);
			if (code != '\r')
				*dest++ = (unsigned char) code;
			data += 2;
		} else
			*dest++ = *data;
		data++;
	}
	*dest = '\0';
	if (!xstrcasecmp(config_console_charset, "ISO-8859-2")) return retval;

	if (!(text = ekg_convert_string(retval, "ISO-8859-2", NULL)))
		return retval;
	xfree(retval);
	return text;
}

/**
 * utfstrchr()
 *
 * Returns pointer to the first occurence of ASCII character in utf-8 string, taking care of multibyte characters.
 *
 * @note It can only find simple ASCII characters. If you need to find some multibyte char, please use wcs* instead.
 *
 * @bug  When @a s is invalid utf-8 sequence, anything can happen.
 *
 * @param s - string to search, as an utf-8 encoded char*
 * @param c - ASCII character to find
 *
 * @return Pointer to found char or NULL, if not found.
 */

unsigned char *utfstrchr(unsigned char *s, unsigned char c) {
	unsigned char *p;

	for (p = s; *p; p++) {
		/* What do we do here? First, we check if we've got single byte char.
		 * If yes, then we do compare it with given char.
		 * If no, then we determine how many bytes we need to skip over. */
		if (*p < 0x80) {
			if (*p == c)
				return p;
		} /* the rest stolen from ../feed/rss.c, where it was stolen from linux/drivers/char/vt.c */
		else if ((*p & 0xe0) == 0xc0)	p++;
		else if ((*p & 0xf0) == 0xe0)	p += 2;
		else if ((*p & 0xf8) == 0xf0)	p += 3;
		else if ((*p & 0xfc) == 0x78)	p += 4;
		else if ((*p & 0xfe) == 0xfc)	p += 5;
	}

	return NULL;
}

/*
 * jabber_handle_write()
 *
 * obs³uga mo¿liwo¶ci zapisu do socketa. wypluwa z bufora ile siê da
 * i je¶li co¶ jeszcze zostanie, ponawia próbê.
 */
WATCHER_LINE(jabber_handle_write) /* tylko gdy jest wlaczona kompresja lub TLS/SSL. dla zwyklych polaczen jest watch_handle_write() */
{
	jabber_private_t *j = data;
	char *compressed = NULL;
	int res = 0;
	size_t len;

	if (type) {
		/* XXX, do we need to make jabber_handle_disconnect() or smth simillar? */
		j->send_watch = NULL;
		return 0;
	}
	
	if (
#ifdef JABBER_HAVE_SSL
	!j->using_ssl && 
#endif
	!j->using_compress) {
		/* XXX ? */
		debug_error("[jabber] jabber_handle_write() nor j->using_ssl nor j->using_compression.... wtf?!\n");
		return 0;
	}

	len = xstrlen(watch);

	switch (j->using_compress) {
		case JABBER_COMPRESSION_NONE:
		case JABBER_COMPRESSION_LZW_INIT:
		case JABBER_COMPRESSION_ZLIB_INIT:
			break;

		case JABBER_COMPRESSION_ZLIB:
#ifdef HAVE_ZLIB
			res = len;
			if (!(compressed = jabber_zlib_compress(watch, &len))) return 0;
#else
			debug_error("[jabber] jabber_handle_write() compression zlib, but no zlib support.. you're joking, right?\n");
#endif
			break;

		case JABBER_COMPRESSION_LZW:	/* XXX */
#warning "LZW SUPPORT !!!"
		default:
			debug_error("[jabber] jabber_handle_write() unknown compression: %d\n", j->using_compress);
	}

	if (compressed) watch = (const char *) compressed;

#ifdef JABBER_HAVE_SSL
	if (j->using_ssl) {
		res = SSL_SEND(j->ssl_session, watch, len);

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

		xfree(compressed);
		return res;
	}
#endif

/* here we call write() */
	write(fd, watch, len);
	xfree(compressed);

	return res;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
