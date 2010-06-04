/*
 *  (C) Copyright 2006-2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *		       2008 Wies³aw Ochmiñski <wiechu@wiechu.com>
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ekg/debug.h>
#include <ekg/net.h>
#include <ekg/plugins.h>
#include <ekg/recode.h>
#include <ekg/queries.h>
#include <ekg/protocol.h>
#include <ekg/themes.h>
#include <ekg/windows.h>	// XXX
#include <ekg/stuff.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

#include <ekg/log.h>
#include <ekg/recode.h>
#include <ekg/msgqueue.h>

#include "icq.h"
#include "misc.h"

#include "icq_caps.h"
#include "icq_const.h"
#include "icq_flap_handlers.h"
#include "icq_snac_handlers.h"

#define ICQ_HUB_SERVER "login.icq.com"
#define ICQ_HUB_PORT	5190

static int icq_theme_init();
PLUGIN_DEFINE(icq, PLUGIN_PROTOCOL, icq_theme_init);

int icq_send_pkt(session_t *s, string_t buf) {
	icq_private_t *j;
	int fd;

	if (!s || !(j = s->priv) || !buf) {
		string_free(buf, 1);
		return -1;
	}

	fd = j->fd;

	debug_io("icq_send_pkt(%s) fd: %d len: %d\n", s->uid, fd, buf->len);
	icq_hexdump(DEBUG_IO, (unsigned char *) buf->str, buf->len);
	ekg_write(fd, buf->str, buf->len);
	string_free(buf, 1);
	return 0;
}

static TIMER_SESSION(icq_ping) {
	icq_private_t *j;
	string_t pkt;

	if (type)
		return 0;

	if (!s || !(j = s->priv) || !s->connected)
		return -1;

	pkt = string_init(NULL);
	icq_makeflap(s, pkt, 0x05);
	icq_send_pkt(s, pkt);

	return 0;
}

int icq_write_status_msg(session_t *s) {
	icq_private_t *j;
	char *msg;

	if (!s || !(j = s->priv))
		return -1;

	if (j->aim == 0)
		return -1;

	if (s->status != EKG_STATUS_AWAY && s->status != EKG_STATUS_GONE && s->status != EKG_STATUS_XA && s->status != EKG_STATUS_FFC && s->status != EKG_STATUS_DND)
		return -1;

	msg = xstrndup(s->descr, 0x1000);	/* XXX, recode */

	/* XXX, cookie */

	icq_send_snac(s, 0x02, 0x04, 0, 0,
			"TT",
			icq_pack_tlv_str(0x03, "text/x-aolrtf; charset=\"utf-8\""),
			icq_pack_tlv_str(0x04, msg));

	xfree(msg);
	return 0;
}

int icq_write_status(session_t *s) {
	icq_private_t *j = s->priv;
	uint32_t status;

	if (!s)
		return 0;

	status = (j->status_flags << 16) | icq_status(s->status);

	icq_send_snac(s, 0x01, 0x001e, 0, 0,
			"tI",
			icq_pack_tlv_dword(0x06, status));	/* TLV 6: Status mode and security flags */
	return 1;
}


int icq_write_info(session_t *s) {
	icq_private_t *j;

	if (!s || !(j = s->priv))
		return -1;

#define m_bAvatarsEnabled 0
#define m_bUtfEnabled 1

	string_t pkt, tlv_5;

	tlv_5 = string_init(NULL);

#ifdef DBG_CAPMTN
	icq_pack_append_cap(tlv_5, CAP_TYPING);
#endif

	icq_pack_append_cap(tlv_5, CAP_SRV_RELAY);	/* Client supports channel 2 extended, TLV(0x2711) based messages. */


	if (m_bUtfEnabled)
		icq_pack_append_cap(tlv_5, CAP_UTF);	/* Broadcasts the capability to receive UTF8 encoded messages */
#ifdef DBG_NEWCAPS
	/* Tells server we understand to new format of caps */
	icq_pack_append_cap(tlv_5, CAP_NEWCAPS);

#endif

#ifdef DBG_CAPXTRAZ
	icq_pack_append_cap(tlv_5, CAP_XTRAZ);		/* Broadcasts the capability to handle Xtraz */
#endif
	if (m_bAvatarsEnabled)
		icq_pack_append_cap(tlv_5, CAP_DEVILS);

#ifdef DBG_OSCARFT
	/* Broadcasts the capability to receive Oscar File Transfers */
	icq_pack_append_cap(tlv_5, CAP_SENDFILE);
#endif

	if (j->aim)
		icq_pack_append_cap(tlv_5, CAP_INTEROPERATE);	/* Tells the server we can speak to AIM */
#ifdef DBG_AIMCONTACTSEND
	icq_pack_append_cap(tlv_5, CAP_CONTACTS);		/* Client supports buddy lists transfer. */
#endif

	if (j->xstatus)
		icq_pack_append_xstatus(tlv_5, j->xstatus);

	icq_pack_append_cap(tlv_5, CAP_ICQDIRECT);		/* Something called "route finder". */

	/*packDWord(&packet, 0x178c2d9b); // Unknown cap
	  packDWord(&packet, 0xdaa545bb);
	  packDWord(&packet, 0x8ddbf3bd);
	  packDWord(&packet, 0xbd53a10a);*/

#ifdef DBG_CAPHTML
	icq_pack_append_cap(tlv_5, CAP_HTML);		/* Broadcasts the capability to receive HTML messages */
#endif
	icq_pack_append(tlv_5, "I", (uint32_t) 0x4D697261);	/* Miranda Signature */
	icq_pack_append(tlv_5, "I", (uint32_t) 0x6E64614D);
	icq_pack_append(tlv_5, "I", (uint32_t) 0x00070800);	/* MIRANDA_VERSION */
	icq_pack_append(tlv_5, "I", (uint32_t) 0x00030a0e);

	pkt = icq_pack("T", icq_pack_tlv(0x05, tlv_5->str, tlv_5->len));

	icq_makesnac(s, pkt, 0x02, 0x04, NULL, 0);
	icq_send_pkt(s, pkt);

	string_free(tlv_5, 1);
	return 0;
}

void icq_set_sequrity(session_t *s) {
	icq_private_t *j;
	string_t pkt;
	uint8_t auth, webaware;

	if (!s || !(j = s->priv))
		return;

	webaware = atoi(session_get(s, "webaware"));
	if (webaware)
		j->status_flags |= STATUS_WEBAWARE;
	else
		j->status_flags &= ~STATUS_WEBAWARE;

	if (!(s->connected))
		return;

	auth = atoi(session_get(s, "require_auth"));
	pkt = icq_pack("w iwww wwc wwc",
				(uint32_t) 4+2+2+2 + 5 + 5,	/* data chunk size */

				(uint32_t) atoi(s->uid+4),	/* request owner uin */
				(uint32_t) 0x7d0,		/* data type: META_DATA_REQ */
				(uint32_t) j->snac_seq++,	/* request sequence number */
				(uint32_t) 0x0c3a,		/* data subtype: CLI_SET_FULLINFO */

				(uint32_t) 0x30c, (uint32_t) 1, (uint32_t) webaware,	/* TLV(0x30c) (LE!) */
				(uint32_t) 0x2f8, (uint32_t) 1, (uint32_t) !auth	/* TLV(0x2f8) (LE!) */
				);
	icq_send_snac(s, 0x15, 0x02, NULL, NULL, "T", icq_pack_tlv(0x1, pkt->str, pkt->len));
	string_free(pkt, 1);
}

void icq_session_connected(session_t *s) {
	icq_private_t *j = s->priv;
	string_t pkt;

	icq_write_info(s);

	/* SNAC 3,4: Tell server who's on our list */
	/* XXX ?wo? SNAC 3,4 is: "Add buddy(s) to contact list" */
	/* XXX ?wo? check & fix */
#if 0
	if (s->userlist) {
		/* XXX, dla kazdego kontaktu... */
		icq_send_snac(s, 0x03, 0x04, 0, 0, NULL);
	}
#endif

	if (s->status == EKG_STATUS_INVISIBLE) {
		/* Tell server who's on our visible list */
#if MIRANDA
		if (!j->ssi)
			sendEntireListServ(ICQ_BOS_FAMILY, ICQ_CLI_ADDVISIBLE, BUL_VISIBLE);
		else
			updateServVisibilityCode(3);
#endif
	} else {
		/* Tell server who's on our invisible list */
#if MIRANDA
		if (!j->ssi)
			sendEntireListServ(ICQ_BOS_FAMILY, ICQ_CLI_ADDINVISIBLE, BUL_INVISIBLE);
		else
			updateServVisibilityCode(4);
#endif
	}

	/* SNAC 1,1E: Set status */
	{
		string_t pkt;
		string_t tlv_c;
		uint32_t cookie, status;

		cookie = rand() <<16 | rand();
		status = (j->status_flags << 16) | icq_status(s->status);

		pkt = string_init(NULL);

		icq_pack_append(pkt, "tI", icq_pack_tlv_dword(0x06, status));	/* TLV 6: Status mode and security flags */
		icq_pack_append(pkt, "tW", icq_pack_tlv_word(0x08, 0x00));	/* TLV 8: Error code */

		/* TLV C: Direct connection info */
		tlv_c = string_init(NULL);
		icq_pack_append(tlv_c, "I", (uint32_t) 0x00000000);	/* XXX, getSettingDword(NULL, "RealIP", 0) */
		icq_pack_append(tlv_c, "I", (uint32_t) 0x00000000);	/* XXX, nPort */
		icq_pack_append(tlv_c, "C", DC_NORMAL);			/* Normal direct connection (without proxy/firewall) */
		icq_pack_append(tlv_c, "W", ICQ_VERSION);		/* Protocol version */
		icq_pack_append(tlv_c, "I", cookie);			/* DC Cookie */
		icq_pack_append(tlv_c, "I", WEBFRONTPORT);		/* Web front port */
		icq_pack_append(tlv_c, "I", CLIENTFEATURES);		/* Client features*/
		icq_pack_append(tlv_c, "I", (uint32_t) 0xffffffff);	/* gbUnicodeCore ? 0x7fffffff : 0xffffffff */ /* Abused timestamp */
		icq_pack_append(tlv_c, "I", (uint32_t) 0x80050003);	/* Abused timestamp */
		icq_pack_append(tlv_c, "I", (uint32_t) 0x00000000);	/* XXX, Timestamp */
		icq_pack_append(tlv_c, "W", (uint32_t) 0x0000);		/* Unknown */

		icq_pack_append(pkt, "T", icq_pack_tlv(0x0C, tlv_c->str, tlv_c->len));

		string_free(tlv_c, 1);

		/* TLV(0x1F) - unknown? */
		icq_pack_append(pkt, "tW", icq_pack_tlv_word(0x1F, 0x00));

		/* TLV(0x1D) -- xstatus (icqmood) */
		if (j->xstatus && (j->xstatus - 1 <= MAX_ICQMOOD)) {
			char *mood = saprintf("icqmood%d", j->xstatus - 1);

			string_t tlv_1d = icq_pack("WCC",
						(uint32_t) 0x0e,	// item type
						(uint32_t) 0,		// item flags
						xstrlen(mood));
			string_append(tlv_1d, mood);

			icq_pack_append(pkt, "T", icq_pack_tlv(0x1d, tlv_1d->str, tlv_1d->len));

			string_free(tlv_1d, 1);
			xfree(mood);
		}

		icq_makesnac(s, pkt, 0x01, 0x1e, NULL, NULL);	/* Set status (set location info) */
		icq_send_pkt(s, pkt);
	}

	/* SNAC(1,0x11) - Set idle time */
	icq_send_snac(s, 0x01, 0x11, NULL, NULL, "I", (uint32_t) 0);

	/* j->idleAllow = 0; */

	/* Finish Login sequence */

	/* SNAC(1,2) - Client is now online and ready for normal function */
	/* imitate ICQ 6 behaviour */
	icq_send_snac(s, 0x01, 0x02, NULL, NULL,
			"WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW",
			/* family number, family version, family tool id, family tool version */
			(uint32_t) 0x01, (uint32_t) 0x04, (uint32_t) 0x0110, (uint32_t) 0x161b,
			(uint32_t) 0x02, (uint32_t) 0x01, (uint32_t) 0x0110, (uint32_t) 0x161b,
			(uint32_t) 0x03, (uint32_t) 0x01, (uint32_t) 0x0110, (uint32_t) 0x161b,
			(uint32_t) 0x04, (uint32_t) 0x01, (uint32_t) 0x0110, (uint32_t) 0x161b,
			(uint32_t) 0x06, (uint32_t) 0x01, (uint32_t) 0x0110, (uint32_t) 0x161b,
			(uint32_t) 0x09, (uint32_t) 0x01, (uint32_t) 0x0110, (uint32_t) 0x161b,
			(uint32_t) 0x0a, (uint32_t) 0x01, (uint32_t) 0x0110, (uint32_t) 0x161b,
			(uint32_t) 0x0b, (uint32_t) 0x01, (uint32_t) 0x0110, (uint32_t) 0x161b,
			(uint32_t) 0x13, (uint32_t) 0x04, (uint32_t) 0x0110, (uint32_t) 0x161b,
			(uint32_t) 0x15, (uint32_t) 0x01, (uint32_t) 0x0110, (uint32_t) 0x161b,
			(uint32_t) 0x22, (uint32_t) 0x01, (uint32_t) 0x0110, (uint32_t) 0x161b
			);

	debug_ok(" *** Yeehah, login sequence complete\n");


#if 0
	info->bLoggedIn = 1;
#endif
	/* login sequence is complete enter logged-in mode */

	if (!s->connected) {
		/* Get Offline Messages Reqeust */
		/* XXX, cookie */
		icq_send_snac(s, 0x4, 0x10, 0, 0, NULL);

		{
			/* Update our information from the server */
			private_data_t *data = NULL;
			private_item_set_int(&data, "uid", atoi(s->uid+4));

			pkt = icq_pack("i", atoi(s->uid+4));
			/* XXX, cookie, in-quiet-mode */
			icq_makemetasnac(s, pkt, 2000, 0x04D0, data, icq_my_meta_information_response);
			icq_send_pkt(s, pkt);
		}

		/* Start sending Keep-Alive packets */
		timer_remove_session(s, "ping");
		timer_add_session(s, "ping", 60, 1, icq_ping);
		timer_remove_session(s, "snac_timeout");
		timer_add_session(s, "snac_timeout", 10, 1, icq_snac_ref_list_cleanup);
#if 0
		if (m_bAvatarsEnabled)
		{ // Send SNAC 1,4 - request avatar family 0x10 connection
			icq_requestnewfamily(ICQ_AVATAR_FAMILY, &CIcqProto::StartAvatarThread);

			m_pendingAvatarsStart = 1;
			NetLog_Server("Requesting Avatar family entry point.");
		}
#endif
	}
	protocol_connected_emit(s);

	icq_set_sequrity(s);
	icq_write_status_msg(s);

	{ /* XXX ?wo? find better place for it */
		if (!j->default_group_id) {
			/* No groups on server!!! */
			/* Once executed ugly code: */
			icq_send_snac(s, 0x13, 0x11, 0, 0, "");	// Contacts edit start (begin transaction)

			j->default_group_id = 69;
			j->default_group_name = xstrdup("ekg2");

			icq_send_snac(s, 0x13, 0x08, 0, 0,		// SSI edit: add item(s)
					"U WW W W WWW",
					j->default_group_name,		// default group name
					(uint16_t) j->default_group_id, // Group#
					(uint16_t) 0,			// Item#
					(uint16_t) 1,			// Group record
					(uint16_t) 6,			// Length of the additional data
					(uint16_t) 0xc8, (uint16_t) 2, (uint16_t) 0	// tlv(0xc8) (len=2, val=0)
					);
			icq_send_snac(s, 0x13, 0x12, 0, 0, "");	// Contacts edit end (finish transaction)
		}
	}
}

static uint32_t icq_get_uid(session_t *s, const char *target) {
	char *uid;
	long int uin;

	if (!target)
		return 0;

	if (!(uid = get_uid(s, target)))
		uid = (char *) target;

	if (!xstrncmp(uid, "icq:", 4))
		uid += 4;

	if (!uid[0])
		return 0;

	uin = strtol(uid, &uid, 10);	/* XXX, strtoll() */

	if (uid[0])
		return 0;

	if (uin <= 0)
		return 0;

	return uin;
}

static QUERY(icq_session_init) {
	char		*session = *(va_arg(ap, char**));
	session_t	*s = session_find(session);
	icq_private_t *j;

	if (!s || s->priv || s->plugin != &icq_plugin)
		return 1;

#if OLD_ICQ
	userlist_free(s);
	userlist_read(s);
#endif

	j = xmalloc(sizeof(icq_private_t));
	j->fd = -1;
	j->fd2= -1;
	j->stream_buf = string_init(NULL);

	s->priv = j;
	return 0;
}

static QUERY(icq_session_deinit) {
	char		*session = *(va_arg(ap, char**));
	session_t	*s = session_find(session);
	icq_private_t *j;

	if (!s || !(j = s->priv) || s->plugin != &icq_plugin)
		return 1;
#if OLD_ICQ
	userlist_write(s);
#endif
	s->priv = NULL;

	private_items_destroy(&j->whoami);
	xfree(j->default_group_name);
	string_free(j->cookie, 1);
	string_free(j->stream_buf, 1);
	icq_snac_references_list_destroy(&j->snac_ref_list);
	icq_rates_destroy(s);

	xfree(j);
	return 0;
}

static QUERY(icq_validate_uid) {
	char	*uid	= *(va_arg(ap, char **));
	int	*valid	= va_arg(ap, int *);

	if (!uid)
		return 0;

	if (!xstrncmp(uid, "icq:", 4) && uid[4]) {
		uid+=4;
		/* now let's check if after icq: we have only digits */
		for (; *uid; uid++)
			if (!isdigit(*uid))
				return 0;

		(*valid)++;
		return -1;
	}
	return 0;
}

static QUERY(icq_print_version) {
	return 0;
}

static QUERY(icq_typing_out) {
	const char *session	= *va_arg(ap, const char **);
	const char *uid		= *va_arg(ap, const char **);
	const int len		= *va_arg(ap, const int *);
	int first		= *va_arg(ap, const int *);
	uint32_t q1 = rand(), q2 = rand();
	uint16_t typing = 0;

	session_t *s = session_find(session);

	if (!s || s->plugin != &icq_plugin)
		return 0;

	if (len>0)
		typing = (first == 1) ? 2 : 1;

	icq_send_snac(s, 0x04, 0x14, NULL, NULL,
			"iiWsW", q1, q2, (uint16_t) 1, uid + 4, typing);

	return 0;
}

void icq_handle_disconnect(session_t *s, const char *reason, int type) {
	icq_private_t *j;
	string_t str;
	const char *__reason = reason ? reason : "";

	if (!s || !(j = s->priv))
		return;

	if (!s->connected && !s->connecting)
		return;

	if (s->connected) {
		/* XXX ?wo? recode & length check */
		str = icq_pack("WC C U",
			    (uint32_t) 2, (uint32_t) 4,		/* avatar type & flags -- offline message */
			    (uint32_t) xstrlen(__reason) + 2,	/* length of message part */
			    __reason				/* message */
			    );
		icq_send_snac(s, 0x01, 0x1e, 0, 0,		/* Set status (set location info) */
			"T", icq_pack_tlv(0x1d, str->str, str->len));
		string_free(str, 1);
	}

	timer_remove_session(s, "ping");
	timer_remove_session(s, "snac_timeout");
	protocol_disconnected_emit(s, reason, type);

	if (j->fd != -1) {
		ekg_close(j->fd);
		j->fd = -1;
	}

	if (j->fd2 != -1) {
		ekg_close(j->fd2);
		j->fd2 = -1;
	}
	string_clear(j->stream_buf);
}

static WATCHER_SESSION(icq_handle_stream);

static WATCHER_SESSION(icq_handle_connect) {
	icq_private_t *j = NULL;
	int res = 0;
	socklen_t res_size = sizeof(res);

	if (type)
		return 0;

	if (type == 1) {
		debug ("[icq] handle_connect(): type %d\n", type);
		return 0;
	}

	if (!s || !(j = s->priv)) {
		debug_error("icq_handle_connect() s: 0x%x j: 0x%x\n", s, j);
		return -1;
	}

	debug("[icq] handle_connect(%d)\n", s->connecting);

	string_clear(j->stream_buf);

	if (type || getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
		if (type)
			debug_error("[icq] handle_connect(): SO_ERROR %s\n", strerror(res));

		icq_handle_disconnect(s, (type == 2) ? "Connection timed out" : strerror(res), EKG_DISCONNECT_FAILURE);
	}

	watch_add_session(s, fd, WATCH_READ, icq_handle_stream);

	return -1;
}

static WATCHER_SESSION(icq_handle_stream) {
	icq_private_t *j = NULL;
	char buf[8192];
	int len, ret, start_len, left;

	if (!s || !(j = s->priv)) {
		debug_error("icq_handle_stream() s: 0x%x j: 0x%x\n", s, j);
		return -1;
	}

	if (type)
		return 0;

	len = read(fd, buf, sizeof(buf));

	string_append_raw(j->stream_buf, buf, len);

	debug_iorecv("icq_handle_stream(%d) fd: %d; rcv: %d, %d in buffer.\n", s->connecting, fd, len, j->stream_buf->len);

	if (len < 1) {
		icq_handle_disconnect(s, strerror(errno), EKG_DISCONNECT_NETWORK);
		return -1;
	}

	icq_hexdump(DEBUG_IORECV, (unsigned char *) j->stream_buf->str, j->stream_buf->len);

	start_len = j->stream_buf->len;

	ret = icq_flap_handler(s, j->stream_buf);

	if ( (left = j->stream_buf->len) > 0 ) {
		j->stream_buf->len = start_len;
		string_remove(j->stream_buf, start_len - left);
	}

	switch (ret) {		/* XXX, magic values */
		case 0:
			/* OK */
			break;

		case -1:
		{
			debug_white("icq_handle_stream() NEED MORE DATA\n");
			break;
		}
		case -2:
			debug_error("icq_handle_stream() DISCONNECT\n");
			return -1;

		default:
			debug_error("icq_handle_stream() icq_flap_loop() returns %d ???\n", ret);
			break;
	}

	if (j->fd2 != -1) {	/* fd changed */
		ekg_close(j->fd);
		j->fd = j->fd2;
		j->fd2 = -1;

		if (s->connecting == 2) {
			watch_add_session(s, j->fd, WATCH_WRITE, icq_handle_connect);
		} else {
			debug_error("unknown s->connecting: %d\n", s->connecting);
		}
		return -1;
	}

	return 0;
}

static WATCHER(icq_handle_hubresolver) {
	session_t *s = session_find((char *) data);
	icq_private_t *j;

	struct sockaddr_in sin;
	struct in_addr a;
	int one = 1;
	int hubport;
	int res;

	if (type) {
		xfree(data);
		close(fd);
		return 0;
	}

	if (!s || !(j = s->priv))
		return -1;

	if (!s->connecting)	/* user makes /disconnect before resolver finished */
		return -1;

	res = read(fd, &a, sizeof(a));

	if ((res != sizeof(a)) || (res && a.s_addr == INADDR_NONE /* INADDR_NONE kiedy NXDOMAIN */)) {
		if (res == -1)
			debug_error("[icq] unable to read data from resolver: %s\n", strerror(errno));
		else
			debug_error("[icq] read %d bytes from resolver. not good\n", res);

		/* no point in reconnecting by icq_handle_disconnect() XXX? */

		print("conn_failed", format_find("conn_failed_resolving"), session_name(s));
		s->connecting = 0;
		return -1;
	}

	debug_function("[icq] resolved to %s\n", inet_ntoa(a));

/* port */
	hubport = session_int_get(s, "hubport");
	if (hubport < 1 || hubport > 65535)
		hubport = ICQ_HUB_PORT;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		debug("[icq] socket() failed: %s\n", strerror(errno));
		icq_handle_disconnect(s, strerror(errno), EKG_DISCONNECT_FAILURE);
		return -1;
	}

	sin.sin_family = AF_INET;
	sin.sin_port = ntohs(hubport);
	sin.sin_addr.s_addr = a.s_addr;

	if (ioctl(fd, FIONBIO, &one) == -1)
		debug_error("[icq] ioctl() FIONBIO failed: %s\n", strerror(errno));
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one)) == -1)
		debug_error("[icq] setsockopt() SO_KEEPALIVE failed: %s\n", strerror(errno));

	res = connect(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in));

	if (res == -1 && errno != EINPROGRESS) {
		int err = errno;

		debug_error("[icq] connect() failed: %s (errno=%d)\n", strerror(err), err);
		icq_handle_disconnect(s, strerror(err), EKG_DISCONNECT_FAILURE);
		return -1;
	}

	j->fd = fd;

	watch_add_session(s, fd, WATCH_WRITE, icq_handle_connect);
	return -1;
}

static COMMAND(icq_command_addssi) {
	userlist_t *u;
	icq_private_t *j;
	private_data_t *refdata = NULL;
	uint32_t u_id;
	uint16_t group;

	char *nickname = NULL;
	char *phone = NULL;
	char *email = NULL;
	char *comment = NULL;

		/* instead of PARAMASTARGET, 'cause that one fails with /add username in query */
	if (get_uid(session, params[0])) {
		/* XXX: create&use shift()? */
		target = params[0];
		params++;
	}

	if ((u = userlist_find(session, target))) {		/* don't allow to add user again */
		printq("user_exists_other", (params[0] ? params[0] : target), format_user(session, u->uid), session_name(session));
		return -1;
	}

	if (!(u_id = icq_get_uid(session, target))) {
		printq("invalid_uid", target);
		return -1;
	}

	if ( !session || !(j = session->priv) ) /* WTF? */
		return -1;

	if (params[0]) {
		char **argv = array_make(params[0], " \t", 0, 1, 1);
		int i;

		for (i = 0; argv[i]; i++) {
			if (match_arg(argv[i], 'g', "group", 2)) {
				/* XXX */
				continue;
			}

			if (match_arg(argv[i], 'p', "phone", 2) && argv[i + 1] && i++) {
				xfree(phone);
				phone = xstrdup(argv[i]);
				continue;
			}

			if (match_arg(argv[i], 'c', "comment", 2) && argv[i + 1] && i++) {
				xfree(comment);
				comment = xstrdup(argv[i]);
				continue;
			}

			if (match_arg(argv[i], 'e', "email", 2) && argv[i + 1] && i++) {
				xfree(email);
				email = xstrdup(argv[i]);
				continue;
			}

			/*    if this is -n smth */
			/* OR if param doesn't looks like command treat as a nickname */
			if ((match_arg(argv[i], 'n', ("nickname"), 2) && argv[i + 1] && i++) || argv[i][0] != '-') {
				if (userlist_find(session, argv[i])) {
					printq("user_exists", argv[i], session_name(session));
					continue;
				}

				xfree(nickname);
				nickname = xstrdup(argv[i]);
				continue;
			}
		}
		array_free(argv);
	}

	/* send packet */
	if (nickname) {
		uint16_t min = 0xffff, max = 0, count = 0, iid;
		string_t buddies = string_init(NULL), data = string_init(NULL);


		for (u = session->userlist; u; u = u->next) {
			iid = user_private_item_get_int(u, "iid");
			icq_pack_append(buddies, "W", iid);
			if (iid>max) max = iid;
			if (iid<min) min = iid;
			count++;
		}

		if (count) {
			if (min>1)
				iid = 1;
/*	XXX ?wo? find iid (for new user) between min, max
			else if (max != count) {

			}
*/
			else
				iid = max + 1;
		} else
			iid = 1;

		icq_pack_append(buddies, "W", iid);

		group = j->default_group_id;


		icq_send_snac(session, 0x13, 0x11, NULL, NULL, "");	// Contacts edit start (begin transaction)

		icq_pack_append(data, "T", icq_pack_tlv_str(0x131, nickname));
		icq_pack_append(data, "T", icq_pack_tlv(0x66, NULL, 0));	//  XXX we are awaiting authorization for this buddy
		if (email)
			icq_pack_append(data, "T", icq_pack_tlv_str(0x137, email));	// locally assigned mail address
		if (phone)
			icq_pack_append(data, "T", icq_pack_tlv_str(0x13a, phone));	// locally assigned SMS number
		if (comment)
			icq_pack_append(data, "T", icq_pack_tlv_str(0x13c, comment));	// buddy comment

		private_item_set_int(&refdata, "uid", u_id);
		private_item_set(&refdata, "nick", nickname);
		private_item_set_int(&refdata, "iid", iid);
		private_item_set_int(&refdata, "gid", group);
		private_item_set_int(&refdata, "quiet", quiet);

		icq_send_snac(session, 0x13, 0x08, refdata, icq_cmd_addssi_ack,		// SSI edit: add item(s)
				"U WWW WA",
				target+4,				// item name (uin)

				group,					// Group#
				(uint16_t) iid,				// Item#
				(uint16_t) 0,				// Buddy record

				(uint16_t) data->len,			// Length of the additional data
				data
				);

		icq_send_snac(session, 0x13, 0x09, NULL, NULL,		// SSI edit: update group header
				"U WWWW T",
				j->default_group_name,			// default group name
				group,					// Group#
				(uint16_t) 0,				// Item#
				(uint16_t) 1,				// Group record
				(uint16_t) buddies->len + 4,		// Length of the additional data
				icq_pack_tlv(0xc8, buddies->str, buddies->len)	// TLV(0xC8) contains the buddy ID#s of all buddies in the group
				);

		icq_send_snac(session, 0x13, 0x12, NULL, NULL, "");	// Contacts edit end (finish transaction)


		string_free(data, 1);
		string_free(buddies, 1);
	}

	xfree(comment);
	xfree(email);
	xfree(nickname);
	xfree(phone);

	return 0;
}

static void icq_send_msg_ch1(session_t *session, const char *uid, const char *message) {
	string_t pkt;
	string_t tlv_2, tlv_101;
	userlist_t *u = userlist_find(session, uid);
	uint16_t enc = 0;	/* ASCII */
	const char *tmp = message;

	while (*tmp) {
		if ((*tmp++ & 0x80)) {
			enc = 2;	/* not ascii char */
			break;
		}
	}
	if ( enc && !(u && user_private_item_get_int(u, "utf")) )
		enc = 3;		/* ANSI -- XXX ?wo? what should we do now? */

	/* TLV(101) */
	tlv_101 = icq_pack("WW", enc, 0x00);		// encoding, codepage
	if (enc == 2) {
		/* send unicode message */
		string_t recode = icq_convert_to_ucs2be((char *) message);
		string_append_raw(tlv_101, recode->str, recode->len);
		string_free(recode, 1);
	} else {
		/* send ASCII/ANSII message */
		string_append(tlv_101, message);
	}

	/* TLV(2) */
	tlv_2 = icq_pack("tcT",
				icq_pack_tlv_char(0x501, 0x1),			/* TLV(501) features, meaning unknown, duplicated from ICQ Lite */
				icq_pack_tlv(0x0101, tlv_101->str, tlv_101->len)/* TLV(101) text TLV. */
			);
	string_free(tlv_101, 1);

	/* main packet */
	pkt = icq_pack("iiWs", (uint32_t) rand(), (uint32_t) rand(), 1, uid+4);	// msgid1, msgid2, channel, recipient
	icq_pack_append(pkt, "TTT",
				icq_pack_tlv(0x02, tlv_2->str, tlv_2->len),	/* TLV(2) message-block */
				icq_pack_tlv(0x03, NULL, 0),			/* TLV(3) server-ack */
				icq_pack_tlv(0x06, NULL, 0)			/* TLV(6) received-offline */
				);

	string_free(tlv_2, 1);

	/* message-header */
	icq_makesnac(session, pkt, 0x04, 0x06, NULL, NULL);	// CLI_SEND_ICBM -- Send message thru server
	icq_send_pkt(session, pkt);
}

static void icq_send_msg_ch2(session_t *session, const char *uid, const char *message) {
	string_t pkt, t5, t2711;
	uint32_t msgid1 = rand(), msgid2 = rand();
	int prio = 1;
	icq_private_t *j = session->priv;
	int cookie = j->snac_seq++;

	t5 = string_init(NULL);
	icq_pack_append(t5, "WII", 0, msgid1, msgid2);	//
	icq_pack_append_cap(t5, CAP_SRV_RELAY);
	icq_pack_append(t5, "tW", icq_pack_tlv_word(0x0a, 1));
	icq_pack_append(t5, "T", icq_pack_tlv(0x0f, NULL, 0));		// empty TLV(0x0f)

	t2711 = string_init(NULL);
{
		icq_pack_append_rendezvous(t2711, ICQ_VERSION, cookie, MTYPE_PLAIN, 0, 1, prio);
		char *recode = ekg_locale_to_utf8_dup(message);
		icq_pack_append_nullterm_msg(t2711, recode);
		xfree(recode);
		icq_pack_append(t2711, "II", 0, 0xffffffff);	// XXX text & background colors
		icq_pack_append(t2711, "i", xstrlen(CAP_UTF8_str));
		string_append(t2711, CAP_UTF8_str);
}
	icq_pack_append(t5, "T", icq_pack_tlv(0x2711, t2711->str, t2711->len));
	string_free(t2711, 1);


	/* main packet */
	pkt = icq_pack("iiWs", msgid1, msgid2, 2, uid+4);	// msgid1, msgid2, channel, recipient
	icq_pack_append(pkt, "T", icq_pack_tlv(0x05, t5->str, t5->len));

	/* message-header */
	icq_makesnac(session, pkt, 0x04, 0x06, NULL, NULL);	// CLI_SEND_ICBM -- Send message thru server
	icq_send_pkt(session, pkt);
}

static COMMAND(icq_command_msg) {
	uint32_t uin;
	char *uid;
	userlist_t *u;

	if (!xstrcmp(target, "*")) {
		if (msg_all(session, name, params[1]) == -1)
			printq("list_empty");
		return 0;
	}

	if (!(uin = icq_get_uid(session, target))) {
		printq("invalid_uid", target);
		return -1;
	}

	uid = saprintf("icq:%u", uin);

	if (!session->connected)
		goto msgdisplay;

	if (config_last & 4)
		last_add(1, uid, time(NULL), 0, params[1]);

	{
		// send "typing finished" snack
		const char *sid	= session_uid_get(session);
		int first = 0, len = 0;
		query_emit_id(NULL, PROTOCOL_TYPING_OUT, &sid, &uid, &len, &first);
	}

	u = userlist_find(session, uid);
	if (u && (u->status != EKG_STATUS_NA) && (user_private_item_get_int(u, "caps") & 1<<CAP_SRV_RELAY) ) {
		icq_send_msg_ch2(session, uid, params[1]);
	} else {
		icq_send_msg_ch1(session, uid, params[1]);
	}

msgdisplay:
	if (!quiet) { /* if (1) ? */
		char **rcpts	= xcalloc(2, sizeof(char *));
		int class	= EKG_MSGCLASS_SENT_CHAT;	/* XXX? */

		rcpts[0]	= xstrdup(uid);
		rcpts[1]	= NULL;

		/* XXX, encrypt */

		protocol_message_emit(session, session->uid, rcpts, params[1], NULL, time(NULL), class, NULL, EKG_NO_BEEP, 0);

		array_free(rcpts);

		/* XXX, it's copied from jabber-plugin, however i think we should _always_ add message to queue (if we're offline), even if we want do it quiet */
		if (!session->connected)
			return msg_queue_add(session_uid_get(session), uid, params[1], "offline", class);
	}

	if (!quiet)
		session_unidle(session);

	return 0;
}

static COMMAND(icq_command_inline_msg) {
	const char *p[2] = { NULL, params[0] };
	if (!params[0] || !target)
		return -1;
	return icq_command_msg(("msg"), p, session, target, quiet);
}

static COMMAND(icq_command_away) {
	const char *descr, *format;
	int allow_descr = 0;

	if (!xstrcmp(name, ("_autoback"))) {
		format = "auto_back";
		session_status_set(session, EKG_STATUS_AUTOBACK);
		session_unidle(session);
	} else if (!xstrcmp(name, ("back"))) {
		format = "back";
		session_status_set(session, EKG_STATUS_AVAIL);
		session_unidle(session);
	} else if (!xstrcmp(name, ("_autoaway"))) {
		format = "auto_away";
		session_status_set(session, EKG_STATUS_AUTOAWAY);
		allow_descr = 1;
	} else if (!xstrcmp(name, ("_autoxa"))) {
		format = "auto_xa";
		session_status_set(session, EKG_STATUS_AUTOXA);
		allow_descr = 1;
	} else if (!xstrcmp(name, ("away"))) {
		format = "away";
		session_status_set(session, EKG_STATUS_AWAY);
		session_unidle(session);
		allow_descr = 1;
	} else if (!xstrcmp(name, ("dnd"))) {
		format = "dnd";
		session_status_set(session, EKG_STATUS_DND);
		session_unidle(session);
		allow_descr = 1;
	} else if (!xstrcmp(name, ("ffc"))) {
		format = "ffc";
		session_status_set(session, EKG_STATUS_FFC);
		session_unidle(session);
		allow_descr = 1;
	} else if (!xstrcmp(name, ("xa"))) {
		format = "xa";
		session_status_set(session, EKG_STATUS_XA);
		session_unidle(session);
		allow_descr = 1;
	} else if (!xstrcmp(name, ("gone"))) {
		format = "gone";
		session_status_set(session, EKG_STATUS_GONE);
		session_unidle(session);
		allow_descr = 1;
	} else if (!xstrcmp(name, ("invisible"))) {
		format = "invisible";
		session_status_set(session, EKG_STATUS_INVISIBLE);
		session_unidle(session);
	} else
		return -1;

	if (allow_descr) {
		if (params[0]) {
			session_descr_set(session, (!xstrcmp(params[0], "-")) ? NULL : params[0]);
			reason_changed = 1;
		} else {
			char *tmp;

			if (!config_keep_reason) {
				session_descr_set(session, NULL);
			} else if ((tmp = ekg_draw_descr(session_status_get(session)))) {
				session_descr_set(session, tmp);
				xfree(tmp);
			}
		}
	} else
		session_descr_set(session, NULL);

	descr = (char *) session_descr_get(session);

	if (descr) {
		char *f = saprintf("%s_descr", format);
		printq(f, descr, "", session_name(session));
		xfree(f);
	} else
		printq(format, session_name(session));

	ekg_update_status(session);

	if (session->connected) {
		/* icq_write_info(session); */
		icq_write_status(session);
		icq_write_status_msg(session);
	}
	return 0;
}

static COMMAND(icq_command_connect) {
	icq_private_t *j = session->priv;
	const char *hubserver;

	if (session->connecting) {
		printq("during_connect", session_name(session));
		return -1;
	}

	if (session->connected) {
		printq("already_connected", session_name(session));
		return -1;
	}

/* proxy */
	if (session_int_get(session, "proxy") == 1) {
		/* XXX, implement proxy connection */
		debug_error("icq_command_connect() proxy?\n");
		return -1;
	}

/* hubserver */
	if (!(hubserver = session_get(session, "server")))
		hubserver = ICQ_HUB_SERVER;

	session->connecting = 1;
	j->ssi = 1;	/* XXX */
	j->aim = 1;	/* XXX */

	if (ekg_resolver2(&icq_plugin, hubserver, icq_handle_hubresolver, xstrdup(session->uid)) == NULL) {
		print("generic_error", strerror(errno));
		session->connecting = 0;
		return -1;
	}

	printq("connecting", session_name(session));
	if ((session_status_get(session) == EKG_STATUS_NA))
		session_status_set(session, EKG_STATUS_AVAIL);

	return 0;
}

static COMMAND(icq_command_disconnect) {
	if (timer_remove_session(session, "reconnect") == 0) {
		printq("auto_reconnect_removed", session_name(session));
		return 0;
	}

	if (!session->connecting && !session->connected) {
		printq("not_connected", session_name(session));
		return -1;
	}

	if (session->connecting)
		icq_handle_disconnect(session, NULL, EKG_DISCONNECT_STOPPED);
	else
		icq_handle_disconnect(session, params[0], EKG_DISCONNECT_USER);

	return 0;
}

static COMMAND(icq_command_reconnect) {
	if (session->connecting || session->connected)
		icq_command_disconnect(name, params, session, target, quiet);

	return icq_command_connect(name, params, session, target, quiet);
}

static COMMAND(icq_command_userinfo) {
	string_t pkt;
	uint32_t number;
	int minimal_req = 0;	/* XXX */
	private_data_t *ref_data = NULL;

	if (!(number = icq_get_uid(session, target))) {
		printq("invalid_uid", target);
		return -1;
	}

	/* XXX xookie */
	private_item_set_int(&ref_data, "uid", number);

	pkt = icq_pack("i", number);
	icq_makemetasnac(session, pkt, 2000, (minimal_req == 0) ? 1202 : 1210, ref_data, NULL);

	icq_send_pkt(session, pkt);
	return 0;
}

static COMMAND(icq_command_searchuin) {
	string_t pkt;
	uint32_t uin;

	debug_function("icq_command_searchuin() %s\n", params[0]);

	if (!(uin = icq_get_uid(session, target))) {
		printq("invalid_uid", target);
		return -1;
	}

	/* XXX, cookie */
	pkt = icq_pack("wwi", icq_pack_tlv_dword(0x0136, uin));		/* TLV_UID */

	icq_makemetasnac(session, pkt, 2000, 0x0569, NULL, 0);	/* META_SEARCH_UIN */
	icq_send_pkt(session, pkt);
	return 0;
}

static COMMAND(icq_command_search) {
	const char *city = NULL;
	const char *email = NULL;
	const char *nickname = NULL;
	const char *last_name = NULL;
	const char *first_name = NULL;
	int only_online = 0;
	int gender = 0;

	char **argv;
	int i;

	string_t pkt;

	argv = array_make(params[0], " \t", 0, 1, 1);

	for (i = 0; argv[i]; i++) {
		if (match_arg(argv[i], 'c', "city", 2) && argv[i+1]) {
			city = argv[++i];
			continue;
		}

		if (match_arg(argv[i], 'e', "email", 2) && argv[i+1]) {
			email = argv[++i];
			continue;
		}

		if (match_arg(argv[i], 'f', "firstname", 2) && argv[i+1]) {
			first_name = argv[++i];
			continue;
		}

		if (match_arg(argv[i], 'n', "nickname", 2) && argv[i+1]) {
			nickname = argv[++i];
			continue;
		}

		if (match_arg(argv[i], 'l', "lastname", 2) && argv[i+1]) {
			last_name = argv[++i];
			continue;
		}

		if (!xstrcasecmp(argv[i], "--female")) {
			gender = 1;
			continue;
		}

		if (!xstrcasecmp(argv[i], "--male")) {
			gender = 2;
			continue;
		}

		if (!xstrcasecmp(argv[i], "--online")) {
			only_online = 1;
			continue;
		}

		/* XXX, madrzej? zgadywanie? */
		printq("invalid_params", name);
		array_free(argv);
		return -1;
	}

	/* XXX, cookie */

	/* Pack the search details */
	pkt = string_init(NULL);

#define wo_idnhtni(type, str) \
	{ \
		uint32_t len = xstrlen(str); \
		icq_pack_append(pkt, "www", (uint32_t) type, len+3, len+1); \
		string_append_raw(pkt, (char *) str, len+1); \
	}

	if (first_name) wo_idnhtni(0x0140, first_name);	/* TLV_FIRSTNAME */
	if (last_name) wo_idnhtni(0x014A, last_name);	/* TLV_LASTNAME */
	if (nickname) wo_idnhtni(0x0154, nickname);	/* TLV_NICKNAME */
	if (email) wo_idnhtni(0x015E, email);		/* TLV_EMAIL */
	if (city) wo_idnhtni(0x0190, city);		/* TLV_CITY */

/* more options:

	searchPackTLVLNTS(&buf, &buflen, hwndDlg, IDC_STATE, TLV_STATE);
	searchPackTLVLNTS(&buf, &buflen, hwndDlg, IDC_COMPANY, TLV_COMPANY);
	searchPackTLVLNTS(&buf, &buflen, hwndDlg, IDC_DEPARTMENT, TLV_DEPARTMENT);
	searchPackTLVLNTS(&buf, &buflen, hwndDlg, IDC_POSITION, TLV_POSITION);
	searchPackTLVLNTS(&buf, &buflen, hwndDlg, IDC_KEYWORDS, TLV_KEYWORDS);

	ppackTLVDWord(&buf, &buflen, (DWORD)getCurItemData(hwndDlg, IDC_AGERANGE),	TLV_AGERANGE,  0);
*/

	if (gender) icq_pack_append(pkt, "wwc", icq_pack_tlv_char(0x017C, gender));

/*
	ppackTLVByte(&buf,  &buflen, (BYTE)getCurItemData(hwndDlg,  IDC_MARITALSTATUS), TLV_MARITAL,   0);
	ppackTLVWord(&buf,  &buflen, (WORD)getCurItemData(hwndDlg,  IDC_LANGUAGE),	TLV_LANGUAGE,  0);
	ppackTLVWord(&buf,  &buflen, (WORD)getCurItemData(hwndDlg,  IDC_COUNTRY),	TLV_COUNTRY,   0);
	ppackTLVWord(&buf,  &buflen, (WORD)getCurItemData(hwndDlg,  IDC_WORKFIELD),	TLV_OCUPATION, 0);

	searchPackTLVWordLNTS(&buf, &buflen, hwndDlg, IDC_PASTKEY, (WORD)getCurItemData(hwndDlg, IDC_PASTCAT), TLV_PASTINFO);
	searchPackTLVWordLNTS(&buf, &buflen, hwndDlg, IDC_INTERESTSKEY, (WORD)getCurItemData(hwndDlg, IDC_INTERESTSCAT), TLV_INTERESTS);
	searchPackTLVWordLNTS(&buf, &buflen, hwndDlg, IDC_ORGKEYWORDS, (WORD)getCurItemData(hwndDlg, IDC_ORGANISATION), TLV_AFFILATIONS);
	searchPackTLVWordLNTS(&buf, &buflen, hwndDlg, IDC_HOMEPAGEKEY, (WORD)getCurItemData(hwndDlg, IDC_HOMEPAGECAT), TLV_HOMEPAGE);
 */

	icq_pack_append(pkt, "wwc", icq_pack_tlv_char(0x0230, only_online));

	icq_makemetasnac(session, pkt, 2000, 0x055F, NULL, 0);	/* META_SEARCH_GENERIC */
	icq_send_pkt(session, pkt);

	array_free(argv);

	return 0;
#undef wo_idnhtni
}

static COMMAND(icq_command_auth) {
	uint32_t number;
	const char *reason = NULL;

	if (match_arg(params[0], 'l', "list", 2)) {
		userlist_t *u;
		for (u = session->userlist; u; u = u->next) {
			if (user_private_item_get_int(u, "auth") == 1) {
				printq("icq_user_info_generic", _("Waiting for authorization"), u->uid);
			}
		}
		return 0;
	}

	if (params[1]) {
		target = params[1];
		reason = params[2];
	} else if (!target) {
		printq("invalid_params", name);
		return -1;
	}

	if (!(number = icq_get_uid(session, target))) {
		printq("invalid_uid", target);
		return -1;
	}

	/* XXX, pending auth!!! like /auth -l in jabber */
	/* XXX, reasons!! */
	/* XXX, messages */

	if (match_arg(params[0], 'r', "request", 2)) {

		if (!reason)
			reason = "Please add me.";

		icq_send_snac(session, 0x13, 0x18, 0, 0,
				"uUW", number, reason, (uint32_t) 0x00);

		return 0;
	}

	if (match_arg(params[0], 'c', "cancel", 2)) {

		icq_send_snac(session, 0x13, 0x16, 0, 0, "u", number);

		return 0;
	}

	if (match_arg(params[0], 'a', "accept", 2) || match_arg(params[0], 'd', "deny", 2)) {	/* accept / deny */
		int auth = (match_arg(params[0], 'a', "accept", 2) != 0);

		icq_send_snac(session, 0x13, 0x1a, 0, 0,
				"ucUW", number, (uint32_t) auth, reason ? reason : "", (uint32_t) 0x00);
		return 0;
	}

	printq("invalid_params", name);
	return -1;
}

static COMMAND(icq_command_rates) {
	icq_private_t *j = session->priv;
	int i;

	for (i=0; i < j->n_rates; i++) {
		if (!i)
			print("icq_rates_header");
		printq("icq_rates",
			itoa(i+1),
			itoa(j->rates[i]->win_size),
			itoa(j->rates[i]->clear_lvl),
			itoa(j->rates[i]->alert_lvl),
			itoa(j->rates[i]->limit_lvl),
			itoa(j->rates[i]->discn_lvl),
			itoa(j->rates[i]->curr_lvl),
			itoa(j->rates[i]->max_lvl));
	}

	return 0;
}

static COMMAND(icq_command_whoami) {
	display_whoami(session);
	return 0;
}

static COMMAND(icq_command_register) {
	printq("generic_error", "Create a new ICQ account on http://lite.icq.com/register");
	return 0;
}

static QUERY(icq_userlist_info_handle) {
	userlist_t *u	= *va_arg(ap, userlist_t **);
	int quiet	= *va_arg(ap, int *);
	const char *tmp;
	int i;

	if (!u || valid_plugin_uid(&icq_plugin, u->uid) != 1)
		return 1;

	if ( (i = user_private_item_get_int(u, "xstatus")) )
		printq("icq_user_info_generic", _("xStatus"), icq_xstatus_name(i));

	if ( (tmp = int2time_str("%Y-%m-%d %H:%M", user_private_item_get_int(u, "online"))) )
		printq("icq_user_info_generic", _("Online since"), tmp);

	if ( (tmp = int2time_str("%Y-%m-%d %H:%M", user_private_item_get_int(u, "member"))) )
		printq("icq_user_info_generic", _("ICQ Member since"), tmp);

	if ( (tmp = user_private_item_get(u, "comment")) )
		printq("icq_user_info_generic", _("Comment"), tmp);
	if ( (tmp = user_private_item_get(u, "email")) )
		printq("icq_user_info_generic", _("e-mail"), tmp);
	if ( user_private_item_get_int(u, "auth"))
		printq("icq_user_info_generic", _("Waiting for authorization"), "yes");

	return 0;
}

static int icq_theme_init() {
#ifndef NO_DEFAULT_THEME
	format_add("icq_auth_subscribe", _("%> (%1) %T%2%n asks for authorisation. Use \"/auth -a %2\" to accept, \"/auth -d %2 [reason]\" to refuse. Reason: %T%3%n"), 1);
	format_add("icq_auth_decline", _("%> (%1) %2 decline your authorization: %3\n"), 1);
	format_add("icq_auth_grant", _("%> (%1) %2 grant your authorization: %3\n"), 1);

	format_add("icq_userinfo_start",	"%g,+=%G----- %g%3%G info for: %Ticq:%2%n", 1);

	format_add("icq_userinfo_affilations",	"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_basic",	"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_email",	"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_short",	"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_more",		"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_work",		"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_interests",	"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_notes",	"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_hpagecat",	"%g|| %n  %T%3:%n %4", 1);

	format_add("icq_userinfo_end",		"%g`+=%G----- End%n", 1);

	format_add("icq_user_info_generic", "%K| %n%1: %T%2%n\n", 1);

	format_add("icq_rates_header", "%>%n # %K|%n Curr %K|%n Alrt %K|%n Limt %K|%n Clear %K|%n Dscn %K|%n  Max %K|%nwin %K|%n\n", 1);
	format_add("icq_rates", "%>%n%[-2]1 %K|%n%[-5]7 %K|%n%[-5]4 %K|%n%[-5]5 %K|%n%[-6]3 %K|%n%[-5]6 %K|%n%[-5]8 %K|%n%[-3]2 %K|%n\n", 1);
	format_add("icq_you_were_added",	"%> (%1) %2 adds you to contact list\n", 1);

#endif
	return 0;
}


static void icq_changed_our_sequrity(session_t *s, const char *var) {
	const char *val;
	icq_private_t *j;
	int webaware;

	if (!s || !(j = s->priv))
		return;

	if (!(val = session_get(s, var)) || !*val)
		return;

	if ((webaware = !xstrcasecmp(var, "webaware")) || !xstrcasecmp(var, "require_auth")) {
		icq_set_sequrity(s);
		if (webaware)
			icq_write_status(s);
	} else if (!xstrcasecmp(var, "hide_ip")) {
		if (*val & 1) {
			j->status_flags |= STATUS_DCAUTH;
			j->status_flags &= ~STATUS_SHOWIP;
		} else {
			j->status_flags |= STATUS_SHOWIP;
			j->status_flags &= ~STATUS_DCAUTH;
		}
		icq_write_status(s);
	}
}

static plugins_params_t icq_plugin_vars[] = {
	PLUGIN_VAR_ADD("alias",			VAR_STR, NULL, 0, NULL),
	PLUGIN_VAR_ADD("auto_connect",		VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("auto_reconnect",	VAR_INT,  "0", 0, NULL),
	PLUGIN_VAR_ADD("log_formats",		VAR_STR, "xml,simple,sqlite", 0, NULL),
	PLUGIN_VAR_ADD("password",		VAR_STR, NULL, 1, NULL),
	PLUGIN_VAR_ADD("plaintext_passwd",	VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("server",		VAR_STR, NULL, 0, NULL),

	PLUGIN_VAR_ADD("hide_ip",		VAR_BOOL,  "1", 0, icq_changed_our_sequrity),
	PLUGIN_VAR_ADD("require_auth",		VAR_BOOL,  "1", 0, icq_changed_our_sequrity),
	PLUGIN_VAR_ADD("webaware",		VAR_BOOL,  "0", 0, icq_changed_our_sequrity),

	PLUGIN_VAR_END()
};

static const char *icq_protocols[] = { "icq:", NULL };
static const status_t icq_statuses[] = {
	EKG_STATUS_NA, EKG_STATUS_GONE, EKG_STATUS_DND, EKG_STATUS_XA,
	EKG_STATUS_AWAY, EKG_STATUS_AVAIL, EKG_STATUS_FFC,
	EKG_STATUS_INVISIBLE, EKG_STATUS_NULL
};

static const struct protocol_plugin_priv icq_priv = {
	.protocols	= icq_protocols,
	.statuses	= icq_statuses
};

EXPORT int icq_plugin_init(int prio) {
#define ICQ_ONLY		SESSION_MUSTBELONG | SESSION_MUSTHASPRIVATE
#define ICQ_FLAGS		ICQ_ONLY | SESSION_MUSTBECONNECTED
#define ICQ_FLAGS_TARGET	ICQ_FLAGS | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET
#define ICQ_FLAGS_MSG		ICQ_ONLY | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET

	PLUGIN_CHECK_VER("icq");

	icq_convert_string_init();
	ekg_recode_utf8_inc();

	icq_plugin.params	= icq_plugin_vars;
	icq_plugin.priv		= &icq_priv;

	plugin_register(&icq_plugin, prio);

	query_connect_id(&icq_plugin, PROTOCOL_VALIDATE_UID, icq_validate_uid, NULL);
	query_connect_id(&icq_plugin, PLUGIN_PRINT_VERSION, icq_print_version, NULL);
	query_connect_id(&icq_plugin, SESSION_ADDED, icq_session_init, NULL);
	query_connect_id(&icq_plugin, SESSION_REMOVED, icq_session_deinit, NULL);
	query_connect_id(&icq_plugin, USERLIST_INFO, icq_userlist_info_handle, NULL);
	query_connect_id(&icq_plugin, PROTOCOL_TYPING_OUT, icq_typing_out, NULL);

	command_add(&icq_plugin, "icq:", "?", icq_command_inline_msg, ICQ_ONLY | COMMAND_PASS_UNCHANGED, NULL);
	command_add(&icq_plugin, "icq:msg", "!uU !", icq_command_msg, ICQ_FLAGS_MSG, NULL);
	command_add(&icq_plugin, "icq:chat", "!uU !", icq_command_msg, ICQ_FLAGS_MSG, NULL);

	command_add(&icq_plugin, "icq:addssi", "!p ?", icq_command_addssi, ICQ_FLAGS, "-p --phone -c --comment -e --email");

	command_add(&icq_plugin, "icq:auth", "!p uU ?", icq_command_auth, ICQ_FLAGS | COMMAND_ENABLEREQPARAMS, "-a --accept -d --deny -l --list -r --request -c --cancel");

	command_add(&icq_plugin, "icq:away", "r", icq_command_away, ICQ_ONLY, NULL);
	command_add(&icq_plugin, "icq:back", NULL, icq_command_away, ICQ_ONLY, NULL);
	command_add(&icq_plugin, "icq:dnd",  "r", icq_command_away, ICQ_ONLY, NULL);
	command_add(&icq_plugin, "icq:ffc",  "r", icq_command_away, ICQ_ONLY, NULL);
	command_add(&icq_plugin, "icq:gone",  "r", icq_command_away, ICQ_ONLY, NULL);
	command_add(&icq_plugin, "icq:invisible", NULL, icq_command_away, ICQ_ONLY, NULL);
	command_add(&icq_plugin, "icq:xa",  "r", icq_command_away, ICQ_ONLY, NULL);

	command_add(&icq_plugin, "icq:userinfo", "!u",	icq_command_userinfo,	ICQ_FLAGS_TARGET, NULL);
	command_add(&icq_plugin, "icq:register", NULL,	icq_command_register,	0, NULL);

	/* XXX, makes generic icq:search */
	command_add(&icq_plugin, "icq:searchuin", "!u",	icq_command_searchuin,	ICQ_FLAGS_TARGET, NULL);
	command_add(&icq_plugin, "icq:search", "!p",	icq_command_search,	ICQ_FLAGS | COMMAND_ENABLEREQPARAMS, NULL);

	command_add(&icq_plugin, "icq:connect", NULL,	icq_command_connect,	ICQ_ONLY, NULL);
	command_add(&icq_plugin, "icq:disconnect", "r", icq_command_disconnect,	ICQ_ONLY, NULL);
	command_add(&icq_plugin, "icq:reconnect", NULL,	icq_command_reconnect,	ICQ_ONLY, NULL);

	command_add(&icq_plugin, "icq:whoami", NULL,	icq_command_whoami,	ICQ_ONLY, NULL);

	command_add(&icq_plugin, "icq:_rates", NULL,	icq_command_rates,	ICQ_ONLY, NULL);

	return 0;
}

static int icq_plugin_destroy() {
	icq_convert_string_destroy();
	plugin_unregister(&icq_plugin);
	ekg_recode_utf8_dec();
	return 0;
}
