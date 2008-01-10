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

#define _XOPEN_SOURCE 600
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

WATCHER_SESSION(jabber_handle_connect_ssl); /* jabber.c */

#define jabberfix(x,a) ((x) ? x : a)

#define JABBER_HANDLER(x) 		static void x(session_t *s, xmlnode_t *n)

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

static xmlnode_t *xmlnode_find_child_xmlns(xmlnode_t *n, const char *name, const char *xmlns) {
	if (!n || !n->children)
		return NULL;

	for (n = n->children; n; n = n->next)
		if (!xstrcmp(n->name, name) && !xstrcmp(jabber_attr(n->atts, "xmlns"), xmlns))
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
	extern void *jconv_out; /* misc.c */
	extern void *tconv_out; /* misc.c */

	jabber_private_t *j = s->priv;

	const char *passwd2 = NULL;			/* if set than jabber_digest() should be done on it. else plaintext_passwd 
							   Variable to keep password from @a password, or generated hash of password with magic constant [tlen] */

	char *resource = tlenjabber_escape(j->resource);/* escaped resource name */
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
		saprintf("<digest>%s</digest>", jabber_digest(stream_id, passwd2, j->istlen ? tconv_out : jconv_out)) :	/* hash */
		saprintf("<password>%s</password>", epasswd);				/* plaintext */
		
	watch_write(j->send_watch, 
			"<iq type=\"set\" id=\"auth\" to=\"%s\"><query xmlns=\"jabber:iq:auth\"><username>%s</username>%s<resource>%s</resource></query></iq>", 
			j->server, username, authpass, resource);
	xfree(authpass);

	xfree(epasswd);
	xfree(resource);
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
					print("xmpp_feature", session_name(s), j->server, ch->name, jabber_attr(ch->atts, "xmlns"), "Register account using /register");
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

		session_int_set(s, "__session_need_start", (use_fjuczers & 1));

	}
	else		/* else here, to avoid : 'stanza sent before session start' */
	if (use_fjuczers & 1) {	/* session */
		watch_write(j->send_watch, 
				"<iq type=\"set\" id=\"auth\"><session xmlns=\"urn:ietf:params:xml:ns:xmpp-session\"/></iq>",
				j->id++);
	}

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
				string_append_n(str, s->uid+5, xstrchr(s->uid+5, '@')-s->uid-5);
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

		{
			char *tmp = strip_spaces(arr[i]);

			if (!xstrcmp(tmp, "realm"))		realm	= arr[i+1];
			else if (!xstrcmp(tmp, "rspauth"))	rspauth	= arr[i+1];
			else if (!xstrcmp(tmp, "nonce"))	nonce	= arr[i+1];
		}

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
		char *username = xstrndup(s->uid+5, xstrchr(s->uid+5, '@')-s->uid-5);
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

JABBER_HANDLER(jabber_handle_proceed) {
	jabber_private_t *j = s->priv;

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
}

JABBER_HANDLER(jabber_handle_stream_error) {
	jabber_private_t *j = s->priv;

	xmlnode_t *text = xmlnode_find_child(n, "text");

	char *text2 = NULL;

	if (text && text->data)
		text2 = jabber_unescape(text->data);

			/* here we should use: EKG_DISCONNECT_FORCED, but because of noautoreconnection and noreason, we'll use: EKG_DISCONNECT_NETWORK */
	j->parser = NULL; jabber_handle_disconnect(s, text2 ? text2 : n->children ? n->children->name : "stream:error XXX", EKG_DISCONNECT_NETWORK);
	xfree(text2);
}

JABBER_HANDLER(jabber_handle_success) {
	jabber_private_t *j = s->priv;

	CHECK_CONNECT(2, 0, return)
	CHECK_XMLNS(n, "urn:ietf:params:xml:ns:xmpp-sasl", return)

	j->parser = jabber_parser_recreate(NULL, XML_GetUserData(j->parser));	/* here could be passed j->parser to jabber_parser_recreate() but unfortunetly expat makes SIGSEGV */
	watch_write(j->send_watch, 
			"<stream:stream to=\"%s\" xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\" version=\"1.0\">", 
			j->server);
}

JABBER_HANDLER(jabber_handle_failure) {
	jabber_private_t *j = s->priv;

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
}

struct jabber_generic_handler {
	const char *name;
	void (*handler)(session_t *s, xmlnode_t *n);
};

static const struct jabber_generic_handler jabber_handlers[] =
{
	{ "message",		jabber_handle_message },
	{ "presence",		jabber_handle_presence },
	{ "iq",			jabber_handle_iq },

	{ "stream:features",	jabber_handle_stream_features },
	{ "stream:error",	jabber_handle_stream_error },

	{ "challenge",		jabber_handle_challenge },
	{ "compressed",		jabber_handle_compressed },

	{ "proceed",		jabber_handle_proceed },
	{ "success",		jabber_handle_success },
	{ "failure",		jabber_handle_failure },

	{ NULL,			NULL }
};

#include "jabber_handlers_tlen.c"

void jabber_handle(void *data, xmlnode_t *n) {
	session_t *s = (session_t *) data;
        jabber_private_t *j;
	struct jabber_generic_handler *tmp;

        if (!s || !(j = s->priv) || !n) {
                debug_error("jabber_handle() invalid parameters\n");
                return;
        }

/* jabber handlers */
	for (tmp = jabber_handlers; tmp->name; tmp++) {
		if (!xstrcmp(n->name, tmp->name)) {
			tmp->handler(s, n);
			return;
		}
	}

	if (!j->istlen) {
		debug_error("[jabber] what's that: %s ?\n", n->name);
		return;
	}

/* tlen handlers */
	for (tmp = tlen_handlers; tmp->name; tmp++) {
		if (!xstrcmp(n->name, tmp->name)) {
			tmp->handler(s, n);
			return;
		}
	}

	debug_error("[tlen] what's that: %s ?\n", n->name);
}

JABBER_HANDLER(jabber_handle_message) {
	jabber_private_t *j = s->priv;

	xmlnode_t *nerr		= xmlnode_find_child(n, "error");
	xmlnode_t *nbody   	= xmlnode_find_child(n, "body");
	xmlnode_t *nsubject	= NULL;
	xmlnode_t *nthread	= NULL;
	xmlnode_t *xitem;
	
	const char *from = jabber_attr(n->atts, "from");
	char *x_encrypted = NULL;

	char *juid 	= jabber_unescape(from); /* was tmp */
	char *uid;
	time_t bsent = 0;
	string_t body;
	int new_line = 0;	/* if there was headlines do we need to display seperator between them and body? */
	int class = -1;
	int composing = 0;
	
	if (j->istlen)	uid = saprintf("tlen:%s", juid);
	else		uid = saprintf("xmpp:%s", juid);

	xfree(juid);

	if (nerr) {
		char *ecode = jabber_attr(nerr->atts, "code");
		char *etext = jabber_unescape(nerr->data);
		char *recipient = get_nickname(s, uid);

		if (nbody && nbody->data) {
			char *tmp2 = jabber_unescape(nbody->data);
			char *mbody = xstrndup(tmp2, 15);
			xstrtr(mbody, '\n', ' ');

			print_window(uid, s, 0, "jabber_msg_failed_long", recipient, ecode, etext, mbody);

			xfree(mbody);
			xfree(tmp2);
		} else
			print_window(uid, s, 0, "jabber_msg_failed", recipient, ecode, etext);

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

				isack = (acktype & 3);

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
					char *__rcpt	= xstrdup(uid);
					int __status  = ((acktype & 1) ? EKG_ACK_DELIVERED : 
							(acktype & 2) ? EKG_ACK_QUEUED : 
							EKG_ACK_UNKNOWN);
					char *__seq	= NULL; /* id ? */
					/* protocol_message_ack; sesja ; uid ; seq (NULL ? ) ; status - delivered ; queued ) */
					query_emit_id(NULL, PROTOCOL_MESSAGE_ACK, &__session, &__rcpt, &__seq, &__status);
					xfree(__session);
					xfree(__rcpt);
					/* xfree(__seq); */
				}

				if (!(composing & 2)) /* '& 2' means we've already got chatstate (higher prio) */
					composing = 1 + (acktype & 4);

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
			} else debug_error("[JABBER, MESSAGE]: <x xmlns=%s>\n", __(ns));
/* x */		} else if (!xstrcmp(jabber_attr(xitem->atts, "xmlns"), "http://jabber.org/protocol/chatstates")) {
			composing = 3;	/* disable + higher prio */
			if (!xstrcmp(xitem->name, "composing"))
				composing = 7; /* enable + higher prio */
			else if (!xstrcmp(xitem->name, "gone"))
				print_window(uid, s, 0, "jabber_gone", session_name(s), get_nickname(s, uid));
/* chatstate */	} else if (!xstrcmp(xitem->name, "subject")) {
			nsubject = xitem;
		} else if (!xstrcmp(xitem->name, "thread")) {
			nthread = xitem;
/* subject */	} else if (!xstrcmp(xitem->name, "body")) {
		} /* XXX, JEP-0085 here */
		else if (!xstrcmp(jabber_attr(xitem->atts, "xmlns"), "http://jabber.org/protocol/chatstates")) {
			if (!xstrcmp(xitem->name, "active"))		{ }
			else if (!xstrcmp(xitem->name, "composing"))	{ } 
			else if (!xstrcmp(xitem->name, "paused"))	{ } 
			else if (!xstrcmp(xitem->name, "inactive"))	{ } 
			else if (!xstrcmp(xitem->name, "gone")) 	{ } 
			else debug_error("[JABBER, MESSAGE]: INVALID CHATSTATE: %s\n", xitem->name);
		} else if (j->istlen && !xstrcmp(xitem->name, "no"))
			class = EKG_MSGCLASS_SYSTEM;
		else debug_error("[JABBER, MESSAGE]: <%s\n", xitem->name);
	}

	const char *type = jabber_attr(n->atts, "type");
	if (class == -1)
		class = (!xstrcmp(type, "chat") || !xstrcmp(type, "groupchat") ? EKG_MSGCLASS_CHAT : EKG_MSGCLASS_MESSAGE);
	const int nonthreaded = (!nthread || !nthread->data);
	const int hassubject = (nsubject && nsubject->data);

	if (hassubject) { /* we need to linearize this earlier */
		char *tmp = nsubject->data;

		while ((tmp = xstrchr(tmp, '\n'))) {
			if (*(tmp+1) == '\0') {
				*tmp = '\0';
				break; /* we really don't need to search again */
			} else
				*tmp = ' ';
		}
	}
	if ((class == EKG_MSGCLASS_MESSAGE) /* conversations only with messages */
			&& (!nonthreaded /* either if we've got thread */
				|| ((nbody || nsubject) && (session_int_get(s, "allow_add_reply_id") > 1))
					/* or we're allowing to use conversations for non-threaded messages */
				)) {
		jabber_conversation_t *thr;
		int i = jabber_conversation_find(j, uid,
				(nonthreaded && hassubject ? nsubject->data : NULL),
				(nonthreaded ? NULL : nthread->data),
				&thr, (session_int_get(s, "allow_add_reply_id") > 0));
		
		if (nonthreaded)
			nthread = xmalloc(sizeof(xmlnode_t)); /* ugly hack, to make non-threaded reply work */
		
		if (thr) { /* XXX, do it nicer */
			xfree(nthread->data);
			nthread->data = saprintf("#%d", i);
			debug("[jabber, message] thread: %s -> #%d\n", thr->thread, i);
		}
	
		if (!(nsubject && nsubject->data)) {
			string_append(body, (thr ? "Reply-ID: " : "Thread: "));
			string_append(body, nthread->data);
			string_append(body, "\n");
			if (nonthreaded)
				xfree(nthread);

			new_line = 1;
		} else if (thr) {
			xfree(thr->subject);
			thr->subject = xstrdup(nsubject->data); /* we should store newest message subject, not first */
		}
	}
	if (hassubject) {
		string_append(body, "Subject: ");
		string_append(body, nsubject->data);
		if (nthread && nthread->data) {
			string_append(body, " [");
			string_append(body, nthread->data);
			string_append(body, "]");
			
			if (!nthread->name) /* this means we're using above hack */
				xfree(nthread);
		}
		string_append(body, "\n");
		new_line = 1;
	}

	if (new_line) string_append(body, "\n"); 	/* let's seperate headlines from message */

	if (x_encrypted) 
		string_append(body, x_encrypted);	/* encrypted message */
	else if (nbody)
		string_append(body, nbody->data);	/* unecrypted message */

	/* 'composing' is quite special variable:
	 *   1st bit determines whether we should send any update,
	 *     if its' off, then other should be too;
	 *   2nd bit determines whether we've got chatstate-based update,
	 *     if its' on, then jabber:x:event can't replace the state;
	 *   3rd bit determines whether the <composing/> is on
	 */
	if (composing) {
		char *__session = xstrdup(session_uid_get(s));
		char *__rcpt	= xstrdup(uid);
		int  state	= (!nbody && (composing & 4) ? EKG_XSTATE_TYPING : 0);
		int  stateo	= (!state ? EKG_XSTATE_TYPING : 0);

		query_emit_id(NULL, PROTOCOL_XSTATE, &__session, &__rcpt, &state, &stateo);
		
		xfree(__session);
		xfree(__rcpt);
	}

	if (nbody || nsubject) {
		char *me	= xstrdup(session_uid_get(s));
		int ekgbeep 	= EKG_TRY_BEEP;
		int secure	= (x_encrypted != NULL);
		char **rcpts 	= NULL;
		char *seq 	= NULL;
		uint32_t *format= NULL;
		time_t sent	= bsent;

		char *text = tlenjabber_unescape(body->str);

		if (!sent) sent = time(NULL);

		debug_function("[jabber,message] type = %s\n", __(type));
		if (!xstrcmp(type, "groupchat")) {
			char *tuid = xstrrchr(uid, '/');				/* temporary */
			char *uid2 = (tuid) ? xstrndup(uid, tuid-uid) : xstrdup(uid);		/* muc room */
			char *nick = (tuid) ? xstrdup(tuid+1) : NULL;				/* nickname */
			newconference_t *c = newconference_find(s, uid2);
			int isour = (c && !xstrcmp(c->private, nick)) ? 1 : 0;			/* is our message? */
			char *formatted;
			userlist_t *u;

			char attr[2] = { ' ', 0 };

		/* jesli (bsent != 0) wtedy mamy do czynienia z backlogiem */

			class	|= EKG_NO_THEMEBIT;
			ekgbeep	= EKG_NO_BEEP;

			if ((u = userlist_find_u(&(c->participants), nick))) {
				jabber_userlist_private_t *up = jabber_userlist_priv_get(u);

			/* glupie, trzeba doszlifowac */

				if (!xstrcmp(up->aff, "owner")) 		attr[0] = '@';
				else if (!xstrcmp(up->aff, "admin"))		attr[0] = '@';

				else if (!xstrcmp(up->role, "moderator"))	attr[0] = '%';

				else						attr[0] = ' ';


			} else debug_error("[MUC, MESSAGE] userlist_find_u(%s) failed\n", nick);

			formatted = format_string(format_find(isour ? "jabber_muc_send" : "jabber_muc_recv"),
				session_name(s), uid2, nick ? nick : uid2+5, text, attr);
			
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

/* idea and some code copyrighted by Marek Marczykowski xmpp:marmarek@jabberpl.org */

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

struct jabber_iq_generic_handler {
	const char *name;
	const char *xmlns;
	void (*handler)(session_t *s, xmlnode_t *n, const char *from, const char *id);
};

static const struct jabber_iq_generic_handler *jabber_iq_find_handler(const struct jabber_iq_generic_handler *items, const char *type, const char *xmlns) {
	struct jabber_iq_generic_handler *tmp = items;

	while (tmp->handler) {
		int matched = !xstrcmp(type, tmp->name);

		do {
			if (matched && !xstrcmp(tmp->xmlns, xmlns))
				return tmp;

			tmp++;
		} while (!tmp->name);

		if (matched)
			break;
	}

	return NULL;
}

#include "jabber_handlers_iq_error.c"
#include "jabber_handlers_iq_get.c"
#include "jabber_handlers_iq_result.c"

JABBER_HANDLER(jabber_handle_iq) {
	jabber_private_t *j = s->priv;

	const char *atype= jabber_attr(n->atts, "type");
	const char *id   = jabber_attr(n->atts, "id");
	const char *from = jabber_attr(n->atts, "from");

	jabber_iq_type_t type = JABBER_IQ_TYPE_NONE;

	const struct jabber_iq_generic_handler *callbacks;
	xmlnode_t *q;

	if (!xstrcmp(atype, "get"))		type = JABBER_IQ_TYPE_GET;
	else if (!xstrcmp(atype, "set"))	type = JABBER_IQ_TYPE_SET;
	else if (!xstrcmp(atype, "result"))	type = JABBER_IQ_TYPE_RESULT;
	else if (!xstrcmp(atype, "error"))	type = JABBER_IQ_TYPE_ERROR;
	else if (!atype) {
		debug_error("[jabber] <iq> without type!\n");
		return;
	}

	if (type == JABBER_IQ_TYPE_RESULT || type == JABBER_IQ_TYPE_ERROR) {
		list_t l;
		char *uid = jabber_unescape(from);

		/* XXX, do sprawdzenia w RFC/ napisania maila do gosci od XMPP.
		 * 	czy jesli nie mamy nic w from, to mamy zakladac ze w from jest 'nasz.jabber.server' (j->server)
		 */

		/* XXX, note: we temporary pass here: 'from' instead of unescaped 'uid'.
		 */

		for (l = j->iq_stanzas; l; l = l->next) {
			jabber_stanza_t *st = l->data;

			if (!xstrcmp(st->id, id)) {
				/* SECURITY NOTE: for instance, mcabber in version 0.9.5 doesn't check from and id of iq is always increment by one ^^ */

				if (!xstrcmp(st->to, uid) /* || jakas_iwil_zmienna [np: bypass_FROM_checkin_from_iq] */) {
					if (type == JABBER_IQ_TYPE_RESULT) {
						if ((q = xmlnode_find_child_xmlns(n, st->type, st->xmlns))) {
							debug("[jabber] Executing handler id: %s <%s xmlns='%s' 0x%x\n", st->id, st->type, st->xmlns, st->handler);
							st->handler(s, q, from, id);
						} else {
							debug_error("[jabber] Warning, [<%s xmlns='%s'] Not found, calling st->error: %x\n", st->type, st->xmlns, st->error);

							st->error(s, NULL, from, id);
						}
					} else {
						q = xmlnode_find_child(n, "error");	/* WARN: IT CAN BE NULL, jabber_iq_error_string() handles it. */

						debug("[jabber] Executing error handler id: %s q: %x <%s xmlns='%s' 0x%x%x\n", st->id, q, st->type, st->xmlns, st->error);
						st->error(s, q, from, id);
					}


					jabber_stanza_freeone(j, st);
					xfree(uid);
					return;
				}

				debug_error("[jabber] Security warning: recved iq from invalid source %s vs %s\n", st->to, __(uid));
				break;
			}
		}
		xfree(uid);
	}

	switch (type) {
		case JABBER_IQ_TYPE_RESULT:		callbacks = jabber_iq_result_handlers_old;	break;
		case JABBER_IQ_TYPE_SET:		callbacks = jabber_iq_set_handlers;		break;
		case JABBER_IQ_TYPE_GET:		callbacks = jabber_iq_get_handlers;		break;

		case JABBER_IQ_TYPE_ERROR:
			jabber_handle_iq_error_generic_old(s, n, from, id);
			return;
		case JABBER_IQ_TYPE_NONE:
			debug_error("[jabber] <iq> wtf iq type: %s\n", atype);
			return;
	}

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

	for (q = n->children; q; q = q->next) {
		const char *ns = jabber_attr(q->atts, "xmlns");
		const struct jabber_iq_generic_handler *tmp = jabber_iq_find_handler(callbacks, q->name, ns);

		if (!tmp) {
			debug_error("[jabber] <iq %s> unknown name: <%s xmlns='%s'\n", atype, __(q->name), __(ns));
			continue;
		}

		debug_function("[jabber] <iq %s> <%s xmlns=%s\n", atype, q->name, ns);
		tmp->handler(s, q, from, id);

		/* XXX, read RFC if we can have more get stanzas */
		/* <query xmlns=http://jabber.org/protocol/disco#items/>
		 * <query xmlns=http://jabber.org/protocol/disco#info/>
		 * ...
		 * ...
		 */
	}
}

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

	jid = tlenjabber_unescape(from);

	if (istlen)	uid = saprintf("tlen:%s", jid);
	else		uid = saprintf("xmpp:%s", jid);

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
						if (tmp) nickjid = saprintf("xmpp:%s", tmp + 1);
						else	 nickjid = xstrdup(uid);

						if (na) 	print_window(mucuid, s, 0, "muc_left", session_name(s), nickjid + 5, jid, mucuid+5, "");

						ulist = newconference_member_find(c, nickjid);
						if (ulist && na) { 
							mucuser_private_deinit(ulist); 
							newconference_member_remove(c, ulist); 
							ulist = NULL; 
						} else if (!ulist) {
							ulist = newconference_member_add(c, nickjid, nickjid + 5);
							print_window(mucuid, s, 0, "muc_joined", session_name(s), nickjid + 5, jid, mucuid+5, "", role, affiliation);
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
			jstatus = tlenjabber_unescape(nshow->data);
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
			descr = saprintf("(%s) %s", ecode, __(etext));
			xfree(etext);

			if (atoi(ecode) == 403 || atoi(ecode) == 401) /* we lack auth */
				status = EKG_STATUS_UNKNOWN; /* shall we remove the error description? */
			else
				status = EKG_STATUS_ERROR;
			na = 1;

			if (istlen) { /* we need to get&fix the UID - userlist entry is sent with @tlen.pl, but error with user-given host */
				char *tmp	= tlenjabber_unescape(jabber_attr(n->atts, "to"));
				char *atsign	= xstrchr(tmp, '@');

				if (atsign)
					*atsign	= 0;
				uid = saprintf("tlen:%s@tlen.pl", tmp);
				xfree(tmp);
			}
		} else if ((nstatus = xmlnode_find_child(n, "status"))) { /* opisowy */
			xfree(descr);
			descr = tlenjabber_unescape(nstatus->data);
		}

		if (!status && (jstatus || (jstatus = xstrdup("unknown"))) && ((status = ekg_status_int(jstatus)) == EKG_STATUS_UNKNOWN))
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

			if (!when) when = time(NULL);
			query_emit_id(NULL, PROTOCOL_STATUS, &session, &uid, &status, &descr, &when);
			
			xfree(session);
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

	if (session_get(s, "__new_account")) {
		print("register", __session);
		if (!xstrcmp(session_get(s, "password"), "foo")) print("register_change_passwd", __session, "foo");
		session_set(s, "__new_account", NULL);
	}

	session_int_set(s, "__roster_retrieved", 0);

	userlist_free(s);
	watch_write(j->send_watch, "<iq type=\"get\"><query xmlns=\"jabber:iq:roster\"/></iq>");
	jabber_write_status(s);

	if (session_int_get(s, "auto_bookmark_sync") != 0) command_exec(NULL, s, ("/xmpp:bookmark --get"), 1);
	if (session_int_get(s, "auto_privacylist_sync") != 0) {
		const char *list = session_get(s, "privacy_list");

		if (!list) list = "ekg2";
		command_exec_format(NULL, s, 1, ("/xmpp:privacy --get %s"), 	list);	/* synchronize list */
		command_exec_format(NULL, s, 1, ("/xmpp:privacy --session %s"), 	list); 	/* set as active */
	}
	/* talk.google.com should work also for Google Apps for your domain */
	if (!xstrcmp(session_get(s, "server"), "talk.google.com")) {
		watch_write(j->send_watch,
			"<iq type=\"set\" to=\"%s\" id=\"gmail%d\"><usersetting xmlns=\"google:setting\"><mailnotifications value=\"true\"/></usersetting></iq>",
			s->uid+5, j->id++);

		watch_write(j->send_watch,
			"<iq type=\"get\" id=\"gmail%d\"><query xmlns=\"google:mail:notify\"/></iq>",
			j->id++);
	}
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
		char *tmp = xstrdup(getenv("TZ"));
		time_t out;

		memset(&tm, 0, sizeof(tm));
		sscanf(stamp, "%4d%2d%2dT%2d:%2d:%2d",
				&tm.tm_year, &tm.tm_mon, &tm.tm_mday,
				&tm.tm_hour, &tm.tm_min, &tm.tm_sec);
		tm.tm_year -= 1900;
		tm.tm_mon -= 1;

		setenv("TZ", "UTC", 1);
		out = mktime(&tm);
		if (tmp)
			setenv("TZ", tmp, 1);
		else
			unsetenv("TZ");
		xfree(tmp);

		return out;
        }
	return time(NULL);

}

const char *jabber_iq_reg(session_t *s, const char *prefix, const char *to, const char *type, const char *xmlns) {
	jabber_private_t *j = jabber_private(s);
	int loop = 10;

	jabber_stanza_t *st;
	list_t l;

	struct jabber_iq_generic_handler *tmp;
	char *id;

	id = saprintf("%s%x", prefix ? prefix : "", j->id++);
again:
	for (l = j->iq_stanzas; l; l = l->next) {
		jabber_stanza_t *i = l->data;

		if (!xstrcmp(id, i->id)) {
			xfree(id);

			if (--loop) {
				debug_error("jabber_iq_reg() avoiding deadlock\n");
				return NULL;
			}
			
			id = saprintf("%s%x_%d", prefix ? prefix : "", j->id++, rand());

			debug_white("jabber_iq_reg() found id: %s, new id: %s\n", i->id, id);
			goto again;
		}
	}

	st = xmalloc(sizeof(jabber_stanza_t));

	st->id = id;
	st->to = xstrdup(to);
	st->type = xstrdup(type);
	st->xmlns = xstrdup(xmlns);

	tmp = jabber_iq_find_handler(jabber_iq_result_handlers, type, xmlns);
	st->handler = tmp ? tmp->handler : jabber_handle_iq_result_generic;

	tmp = jabber_iq_find_handler(jabber_iq_error_handlers, type, xmlns);
	st->error = tmp ? tmp->handler : jabber_handle_iq_error_generic;

	list_add_beginning(&(j->iq_stanzas), st, 0);

	return id;
}

const char *jabber_iq_send(session_t *s, const char *prefix, jabber_iq_type_t iqtype, const char *to, const char *type, const char *xmlns) {
	jabber_private_t *j = jabber_private(s);
	const char *id;

	char *tmp;
	char *aiqtype;

	if (iqtype == JABBER_IQ_TYPE_GET) 	aiqtype = "get";
	else if (iqtype == JABBER_IQ_TYPE_SET)	aiqtype = "set";
	else {
		debug_error("jabber_iq_send() wrong iqtype passed\n");
		return NULL;
	}
	
	if (!(id = jabber_iq_reg(s, prefix, to, type, xmlns)))
		return NULL;

	tmp = jabber_escape(to);	/* XXX: really worth escaping? */
	watch_write(j->send_watch, "<iq id='%s' to='%s' type='%s'><%s xmlns='%s'/></iq>", id, tmp, aiqtype, type, xmlns);
	xfree(tmp);

	return id;
}


