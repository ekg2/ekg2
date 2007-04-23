/*
 *  (C) Copyright 2003-2006 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Tomasz Torcz <zdzichu@irc.pl>
 *                          Leszek Krupiñski <leafnode@pld-linux.org>
 *                          Piotr Paw³ow and other libtlen developers (http://libtlen.sourceforge.net/index.php?theme=teary&page=authors)
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

#include "ekg2-config.h"
#include <ekg/win32.h>

#include <sys/types.h>

#ifndef NO_POSIX_SYSTEM
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/utsname.h> /* dla jabber:iq:version */
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#ifndef NO_POSIX_SYSTEM
#include <netdb.h>
#endif

#ifdef HAVE_EXPAT_H
#  include <expat.h>
#endif

#ifdef __sun      /* Solaris, thanks to Beeth */
#include <sys/filio.h>
#endif
#include <time.h>

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/themes.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include <ekg/log.h>

#include <ekg/queries.h>

#include "jabber.h"
#include "jabber_dcc.h"

#define jabberfix(x,a) ((x) ? x : a)

#define JABBER_HANDLER(x) 		static void x(session_t *s, xmlnode_t *n)
#define JABBER_HANDLER_IQ(x) 		static void x(session_t *s, xmlnode_t *n, jabber_iq_type_t iqtype, const char *from, const char *id)
#define JABBER_HANDLER_GET_REPLY(x) 	static void x(session_t *s, jabber_private_t *j, xmlnode_t *n, const char *from, const char *id)
#define JABBER_HANDLER_RESULT(x) 	static void x(session_t *s, xmlnode_t *n, const char *from, const char *id)

JABBER_HANDLER(jabber_handle_message);
JABBER_HANDLER(jabber_handle_iq);
JABBER_HANDLER(jabber_handle_presence);

static time_t jabber_try_xdelay(const char *stamp);
static void jabber_session_connected(session_t *s);

static void newmail_common(session_t *s); 

/**
 * xmlnode_find_child()
 *
 * Find child of @a node, with @a name
 *
 * @param n - node
 * @param name - name
 *
 * @return Pointer to node if such child was found, else NULL
 */

static xmlnode_t *xmlnode_find_child(xmlnode_t *n, const char *name) {
	if (!n || !n->children)
		return NULL;

	for (n = n->children; n; n = n->next)
		if (!xstrcmp(n->name, name))
			return n;
	return NULL;
}

/**
 * jabber_iq_auth_send()
 *
 * Send jabber:iq:auth with <i><digest>DIGEST</digest></i> or <i><password>PLAINTEXT_PASSWORD</password></i><br>
 * It support both tlen auth, and jabber NON-SASL Authentication [XEP-0078]<br>
 *
 * @note	<b>XEP-0078:</b> Non-SASL Authentication: http://www.xmpp.org/extensions/xep-0078.html
 *
 * @todo	It's not really XEP-0078 cause ekg2 don't support it. But it this done that way.. I don't know any server with XEP-0078 functonality..<br>
 * 		I still rcv 'service-unavailable' or 'bad-request' ;(<br>
 * 		But it <b>MUST</b> be implemented for <i>/session disable_sasl 1</i><br>
 * 		So it's just <i>jabber:iq:auth</i> for <i>disable_sasl</i> 2.
 *
 * @note 	Tlen Authentication was stolen from libtlen calc_passcode() with magic stuff (C) libtlen's developer and Piotr Paw³ow<br>
 * 		see: http://libtlen.sourceforge.net/
 *
 * @param	s 		- session to authenticate <b>CANNOT BE NULL</b>
 * @param	username	- username
 * @param	passwd		- password to hash or to escape
 * @param	stream_id	- id of stream.
 */

void jabber_iq_auth_send(session_t *s, const char *username, const char *passwd, const char *stream_id) {
	jabber_private_t *j = s->priv;

	const char *passwd2 = NULL;			/* if set than jabber_digest() should be done on it. else plaintext_passwd 
							   Variable to keep password from @a password, or generated hash of password with magic constant [tlen] */

	char *resource = jabber_escape(j->resource);	/* escaped resource name */
	char *epasswd = NULL;				/* temporary password [escaped, or hash], if needed for xfree() */
	char *authpass;					/* <digest>digest</digest> or <password>plaintext_password</password> */

	/* stolen from libtlen function calc_passcode() Copyrighted by libtlen's developer and Piotr Paw³ow */
	if (j->istlen) {
		int     magic1 = 0x50305735, magic2 = 0x12345671, sum = 7;
		char    z;
		while ((z = *passwd++) != 0) {
			if (z == ' ' || z == '\t') continue;
			magic1 ^= (((magic1 & 0x3f) + sum) * z) + (magic1 << 8);
			magic2 += (magic2 << 8) ^ magic1;
			sum += z;
		}
		magic1 &= 0x7fffffff;
		magic2 &= 0x7fffffff;

		passwd2 = epasswd = saprintf("%08x%08x", magic1, magic2);
	} else if (session_int_get(s, "plaintext_passwd")) {
		epasswd = jabber_escape(passwd);
	} else 	passwd2 = passwd;


	authpass = (passwd2) ?
		saprintf("<digest>%s</digest>", jabber_digest(stream_id, passwd2)) :	/* hash */
		saprintf("<password>%s</password>", epasswd);				/* plaintext */
		
	watch_write(j->send_watch, 
			"<iq type=\"set\" id=\"auth\" to=\"%s\"><query xmlns=\"jabber:iq:auth\"><username>%s</username>%s<resource>%s</resource></query></iq>", 
			j->server, username, authpass, resource);
	xfree(authpass);

	xfree(epasswd);
	xfree(resource);
}


/*
 * tlen_handle_notification()
 *
 *
 */

JABBER_HANDLER(tlen_handle_notification) {	/* n->name: "m" TLEN only: typing, nottyping, and alert notifications */
	char *type = jabber_attr(n->atts, "tp");
	char *from = jabber_attr(n->atts, "f");
	char *typeadd = jabber_attr(n->atts, "type");


	if (!type || !from || (typeadd && !xstrcmp(typeadd, "error"))) {
		debug_error("tlen_handle() %d %s/%s/%s", __LINE__, type, from, typeadd);
		return;
	}

	if (!xstrcmp(type, "t") || !xstrcmp(type, "u")) {
		char *uid = saprintf("tlen:%s", from);

		/* typing notification */
		char *session	= xstrdup(session_uid_get(s));
		int stateo	= !xstrcmp(type, "u") ? EKG_XSTATE_TYPING : 0;
		int state	= stateo ? EKG_XSTATE_TYPING : 0;

		query_emit_id(NULL, PROTOCOL_XSTATE, &session, &uid, &state, &stateo);

		xfree(session);
		xfree(uid);
		return;
	}

	if (!xstrcmp(type, "a")) {	/* funny thing called alert */
		char *uid = saprintf("tlen:%s", from);
		print_window(uid, s, 0, "tlen_alert", session_name(s), format_user(s, uid));

		if (config_sound_notify_file)
			play_sound(config_sound_notify_file);
		else if (config_beep && config_beep_notify)
			query_emit_id(NULL, UI_BEEP, NULL);
		xfree(uid);
		return;
	}

}

/*
 * tlen_handle_newmail()
 *
 */

JABBER_HANDLER(tlen_handle_newmail) {
	char *from = tlen_decode(jabber_attr(n->atts, "f"));
	char *subj = tlen_decode(jabber_attr(n->atts, "s"));

	print("tlen_mail", session_name(s), from, subj);
	newmail_common(s);

	xfree(from);
	xfree(subj);
}

/*
 * tlen_handle_webmessage()
 * 
 *
 */

JABBER_HANDLER(tlen_handle_webmessage) {
	char *from = jabber_attr(n->atts, "f");
	char *mail = jabber_attr(n->atts, "e");
	char *content = n->data;
	string_t body = string_init("");

	char *text;

	if (from || mail) {
		string_append(body, "From:");
		if (from) {
			string_append_c(body, ' ');
			string_append(body, from);
		}
		if (mail) {
			string_append(body, " <");
			string_append(body, mail);
			string_append_c(body, '>');
		}
		string_append_c(body, '\n');
	}

	if (body->len) string_append_c(body, '\n');

	string_append(body, content);
	text = tlen_decode(body->str);
	string_free(body, 1);

	{
		char *me	= xstrdup(session_uid_get(s));
		char *uid	= xstrdup("ludzie.tlen.pl");
		char **rcpts 	= NULL;
		uint32_t *format= NULL;
		time_t sent	= time(NULL);
		int class 	= EKG_MSGCLASS_MESSAGE;
		char *seq 	= NULL;
		int ekgbeep 	= EKG_TRY_BEEP;
		int secure	= 0;

		query_emit_id(NULL, PROTOCOL_MESSAGE, &me, &uid, &rcpts, &text, &format, &sent, &class, &seq, &ekgbeep, &secure);

		xfree(me);
		xfree(uid);
	}

	xfree(text);
}

/**
 * tlen_handle()
 *
 * Handle misc tlen-only things...<br>
 * Executed by jabber_handle() when unknown n->name found, and it's tlen session.<br>
 * It handle: 
 * 	- "m" using tlen_handle_notification()		[user-notifications like: typing, stop typing, and soundalers]
 * 	- "n" using tlen_handle_newmail()		[when server inform us about new mail]
 * 	- "w" using tlen_handle_webmessage()		[when we receive webmessage]
 * 	- else print error in __debug window.
 *
 * @todo Move this and stuff to jabber_tlen_handlers.c
 */

JABBER_HANDLER(tlen_handle) {
	if (!xstrcmp(n->name, "m")) {		/* user-notifications */
		tlen_handle_notification(s, n);	return;
	}

	if (!xstrcmp(n->name, "n")) {		/* newmail */
		tlen_handle_newmail(s, n);	return;
	}

	if (!xstrcmp(n->name, "w")) {		/* web messages */
		tlen_handle_webmessage(s, n);	return;
	}

	debug_error("tlen_handle() what's that: %s ?\n", n->name);
}

#define CHECK_CONNECT(connecting_, connected_, func) if (j->connecting != connecting_ || s->connected != connected_) { \
			debug_error("[jabber] %s:%d ASSERT_CONNECT j->connecting: %d (shouldbe: %d) s->connected: %d (shouldbe: %d)\n", \
				__FILE__, __LINE__, j->connecting, connecting_, s->connected, connected_);	func; }

#define CHECK_XMLNS(n, xmlns, func) if (xstrcmp(jabber_attr(n->atts, "xmlns"), xmlns)) { \
			debug_error("[jabber] %s:%d ASSERT_XMLNS BAD XMLNS, IS: %s SHOULDBE: %s\n", __FILE__, __LINE__, jabber_attr(n->atts, "xmlns"), xmlns);	func; }

JABBER_HANDLER(jabber_handle_stream_features) {
	jabber_private_t *j = s->priv;

	int use_sasl = j->connecting == 1 && (session_int_get(s, "disable_sasl") != 2);
	int use_fjuczers = 0;	/* bitmaska (& 1 -> session) (& 2 -> bind) */

	int display_fjuczers = session_int_get(s, "display_server_features");

	xmlnode_t *mech_node = NULL;

	if (display_fjuczers < 0)
		display_fjuczers = 0;

	if (display_fjuczers == 1 && session_int_get(s, "__features_displayed") == 1)
		display_fjuczers = 0;


	if (display_fjuczers) {
		xmlnode_t *ch;

		print("xmpp_feature_header", session_name(s), j->server, "");

		for (ch = n->children; ch; ch = ch->next) {
			xmlnode_t *another;
				if (!xstrcmp(ch->name, "starttls")) {
					print("xmpp_feature", session_name(s), j->server, ch->name, jabber_attr(ch->atts, "xmlns"), "/session use_tls"); 
					continue;
				}

				if (!xstrcmp(ch->name, "mechanisms")) {
					print("xmpp_feature", session_name(s), j->server, ch->name, jabber_attr(ch->atts, "xmlns"), "/session disable_sasl");
					for (another = ch->children; another; another = another->next) {	
						if (!xstrcmp(another->name, "mechanism")) {
							if (!xstrcmp(another->data, "DIGEST-MD5"))
								print("xmpp_feature_sub", session_name(s), j->server, "SASL", another->data, "Default");
							else if (!xstrcmp(another->data, "PLAIN"))
								print("xmpp_feature_sub", session_name(s), j->server, "SASL", another->data, "/sesion plaintext_passwd");
							else	print("xmpp_feature_sub_unknown", session_name(s), j->server, "SASL", __(another->data), "");
						}
					}
					continue;
				}
#if 0
				if (!xstrcmp(ch->name, "compression")) {
					print("xmpp_feature", session_name(s), j->server, ch->name, jabber_attr(ch->atts, "xmlns"), "/session use_compression method1,method2,..");
					for (another = ch->children; another; another = another->next) {
						if (!xstrcmp(another->name, "method")) {
							if (!xstrcmp(another->data, "zlib"))
								print("xmpp_feature_sub", session_name(s), j->server, "COMPRESSION", another->data, "/session use_compression zlib");
							else if (!xstrcmp(another->data, "lzw"))
								print("xmpp_feature_sub", session_name(s), j->server, "COMPRESSION", another->data, "/session use_compression lzw");
							else	print("xmpp_feature_sub_unknown", session_name(s), j->server, "COMPRESSION", __(another->data), "");
						}
					}
					continue;
				}
#endif
				if (!xstrcmp(ch->name, "session"))
					print("xmpp_feature", session_name(s), j->server, ch->name, jabber_attr(ch->atts, "xmlns"), "Manage session");
				else if (!xstrcmp(ch->name, "bind"))
					print("xmpp_feature", session_name(s), j->server, ch->name, jabber_attr(ch->atts, "xmlns"), "Bind resource");
				else if (!xstrcmp(ch->name, "register"))
					print("xmpp_feature", session_name(s), j->server, ch->name, jabber_attr(ch->atts, "xmlns"), "Reigster account using /register");
				else	print("xmpp_feature_unknown", session_name(s), j->server, ch->name, jabber_attr(ch->atts, "xmlns"));

		}
		print("xmpp_feature_footer", session_name(s), j->server);
	}

	for (n = n->children; n; n = n->next) {
		if (!xstrcmp(n->name, "starttls")) {
#ifdef JABBER_HAVE_SSL
			CHECK_CONNECT(1, 0, continue)
			CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-tls", continue)

			if (!j->using_ssl && session_int_get(s, "use_tls") == 1 && session_int_get(s, "use_ssl") == 0) {
				debug_function("[jabber] stream:features && TLS! let's rock.\n");

				watch_write(j->send_watch, "<starttls xmlns=\"urn:ietf:params:xml:ns:xmpp-tls\"/>");
				return;
			}
#endif
		} else if (!xstrcmp(n->name, "mechanisms") && !mech_node) {		/* faster than xmlnode_find_child */
			CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-sasl", continue)
			mech_node = n->children;
		} else if (!xstrcmp(n->name, "session")) {
			CHECK_CONNECT(2, 0, continue)
			CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-session", continue)

			use_fjuczers |= 1;

		} else if (!xstrcmp(n->name, "bind")) {
			CHECK_CONNECT(2, 0, continue)
			CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-bind", continue)

			use_fjuczers |= 2;
		} else if (!xstrcmp(n->name, "compression")) {
			const char *tmp		= session_get(s, "use_compression");
			const char *method_comp = NULL;
			char *tmp2;

			xmlnode_t *method;

			if (!tmp) continue;
			CHECK_CONNECT(1, 0, continue)
			CHECK_XMLNS(n, "http://jabber.org/features/compress", continue)

			j->using_compress = JABBER_COMPRESSION_NONE;

			for (method = n->children; method; method = method->next) {
				if (!xstrcmp(method->name, "method")) {
					if (!xstrcmp(method->data, "zlib")) {
#ifdef HAVE_ZLIB
						if ((tmp2 = xstrstr(tmp, "zlib")) && ((tmp2 < method_comp) || (!method_comp)) && 
								(tmp2[4] == ',' || tmp2[4] == '\0')) {
							method_comp = tmp2;	 /* found more preferable method */
							j->using_compress = JABBER_COMPRESSION_ZLIB_INIT;
						}
#else
						debug_error("[jabber] compression... NO ZLIB support\n");
#endif
					} else if (!xstrcmp(method->data, "lzw")) {
						if ((tmp2 = xstrstr(tmp, "zlib")) && ((tmp2 < method_comp) || (!method_comp)) &&
								(tmp2[3] == ',' || tmp2[3] == '\0')) {
							/* method_comp = tmp2 */			/* nieczynne */
							/* j->using_compress = JABBER_COMPRESSION_LZW_INIT; */
						}
						debug_error("[jabber] compression... sorry NO LZW support\n");
					} else	debug_error("[jabber] compression %s\n", __(method->data));

				} else debug_error("[jabber] stream:features/compression %s\n", __(method->name));
			}
			debug_function("[jabber] compression, method: %d\n", j->using_compress);

			if (!j->using_compress) continue;

			if (j->using_compress == JABBER_COMPRESSION_ZLIB_INIT)		method_comp = "zlib";
			else if (j->using_compress == JABBER_COMPRESSION_LZW_INIT)	method_comp = "lzw";
			else {
				debug_error("[jabber] BLAH [%s:%d] %s; %d\n", __FILE__, __LINE__, method_comp, j->using_compress);
				continue;
			}

			watch_write(j->send_watch, 
					"<compress xmlns=\"http://jabber.org/protocol/compress\"><method>%s</method></compress>", method_comp);
			return;
		} else {
			debug_error("[jabber] stream:features %s\n", __(n->name));
		}
	}

	if (j->send_watch) j->send_watch->transfer_limit = -1;

	if (use_fjuczers & 2) {	/* bind */
		char *resource = jabber_escape(j->resource);
		watch_write(j->send_watch, 
				"<iq type=\"set\" id=\"bind%d\"><bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\"><resource>%s</resource></bind></iq>", 
				j->id++, resource);
		xfree(resource);
	}
	if (use_fjuczers & 1)	/* session */
		watch_write(j->send_watch, 
				"<iq type=\"set\" id=\"auth\"><session xmlns=\"urn:ietf:params:xml:ns:xmpp-session\"/></iq>",
				j->id++);

	if (j->connecting == 2 && !(use_fjuczers & 2)) {	/* STRANGE, BUT MAYBE POSSIBLE? */
		jabber_session_connected(s);
	}
	
	/* i think it's done here.. */
	if (j->connecting == 2 && display_fjuczers)
		session_int_set(s, "__features_displayed", 1);

	JABBER_COMMIT_DATA(j->send_watch);	

	if (use_sasl && mech_node) {
		enum {
			JABBER_SASL_AUTH_UNKNOWN,			/* UNKNOWN */
			JABBER_SASL_AUTH_PLAIN,				/* PLAIN */
			JABBER_SASL_AUTH_DIGEST_MD5,			/* DIGEST-MD5 */
		} auth_type = JABBER_SASL_AUTH_UNKNOWN;

		for (; mech_node; mech_node = mech_node->next) {
			if (!xstrcmp(mech_node->name, "mechanism")) {

				if (!xstrcmp(mech_node->data, "DIGEST-MD5"))	auth_type = JABBER_SASL_AUTH_DIGEST_MD5;
				else if (!xstrcmp(mech_node->data, "PLAIN")) {
					if ((session_int_get(s, "plaintext_passwd"))) {
						auth_type = JABBER_SASL_AUTH_PLAIN;
						break;	/* jesli plaintext jest prefered wychodzimy */
					}
					/* ustaw tylko wtedy gdy nie ma ustawionego, wolimy MD5 */
					if (auth_type == JABBER_SASL_AUTH_UNKNOWN) auth_type = JABBER_SASL_AUTH_PLAIN;

				} else debug_error("[jabber] SASL, unk mechanism: %s\n", __(mech_node->data));
			} else debug_error("[jabber] SASL mechanisms: %s\n", mech_node->name);
		}

		if (auth_type != JABBER_SASL_AUTH_UNKNOWN) 
			j->connecting = 2;

		switch (auth_type) {
			string_t str;
			char *encoded;

			case JABBER_SASL_AUTH_DIGEST_MD5:
				debug_function("[jabber] SASL chosen: JABBER_SASL_AUTH_DIGEST_MD5\n");
				watch_write(j->send_watch, 
					"<auth xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\" mechanism=\"DIGEST-MD5\"/>");
			break;
			case JABBER_SASL_AUTH_PLAIN:
				debug_function("[jabber] SASL chosen: JABBER_SASL_AUTH_PLAIN\n");
				str = string_init(NULL);

				string_append_raw(str, "\0", 1);
				string_append_n(str, s->uid+4, xstrchr(s->uid+4, '@')-s->uid-4);
				string_append_raw(str, "\0", 1);
				string_append(str, session_get(s, "password"));

				encoded = base64_encode(str->str, str->len);

				watch_write(j->send_watch,
					"<auth xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\" mechanism=\"PLAIN\">%s</auth>", encoded);

				xfree(encoded);
				string_free(str, 1);
			break;
			default:
				debug_error("[jabber] SASL auth_type: UNKNOWN!!!, disconnecting from host.\n");
				j->parser = NULL; jabber_handle_disconnect(s, 
						"We tried to auth using SASL but none of method supported by server we know. "
						"Check __debug window and supported SASL server auth methods and sent them to ekg2 devs. "
						"Temporary you can turn off SASL auth using by setting disable_sasl to 1 or 2. "
						"/session disable_sasl 2", EKG_DISCONNECT_FAILURE);
			break;
		}
	}
}

JABBER_HANDLER(jabber_handle_compressed) {
	jabber_private_t *j = s->priv;

	CHECK_CONNECT(1, 0, return)
	CHECK_XMLNS(n, "http://jabber.org/protocol/compress", return)

	/* REINITIALIZE STREAM WITH COMPRESSION TURNED ON */

	switch (j->using_compress) {
		case JABBER_COMPRESSION_NONE: 		break;
		case JABBER_COMPRESSION_ZLIB_INIT: 	j->using_compress = JABBER_COMPRESSION_ZLIB;	break;
		case JABBER_COMPRESSION_LZW_INIT:	j->using_compress = JABBER_COMPRESSION_LZW;	break;

		default:
							debug_error("[jabber] invalid j->use_compression (%d) state..\n", j->using_compress);
							j->using_compress = JABBER_COMPRESSION_NONE;
	}

	if (j->using_compress == JABBER_COMPRESSION_NONE) {
		debug_error("[jabber] j->using_compress == JABBER_COMPRESSION_NONE but, compressed stanza?\n");
		return;
	}

	j->parser = jabber_parser_recreate(NULL, XML_GetUserData(j->parser));
	j->send_watch->handler	= jabber_handle_write;

	watch_write(j->send_watch,
			"<stream:stream to=\"%s\" xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\" version=\"1.0\">",
			j->server);
}

JABBER_HANDLER(jabber_handle_challenge) {
	jabber_private_t *j =  s->priv;

	char *data;
	char **arr;
	int i;

	char *realm	= NULL;
	char *rspauth	= NULL;
	char *nonce	= NULL;

	CHECK_CONNECT(2, 0, return)
	CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-sasl", return)

	if (!n->data) {
		debug_error("[jabber] challenge, no data.. (XXX?) disconnecting from host.\n");
		return;
	}

	/* decode && parse challenge data */
	data = base64_decode(n->data);
	debug_error("[jabber] PARSING challange (%s): \n", data);
	arr = array_make(data, "=,", 0, 1, 1);	/* maybe we need to change/create another one parser... i'm not sure. please notify me, 
						   I'm lazy, sorry */
	/* for chrome.pl and jabber.autocom.pl it works */
	xfree(data);

	/* check parsed data... */
	i = 0;
	while (arr[i]) {
		debug_error("[%d] %s: %s\n", i / 2, arr[i], __(arr[i+1]));
		if (!arr[i+1]) {
			debug_error("Parsing var<=>value failed, NULL....\n");
			array_free(arr);
			j->parser = NULL; 
			jabber_handle_disconnect(s, "IE, Current SASL support for ekg2 cannot handle with this data, sorry.", EKG_DISCONNECT_FAILURE);
			return;
		}

		if (!xstrcmp(arr[i], "realm"))		realm	= arr[i+1];
		if (!xstrcmp(arr[i], "rspauth"))	rspauth	= arr[i+1];
		if (!xstrcmp(arr[i], "nonce"))		nonce	= arr[i+1];

		i++;
		if (arr[i]) i++;
	}

	if (rspauth) {
		const char *tmp = session_get(s, "__sasl_excepted");

		if (!xstrcmp(tmp, rspauth)) {
			debug_function("[jabber] KEYS MATCHED, THX FOR USING SASL SUPPORT IN EKG2.\n");
			watch_write(j->send_watch, "<response xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\"/>");
		} else {
			debug_error("[jabber] RSPAUTH BUT KEYS DON'T MATCH!!! IS: %s EXCEPT: %s, DISCONNECTING\n", __(rspauth), __(tmp));
			j->parser = NULL; jabber_handle_disconnect(s, "IE, SASL RSPAUTH DOESN'T MATCH!!", EKG_DISCONNECT_FAILURE);
		}
		session_set(s, "__sasl_excepted", NULL);
	} else {
		char *username = xstrndup(s->uid+4, xstrchr(s->uid+4, '@')-s->uid-4);
		const char *password = session_get(s, "password");

		string_t str = string_init(NULL);	/* text to encode && sent */
		char *encoded;				/* BASE64 response text */

		char tmp_cnonce[32];
		char *cnonce;

		char *xmpp_temp;
		char *auth_resp;

		if (!realm) realm = j->server;

		for (i=0; i < sizeof(tmp_cnonce); i++) tmp_cnonce[i] = (char) (256.0*rand()/(RAND_MAX+1.0));	/* generate random number using high-order bytes man 3 rand() */

		cnonce = base64_encode(tmp_cnonce, sizeof(tmp_cnonce));

		xmpp_temp	= saprintf(":xmpp/%s", realm);
		auth_resp	= jabber_challange_digest(username, password, nonce, cnonce, xmpp_temp, realm);
		session_set(s, "__sasl_excepted", auth_resp);
		xfree(xmpp_temp);

		xmpp_temp	= saprintf("AUTHENTICATE:xmpp/%s", realm);
		auth_resp	= jabber_challange_digest(username, password, nonce, cnonce, xmpp_temp, realm);
		xfree(xmpp_temp);

		string_append(str, "username=\"");	string_append(str, username);	string_append_c(str, '\"');
		string_append(str, ",realm=\"");	string_append(str, realm);	string_append_c(str, '\"');
		string_append(str, ",nonce=\"");	string_append(str, nonce);	string_append_c(str, '\"');
		string_append(str, ",cnonce=\"");	string_append(str, cnonce);	string_append_c(str, '\"');
		string_append(str, ",nc=00000001");
		string_append(str, ",digest-uri=\"xmpp/"); string_append(str, realm);	string_append_c(str, '\"');
		string_append(str, ",qop=auth");
		string_append(str, ",response=");	string_append(str, auth_resp);
		string_append(str, ",charset=utf-8");

		encoded = base64_encode(str->str, str->len);
		watch_write(j->send_watch, "<response xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\">%s</response>", encoded);
		xfree(encoded);

		string_free(str, 1);
		xfree(username);
		xfree(cnonce);
	}
	array_free(arr);
}

/**
 * jabber_handle()
 *
 * It handle:
 * 	- <i>message</i> using jabber_handle_message()
 * 	- <i>iq</i> using jabber_handle_iq()
 *	- <i>presence</i> using jabber_handle_presence()
 *	- <i>stream:features</i> using jabber_handle_stream_features()
 *	- <i>stream:error</i>
 *	- <i>challenge</i> using jabber_handle_challenge()
 *	- <i>compressed</i> using jabber_handle_compressed()
 *	- <i>proceed</i> xmlns:<i>urn:ietf:params:xml:ns:xmpp-tls</i> using jabber_handle_connect_ssl()
 *	- <i>success</i> xmlns:<i>urn:ietf:params:xml:ns:xmpp-sasl</i>
 *	- <i>failure</i> xmlns:<i>urn:ietf:params:xml:ns:xmpp-sasl</i>
 *	- else if tlen protocol than forward to tlen_handle()
 *	- else print error in __debug window.
 *
 * @todo See XXX's
 */

void jabber_handle(void *data, xmlnode_t *n) {
	session_t *s = (session_t *) data;
        jabber_private_t *j;

        if (!s || !(j = s->priv) || !n) {
                debug_error("jabber_handle() invalid parameters\n");
                return;
        }

        if (!xstrcmp(n->name, "message")) {
		jabber_handle_message(s, n);
		return;
	}

	if (!xstrcmp(n->name, "iq")) {
		jabber_handle_iq(s, n);
		return;
	}

	if (!xstrcmp(n->name, "presence")) {
		jabber_handle_presence(s, n);
		return;
	}

	if (!xstrcmp(n->name, "stream:features")) {
		jabber_handle_stream_features(s, n);
		return;
	}

	if (!xstrcmp(n->name, "challenge")) {
		jabber_handle_challenge(s, n);
		return;
	}

	if (!xstrcmp(n->name, "compressed")) {	/* COMPRESSION HERE */
		jabber_handle_compressed(s, n);
		return;
	}

	if (!xstrcmp(n->name, "proceed")) {
		CHECK_CONNECT(1, 0, return)

		if (!xstrcmp(jabber_attr(n->atts, "xmlns"), "urn:ietf:params:xml:ns:xmpp-tls")) {
#ifdef JABBER_HAVE_SSL
			debug_function("[jabber] proceed urn:ietf:params:xml:ns:xmpp-tls TLS let's rock\n");

			/* XXX HERE WE SHOULD DISABLE RECV_WATCH && (SEND WATCH TOO?) */
			// j->send_watch->type = WATCH_NONE;

			jabber_handle_connect_ssl(-1, j->fd, WATCH_NONE, s);
#else
			debug_error("[jabber] proceed + urn:ietf:params:xml:ns:xmpp-tls but jabber compilated without ssl support?\n");
#endif
		} else	debug_error("[jabber] proceed what's that xmlns: %s ?\n", jabber_attr(n->atts, "xmlns"));
		return;
	}

	if (!xstrcmp(n->name, "success")) {
		CHECK_CONNECT(2, 0, return)
		CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-sasl", return)

		j->parser = jabber_parser_recreate(NULL, XML_GetUserData(j->parser));	/* here could be passed j->parser to jabber_parser_recreate() but unfortunetly expat makes SIGSEGV */
		watch_write(j->send_watch, 
				"<stream:stream to=\"%s\" xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\" version=\"1.0\">", 
				j->server);
		return;
	} 

	if (!xstrcmp(n->name, "failure")) {
		char *reason;

		CHECK_CONNECT(2, 0, return)
		CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-sasl", return)

		reason = n->children ? n->children->name : NULL;
		debug_function("[jabber] failure n->child: 0x%x n->child->name: %s\n", n->children, __(reason));

/* XXX here, think about nice reasons */
		if (!reason)						reason = "(SASL) GENERIC FAILURE";
		else if (!xstrcmp(reason, "temporary-auth-failure"))	reason = "(SASL) TEMPORARY AUTH FAILURE";
		else debug_error("[jabber] UNKNOWN reason: %s\n", reason);

		j->parser = NULL; jabber_handle_disconnect(s, reason, EKG_DISCONNECT_FAILURE);
		
		return;
	}

	if (!xstrcmp(n->name, "stream:error")) {	/* BIG XXX, EKG_DISCONNECT_FAILURE bad here */
		xmlnode_t *text = xmlnode_find_child(n, "text");
		char *text2 = NULL;

		if (text && text->data)
			text2 = jabber_unescape(text->data);

		j->parser = NULL; jabber_handle_disconnect(s, text2 ? text2 : n->children ? n->children->name : "stream:error XXX", EKG_DISCONNECT_FAILURE);
		xfree(text2);
		return;
	}

	if (j->istlen) {
		tlen_handle(s, n);
		return;
	}

	debug_error("[jabber] what's that: %s ?\n", n->name);
}

JABBER_HANDLER(jabber_handle_message) {
	jabber_private_t *j = s->priv;

	xmlnode_t *nerr		= xmlnode_find_child(n, "error");
	xmlnode_t *nbody   	= xmlnode_find_child(n, "body");
	xmlnode_t *nsubject	= NULL;
	xmlnode_t *xitem;
	
	const char *from = jabber_attr(n->atts, "from");
	char *x_encrypted = NULL;

	char *juid 	= jabber_unescape(from); /* was tmp */
	char *uid;
	time_t bsent = 0;
	string_t body;
	int new_line = 0;	/* if there was headlines do we need to display seperator between them and body? */
	
	if (j->istlen)	uid = saprintf("tlen:%s", juid);
	else		uid = saprintf("jid:%s", juid);

	xfree(juid);

	if (nerr) {
		char *ecode = jabber_attr(nerr->atts, "code");
		char *etext = jabber_unescape(nerr->data);
		char *recipient = get_nickname(s, uid);

		if (nbody && nbody->data) {
			char *tmp2 = jabber_unescape(nbody->data);
			char *mbody = xstrndup(tmp2, 15);
			xstrtr(mbody, '\n', ' ');

			print("jabber_msg_failed_long", recipient, ecode, etext, mbody);

			xfree(mbody);
			xfree(tmp2);
		} else
			print("jabber_msg_failed", recipient, ecode, etext);

		xfree(etext);
		xfree(uid);
		return;
	} /* <error> */

	if (j->istlen) {	/* disable typing notify, tlen protocol doesn't send info about it (?) XXX */
		char *session 	= xstrdup(session_uid_get(s));
		char *rcpt	= xstrdup(uid);
		int state 	= 0;
		int stateo 	= EKG_XSTATE_TYPING;

		query_emit_id(NULL, PROTOCOL_XSTATE, &session, &rcpt, &state, &stateo);

		xfree(session);
		xfree(rcpt);
	}
	
	body = string_init("");

	for (xitem = n->children; xitem; xitem = xitem->next) {
		if (!xstrcmp(xitem->name, "x")) {
			const char *ns = jabber_attr(xitem->atts, "xmlns");
			
			if (!xstrcmp(ns, "jabber:x:encrypted")) {	/* JEP-0027 */
				x_encrypted = xstrdup(xitem->data);
				char *error = NULL;

				if (!(x_encrypted = jabber_openpgp(s, uid, JABBER_OPENGPG_DECRYPT, x_encrypted, NULL, &error))) {
					string_append(body, "Encrypted message but error: ");
					string_append(body, error);
					string_append_c(body, '\n');
					new_line = 1;
				}
				xfree(error);

			} else if (!xstrncmp(ns, "jabber:x:event", 14)) {
				int acktype = 0; /* bitmask: 2 - queued ; 1 - delivered */
				int isack;

				if (xmlnode_find_child(xitem, "delivered"))	acktype |= 1;	/* delivered */
				if (xmlnode_find_child(xitem, "offline"))	acktype	|= 2;	/* queued */
				if (xmlnode_find_child(xitem, "composing"))	acktype |= 4;	/* composing */

				isack = (acktype & 1) || (acktype & 2);

				/* jesli jest body, to mamy do czynienia z prosba o potwierdzenie */
				if (nbody && isack) {
					char *id = jabber_attr(n->atts, "id");
					const int our_status = session_status_get(s);

					if (j->send_watch) j->send_watch->transfer_limit = -1;

					watch_write(j->send_watch, "<message to=\"%s\"><x xmlns=\"jabber:x:event\">", from);

					if (our_status == EKG_STATUS_INVISIBLE) {
						watch_write(j->send_watch, "<offline/>");
					} else {
						if (acktype & 1)
							watch_write(j->send_watch, "<delivered/>");
						if (acktype & 2)
							watch_write(j->send_watch, "<displayed/>");
					};
					watch_write(j->send_watch, "<id>%s</id></x></message>", id);

					JABBER_COMMIT_DATA(j->send_watch);
				}
				/* je¶li body nie ma, to odpowiedz na nasza prosbe */
				if (!nbody && isack) {
					char *__session = xstrdup(session_uid_get(s));
					char *__rcpt	= xstrdup(uid); /* was uid+4 */
					int __status  = ((acktype & 1) ? EKG_ACK_DELIVERED : 
							(acktype & 2) ? EKG_ACK_QUEUED : 
							EKG_ACK_UNKNOWN);
					char *__seq	= NULL; /* id ? */
					/* protocol_message_ack; sesja ; uid + 4 ; seq (NULL ? ) ; status - delivered ; queued ) */
					query_emit_id(NULL, PROTOCOL_MESSAGE_ACK, &__session, &__rcpt, &__seq, &__status);
					xfree(__session);
					xfree(__rcpt);
					/* xfree(__seq); */
				}
				
				{
					char *__session = xstrdup(session_uid_get(s));
					char *__rcpt	= xstrdup(uid);
					int  state	= (!nbody && (acktype & 4) ? EKG_XSTATE_TYPING : 0);
					int  stateo	= (!state ? EKG_XSTATE_TYPING : 0);

					query_emit_id(NULL, PROTOCOL_XSTATE, &__session, &__rcpt, &state, &stateo);
					
					xfree(__session);
					xfree(__rcpt);
				}
/* jabber:x:event */	} else if (!xstrncmp(ns, "jabber:x:oob", 12)) {
				xmlnode_t *xurl;
				xmlnode_t *xdesc;

				if ((xurl = xmlnode_find_child(xitem, "url"))) {
					string_append(body, "URL: ");
					string_append(body, xurl->data);
					if ((xdesc = xmlnode_find_child(xitem, "desc"))) {
						string_append(body, " (");
						string_append(body, xdesc->data);
						string_append(body, ")");
					}
					string_append(body, "\n");
					new_line = 1;
				}
/* jabber:x:oob */	} else if (!xstrncmp(ns, "jabber:x:delay", 14)) {
				bsent = jabber_try_xdelay(jabber_attr(xitem->atts, "stamp"));
#if 0		/* XXX, fjuczer? */
				if (nazwa_zmiennej_do_formatowania_czasu) {
					/* some people don't have time in formats... and if we really do like in emails (first headlines than body) so display it too.. */
					stuct tm *tm = localtime(&bsent);
					char buf[100];
					string_append(body, "Sent: ");
					if (!strftime(buf, sizeof(buf), nazwa_zmiennej_do_formatowania_czasu, tm) 
						string_append(body, itoa(bsent);	/* if too long display seconds since the Epoch */
					else	string_append(body, buf);	/* otherwise display formatted time */
					new_line = 1;
				}
#endif
			} else debug_error("[JABBER, MESSAGE]: <x xmlns=%s\n", ns);
/* x */		} else if (!xstrcmp(xitem->name, "subject")) {
			nsubject = xitem;
			if (nsubject->data) {
				string_append(body, "Subject: ");
				string_append(body, nsubject->data);
				string_append(body, "\n");
				new_line = 1;
			}
/* subject */	} else if (!xstrcmp(xitem->name, "body")) {
		} /* XXX, JEP-0085 here */
		else if (!xstrcmp(jabber_attr(xitem->atts, "xmlns"), "http://jabber.org/protocol/chatstates")) {
			if (!xstrcmp(xitem->name, "active"))		{ }
			else if (!xstrcmp(xitem->name, "composing"))	{ } 
			else if (!xstrcmp(xitem->name, "paused"))	{ } 
			else if (!xstrcmp(xitem->name, "inactive"))	{ } 
			else if (!xstrcmp(xitem->name, "gone")) 	{ } 
			else debug_error("[JABBER, MESSAGE]: INVALID CHATSTATE: %s\n", xitem->name);
		} else debug_error("[JABBER, MESSAGE]: <%s\n", xitem->name);
	}
	if (new_line) string_append(body, "\n"); 	/* let's seperate headlines from message */

	if (x_encrypted) 
		string_append(body, x_encrypted);	/* encrypted message */
	else if (nbody)
		string_append(body, nbody->data);	/* unecrpyted message */

	if (nbody || nsubject) {
		const char *type = jabber_attr(n->atts, "type");

		char *me	= xstrdup(session_uid_get(s));
		int class 	= (!xstrcmp(type, "chat") || !xstrcmp(type, "groupchat") ? EKG_MSGCLASS_CHAT : EKG_MSGCLASS_MESSAGE);
		int ekgbeep 	= EKG_TRY_BEEP;
		int secure	= (x_encrypted != NULL);
		char **rcpts 	= NULL;
		char *seq 	= NULL;
		uint32_t *format= NULL;
		time_t sent	= bsent;

		char *text = tlenjabber_unescape(body->str);

		if (!sent) sent = time(NULL);

		debug_function("[jabber,message] type = %s\n", type);
		if (!xstrcmp(type, "groupchat")) {
			char *tuid = xstrrchr(uid, '/');				/* temporary */
			char *uid2 = (tuid) ? xstrndup(uid, tuid-uid) : xstrdup(uid);		/* muc room */
			char *nick = (tuid) ? xstrdup(tuid+1) : NULL;				/* nickname */
			newconference_t *c = newconference_find(s, uid2);
			int isour = (c && !xstrcmp(c->private, nick)) ? 1 : 0;			/* is our message? */
			char *formatted;

		/* jesli (bsent != 0) wtedy mamy do czynienia z backlogiem */

			class	|= EKG_NO_THEMEBIT;
			ekgbeep	= EKG_NO_BEEP;

			formatted = format_string(format_find(isour ? "jabber_muc_send" : "jabber_muc_recv"),
				session_name(s), uid2, nick ? nick : uid2+4, text, "");
			
			debug("[MUC,MESSAGE] uid2:%s uuid:%s message:%s\n", uid2, nick, text);
			query_emit_id(NULL, PROTOCOL_MESSAGE, &me, &uid, &rcpts, &formatted, &format, &sent, &class, &seq, &ekgbeep, &secure);

			xfree(uid2);
			xfree(nick);
			xfree(formatted);
		} else {
			query_emit_id(NULL, PROTOCOL_MESSAGE, &me, &uid, &rcpts, &text, &format, &sent, &class, &seq, &ekgbeep, &secure);
		}

		xfree(me);
		xfree(text);
		array_free(rcpts);
/*
		xfree(seq);
		xfree(format);
*/
	}
	xfree(x_encrypted);

	string_free(body, 1);
	xfree(uid);
} /* */

/* idea and some code copyrighted by Marek Marczykowski jid:marmarek@jabberpl.org */

/* handlue <x xmlns=jabber:x:data type=form */
static void jabber_handle_xmldata_form(session_t *s, const char *uid, const char *command, xmlnode_t *form, const char *param) { /* JEP-0004: Data Forms */
	xmlnode_t *node;
	int fieldcount = 0;
/*	const char *FORM_TYPE = NULL; */

	for (node = form; node; node = node->next) {
		if (!xstrcmp(node->name, "title")) {
			char *title = jabber_unescape(node->data);
			print("jabber_form_title", session_name(s), uid, title);
			xfree(title);
		} else if (!xstrcmp(node->name, "instructions")) {
			char *inst = jabber_unescape(node->data);
			print("jabber_form_instructions", session_name(s), uid, inst);
			xfree(inst);
		} else if (!xstrcmp(node->name, "field")) {
			xmlnode_t *child;
			char *label	= jabber_unescape(jabber_attr(node->atts, "label"));
			char *var	= jabber_unescape(jabber_attr(node->atts, "var"));
			char *def_option = NULL;
			string_t sub = NULL;
			int subcount = 0;

			int isreq = 0;	/* -1 - optional; 1 - required */
			
			if (!fieldcount) print("jabber_form_command", session_name(s), uid, command, param);

			for (child = node->children; child; child = child->next) {
				if (!xstrcmp(child->name, "required")) isreq = 1;
				else if (!xstrcmp(child->name, "value")) {
					xfree(def_option); 
					def_option	= jabber_unescape(child->data); 
				} 
				else if (!xstrcmp(child->name, "option")) {
					xmlnode_t *tmp;
					char *opt_value = jabber_unescape( (tmp = xmlnode_find_child(child, "value")) ? tmp->data : NULL);
					char *opt_label = jabber_unescape(jabber_attr(child->atts, "label"));
					char *fritem;

					fritem = format_string(format_find("jabber_form_item_val"), session_name(s), uid, opt_value, opt_label);
					if (!sub)	sub = string_init(fritem);
					else		string_append(sub, fritem);
					xfree(fritem);

/*					print("jabber_form_item_sub", session_name(s), uid, opt_label, opt_value); */
/*					debug("[[option]] [value] %s [label] %s\n", opt_value, opt_label); */

					xfree(opt_value);
					xfree(opt_label);
					subcount++;
					if (!(subcount % 4)) string_append(sub, "\n\t");
				} else debug_error("[jabber] wtf? FIELD->CHILD: %s\n", child->name);
			}

			print("jabber_form_item", session_name(s), uid, label, var, def_option, 
				isreq == -1 ? "X" : isreq == 1 ? "V" : " ");

			if (sub && sub->len > 1) {
				int len = sub->len;
				if (sub->str[len-1] == '\t' && sub->str[len-2] == '\n') sub->str[len-2] = 0;
				print("jabber_form_item_sub", session_name(s), uid, sub->str);
				string_free(sub, 1);
			}
			fieldcount++;
			xfree(var);
			xfree(label);
		}
	}
	if (!fieldcount) print("jabber_form_command", session_name(s), uid, command);
	print("jabber_form_end", session_name(s), uid, command, param);
}

/* handluje <x xmlns=jabber:x:data type=submit */
static int jabber_handle_xmldata_submit(session_t *s, xmlnode_t *form, const char *FORM_TYPE, int alloc, ...) {
	char **atts	= NULL;
	int valid	= 0;
	int count	= 0;
	char *vatmp;
	va_list ap;

	va_start(ap, alloc);

	if (!alloc) while ((vatmp = va_arg(ap, char *))) { 
		atts		= (char **) xrealloc(atts, sizeof(char *) * (count + 3));
		atts[count]	= xstrdup(vatmp);
		atts[count+1]	= (char *) va_arg(ap, char **);					/* here is char ** */
		atts[count+2]	= NULL;
		count += 2;
	}

	for (; form; form = form->next) {
		if (!xstrcmp(form->name, "field")) {
			int quiet = 0;
			const char *vartype	= jabber_attr(form->atts, "type"); 
			const char *varname	= jabber_attr(form->atts, "var");
			char *value		= jabber_unescape(form->children ? form->children->data : NULL);
			char **tmp; 
							
			if (FORM_TYPE && (!xstrcmp(varname, "FORM_TYPE") && !xstrcmp(vartype, "hidden") && !xstrcmp(value, FORM_TYPE))) { valid = 1; quiet = 1;	}
			if ((tmp = (char **) jabber_attr(atts, varname))) {
				if (!alloc)	{ xfree(*tmp);		*tmp = value; }			/* here is char ** */
				else 		{ xfree((char *) tmp);	 tmp = (char **) value; }	/* here is char * */
				value	= NULL;
			} else if (alloc) {
				atts            = (char **) xrealloc(atts, sizeof(char *) * (count + 3));
				atts[count]     = xstrdup(varname);					
				atts[count+1]	= value;						/* here is char * */
				atts[count+2]	= NULL;
				count += 2;
				value = NULL;
			} else if (!quiet) debug_error("JABBER, RC, FORM_TYPE: %s ATTR NOT IN ATTS: %s (SOMEONE IS DOING MESS WITH FORM_TYPE?)\n", FORM_TYPE, varname);
			xfree(value);
		}
	}
	if (alloc)	(*(va_arg(ap, char ***))) = atts;
	va_end(ap);
	return valid;
}

/* handlue <x xmlns=jabber:x:data type=result */
static void jabber_handle_xmldata_result(session_t *s, xmlnode_t *form, const char *uid) {
	int print_end = 0;
	char **labels = NULL;
	int labels_count = 0;

	for (; form; form = form->next) {
		if (!xstrcmp(form->name, "title")) {
			char *title = jabber_unescape(form->data);
			print("jabber_form_title", session_name(s), uid, title);
			print_end = 1;
			xfree(title);
		} else if (!xstrcmp(form->name, "item")) {
			xmlnode_t *q;
			print("jabber_form_item_beg", session_name(s), uid);
			for (q = form->children; q; q = q->next) {
				if (!xstrcmp(q->name, "field")) {
					xmlnode_t *temp;
					char *var = jabber_attr(q->atts, "var");
					char *tmp = jabber_attr(labels, var);
					char *val = ((temp = xmlnode_find_child(q, "value"))) ? jabber_unescape(temp->data) : NULL;

					print("jabber_form_item_plain", session_name(s), uid, tmp ? tmp : var, var, val);
					xfree(val);
				}
			}
			print("jabber_form_item_end", session_name(s), uid);
		} else if (!xstrcmp(form->name, "reported")) {
			xmlnode_t *q;
			for (q = form->children; q; q = q->next) {
				if (!xstrcmp(q->name, "field")) {
					labels				= (char **) xrealloc(labels, (sizeof(char *) * ((labels_count+1) * 2)) + 1);
					labels[labels_count*2]		= xstrdup(jabber_attr(q->atts, "var"));
					labels[labels_count*2+1]	= jabber_unescape(jabber_attr(q->atts,"label"));
					labels[labels_count*2+2]	= NULL;
					labels_count++;
				}
			}
		} else if (!xstrcmp(form->name, "field")) {
			xmlnode_t *temp;
			char *var	= jabber_attr(form->atts, "var");
			char *label	= jabber_unescape(jabber_attr(form->atts, "label"));
			char *val	= jabber_unescape(((temp = xmlnode_find_child(form, "value"))) ? temp->data : NULL);

			print("jabber_privacy_list_item" /* XXX */, session_name(s), uid, label ? label : var, val);
			xfree(label); xfree(val);
		} else debug_error("jabber_handle_xmldata_result() name: %s\n", form->name);
	}
	if (print_end) print("jabber_form_end", session_name(s), uid, "");
	array_free(labels);
}

/**
 * jabber_handle_iq_get_disco()
 *
 * Handler for IQ GET QUERY xmlns="http://jabber.org/protocol/disco#items"<br>
 * Send some info about what ekg2 can do/know with given node [node= in n->atts]<br>
 * XXX info about it in XEP/RFC
 *
 * @todo 	We send here only info about node: http://jabber.org/protocol/commands
 * 		Be more XEP/RFC compilant... return error if node not known, return smth
 * 		what we can do at all. etc. etc.
 */

JABBER_HANDLER_GET_REPLY(jabber_handle_iq_get_disco) {
	if (!xstrcmp(jabber_attr(n->atts, "node"), "http://jabber.org/protocol/commands")) {	/* jesli node commandowe */
		/* XXX, check if $uid can do it */
		watch_write(j->send_watch, 
				"<iq to=\"%s\" type=\"result\" id=\"%s\">"
				"<query xmlns=\"http://jabber.org/protocol/disco#items\" node=\"http://jabber.org/protocol/commands\">"
				"<item jid=\"%s/%s\" name=\"Set Status\" node=\"http://jabber.org/protocol/rc#set-status\"/>"
				"<item jid=\"%s/%s\" name=\"Forward Messages\" node=\"http://jabber.org/protocol/rc#forward\"/>"
				"<item jid=\"%s/%s\" name=\"Set Options\" node=\"http://jabber.org/protocol/rc#set-options\"/>"
				"<item jid=\"%s/%s\" name=\"Set ALL ekg2 Options\" node=\"http://ekg2.org/jabber/rc#ekg-set-all-options\"/>"
				"<item jid=\"%s/%s\" name=\"Manage ekg2 plugins\" node=\"http://ekg2.org/jabber/rc#ekg-manage-plugins\"/>"
				"<item jid=\"%s/%s\" name=\"Manage ekg2 plugins\" node=\"http://ekg2.org/jabber/rc#ekg-manage-sessions\"/>"
				"<item jid=\"%s/%s\" name=\"Execute ANY command in ekg2\" node=\"http://ekg2.org/jabber/rc#ekg-command-execute\"/>"
				"</query></iq>", from, id, 
				s->uid+4, j->resource, s->uid+4, j->resource, 
				s->uid+4, j->resource, s->uid+4, j->resource,
				s->uid+4, j->resource, s->uid+4, j->resource,
				s->uid+4, j->resource);
		return;
	}
	/* XXX, tutaj jakies ogolne informacje co umie ekg2 */
}

JABBER_HANDLER_RESULT(jabber_handle_iq_result_disco) {
	xmlnode_t *item = xmlnode_find_child(n, "item");
	char *uid = jabber_unescape(from);

	int iscontrol = !xstrncmp(id, "control", 7);

	if (item) {
		int i = 1;
		print(iscontrol ? "jabber_remotecontrols_list_begin" : "jabber_transport_list_begin", session_name(s), uid);
		for (; item; item = item->next, i++) {
			char *sjid  = jabber_unescape(jabber_attr(item->atts, "jid"));
			char *sdesc = jabber_unescape(jabber_attr(item->atts, "name"));
			char *snode = jabber_unescape(jabber_attr(item->atts, "node"));

			if (!iscontrol)	print(snode ? "jabber_transport_list_item_node" : "jabber_transport_list_item", 
					session_name(s), uid, sjid, snode, sdesc, itoa(i));
			else		print("jabber_remotecontrols_list_item", 
					session_name(s), uid, sjid, snode, sdesc, itoa(i));
			xfree(sdesc);
			xfree(sjid);
			xfree(snode);
		}
		print(iscontrol ? "jabber_remotecontrols_list_end"	: "jabber_transport_list_end", session_name(s), uid);
	} else	print(iscontrol ? "jabber_remotecontrols_list_nolist" : "jabber_transport_list_nolist", session_name(s), uid);
	xfree(uid);
}

/**
 * jabber_handle_iq_get_disco_info()
 *
 * Handler for IQ GET QUERY xmlns="http://jabber.org/protocol/disco#info"<br>
 * XXX
 *
 */

JABBER_HANDLER_GET_REPLY(jabber_handle_iq_get_disco_info) {
	watch_write(j->send_watch, "<iq to=\"%s\" type=\"result\" id=\"%s\">"
			"<query xmlns=\"http://jabber.org/protocol/disco#info\">"
			"<feature var=\"http://jabber.org/protocol/commands\"/>"
			"<feature var=\"http://jabber.org/protocol/bytestreams\"/>"
			"<feature var=\"http://jabber.org/protocol/si\"/>"
			"<feature var=\"http://jabber.org/protocol/si/profile/file-transfer\"/>"
			"</query></iq>", from, id);


}

JABBER_HANDLER_RESULT(jabber_handle_iq_result_disco_info) {
	const char *wezel = jabber_attr(n->atts, "node");
	char *uid = jabber_unescape(from);
	xmlnode_t *node;

	print((wezel) ? "jabber_transinfo_begin_node" : "jabber_transinfo_begin", session_name(s), uid, wezel);

	for (node = n->children; node; node = node->next) {
		if (!xstrcmp(node->name, "identity")) {
			char *name	= jabber_unescape(jabber_attr(node->atts, "name"));	/* nazwa */
//			char *cat	= jabber_attr(node->atts, "category");			/* server, gateway, directory */
//			char *type	= jabber_attr(node->atts, "type");			/* typ: im */

			if (name) /* jesli nie ma nazwy don't display it. */
				print("jabber_transinfo_identify" /* _server, _gateway... ? */, session_name(s), uid, name);

			xfree(name);
		} else if (!xstrcmp(node->name, "feature")) {
			char *var = jabber_attr(node->atts, "var");
			char *tvar = NULL; /* translated */
			int user_command = 0;

			/* dj, jakas glupota... ale ma ktos pomysl zeby to inaczej zrobic?... jeszcze istnieje pytanie czy w ogole jest sens to robic.. */
			if (!xstrcmp(var, "http://jabber.org/protocol/disco#info")) 		tvar = "/jid:transpinfo";
			else if (!xstrcmp(var, "http://jabber.org/protocol/disco#items")) 	tvar = "/jid:transports";
			else if (!xstrcmp(var, "http://jabber.org/protocol/disco"))		tvar = "/jid:transpinfo && /jid:transports";
			else if (!xstrcmp(var, "http://jabber.org/protocol/muc"))		tvar = "/jid:mucpart && /jid:mucjoin";
			else if (!xstrcmp(var, "http://jabber.org/protocol/stats"))		tvar = "/jid:stats";
			else if (!xstrcmp(var, "jabber:iq:register"))		    		tvar = "/jid:register";
			else if (!xstrcmp(var, "jabber:iq:search"))				tvar = "/jid:search";

			else if (!xstrcmp(var, "http://jabber.org/protocol/bytestreams")) { user_command = 1; tvar = "/jid:dcc (PROT: BYTESTREAMS)"; }
			else if (!xstrcmp(var, "http://jabber.org/protocol/si/profile/file-transfer")) { user_command = 1; tvar = "/jid:dcc"; }
			else if (!xstrcmp(var, "http://jabber.org/protocol/commands")) { user_command = 1; tvar = "/jid:control"; } 
			else if (!xstrcmp(var, "jabber:iq:version"))	{ user_command = 1;	tvar = "/jid:ver"; }
			else if (!xstrcmp(var, "jabber:iq:last"))	{ user_command = 1;	tvar = "/jid:lastseen"; }
			else if (!xstrcmp(var, "vcard-temp"))		{ user_command = 1;	tvar = "/jid:change && /jid:userinfo"; }
			else if (!xstrcmp(var, "message"))		{ user_command = 1;	tvar = "/jid:msg"; }

			else if (!xstrcmp(var, "http://jabber.org/protocol/vacation"))	{ user_command = 2;	tvar = "/jid:vacation"; }
			else if (!xstrcmp(var, "presence-invisible"))	{ user_command = 2;	tvar = "/invisible"; } /* we ought use jabber:iq:privacy */
			else if (!xstrcmp(var, "jabber:iq:privacy"))	{ user_command = 2;	tvar = "/jid:privacy"; }
			else if (!xstrcmp(var, "jabber:iq:private"))	{ user_command = 2;	tvar = "/jid:private && /jid:config && /jid:bookmark"; }

			if (tvar)	print(	user_command == 2 ? "jabber_transinfo_comm_not" : 
					user_command == 1 ? "jabber_transinfo_comm_use" : "jabber_transinfo_comm_ser", 

					session_name(s), uid, tvar, var);
			else		print("jabber_transinfo_feature", session_name(s), uid, var, var);
		} else if (!xstrcmp(node->name, "x") && !xstrcmp(jabber_attr(node->atts, "xmlns"), "jabber:x:data") && !xstrcmp(jabber_attr(node->atts, "type"), "result")) {
			jabber_handle_xmldata_result(s, node->children, uid);
		}
	}
	print("jabber_transinfo_end", session_name(s), uid);
	xfree(uid);
}

/**
 * jabber_handle_iq_get_last()
 *
 * Handler for IQ GET QUERY xmlns="jabber:iq:last"<br>
 * Send reply about our last activity.<br>
 * XXX info about it from XEP/RFC
 */

JABBER_HANDLER_GET_REPLY(jabber_handle_iq_get_last) {
	watch_write(j->send_watch, 
			"<iq to=\"%s\" type=\"result\" id=\"%s\">"
			"<query xmlns=\"jabber:iq:last\" seconds=\"%d\">"
			"</query></iq>", from, id, (time(NULL)-s->activity));
}

/**
 * jabber_handle_iq_result_last()
 *
 * Handler for IQ RESULT QUERY xmlns="jabber:iq:last"<br>
 * Display info about server uptime/client idle/client logout time.
 *
 * @todo It's ugly written.
 */

JABBER_HANDLER_RESULT(jabber_handle_iq_result_last) {
	const char *last = jabber_attr(n->atts, "seconds");
	char buff[21];
	char *from_str;
	int seconds;

	seconds = atoi(last);

	if ((seconds>=0) && (seconds < 999*24*60*60-1))
		/* days, hours, minutes, seconds... */
		snprintf(buff, sizeof(buff), _("%03dd %02dh %02dm %02ds"),seconds / 86400, \
				(seconds / 3600) % 24, (seconds / 60) % 60, seconds % 60);
	else
		strcpy(buff, _("very long"));

	from_str = (from) ? jabber_unescape(from) : NULL;

	print(
		(xstrchr(from_str, '/') ? "jabber_lastseen_idle" :	/* if we have resource than display -> user online+idle		*/
		xstrchr(from_str, '@') ? "jabber_lastseen_response" :	/* if we have '@' in jid: than display ->  user logged out	*/
					"jabber_lastseen_uptime"),	/* otherwise we have server -> server up for: 			*/
			jabberfix(from_str, "unknown"), buff);
	xfree(from_str);
}

/**
 * jabber_handle_iq_get_version()
 *
 * Handler for IQ GET QUERY xmlns="jabber:iq:version"<br>
 * Send info about our program and system<br>
 *
 * <b>PRIVACY WARNING:</b> It'll send potential useful information like: what version of kernel you use.<br>
 * If you don't want to send this information set session variables:<br>
 * 	- <i>ver_client_name</i> - If you want spoof program name. [Although I think it's good to send info about ekg2. Because it's good program.]<br>
 * 	- <i>ver_client_version</i> - If you want to spoof program version.<br>
 * 	- <i>ver_os</i> - The most useful, to spoof OS name, version and stuff.<br>
 */

JABBER_HANDLER_GET_REPLY(jabber_handle_iq_get_version) {
	const char *ver_os;
	const char *tmp;

	char *escaped_client_name	= jabber_escape(jabberfix((tmp = session_get(s, "ver_client_name")), DEFAULT_CLIENT_NAME));
	char *escaped_client_version	= jabber_escape(jabberfix((tmp = session_get(s, "ver_client_version")), VERSION));
	char *osversion;

	if (!(ver_os = session_get(s, "ver_os"))) {
		struct utsname buf;

		if (uname(&buf) != -1) {
			char *osver = saprintf("%s %s %s", buf.sysname, buf.release, buf.machine);
			osversion = jabber_escape(osver);
			xfree(osver);
		} else {
			osversion = xstrdup(("unknown")); /* uname failed and not ver_os session variable */
		}
	} else {
		osversion = jabber_escape(ver_os);	/* ver_os session variable */
	}

	watch_write(j->send_watch, "<iq to=\"%s\" type=\"result\" id=\"%s\">" 
			"<query xmlns=\"jabber:iq:version\">"
			"<name>%s</name>"
			"<version>%s</version>"
			"<os>%s</os></query></iq>", 
			from, id, 
			escaped_client_name, escaped_client_version, osversion);

	xfree(escaped_client_name);
	xfree(escaped_client_version);
	xfree(osversion);
}

/**
 * jabber_handle_iq_result_version()
 *
 * Handler for IQ RESULT QUERY xmlns="jabber:iq:version"<br>
 * Display info about smb program version and system<br>
 *
 */

JABBER_HANDLER_RESULT(jabber_handle_iq_result_version) {
	xmlnode_t *name = xmlnode_find_child(n, "name");
	xmlnode_t *version = xmlnode_find_child(n, "version");
	xmlnode_t *os = xmlnode_find_child(n, "os");

	char *from_str	= (from) ?	jabber_unescape(from) : NULL;
	char *name_str	= (name) ?	jabber_unescape(name->data) : NULL;
	char *version_str = (version) ? jabber_unescape(version->data) : NULL;
	char *os_str	= (os) ?	jabber_unescape(os->data) : NULL;

	print("jabber_version_response",
			jabberfix(from_str, "unknown"), jabberfix(name_str, "unknown"), 
			jabberfix(version_str, "unknown"), jabberfix(os_str, "unknown"));
	xfree(os_str);
	xfree(version_str);
	xfree(name_str);
	xfree(from_str);
}

/* XXX, check if this is really result. */
JABBER_HANDLER_RESULT(jabber_handle_iq_result_privacy) {
	jabber_private_t *j = s->priv;

	xmlnode_t *active 	= xmlnode_find_child(n, "active");
	xmlnode_t *def		= xmlnode_find_child(n, "default");
	char *activename   	= active ? jabber_attr(active->atts, "name") : NULL;
	char *defaultname 	= def ? jabber_attr(def->atts, "name") : NULL;

	xmlnode_t *node;
	int i = 0;

	for (node = n->children; node; node = node->next) {
		if (!xstrcmp(node->name, "list")) {
			if (node->children) {	/* Display all details */
				list_t *lista = NULL;
				xmlnode_t *temp;
				i = -1;

				if (session_int_get(s, "auto_privacylist_sync") != 0) {
					const char *list = session_get(s, "privacy_list");
					if (!list) list = "ekg2";

					if (!xstrcmp(list, jabber_attr(node->atts, "name"))) {
						jabber_privacy_free(j);
						lista = &(j->privacy);
					}
				}

				print("jabber_privacy_item_header", session_name(s), j->server, jabber_attr(node->atts, "name"));

				for (temp=node->children; temp; temp = temp->next) {	
					if (!xstrcmp(temp->name, "item")) {
						/* (we should sort it by order! XXX) (server send like it was set) */
						/* a lot TODO */
						xmlnode_t *iq		= xmlnode_find_child(temp, "iq");
						xmlnode_t *message	= xmlnode_find_child(temp, "message");
						xmlnode_t *presencein	= xmlnode_find_child(temp, "presence-in");
						xmlnode_t *presenceout	= xmlnode_find_child(temp, "presence-out");

						char *value = jabber_attr(temp->atts, "value");
						char *type  = jabber_attr(temp->atts, "type");
						char *action = jabber_attr(temp->atts, "action");
						char *jid, *formated;
						char *formatedx;

						if (!xstrcmp(type, "jid")) 		jid = saprintf("jid:%s", value);
						else if (!xstrcmp(type, "group")) 	jid = saprintf("@%s", value);
						else					jid = xstrdup(value);

						formated = format_string(
								format_find(!xstrcmp(action,"allow") ? "jabber_privacy_item_allow" : "jabber_privacy_item_deny"), jid);

						formatedx = format_string(
								format_find(!xstrcmp(action,"allow") ? "jabber_privacy_item_allow" : "jabber_privacy_item_deny"), "*");

						xfree(formatedx);
						formatedx = xstrdup("x");

						if (lista) {
							jabber_iq_privacy_t *item = xmalloc(sizeof(jabber_iq_privacy_t));

							item->type  = xstrdup(type);
							item->value = xstrdup(value);
							item->allow = !xstrcmp(action,"allow");

							if (iq)		item->items |= PRIVACY_LIST_IQ;
							if (message)	item->items |= PRIVACY_LIST_MESSAGE;
							if (presencein)	item->items |= PRIVACY_LIST_PRESENCE_IN;
							if (presenceout)item->items |= PRIVACY_LIST_PRESENCE_OUT;
							item->order = atoi(jabber_attr(temp->atts, "order"));

							list_add_sorted(lista, item, 0, jabber_privacy_add_compare);
						}

						print("jabber_privacy_item", session_name(s), j->server, 
								jabber_attr(temp->atts, "order"), formated, 
								message		? formatedx : " ",
								presencein	? formatedx : " ", 
								presenceout	? formatedx : " ", 
								iq		? formatedx : " ");

						xfree(formated);
						xfree(formatedx);
						xfree(jid);
					}
				}
				{	/* just help... */
					char *allowed = format_string(format_find("jabber_privacy_item_allow"), "jid:allowed - JID ALLOWED");
					char *denied  = format_string(format_find("jabber_privacy_item_deny"), "@denied - GROUP DENIED");
					print("jabber_privacy_item_footer", session_name(s), j->server, allowed, denied);
					xfree(allowed); xfree(denied);
				}
			} else { 		/* Display only name */
				int dn = 0;
				/* rekurencja, ask for this item (?) */
				if (!i) print("jabber_privacy_list_begin", session_name(s), j->server);
				if (i != -1) i++;
				if (active && !xstrcmp(jabber_attr(node->atts, "name"), activename)) {
					print("jabber_privacy_list_item_act", session_name(s), j->server, itoa(i), activename); dn++; }
				if (def && !xstrcmp(jabber_attr(node->atts, "name"), defaultname)) {
					print("jabber_privacy_list_item_def", session_name(s), j->server, itoa(i), defaultname); dn++; }
				if (!dn) print("jabber_privacy_list_item", session_name(s), j->server, itoa(i), jabber_attr(node->atts, "name")); 
			}
		}
	}
	if (i > 0)  print("jabber_privacy_list_end", session_name(s), j->server);
	if (i == 0) print("jabber_privacy_list_noitem", session_name(s), j->server);
}

/* XXX, check if this is really result. */
JABBER_HANDLER_RESULT(jabber_handle_iq_result_private) {
	jabber_private_t *j = s->priv;
	xmlnode_t *node;

	for (node = n->children; node; node = node->next) {
		char *lname	= jabber_unescape(node->name);
		char *ns	= jabber_attr(node->atts, "xmlns");
		xmlnode_t *child;

		int config_display = 1;
		int bookmark_display = 1;
		int quiet = 0;

		if (!xstrcmp(node->name, "ekg2")) {
			if (!xstrcmp(ns, "ekg2:prefs") && !xstrncmp(id, "config", 6)) 
				config_display = 0;	/* if it's /jid:config --get (not --display) we don't want to display it */ 
			/* XXX, other */
		}
		if (!xstrcmp(node->name, "storage")) {
			if (!xstrcmp(ns, "storage:bookmarks") && !xstrncmp(id, "config", 6))
				bookmark_display = 0;	/* if it's /jid:bookmark --get (not --display) we don't want to display it (/jid:bookmark --get performed @ connect) */
		}

		if (!config_display || !bookmark_display) quiet = 1;

		if (node->children)	printq("jabber_private_list_header", session_name(s), lname, ns);
		if (!xstrcmp(node->name, "ekg2") && !xstrcmp(ns, "ekg2:prefs")) { 	/* our private struct, containing `full` configuration of ekg2 */
			for (child = node->children; child; child = child->next) {
				char *cname	= jabber_unescape(child->name);
				char *cvalue	= jabber_unescape(child->data);
				if (!xstrcmp(child->name, "plugin") && !xstrcmp(jabber_attr(child->atts, "xmlns"), "ekg2:plugin")) {
					xmlnode_t *temp;
					printq("jabber_private_list_plugin", session_name(s), lname, ns, jabber_attr(child->atts, "name"), jabber_attr(child->atts, "prio"));
					for (temp = child->children; temp; temp = temp->next) {
						char *snname = jabber_unescape(temp->name);
						char *svalue = jabber_unescape(temp->data);
						printq("jabber_private_list_subitem", session_name(s), lname, ns, snname, svalue);
						xfree(snname);
						xfree(svalue);
					}
				} else if (!xstrcmp(child->name, "session") && !xstrcmp(jabber_attr(child->atts, "xmlns"), "ekg2:session")) {
					xmlnode_t *temp;
					printq("jabber_private_list_session", session_name(s), lname, ns, jabber_attr(child->atts, "uid"));
					for (temp = child->children; temp; temp = temp->next) {
						char *snname = jabber_unescape(temp->name);
						char *svalue = jabber_unescape(temp->data);
						printq("jabber_private_list_subitem", session_name(s), lname, ns, snname, svalue);
						xfree(snname);
						xfree(svalue);
					}
				} else	printq("jabber_private_list_item", session_name(s), lname, ns, cname, cvalue);
				xfree(cname); xfree(cvalue);
			}
		} else if (!xstrcmp(node->name, "storage") && !xstrcmp(ns, "storage:bookmarks")) { /* JEP-0048: Bookmark Storage */
			/* destroy previously-saved list */
			jabber_bookmarks_free(j);

			/* create new-one */
			for (child = node->children; child; child = child->next) {
				jabber_bookmark_t *book = xmalloc(sizeof(jabber_bookmark_t));

				debug_function("[JABBER:IQ:PRIVATE BOOKMARK item=%s\n", child->name);
				if (!xstrcmp(child->name, "conference")) {
					xmlnode_t *temp;

					book->type	= JABBER_BOOKMARK_CONFERENCE;
					book->private.conf		= xmalloc(sizeof(jabber_bookmark_conference_t));
					book->private.conf->name	= jabber_unescape(jabber_attr(child->atts, "name"));
					book->private.conf->jid		= jabber_unescape(jabber_attr(child->atts, "jid"));
					book->private.conf->autojoin	= !xstrcmp(jabber_attr(child->atts, "autojoin"), "true");

					book->private.conf->nick	= jabber_unescape( (temp = xmlnode_find_child(child, "nick")) ? temp->data : NULL);
					book->private.conf->pass        = jabber_unescape( (temp = xmlnode_find_child(child, "password")) ? temp->data : NULL);

					printq("jabber_bookmark_conf", session_name(s), book->private.conf->name, book->private.conf->jid,
							book->private.conf->autojoin ? "X" : " ", book->private.conf->nick, book->private.conf->pass);

				} else if (!xstrcmp(child->name, "url")) {
					book->type	= JABBER_BOOKMARK_URL;
					book->private.url	= xmalloc(sizeof(jabber_bookmark_url_t));
					book->private.url->name	= jabber_unescape(jabber_attr(child->atts, "name"));
					book->private.url->url	= jabber_unescape(jabber_attr(child->atts, "url"));

					printq("jabber_bookmark_url", session_name(s), book->private.url->name, book->private.url->url);

				} else { debug_error("[JABBER:IQ:PRIVATE:BOOKMARK UNKNOWNITEM=%s\n", child->name); xfree(book); book = NULL; }

				if (book) list_add(&j->bookmarks, book, 0);
			}
		} else {
			/* DISPLAY IT ? w jakim formacie?
			 * + CHILD item=value item2=value2
			 *  - SUBITEM .....
			 *  - SUBITEM........
			 *  + SUBCHILD ......
			 *   - SUBITEM
			 * ? XXX
			 */
		}
		if (node->children)	printq("jabber_private_list_footer", session_name(s), lname, ns);
		else 			printq("jabber_private_list_empty", session_name(s), lname, ns);
		xfree(lname);
	}
}

JABBER_HANDLER_RESULT(jabber_handle_gmail_result_mailbox) {
	jabber_private_t *j = s->priv;

	char *mailcount = jabber_attr(n->atts, "total-matched");
	int tid_set = 0;
	xmlnode_t *child;
	xfree(j->last_gmail_result_time);
	j->last_gmail_result_time = xstrdup(jabber_attr(n->atts, "result-time"));

	print("gmail_count", session_name(s), mailcount);

	/* http://code.google.com/apis/talk/jep_extensions/gmail.html */
	for (child = n->children; child; child = child->next) {
		if (!xstrcmp(child->name, "mail-thread-info")) {
			if (!tid_set)
			{
				xfree(j->last_gmail_tid);
				j->last_gmail_tid = xstrdup(jabber_attr(child->atts, "tid"));
			}
			tid_set = 1;
			xmlnode_t *subchild;
			string_t from = string_init(NULL);

			char *amessages = jabber_attr(child->atts, "messages");		/* messages count in thread */
			char *subject = NULL;
			int firstsender = 1;

			for (subchild = child->children; subchild; subchild = subchild->next) {
				if (0) {
				} else if (!xstrcmp(subchild->name, "subject")) {
					if (xstrcmp(subchild->data, "")) {
						xfree(subject);
						subject = jabber_unescape(subchild->data);
					}

				} else if (!xstrcmp(subchild->name, "senders")) {
					xmlnode_t *senders;

					for (senders = subchild->children; senders; senders = senders->next) {
						char *aname = jabber_unescape(jabber_attr(senders->atts, "name"));
						char *amail = jabber_attr(senders->atts, "address");

						if (!firstsender)
							string_append(from, ", ");

						if (aname) {
							char *tmp = saprintf("%s <%s>", aname, amail);
							string_append(from, tmp);
							xfree(tmp);
						} else {
							string_append(from, amail);
						}

						firstsender = 0;
						xfree(aname);
					}
				} else if (!xstrcmp(subchild->name, "labels")) {	/* <labels>        | 
											   A tag that contains a pipe ('|') delimited list of labels applied to this thread. */
				} else if (!xstrcmp(subchild->name, "snippet")) {	/* <snippet>       | 
											   A snippet from the body of the email. This must be HTML-encoded. */
				} else debug_error("[jabber] google:mail:notify/mail-thread-info wtf: %s\n", __(subchild->name));
			}

			print((amessages && atoi(amessages) > 1) ? "gmail_thread" : "gmail_mail", 
					session_name(s), from->str, jabberfix(subject, "(no subject)"), amessages);

			string_free(from, 1);
			xfree(subject);
		} else debug_error("[jabber, iq] google:mail:notify wtf: %s\n", __(child->name));
	}
	if (mailcount && atoi(mailcount)) /* we don't want to beep or send events if no new mail is available */
		newmail_common(s);
}

/* XXX, check if this is result */
JABBER_HANDLER_RESULT(jabber_handle_iq_result_search) {
	jabber_private_t *j = s->priv;

	xmlnode_t *node;
	int rescount = 0;
	char *uid = jabber_unescape(from);
	int formdone = 0;

	for (node = n->children; node; node = node->next) {
		if (!xstrcmp(node->name, "item")) rescount++;
	}
	if (rescount > 1) print("jabber_search_begin", session_name(s), uid);

	for (node = n->children; node; node = node->next) {
		if (!xstrcmp(node->name, "item")) {
			xmlnode_t *tmp;
			char *jid 	= jabber_attr(node->atts, "jid");
			char *nickname	= tlenjabber_unescape( (tmp = xmlnode_find_child(node, "nick"))  ? tmp->data : NULL);
			char *fn	= tlenjabber_unescape( (tmp = xmlnode_find_child(node, "first")) ? tmp->data : NULL);
			char *lastname	= tlenjabber_unescape( (tmp = xmlnode_find_child(node, "last"))  ? tmp->data : NULL);
			char *email	= tlenjabber_unescape( (tmp = xmlnode_find_child(node, "email")) ? tmp->data : NULL);

			/* idea about displaink user in depend of number of users founded gathered from gg plugin */
			print(rescount > 1 ? "jabber_search_items" : "jabber_search_item", 
					session_name(s), uid, jid, nickname, fn, lastname, email);
			xfree(nickname);
			xfree(fn);
			xfree(lastname);
			xfree(email);
		} else {
			xmlnode_t *reg;
			if (rescount == 0) rescount = -1;
			if (formdone) continue;

			for (reg = n->children; reg; reg = reg->next) {
				if (!xstrcmp(reg->name, "x") && !xstrcmp("jabber:x:data", jabber_attr(reg->atts, "xmlns"))) {
					if (!xstrcmp(jabber_attr(reg->atts, "type"), "form")) {
						formdone = 1;
						jabber_handle_xmldata_form(s, uid, "search", reg->children, "--jabber_x_data");
						break;
					} else if (!xstrcmp(jabber_attr(reg->atts, "type"), "result")) {
						formdone = 1;
						jabber_handle_xmldata_result(s, reg->children, uid);
						break;
					}
				}
			}

			if (!formdone) {
				/* XXX */
			}
		}
	}
	if (rescount > 1) print("jabber_search_end", session_name(s), uid);
	if (rescount == 0) print("search_not_found"); /* not found */
	xfree(uid);
}

JABBER_HANDLER_RESULT(jabber_handle_result_pubsub) {
	const char *xmlns = jabber_attr(n->atts, "xmlns");
	/* XXX, we really need make functions for iq's. */
	if (!xstrcmp(xmlns, "http://jabber.org/protocol/pubsub#event")) {
		xmlnode_t *p;

		for (p = n->children; p; p = p->next) {
			if (!xstrcmp(p->name, "items")) {
				const char *nodename = jabber_attr(p->atts, "node");
				xmlnode_t *node;

				debug_error("XXX %s\n", __(nodename));

				for (node = p->children; node; node = node->next) {
					if (!xstrcmp(node->name, "item")) {
						const char *itemid = jabber_attr(node->atts, "id");
						debug_error("XXX XXX %s\n", __(itemid));

						/* XXX HERE, node->children... is entry. */
					} else debug_error("[%s:%d] wtf? %s\n", __FILE__, __LINE__, __(node->name));
				} 
			} else debug_error("[%s:%d] wtf? %s\n", __FILE__, __LINE__, __(p->name));
		}
	} else debug_error("[JABBER] RESULT/PUBSUB wtf XMLNS: %s\n", __(xmlns));
}

typedef enum {
	JABBER_IQ_TYPE_NONE,
	JABBER_IQ_TYPE_GET,
	JABBER_IQ_TYPE_SET,
	JABBER_IQ_TYPE_RESULT,
	JABBER_IQ_TYPE_ERROR,
} jabber_iq_type_t;

JABBER_HANDLER_IQ(jabber_handle_si) {
/* dj, I think we don't need to unescape rows (tags) when there should be int, do we?  ( <size> <offset> <length>... )*/
	jabber_private_t *j = s->priv;
	xmlnode_t *p;

	if (iqtype == JABBER_IQ_TYPE_RESULT) {
		char *uin = jabber_unescape(from);
		dcc_t *d;

		if ((d = jabber_dcc_find(uin, id, NULL))) {
			xmlnode_t *node;
			jabber_dcc_t *p = d->priv;
			char *stream_method = NULL;

			for (node = n->children; node; node = node->next) {
				if (!xstrcmp(node->name, "feature") && !xstrcmp(jabber_attr(node->atts, "xmlns"), "http://jabber.org/protocol/feature-neg")) {
					xmlnode_t *subnode;
					for (subnode = node->children; subnode; subnode = subnode->next) {
						if (!xstrcmp(subnode->name, "x") && !xstrcmp(jabber_attr(subnode->atts, "xmlns"), "jabber:x:data") && 
								!xstrcmp(jabber_attr(subnode->atts, "type"), "submit")) {
							/* var stream-method == http://jabber.org/protocol/bytestreams */
							jabber_handle_xmldata_submit(s, subnode->children, NULL, 0, "stream-method", &stream_method, NULL);
						}
					}
				}
			}
			if (!xstrcmp(stream_method, "http://jabber.org/protocol/bytestreams")) 	p->protocol = JABBER_DCC_PROTOCOL_BYTESTREAMS; 
			else debug_error("[JABBER] JEP-0095: ERROR, stream_method XYZ error: %s\n", stream_method);
			xfree(stream_method);

			if (p->protocol == JABBER_DCC_PROTOCOL_BYTESTREAMS) {
				struct jabber_streamhost_item streamhost;
				jabber_dcc_bytestream_t *b;
				list_t l;

				b = p->private.bytestream = xmalloc(sizeof(jabber_dcc_bytestream_t));
				b->validate = JABBER_DCC_PROTOCOL_BYTESTREAMS;

				if (jabber_dcc_ip && jabber_dcc) {
					/* basic streamhost, our ip, default port, our jid. check if we enable it. XXX*/
					streamhost.jid	= saprintf("%s/%s", s->uid+4, j->resource);
					streamhost.ip	= xstrdup(jabber_dcc_ip);
					streamhost.port	= jabber_dcc_port;
					list_add(&(b->streamlist), &streamhost, sizeof(struct jabber_streamhost_item));
				}

/* 	... other, proxy, etc, etc..
				streamhost.ip = ....
				streamhost.port = ....
				list_add(...);
 */

				xfree(p->req);
				p->req = xstrdup(itoa(j->id++));

				watch_write(j->send_watch, "<iq type=\"set\" to=\"%s\" id=\"%s\">"
						"<query xmlns=\"http://jabber.org/protocol/bytestreams\" mode=\"tcp\" sid=\"%s\">", 
						d->uid+4, p->req, p->sid);

				for (l = b->streamlist; l; l = l->next) {
					struct jabber_streamhost_item *item = l->data;
					watch_write(j->send_watch, "<streamhost port=\"%d\" host=\"%s\" jid=\"%s\"/>", item->port, item->ip, item->jid);
				}
				watch_write(j->send_watch, "<fast xmlns=\"http://affinix.com/jabber/stream\"/></query></iq>");

			}
		} else /* XXX */;
	}

	if (iqtype == JABBER_IQ_TYPE_SET && ((p = xmlnode_find_child(n, "file")))) {  /* JEP-0096: File Transfer */
		dcc_t *D;
		char *uin = jabber_unescape(from);
		char *uid;
		char *filename	= jabber_unescape(jabber_attr(p->atts, "name"));
		char *size 	= jabber_attr(p->atts, "size");
#if 0
		xmlnode_t *range; /* unused? */
#endif
		jabber_dcc_t *jdcc;

		uid = saprintf("jid:%s", uin);

		jdcc = xmalloc(sizeof(jabber_dcc_t));
		jdcc->session	= s;
		jdcc->req 	= xstrdup(id);
		jdcc->sid	= jabber_unescape(jabber_attr(n->atts, "id"));
		jdcc->sfd	= -1;

		D = dcc_add(uid, DCC_GET, NULL);
		dcc_filename_set(D, filename);
		dcc_size_set(D, atoi(size));
		dcc_private_set(D, jdcc);
		dcc_close_handler_set(D, jabber_dcc_close_handler);
/* XXX, result
		if ((range = xmlnode_find_child(p, "range"))) {
			char *off = jabber_attr(range->atts, "offset");
			char *len = jabber_attr(range->atts, "length");
			if (off) dcc_offset_set(D, atoi(off));
			if (len) dcc_size_set(D, atoi(len));
		}
*/
		print("dcc_get_offer", format_user(s, uid), filename, size, itoa(dcc_id_get(D))); 

		xfree(uin);
		xfree(uid);
		xfree(filename);
	}
}

JABBER_HANDLER_IQ(jabber_handle_bytestreams) {
	jabber_private_t *j = s->priv;

	char *uid = jabber_unescape(from);		/* jid */
	char *sid = jabber_attr(n->atts, "sid");	/* session id */
#if 0 /* unused */
	char *smode = jabber_attr(q->atts, "mode"); 	/* tcp, udp */
#endif
	dcc_t *d = NULL;

	if (iqtype == JABBER_IQ_TYPE_SET && (d = jabber_dcc_find(uid, NULL, sid))) {
		/* w sumie jak nie mamy nawet tego dcc.. to mozemy kontynuowac ;) */
		/* problem w tym czy user chce ten plik.. etc.. */
		/* i tak to na razie jest jeden wielki hack, trzeba sprawdzac czy to dobry typ dcc. etc, XXX */
		xmlnode_t *node;
		jabber_dcc_t *p = d->priv;
		jabber_dcc_bytestream_t *b = NULL;

		list_t host_list = NULL, l;
		struct jabber_streamhost_item *streamhost;

		if (d->type == DCC_SEND) {
			watch_write(j->send_watch, 
					"<iq type=\"error\" to=\"%s\" id=\"%s\"><error code=\"406\">Declined</error></iq>", d->uid+4, id);
			return;
		}

		p->protocol = JABBER_DCC_PROTOCOL_BYTESTREAMS;

		xfree(p->req);
		p->req = xstrdup(id);
		/* XXX, set our streamhost && send them too */
		for (node = n->children; node; node = node->next) {
			if (!xstrcmp(node->name, "streamhost")) {
				struct jabber_streamhost_item *newstreamhost = xmalloc(sizeof(struct jabber_streamhost_item));

				newstreamhost->ip	= xstrdup(jabber_attr(node->atts, "host"));	/* XXX in host can be hostname */
				newstreamhost->port	= atoi(jabber_attr(node->atts, "port"));
				newstreamhost->jid	= xstrdup(jabber_attr(node->atts, "jid"));
				list_add(&host_list, newstreamhost, 0);
			}
		}
		l = host_list;
find_streamhost:
		streamhost = NULL;
		for (; l; l = l->next) {
			struct jabber_streamhost_item *item = l->data;
			struct sockaddr_in sin;
			/* let's search the list for ipv4 address... for now only this we can handle */
			if ((inet_pton(AF_INET, item->ip, &(sin.sin_addr)) > 0)) {
				streamhost = host_list->data;
				break;
			}
		}

		if (streamhost) {
			struct sockaddr_in sin;
			int fd;
			char socks5[4];

			fd = socket(AF_INET, SOCK_STREAM, 0);

			sin.sin_family = AF_INET;
			sin.sin_port	= htons(streamhost->port);
			inet_pton(AF_INET, streamhost->ip, &(sin.sin_addr));

			if (connect(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in)) == -1) {
				/* let's try connect once more to another host? */
				debug_error("[jabber] dcc connecting to: %s failed (%s)\n", streamhost->ip, strerror(errno));
				goto find_streamhost;
			}
			p->sfd = fd;

			watch_add(&jabber_plugin, fd, WATCH_READ, jabber_dcc_handle_recv, d);

			p->private.bytestream = b = xmalloc(sizeof(jabber_dcc_bytestream_t));
			b->validate	= JABBER_DCC_PROTOCOL_BYTESTREAMS;
			b->step		= SOCKS5_CONNECT;
			b->streamlist	= host_list;
			b->streamhost	= streamhost;

			socks5[0] = 0x05;	/* socks version 5 */
			socks5[1] = 0x02;	/* number of methods */
			socks5[2] = 0x00;	/* no auth */
			socks5[3] = 0x02;	/* username */
			write(fd, (char *) &socks5, sizeof(socks5));
		} else {
			list_t l;

			debug_error("[jabber] We cannot connect to any streamhost with ipv4 address.. sorry, closing connection.\n");

			for (l = host_list; l; l = l->next) {
				struct jabber_streamhost_item *i = l->data;

				xfree(i->jid);
				xfree(i->ip);
			}
			list_destroy(host_list, 1);

			watch_write(j->send_watch, 
					"<iq type=\"error\" to=\"%s\" id=\"%s\"><error code=\"404\" type=\"cancel\">"
					/* Psi: <error code='404'>Could not connect to given hosts</error> */
					"<item-not-found xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
					"</error></iq>", d->uid+4, id);

			print("dcc_error_refused", format_user(s, d->uid));

			d->active = 1;		/* hack to avoid sending 403 */
			dcc_close(d);		/* zamykamy dcc */
		}
	} else if (iqtype == JABBER_IQ_TYPE_RESULT) {
		xmlnode_t *used = xmlnode_find_child(n, "streamhost-used");
		jabber_dcc_t *p;
		jabber_dcc_bytestream_t *b;
		list_t l;

		if ((d = jabber_dcc_find(uid, id, NULL))) {
			char *usedjid = (used) ? jabber_attr(used->atts, "jid") : NULL;
			watch_t *w;

			p = d->priv;
			b = p->private.bytestream;

			for (l = b->streamlist; l; l = l->next) {
				struct jabber_streamhost_item *item = l->data;
				if (!xstrcmp(item->jid, usedjid)) {
					b->streamhost = item;
				}
			}
			debug_function("[STREAMHOST-USED] stream: 0x%x\n", b->streamhost);
			d->active	= 1;

			w = watch_find(&jabber_plugin, p->sfd, WATCH_NONE);

			if (w && /* w->handler == jabber_dcc_handle_send && */ w->data == d)
				w->type = WATCH_WRITE;
			else {
				debug_error("[jabber] %s:%d WATCH BAD DATA/NOT FOUND, 0x%x 0x%x 0x%x\n", __FILE__, __LINE__, w, w ? w->handler : NULL, w ? w->data : NULL);
				/* XXX, SHOW ERROR ON __STATUS */
				dcc_close(d);
				return;
			}

			{	/* activate stream */
				char buf[1];
				buf[0] = '\r';
				write(p->sfd, &buf[0], 1);
			}
		}
	}
	debug_function("[FILE - BYTESTREAMS] 0x%x\n", d);
}

JABBER_HANDLER(jabber_handle_iq) {
	jabber_private_t *j = s->priv;

	const char *atype= jabber_attr(n->atts, "type");
	const char *id   = jabber_attr(n->atts, "id");
	const char *from = jabber_attr(n->atts, "from");

	xmlnode_t *q;

	jabber_iq_type_t type = JABBER_IQ_TYPE_NONE;

	if (0);
	else if (!xstrcmp(atype, "get"))	type = JABBER_IQ_TYPE_GET;
	else if (!xstrcmp(atype, "set"))	type = JABBER_IQ_TYPE_SET;
	else if (!xstrcmp(atype, "result"))	type = JABBER_IQ_TYPE_RESULT;
	else if (!xstrcmp(atype, "error"))	type = JABBER_IQ_TYPE_ERROR;

	else if (atype)				debug_error("[jabber] <iq> wtf iq type: %s\n", atype);
	else {					debug_error("[jabber] <iq> without type!\n");
						return;
	}

	if (type == JABBER_IQ_TYPE_ERROR) {
		xmlnode_t *e = xmlnode_find_child(n, "error");
		char *reason = (e) ? jabber_unescape(e->data) : NULL;

		if (!xstrncmp(id, "register", 8)) {
			print("register_failed", jabberfix(reason, "?"));
		} else if (!xstrcmp(id, "auth")) {
			j->parser = NULL; jabber_handle_disconnect(s, reason ? reason : _("Error connecting to Jabber server"), EKG_DISCONNECT_FAILURE);

		} else if (!xstrncmp(id, "passwd", 6)) {
			print("passwd_failed", jabberfix(reason, "?"));
			session_set(s, "__new_password", NULL);
		} else if (!xstrncmp(id, "search", 6)) {
			debug_error("[JABBER] search failed: %s\n", reason);
		}
		else if (!xstrncmp(id, "offer", 5)) {
			char *uin = jabber_unescape(from);
			dcc_t *p = jabber_dcc_find(uin, id, NULL);

			if (p) {
				/* XXX, new theme it's for ip:port */
				print("dcc_error_refused", format_user(s, p->uid));
				dcc_close(p);
			} else {
				/* XXX, possible abuse attempt */
			}
			xfree(uin);
		}
		else debug_error("[JABBER] GENERIC IQ ERROR: %s\n", reason);

		xfree(reason);
		return;			/* we don't need to go down */
	}

/* thx to Michal Gorny for following code. 
 * handle google mail notify. */
#if GMAIL_MAIL_NOTIFY
	if (type == JABBER_IQ_TYPE_RESULT && (q = xmlnode_find_child(n, "new-mail")) && !xstrcmp(jabber_attr(q->atts, "xmlns"), "google:mail:notify"))
		watch_write(j->send_watch, "<iq type=\"get\" id=\"gmail%d\"><query xmlns=\"google:mail:notify\"/></iq>", j->id++);
	if (type == JABBER_IQ_TYPE_SET && (q = xmlnode_find_child(n, "new-mail")) && !xstrcmp(jabber_attr(q->atts, "xmlns"), "google:mail:notify")) {
		print("gmail_new_mail", session_name(s));
		watch_write(j->send_watch, "<iq type='result' id='%s'/>", jabber_attr(n->atts, "id"));
		if (j->last_gmail_result_time && j->last_gmail_tid)
			watch_write(j->send_watch, "<iq type=\"get\" id=\"gmail%d\"><query xmlns=\"google:mail:notify\" newer-than-time=\"%s\" newer-than-tid=\"%s\" /></iq>", j->id++, j->last_gmail_result_time, j->last_gmail_tid);
		else
			watch_write(j->send_watch, "<iq type=\"get\" id=\"gmail%d\"><query xmlns=\"google:mail:notify\"/></iq>", j->id++);
	}
	if (type == JABBER_IQ_TYPE_RESULT && (q = xmlnode_find_child(n, "mailbox")) && !xstrcmp(jabber_attr(q->atts, "xmlns"), "google:mail:notify")) {
		jabber_handle_gmail_result_mailbox(s, q, from, id);
	}
#endif

	if (!xstrcmp(id, "auth")) {
		if (type == JABBER_IQ_TYPE_RESULT) {
			jabber_session_connected(s);
		} else {	/* Can someone look at that, i don't undestand for what is it here... */
			s->last_conn = time(NULL);
			j->connecting = 0;
		}
	}

	if (!xstrncmp(id, "passwd", 6)) {
		if (type == JABBER_IQ_TYPE_RESULT) {
			const char *new_passwd = session_get(s, "__new_password");
			if (new_passwd && (!from || !xstrcmp(from, j->server))) {
				session_set(s, "password", new_passwd);
				session_set(s, "__new_password", NULL);
				print("passwd");
			} else {
				print(new_passwd ? "passwd_possible_abuse" : "passwd_abuse", session_name(s), from);
			}
		} 
		session_set(s, "__new_password", NULL);
		return;
	}


	if (type == JABBER_IQ_TYPE_RESULT && (q = xmlnode_find_child(n, "pubsub"))) {
		jabber_handle_result_pubsub(s, q, from, id);
	}

	if ((q = xmlnode_find_child(n, "si"))) /* JEP-0095: Stream Initiation */
		jabber_handle_si(s, q, type, from, id);

	/* XXX: temporary hack: roster przychodzi jako typ 'set' (przy dodawaniu), jak
	        i typ "result" (przy za¿±daniu rostera od serwera) */
	if (type == JABBER_IQ_TYPE_RESULT || type == JABBER_IQ_TYPE_SET) {
		xmlnode_t *q;

		/* First we check if there is vCard... */
		if ((q = xmlnode_find_child(n, "vCard"))) {
			const char *ns = jabber_attr(q->atts, "xmlns");

			if (!xstrncmp(ns, "vcard-temp", 10)) {
				xmlnode_t *fullname = xmlnode_find_child(q, "FN");
				xmlnode_t *nickname = xmlnode_find_child(q, "NICKNAME");
				xmlnode_t *birthday = xmlnode_find_child(q, "BDAY");
				xmlnode_t *adr  = xmlnode_find_child(q, "ADR");
				xmlnode_t *city = xmlnode_find_child(adr, "LOCALITY");
				xmlnode_t *desc = xmlnode_find_child(q, "DESC");

				char *from_str     = (from)	? jabber_unescape(from) : NULL;
				char *fullname_str = (fullname) ? jabber_unescape(fullname->data) : NULL;
				char *nickname_str = (nickname) ? jabber_unescape(nickname->data) : NULL;
				char *bday_str     = (birthday) ? jabber_unescape(birthday->data) : NULL;
				char *city_str     = (city)	? jabber_unescape(city->data) : NULL;
				char *desc_str     = (desc)	? jabber_unescape(desc->data) : NULL;

				print("jabber_userinfo_response", 
						jabberfix(from_str, _("unknown")), 	jabberfix(fullname_str, _("unknown")),
						jabberfix(nickname_str, _("unknown")),	jabberfix(bday_str, _("unknown")),
						jabberfix(city_str, _("unknown")),	jabberfix(desc_str, _("unknown")));
				xfree(desc_str);
				xfree(city_str);
				xfree(bday_str);
				xfree(nickname_str);
				xfree(fullname_str);
				xfree(from_str);
			}
		}

		if ((q = xmlnode_find_child(n, "query"))) {
			const char *ns = jabber_attr(q->atts, "xmlns");

			if (!xstrcmp(ns, "jabber:iq:privacy")) { /* RFC 3921 (10th: privacy) */
				jabber_handle_iq_result_privacy(s, q, from, id); return;
			}
			
			if (!xstrcmp(ns, "jabber:iq:private")) {
				jabber_handle_iq_result_private(s, q, from, id); return;
			}

			if (!xstrcmp(ns, "http://jabber.org/protocol/vacation")) { /* JEP-0109: Vacation Messages */
				xmlnode_t *temp;

				char *message	= jabber_unescape( (temp = xmlnode_find_child(q, "message")) ? temp->data : NULL);
				char *begin	= (temp = xmlnode_find_child(q, "start")) && temp->data ? temp->data : _("begin");
				char *end	= (temp = xmlnode_find_child(q, "end")) && temp->data  ? temp->data : _("never");

				print("jabber_vacation", session_name(s), message, begin, end);

				xfree(message);
				return;
			}

			if (!xstrcmp(ns, "http://jabber.org/protocol/disco#info")) {
				jabber_handle_iq_result_disco_info(s, q, from, id);	return;
			}
			
			if (!xstrcmp(ns, "http://jabber.org/protocol/disco#items")) {
				jabber_handle_iq_result_disco(s, q, from, id);		return;
			}


			if (!xstrcmp(ns, "http://jabber.org/protocol/muc#owner")) {
				xmlnode_t *node;
				int formdone = 0;
				char *uid = jabber_unescape(from);

				for (node = q->children; node; node = node->next) {
					if (!xstrcmp(node->name, "x") && !xstrcmp("jabber:x:data", jabber_attr(node->atts, "xmlns"))) {
						if (!xstrcmp(jabber_attr(node->atts, "type"), "form")) {
							formdone = 1;
							jabber_handle_xmldata_form(s, uid, "admin", node->children, NULL);
							break;
						} 
					}
				}
//				if (!formdone) ;	// XXX
				xfree(uid);
				return;
			}
			
			if (!xstrcmp(ns, "http://jabber.org/protocol/muc#admin")) {
				xmlnode_t *node;
				int count = 0;

				for (node = q->children; node; node = node->next) {
					if (!xstrcmp(node->name, "item")) {
						char *jid		= jabber_attr(node->atts, "jid");
//						char *aff		= jabber_attr(node->atts, "affiliation");
						xmlnode_t *reason	= xmlnode_find_child(node, "reason");
						char *rsn		= reason ? jabber_unescape(reason->data) : NULL;
						
						print("jabber_muc_banlist", session_name(s), from, jid, rsn ? rsn : "", itoa(++count));
						xfree(rsn);
					}
				}

			} 

			if (!xstrcmp(ns, "jabber:iq:search")) {
				jabber_handle_iq_result_search(s, q, from, id);	return;
			}
			
			if (!xstrcmp(ns, "jabber:iq:register")) {
				xmlnode_t *reg;
				char *from_str = (from) ? jabber_unescape(from) : xstrdup(_("unknown"));
				int done = 0;

				for (reg = q->children; reg; reg = reg->next) {
					if (!xstrcmp(reg->name, "x") && !xstrcmp("jabber:x:data", jabber_attr(reg->atts, "xmlns")) && 
						( !xstrcmp("form", jabber_attr(reg->atts, "type")) || !jabber_attr(reg->atts, "type")))
					{
						done = 1;
						jabber_handle_xmldata_form(s, from_str, "register", reg->children, "--jabber_x_data");
						break;
					}
				}
				if (!done && !q->children) { 
					/* XXX */
					done = 1;
				}
				if (!done) {
					xmlnode_t *instr = xmlnode_find_child(q, "instructions");
					print("jabber_form_title", session_name(s), from_str, from_str);

					if (instr && instr->data) {
						char *instr_str = jabber_unescape(instr->data);
						print("jabber_form_instructions", session_name(s), from_str, instr_str);
						xfree(instr_str);
					}
					print("jabber_form_command", session_name(s), from_str, "register", "");

					for (reg = q->children; reg; reg = reg->next) {
						char *jname, *jdata;
						if (!xstrcmp(reg->name, "instructions")) continue;

						jname = jabber_unescape(reg->name);
						jdata = jabber_unescape(reg->data);
						print("jabber_registration_item", session_name(s), from_str, jname, jdata);
						xfree(jname);
						xfree(jdata);
					}
					print("jabber_form_end", session_name(s), from_str, "register");
				}
				xfree(from_str);
				return;
			}

			if (!xstrcmp(ns, "http://jabber.org/protocol/bytestreams")) { /* JEP-0065: SOCKS5 Bytestreams */
				jabber_handle_bytestreams(s, q, type, from, id);	return;
			}

			if (!xstrncmp(ns, "jabber:iq:version", 17)) {
				jabber_handle_iq_result_version(s, q, from, id); 	return;
			}
			
			if (!xstrncmp(ns, "jabber:iq:last", 14)) {
				jabber_handle_iq_result_last(s, q, from, id);		return;
			}
			
			if (!xstrncmp(ns, "jabber:iq:roster", 16)) {
				xmlnode_t *item = xmlnode_find_child(q, "item");

				for (; item ; item = item->next) {
					userlist_t *u;
					char *uid;
					if (j->istlen)	uid = saprintf("tlen:%s", jabber_attr(item->atts, "jid"));
					else 		uid = saprintf("jid:%s",jabber_attr(item->atts, "jid"));

					/* je¶li element rostera ma subscription = remove to tak naprawde u¿ytkownik jest usuwany;
					w przeciwnym wypadku - nalezy go dopisaæ do userlisty; dodatkowo, jesli uzytkownika
					mamy ju¿ w liscie, to przyszla do nas zmiana rostera; usunmy wiec najpierw, a potem
					sprawdzmy, czy warto dodawac :) */

					if ((session_int_get(s, "__roster_retrieved") == 1) && (u = userlist_find(s, uid)))
						userlist_remove(s, u);

					if (!xstrncmp(jabber_attr(item->atts, "subscription"), "remove", 6)) {
						/* nic nie robimy, bo juz usuniete */
					} else {
						char *nickname 	= jabber_unescape(jabber_attr(item->atts, "name"));
						const char *authval;
						xmlnode_t *group = xmlnode_find_child(item,"group");
						/* czemu sluzy dodanie usera z nickname uid jesli nie ma nickname ? */
						u = userlist_add(s, uid, nickname ? nickname : uid); 

						if ((authval = jabber_attr(item->atts, "subscription"))) {
							jabber_userlist_private_t *up = jabber_userlist_priv_get(u);

								/* case dependent? */
							if (up)
								for (up->authtype = EKG_JABBER_AUTH_BOTH; (up->authtype > EKG_JABBER_AUTH_NONE) && xstrcmp(authval, jabber_authtypes[up->authtype]); (up->authtype)--);

							if (!up || !(up->authtype & EKG_JABBER_AUTH_TO)) {
								if (u && u->status == EKG_STATUS_NA)
									u->status = EKG_STATUS_UNKNOWN;
							} else {
								if (u && u->status == EKG_STATUS_UNKNOWN)
									u->status = EKG_STATUS_NA;
							}
						}
						
						for (; group ; group = group->next ) {
							char *gname = jabber_unescape(group->data);
							ekg_group_add(u, gname);
							xfree(gname);
						}

						if ((session_int_get(s, "__roster_retrieved") == 1)) {
							command_exec_format(NULL, s, 1, ("/auth --probe %s"), uid);
						}
						xfree(nickname); 
					}
					xfree(uid);
				}; /* for */
				session_int_set(s, "__roster_retrieved", 1);
			} /* jabber:iq:roster */
		} /* if query */
	} /* type == set */

	if (type == JABBER_IQ_TYPE_GET) {
		xmlnode_t *q;

		if ((q = xmlnode_find_child(n, "query"))) {
			const char *ns = jabber_attr(q->atts, "xmlns");

			if (!xstrcmp(ns, "http://jabber.org/protocol/disco#items")) {
				jabber_handle_iq_get_disco(s, j, q, from, id);	return;
			}

			if (!xstrcmp(ns, "jabber:iq:last")) {
				jabber_handle_iq_get_last(s, j, q, from, id);	return;
			}
			
			if (!xstrcmp(ns, "http://jabber.org/protocol/disco#info")) {
				jabber_handle_iq_get_disco_info(s, j, q, from, id); return;
			}

			if (!xstrcmp(ns, "jabber:iq:version") && id && from) {
				jabber_handle_iq_get_version(s, j, q, from, id); return;
			}

		} /* if query */
	} /* type == get */
} /* iq */

static inline void mucuser_private_deinit(userlist_t *u) {
	jabber_userlist_private_t *up = jabber_userlist_priv_get(u);

	if (up) {
		xfree(up->role);
		xfree(up->aff);
	}
}

JABBER_HANDLER(jabber_handle_presence) {
	jabber_private_t *j = s->priv;

	const char *from = jabber_attr(n->atts, "from");
	const char *type = jabber_attr(n->atts, "type");
	char *jid, *uid;
	time_t when = 0;
	xmlnode_t *q;
	int ismuc = 0;
	int istlen = j ? j->istlen : 0;

	int na = !xstrcmp(type, "unavailable");

	jid = jabber_unescape(from);

	if (istlen)	uid = saprintf("tlen:%s", jid);
	else		uid = saprintf("jid:%s", jid);

	xfree(jid);

	if (from && !xstrcmp(type, "subscribe")) {
		int auto_auth = session_int_get(s, "auto_auth");

		if (auto_auth == -1)
			auto_auth = 0;
		if ((auto_auth & 1)) {
			if (!(auto_auth & 4)) /* auto-accept */
				command_exec_format(NULL, s, 2, "/auth --accept %s", uid);
			/* else ignore */
		} else if ((auto_auth & 4)) /* auto-deny */
			command_exec_format(NULL, s, 2, "/auth --deny %s", uid);
		else /* ask */
			print("jabber_auth_subscribe", uid, session_name(s));
		xfree(uid);
		return;
	}

	if (from && !xstrcmp(type, "unsubscribe")) {
		int auto_auth = session_int_get(s, "auto_auth");

		if (auto_auth == -1)
			auto_auth = 0;
		if ((auto_auth & 2)) {
			if (!(auto_auth & 8)) /* auto-accept */
				command_exec_format(NULL, s, 2, "/auth --deny %s", uid);
			/* else ignore */
		} else if ((auto_auth & 8)) /* auto-deny, czyli robienie na opak? */
			command_exec_format(NULL, s, 2, "/auth --accept %s", uid);
		else /* ask */
			print("jabber_auth_unsubscribe", uid, session_name(s));
		xfree(uid);
		return;
	}

	for (q = n->children; q; q = q->next) {
		char *tmp	= xstrchr(uid, '/');
		char *mucuid	= xstrndup(uid, tmp ? tmp - uid : -1);
		char *ns	= jabber_attr(q->atts, "xmlns");

		if (!xstrcmp(q->name, "x")) {
			if (!xstrcmp(ns, "http://jabber.org/protocol/muc#user")) {
				xmlnode_t *child;

				for (child = q->children; child; child = child->next) {
					if (!xstrcmp(child->name, "status")) {		/* status codes, --- http://www.jabber.org/jeps/jep-0045.html#registrar-statuscodes */
						char *code = jabber_attr(child->atts, "code");
						int codenr = code ? atoi(code) : -1;

						switch (codenr) {
							case 201: print_window(mucuid, s, 0, "jabber_muc_room_created", session_name(s), mucuid);	break;
							case  -1: debug("[jabber, iq, muc#user] codenr: -1 code: %s\n", code);				break;
							default : debug("[jabber, iq, muc#user] XXX codenr: %d code: %s\n", codenr, code);
						}
					} else if (!xstrcmp(child->name, "item")) { /* lista userow */
						char *jid	  = jabber_unescape(jabber_attr(child->atts, "jid"));		/* jid */
						char *role	  = jabber_unescape(jabber_attr(child->atts, "role"));		/* ? */
						char *affiliation = jabber_unescape(jabber_attr(child->atts, "affiliation"));	/* ? */
						char *nickjid	  = NULL;

						newconference_t *c;
						userlist_t *ulist;

						if (!(c = newconference_find(s, mucuid))) {
							debug("[jabber,muc] recved muc#user but conference: %s not found ?\n", mucuid);
							xfree(jid); xfree(role); xfree(affiliation);
							break;
						}
						if (tmp) nickjid = saprintf("jid:%s", tmp + 1);
						else	 nickjid = xstrdup(uid);

						if (na) 	print_window(mucuid, s, 0, "muc_left", session_name(s), nickjid + 4, jid, mucuid+4, "");

						ulist = newconference_member_find(c, nickjid);
						if (ulist && na) { 
							mucuser_private_deinit(ulist); 
							newconference_member_remove(c, ulist); 
							ulist = NULL; 
						} else if (!ulist) {
							ulist = newconference_member_add(c, nickjid, nickjid + 4);
							print_window(mucuid, s, 0, "muc_joined", session_name(s), nickjid + 4, jid, mucuid+4, "", role, affiliation);
						}

						if (ulist) {
							jabber_userlist_private_t *up = jabber_userlist_priv_get(ulist);
							ulist->status = EKG_STATUS_AVAIL;
							
							mucuser_private_deinit(ulist);
							if (up) {
								up->role	= xstrdup(role);
								up->aff		= xstrdup(affiliation);
							}
						}
						debug("[MUC, PRESENCE] NEWITEM: %s (%s) ROLE:%s AFF:%s\n", nickjid, __(jid), role, affiliation);
						xfree(nickjid);
						xfree(jid); xfree(role); xfree(affiliation);
					} else {
						debug_error("[MUC, PRESENCE] wtf? child->name: %s\n", child->name);
					}
				}
				ismuc = 1;
			} else if (!xstrcmp(ns, "jabber:x:signed")) {	/* JEP-0027 */
				char *x_signed	= xstrdup(q->data);
				char *x_status	= NULL;
				char *x_key;

				xmlnode_t *nstatus = xmlnode_find_child(n, "status");

				x_key = jabber_openpgp(s, mucuid, JABBER_OPENGPG_VERIFY, nstatus ? nstatus->data ? nstatus->data : "" : "", x_signed, &x_status);
				/* @ x_key KEY, x_status STATUS of verification */
				debug("jabber_openpgp() %s %s\n", __(x_key), __(x_status));
				xfree(x_key);
				xfree(x_status);
			} else if (!xstrncmp(ns, "jabber:x:delay", 14)) {
				when = jabber_try_xdelay(jabber_attr(q->atts, "stamp"));
			} else debug("[JABBER, PRESENCE]: <x xmlns=%s\n", ns);
		}		/* <x> */
		xfree(mucuid);
	}
	if (!ismuc && (!type || ( na || !xstrcmp(type, "error") || !xstrcmp(type, "available")))) {
		xmlnode_t *nshow, *nstatus, *nerr, *temp;
		char *descr = NULL;
		int status = 0;
		char *jstatus = NULL;
		char *tmp2;

		int prio = (temp = xmlnode_find_child(n, "priority")) ? atoi(temp->data) : 10;

		if ((nshow = xmlnode_find_child(n, "show"))) {	/* typ */
			jstatus = jabber_unescape(nshow->data);
			if (!xstrcmp(jstatus, "na") || na) {
				status = EKG_STATUS_NA;
				na = 1;
			}
		} else {
			if (na)
				status = EKG_STATUS_NA;
			else	status = EKG_STATUS_AVAIL;
		}

		if ((nerr = xmlnode_find_child(n, "error"))) { /* bledny */
			char *ecode = jabber_attr(nerr->atts, "code");
			char *etext = jabber_unescape(nerr->data);
			descr = saprintf("(%s) %s", ecode, etext);
			xfree(etext);

			status = EKG_STATUS_ERROR;
		} 
		if ((nstatus = xmlnode_find_child(n, "status"))) { /* opisowy */
			xfree(descr);
			descr = tlenjabber_unescape(nstatus->data);
		}

		if (!jstatus)
			jstatus = xstrdup("unknown");

		if (!status && ((status = ekg_status_int(jstatus)) == EKG_STATUS_UNKNOWN))
			debug_error("[jabber] Unknown presence: %s from %s. Please report!\n", jstatus, uid);
		xfree(jstatus);
		{
			userlist_t *u = userlist_find(s, uid);
			jabber_userlist_private_t *up = jabber_userlist_priv_get(u);
			
			if ((status == EKG_STATUS_NA) && (!up || !(up->authtype & EKG_JABBER_AUTH_TO)))
				status = EKG_STATUS_UNKNOWN;
		}

		if ((tmp2 = xstrchr(uid, '/'))) {
			userlist_t *ut;
	
			if ((ut = userlist_find(s, uid))) {
				ekg_resource_t *r;

				if ((r = userlist_resource_find(ut, tmp2+1))) {
					if (na) {				/* if resource went offline remove... */
						userlist_resource_remove(ut, r);
						r = NULL;
					}
				} else r = userlist_resource_add(ut, tmp2+1, prio);

				if (r && r->prio != prio) {
					/* XXX, here resort, stupido */
					r->prio = prio;
				}
			}
		}
		{
			char *session 	= xstrdup(session_uid_get(s));
			char *host 	= NULL;
			int port 	= 0;

			if (!when) when = time(NULL);
			query_emit_id(NULL, PROTOCOL_STATUS, &session, &uid, &status, &descr, &when);
			
			xfree(session);
/*			xfree(host); */
		}
		xfree(descr);
	}
	xfree(uid);
} /* <presence> */

static void jabber_session_connected(session_t *s) {
	jabber_private_t *j = jabber_private(s);
	char *__session = xstrdup(session_uid_get(s));

	j->connecting = 0;

	query_emit_id(NULL, PROTOCOL_CONNECTED, &__session);

	if (session_get(s, "__new_acount")) {
		print("register", __session);
		if (!xstrcmp(session_get(s, "password"), "foo")) print("register_change_passwd", __session, "foo");
		session_set(s, "__new_acount", NULL);
	}

	session_int_set(s, "__roster_retrieved", 0);

	userlist_free(s);
	watch_write(j->send_watch, "<iq type=\"get\"><query xmlns=\"jabber:iq:roster\"/></iq>");
	jabber_write_status(s);

	if (session_int_get(s, "auto_bookmark_sync") != 0) command_exec(NULL, s, ("/jid:bookmark --get"), 1);
	if (session_int_get(s, "auto_privacylist_sync") != 0) {
		const char *list = session_get(s, "privacy_list");

		if (!list) list = "ekg2";
		command_exec_format(NULL, s, 1, ("/jid:privacy --get %s"), 	list);	/* synchronize list */
		command_exec_format(NULL, s, 1, ("/jid:privacy --session %s"), 	list); 	/* set as active */
	}
#if GMAIL_MAIL_NOTIFY
	/* talk.google.com should work also for Google Apps for your domain */
	if (!xstrcmp(session_get(s, "server"), "talk.google.com")) {
		watch_write(j->send_watch,
			"<iq type=\"set\" to=\"%s\" id=\"gmail%d\"><usersetting xmlns=\"google:setting\"><mailnotifications value=\"true\"/></usersetting></iq>",
			s->uid+4, j->id++);

		watch_write(j->send_watch,
			"<iq type=\"get\" id=\"gmail%d\"><query xmlns=\"google:mail:notify\"/></iq>",
			j->id++);
	}
#endif
	xfree(__session);
}

static void newmail_common(session_t *s) { /* maybe inline? */
	if (config_sound_mail_file) 
		play_sound(config_sound_mail_file);
	else if (config_jabber_beep_mail)
		query_emit_id(NULL, UI_BEEP, NULL);
	/* XXX, we NEED to connect to MAIL_COUNT && display info about mail like mail plugin do. */
	/* XXX, emit events */
}

static time_t jabber_try_xdelay(const char *stamp) {
	/* try to parse timestamp */
	if (stamp) {
        	struct tm tm;
       	        memset(&tm, 0, sizeof(tm));
               	sscanf(stamp, "%4d%2d%2dT%2d:%2d:%2d",
                        &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                       	&tm.tm_hour, &tm.tm_min, &tm.tm_sec);
       	        tm.tm_year -= 1900;
               	tm.tm_mon -= 1;
                return mktime(&tm);
        }
	return time(NULL);

}

