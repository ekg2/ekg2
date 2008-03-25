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
#include <ekg/protocol.h>
#include <ekg/themes.h>
#include <ekg/stuff.h>
#include <ekg/xmalloc.h>
#include <ekg/log.h>

#include <ekg/queries.h>

#include "jabber.h"
#include "jabber-ssl.h"

void *jconv_in = (void*) -1;
void *jconv_out = (void*) -1;
void *tconv_in = (void*) -1;
void *tconv_out = (void*) -1;

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

	if (compress((unsigned char *) compressed, &destlen, (unsigned char *) buf, *len) != Z_OK) {
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

	unsigned char *uncompressed = NULL;

	zlib_stream.zalloc 	= Z_NULL;
	zlib_stream.zfree	= Z_NULL;
	zlib_stream.opaque	= Z_NULL;

	if ((err = inflateInit(&zlib_stream)) != Z_OK) {
		debug_error("[jabber] jabber_handle_stream() inflateInit() %d != Z_OK\n", err);
		return NULL;
	}

	zlib_stream.next_in	= (unsigned char *) buf;
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

	return (char *) uncompressed;
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
	char *utftext;
	char *res;

	if (!text)
		return NULL;

	utftext = ekg_convert_string_p(text, jconv_out);
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
	char *s;

	if (!text)
		return NULL;
	s = ekg_convert_string_p(text, jconv_in);

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
	unsigned char *s;
	unsigned char *ptr, *str;
	char *text = NULL;

	if (!what) return NULL;

	text = ekg_convert_string_p(what, tconv_out);
	s = (unsigned char *) (text ? text : what);

	str = ptr = (unsigned char *) xcalloc(3 * xstrlen((char*) s) + 1, 1);
	while (*s) {
		if (*s == ' ')
			*ptr++ = '+';
		else if ((*s < '0' && *s != '-' && *s != '.')
			 || (*s < 'A' && *s > '9') || (*s > 'Z' && *s < 'a' && *s != '_')
			 || (*s > 'z')) 
		{
			sprintf((char*) ptr, "%%%02X", *s);
			ptr += 3;
		} else
			*ptr++ = *s;
		s++;
	}
	xfree(text);
	return (char*) str;
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
	dest = data = retval = (unsigned char *) xstrdup(what);
	while (*data) {
		if (*data == '+')
			*dest++ = ' ';
		else if ((*data == '%') && xisxdigit(data[1]) && xisxdigit(data[2])) {
			int code;

			sscanf((char*) (data + 1), "%2x", &code);
			if (code != '\r')
				*dest++ = (unsigned char) code;
			data += 2;
		} else
			*dest++ = *data;
		data++;
	}
	*dest = '\0';

	if (!(text = ekg_convert_string_p((char*) retval, tconv_in)))
		return (char*) retval;
	xfree(retval);
	return text;
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
	int res = 0, len;

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
		default:
			debug_error("[jabber] jabber_handle_write() unknown compression: %d\n", j->using_compress);
	}

	if (compressed) watch = (const char *) compressed;

#ifdef JABBER_HAVE_SSL
	if (j->using_ssl) {
		res = SSL_SEND(j->ssl_session, watch, (size_t) len);

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

/* called within jabber_session_init() */
void jabber_convert_string_init(int is_tlen) {
	if (is_tlen && (tconv_in == (void*) -1))
		tconv_in = ekg_convert_string_init("ISO-8859-2", NULL, &tconv_out);
	else if (!is_tlen && (jconv_in == (void*) -1))
		jconv_in = ekg_convert_string_init("UTF-8", NULL, &jconv_out);
}

void jabber_convert_string_destroy() {
	if (tconv_in != (void*) -1) {
		ekg_convert_string_destroy(tconv_in);
		ekg_convert_string_destroy(tconv_out);
	}
	if (jconv_in != (void*) -1) {
		ekg_convert_string_destroy(jconv_in);
		ekg_convert_string_destroy(jconv_out);
	}
}

/* CONFIG_POSTINIT */
QUERY(jabber_convert_string_reinit) {
	jabber_convert_string_destroy();

	if (tconv_in != (void*) -1) {
		tconv_in = (void*) -1;
		jabber_convert_string_init(1);
	}
	if (jconv_in != (void*) -1) {
		jconv_in = (void*) -1;
		jabber_convert_string_init(0);
	}

	return 0;
}

/* conversations */

/**
 * jabber_conversation_find() searches session's conversation list for matching one.
 *
 * @param	j	- private data of session.
 * @param	uid	- UID of recipient.
 * @param	subject	- message subject (for non-threaded conversations).
 * @param	thread	- jabber thread ID, if threaded.
 * @param	result	- place to write address of jabber_conversation_t or NULL, if not needed.
 * @param	can_add	- if nonzero, we can create new conversation, if none match.
 *
 * @return	Reply-ID of conversation.
 */
int jabber_conversation_find(jabber_private_t *j, const char *uid, const char *subject, const char *thread, jabber_conversation_t **result, const int can_add) {
	jabber_conversation_t *thr, *prev;
	const char *resubject = NULL;
        int i, l = 0;
	
	if (!thread && subject && !xstrncmp(subject, config_subject_reply_prefix, (l = xstrlen(config_subject_reply_prefix))))
		resubject = subject + l;
	
        for (thr = j->conversations, prev = NULL, i = 1;
                thr && ((thread ? xstrcmp(thr->thread, thread) /* try to match the thread, if avail */
			: (subject ? xstrcmp(thr->subject, subject) /* else try to match the subject... */
				&& xstrcmp(thr->subject, resubject) /* ...also with Re: prefix... */
				&& (xstrncmp(thr->subject, config_subject_reply_prefix, l)
					|| xstrcmp(thr->subject+l, subject)) /* ...on both sides... */
				: !!thr->subject)) || !uid || xstrcmp(thr->uid, uid)); /* ...and UID */
	                prev = thr, thr = thr->next, i++); /* <- that's third param for 'for' */

	if (!thr && can_add) { /* haven't found anything, but can create something */
                thr		= xmalloc(sizeof(jabber_conversation_t));
                thr->thread	= xstrdup(thread);
		thr->uid	= xstrdup(uid);
			/* IMPORTANT: thr->subject is maintained by message handler
			 * Now I know why I haven't added it earlier here */
                if (prev)
                        prev->next		= thr;
                else
                        j->conversations	= thr;
        }
	
	if (result)
	        *result = thr;
        return i;
}

/**
 * jabber_conversation_get() is used to get conversation by its Reply-ID.
 *
 * @param	j	- private data of session.
 * @param	n	- Reply-ID.
 *
 * @return	Pointer to jabber_conversation_t or NULL, when no conversation found.
 */
jabber_conversation_t *jabber_conversation_get(jabber_private_t *j, const int n) {
	jabber_conversation_t *thr;
	int i;
	
	for (thr = j->conversations, i = 1;
		thr && (i < n);
		thr = thr->next, i++);
	
	return thr;
}

/**
 * jabber_thread_gen() generates new thread-ID for outgoing messages.
 *
 * @param	j	- private data of session.
 * @param	uid	- recipient UID.
 *
 * @return	New, session-unique thread-ID.
 */
char *jabber_thread_gen(jabber_private_t *j, const char *uid) {
	int i, k, n	= 0;
	char *thread	= NULL;

		/* just trying to find first free one */
	for (i = jabber_conversation_find(j, NULL, NULL, NULL, NULL, 0), k = i; n != k; i++) {
		xfree(thread);
		thread = saprintf("thr%d-%8x-%8x", i, rand(), (unsigned int) time(NULL)); /* that should look gorgeous */
		n = jabber_conversation_find(j, thread, NULL, uid, NULL, 0);
		debug("[jabber,thread_gen] i = %d, k = %d, n = %d, t = %s\n", i, n, k, thread);
	}
	
	return thread;
}

static inline uint32_t jabber_formatchar(const char c) {
	if (c == '*')	return EKG_FORMAT_BOLD;
	if (c == '_')	return EKG_FORMAT_UNDERLINE;
	if (c == '/')	return EKG_FORMAT_ITALIC;
	
	return 0;
}

/* detect whether formatchar is surrounded by space,
 * when beginning == NULL, check following chars,
 * else check preceding chars
 * mode:	0 - check for spaces
 * 		1 - check for previous/following occurence */
static inline int jabber_fc_check(const char *curr, const char *beginning, const int mode) {
	const char c = *curr;

	while (beginning ? --curr >= beginning : *(++curr)) {
		if (mode ? *curr == c : isspace(*curr))
			return 1;
		else if (!jabber_formatchar(*curr))
			return 0;
	}

	return !mode;
}

/* currently parses message text and tries to add some formatting,
 * e.g. *bold* /italic/
 *
 * some time may also parse XHTML */
uint32_t *jabber_msg_format(const char *plaintext, const char *html) {
	uint32_t *fmtstring = NULL, *p = NULL, *pf = NULL;
	const char *c;

	return NULL; /* disable, because of XXX:
		1) slash-asterisk-slash test slash-asterisk-slash - first slash disabler is still italic
		2) it collides with popular emoticons
		3) and needs to be also updated to match more special chars than spaces - for example, question marks
		4) and after that modification, it would match more popular emoticons
		5) thou it sucks */

	for (c = plaintext; *c; c++) {
		int enabling;
		int flag = jabber_formatchar(*c);

		if (p) {
			*p |= *pf; /* 'or' to not lose just-enabled formatting */
			pf = p++;
		}

		if (flag) {
			enabling = (!pf || !(*pf & flag));

			if (enabling) {
				const char *tmp = c+1;
				if (!jabber_fc_check(c, plaintext, 0)) /* ignore middle-of-a-word formatstrings */
					continue;
				if (jabber_fc_check(c, NULL, 1)) /* use last formatchar in a row */
					continue;

				do {		/* check if formatstring is finished */
					tmp = xstrchr(tmp, *c);
					if (tmp && !jabber_fc_check(tmp, NULL, 0))
						tmp += 1;
					else
						break;
				} while (1);
				if (!tmp)
					continue;

			} else if (!jabber_fc_check(c, NULL, 0)) /* like above */
				continue;
			else if (jabber_fc_check(c, plaintext, 1)) /* use first disabling formatchar in a row */
					continue;

			if (!p) {
				fmtstring = xcalloc(xstrlen(plaintext), sizeof(uint32_t));
				pf = &fmtstring[c - plaintext];
				p = pf+1;
			}

				/* p is always +1 here */
			if (enabling)
				*p	|= flag; /* don't need to check p+1 - unfinished formatting check does it for us */
			else
				*pf	&= ~flag;
		}
	}

	return fmtstring;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
