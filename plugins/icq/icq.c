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
#include <ekg/queries.h>
#include <ekg/protocol.h>
#include <ekg/themes.h>
#include <ekg/windows.h>	// XXX
#include <ekg/stuff.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

#include <ekg/log.h>
#include <ekg/msgqueue.h>

#include "icq.h"
#include "misc.h"

#include "icq_flap_handlers.h"
#include "icq_snac_handlers.h"

#define ICQ_HUB_SERVER "login.icq.com"
#define ICQ_HUB_PORT	5190

static int icq_theme_init();
PLUGIN_DEFINE(icq, PLUGIN_PROTOCOL, icq_theme_init);

int icq_send_pkt(session_t *s, string_t buf) {
	icq_private_t *j;
	int fd;

	if (!s || !(j = s->priv) || !buf)
		return -1;

	fd = j->fd;

	debug_io("icq_send_pkt(%s) fd: %d len: %d\n", s->uid, fd, buf->len);
	icq_hexdump(DEBUG_IO, (unsigned char *) buf->str, buf->len);
	ekg_write(fd, buf->str, buf->len);
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

	if (s->status != EKG_STATUS_AWAY && s->status != EKG_STATUS_XA && s->status != EKG_STATUS_FFC && s->status != EKG_STATUS_DND)
		return -1;
	/* XXX, NA also supported */

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
	uint16_t status;

	if (!s)
		return 0;

	status = icq_status(s->status);

	icq_send_snac(s, 0x01, 0x001e, 0, 0,
			"tI",
			icq_pack_tlv_dword(0x06, (0x00 << 8 | status)));	/* TLV 6: Status mode and security flags */
	return 1;
}

int icq_write_info(session_t *s) {
	icq_private_t *j;
	
	if (!s || !(j = s->priv))
		return -1;

#define m_bAvatarsEnabled 0
#define m_bUtfEnabled 0
#define DBG_CAPMTN 1
#define DBG_AIMCONTACTSEND 1

	string_t pkt, tlv_5;

	tlv_5 = string_init(NULL);

#ifdef DBG_CAPMTN
	icq_pack_append(tlv_5, "IIII",	(uint32_t) 0x563FC809,		/* CAP_TYPING */
					(uint32_t) 0x0B6F41BD,
					(uint32_t) 0x9F794226,
					(uint32_t) 0x09DFA2F3);
#endif

	icq_pack_append(tlv_5, "P", (uint32_t) 0x1349);			/* AIM_CAPS_ICQSERVERRELAY - Client supports channel 2 extended, TLV(0x2711) based messages. */

	/* Broadcasts the capability to receive UTF8 encoded messages */
	if (m_bUtfEnabled) 
		icq_pack_append(tlv_5, "P", (uint32_t) 0x134E);		/* CAP_UTF8MSGS - Client supports UTF-8 messages.*/
#ifdef DBG_NEWCAPS
	/* Tells server we understand to new format of caps */
	icq_pack_append(tlv_5, "I", (uint32_t) 0x0000);			/* CAP_NEWCAPS */
#endif

#ifdef DBG_CAPXTRAZ
	icq_pack_append(tlv_5, "I", (uint32_t) 0x1a093c6c);		/* CAP_XTRAZ */
	icq_pack_append(tlv_5, "I", (uint32_t) 0xd7fd4ec5);		/* Broadcasts the capability to handle */
	icq_pack_append(tlv_5, "I", (uint32_t) 0x9d51a647);		/* Xtraz */
	icq_pack_append(tlv_5, "I", (uint32_t) 0x4e34f5a0);
#endif
	if (m_bAvatarsEnabled)
		icq_pack_append(tlv_5, "P", (uint32_t) 0x134C);		/* CAP_DEVILS */

#ifdef DBG_OSCARFT
	/* Broadcasts the capability to receive Oscar File Transfers */
	icq_pack_append(tlv_5, "P", (uint32_t) 0x1343);			/* CAP_AIM_FILE - Client supports file transfer (can send files). */
#endif

	if (j->aim)
		icq_pack_append(tlv_5, "I", (uint32_t) 0x134D);	/* Tells the server we can speak to AIM */
#ifdef DBG_AIMCONTACTSEND
	icq_pack_append(tlv_5, "P", (uint32_t) 0x134B);		/* CAP_AIM_SENDBUDDYLIST - Client supports buddy lists transfer. */
#endif
#if 0
	BYTE bXStatus = getContactXStatus(NULL);
	if (bXStatus)
	{
		packBuffer(tlv_5, capXStatus[bXStatus-1], 0x10);
	}
#endif
	icq_pack_append(tlv_5, "P", (uint32_t) 0x1344);		/* AIM_CAPS_ICQDIRECT - Something called "route finder". */

	/*packDWord(&packet, 0x178c2d9b); // Unknown cap
	  packDWord(&packet, 0xdaa545bb);
	  packDWord(&packet, 0x8ddbf3bd);
	  packDWord(&packet, 0xbd53a10a);*/

#ifdef DBG_CAPHTML
	icq_pack_append(tlv_5, "I", (uint32_t) 0x0138ca7b);	/* CAP_HTMLMSGS */
	icq_pack_append(tlv_5, "I", (uint32_t) 0x769a4915);	/* Broadcasts the capability to receive */
	icq_pack_append(tlv_5, "I", (uint32_t) 0x88f213fc);	/* HTML messages */
	icq_pack_append(tlv_5, "I", (uint32_t) 0x00979ea8);
#endif
	icq_pack_append(tlv_5, "I", (uint32_t) 0x4D697261);	/* Miranda Signature */
	icq_pack_append(tlv_5, "I", (uint32_t) 0x6E64614D);
	icq_pack_append(tlv_5, "I", (uint32_t) 0x00070800);	/* MIRANDA_VERSION */
	icq_pack_append(tlv_5, "I", (uint32_t) 0x00030a0e);

	pkt = icq_pack("T", icq_pack_tlv(0x05, tlv_5->str, tlv_5->len));

	icq_makesnac(s, pkt, 0x02, 0x04, 0, 0);
	icq_send_pkt(s, pkt);

	string_free(tlv_5, 1);
	return 0;
}

void icq_session_connected(session_t *s) {
	icq_private_t *j;
	string_t pkt;

	icq_write_info(s);

	/* SNAC 3,4: Tell server who's on our list */
	/* XXX ?wo? SNAC 3,4 is: "Add buddy(s) to contact list" */
	if (s->userlist) {
		/* XXX, dla kazdego kontaktu... */
		icq_send_snac(s, 0x03, 0x04, 0, 0, NULL);
	}

	if (s->status == EKG_STATUS_INVISIBLE) {
		/* Tell server who's on our visible list */
#if MIRANDA
		if (!j->ssi)
			sendEntireListServ(ICQ_BOS_FAMILY, ICQ_CLI_ADDVISIBLE, BUL_VISIBLE);
		else
			updateServVisibilityCode(3);
#endif
	}

	if (s->status != EKG_STATUS_INVISIBLE)
	{
		/* Tell server who's on our invisible list */
#if MIRANDA
		if (!j->ssi)
			sendEntireListServ(ICQ_BOS_FAMILY, ICQ_CLI_ADDINVISIBLE, BUL_INVISIBLE);
		else
			updateServVisibilityCode(4);
#endif
		;
	}

	/* SNAC 1,1E: Set status */
	{
#if MIRANDA
		DWORD dwDirectCookie = rand() ^ (rand() << 16);
		BYTE bXStatus = getContactXStatus(NULL);
		char szMoodId[32];
		WORD cbMoodId = 0;
		WORD cbMoodData = 0;

		if (bXStatus && moodXStatus[bXStatus-1] != -1)
		{ // prepare mood id
			null_snprintf(szMoodId, SIZEOF(szMoodId), "icqmood%d", moodXStatus[bXStatus-1]);
			cbMoodId = strlennull(szMoodId);
			cbMoodData = 8;
		}
#endif
		string_t pkt;
		string_t tlv_c;
		uint16_t status;

		status = icq_status(s->status);

		pkt = string_init(NULL);

		icq_pack_append(pkt, "tI", icq_pack_tlv_dword(0x06, (0x00 << 8 | status)));	/* TLV 6: Status mode and security flags */
		icq_pack_append(pkt, "tW", icq_pack_tlv_word(0x08, 0x00));			/* TLV 8: Error code */

	/* TLV C: Direct connection info */
		tlv_c = string_init(NULL);
		icq_pack_append(tlv_c, "I", (uint32_t) 0x00000000);	/* XXX, getSettingDword(NULL, "RealIP", 0) */
		icq_pack_append(tlv_c, "I", (uint32_t) 0x00000000);	/* XXX, nPort */
		icq_pack_append(tlv_c, "C", (uint32_t) 0x04);		/* Normal direct connection (without proxy/firewall) */
		icq_pack_append(tlv_c, "W", (uint32_t) 0x08);		/* Protocol version */
		icq_pack_append(tlv_c, "I", (uint32_t) 0x00);		/* XXX, DC Cookie */
		icq_pack_append(tlv_c, "I", (uint32_t) 0x50);		/* WEBFRONTPORT */
		icq_pack_append(tlv_c, "I", (uint32_t) 0x03);		/* CLIENTFEATURES */
		icq_pack_append(tlv_c, "I", (uint32_t) 0xffffffff);	/* gbUnicodeCore ? 0x7fffffff : 0xffffffff */ /* Abused timestamp */
		icq_pack_append(tlv_c, "I", (uint32_t) 0x80050003);	/* Abused timestamp */
		icq_pack_append(tlv_c, "I", (uint32_t) 0x00000000);	/* XXX, Timestamp */
		icq_pack_append(tlv_c, "W", (uint32_t) 0x0000);		/* Unknown */

		icq_pack_append(pkt, "T", icq_pack_tlv(0x0C, tlv_c->str, tlv_c->len));
		/* XXX, assert(tlv_c->len == 0x25) */
		string_free(tlv_c, 1);

		icq_pack_append(pkt, "tW", icq_pack_tlv_word(0x1F, 0x00));

#if MIRANDA
		if (cbMoodId)
		{ // Pack mood data
			packWord(&packet, 0x1D);	      // TLV 1D
			packWord(&packet, (WORD)(cbMoodId + 4)); // TLV length
			packWord(&packet, 0x0E);	      // Item Type
			packWord(&packet, cbMoodId);	      // Flags + Item Length
			packBuffer(&packet, (LPBYTE)szMoodId, cbMoodId); // Mood
		}
#endif
		icq_makesnac(s, pkt, 0x01, 0x1E, 0, 0);
		icq_send_pkt(s, pkt); pkt = NULL;
	}

	/* SNAC 1,11 */
	icq_send_snac(s, 0x01, 0x11, 0, 0, "I", (uint32_t) 0x00000000);

	/* j->idleAllow = 0; */

	/* Finish Login sequence */

	pkt = string_init(NULL);

	icq_pack_append(pkt, "WWI", (uint32_t) 0x22, (uint32_t) 0x01, (uint32_t) 0x0110161b);	/* imitate ICQ 6 behaviour */
	icq_pack_append(pkt, "WWI", (uint32_t) 0x01, (uint32_t) 0x04, (uint32_t) 0x0110161b);
	icq_pack_append(pkt, "WWI", (uint32_t) 0x13, (uint32_t) 0x04, (uint32_t) 0x0110161b);
	icq_pack_append(pkt, "WWI", (uint32_t) 0x02, (uint32_t) 0x01, (uint32_t) 0x0110161b);
	icq_pack_append(pkt, "WWI", (uint32_t) 0x03, (uint32_t) 0x01, (uint32_t) 0x0110161b);
	icq_pack_append(pkt, "WWI", (uint32_t) 0x15, (uint32_t) 0x01, (uint32_t) 0x0110161b);
	icq_pack_append(pkt, "WWI", (uint32_t) 0x04, (uint32_t) 0x01, (uint32_t) 0x0110161b);
	icq_pack_append(pkt, "WWI", (uint32_t) 0x06, (uint32_t) 0x01, (uint32_t) 0x0110161b);
	icq_pack_append(pkt, "WWI", (uint32_t) 0x09, (uint32_t) 0x01, (uint32_t) 0x0110161b);
	icq_pack_append(pkt, "WWI", (uint32_t) 0x0a, (uint32_t) 0x01, (uint32_t) 0x0110161b);
	icq_pack_append(pkt, "WWI", (uint32_t) 0x0b, (uint32_t) 0x01, (uint32_t) 0x0110161b);

	icq_makesnac(s, pkt, 0x01, 0x02, 0, 0);
	icq_send_pkt(s, pkt); pkt = NULL;

	debug_white(" *** Yeehah, login sequence complete\n");


#if 0
	info->bLoggedIn = 1;
#endif
	/* login sequence is complete enter logged-in mode */

	if (!s->connected) {
		/* Get Offline Messages Reqeust */
		/* XXX, cookie */
		icq_send_snac(s, 0x4, 0x10, 0, 0, NULL);

		/* Update our information from the server */
		pkt = icq_pack("i", atoi(s->uid+4));
		/* XXX, cookie, in-quiet-mode */
		icq_makemetasnac(s, pkt, 2000, 0x04D0, 0);
		icq_send_pkt(s, pkt);

		/* Start sending Keep-Alive packets */
		timer_remove_session(s, "ping");
		timer_add_session(s, "ping", 60, 1, icq_ping);
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

	icq_write_status_msg(s);

	{ /* XXX ?wo? find better place for it */
		if (s && (j = s->priv) && (!j->user_default_group)) {
			/* No groups on server!!! */
			/* Once executed ugly code: */
			icq_send_snac(s, 0x13, 0x11, 0, 0, "");	// Contacts edit start (begin transaction)

			j->user_default_group = 69;

			icq_send_snac(s, 0x13, 0x08, 0, 0,		// SSI edit: add item(s)
					"U WW W W WWW",
					"ekg2",				// default group name
					(uint16_t) j->user_default_group, // Group#
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

	string_free(j->cookie, 1);
	string_free(j->stream_buf, 1);

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

	icq_send_snac(s, 0x04, 0x14, 0, 0,
			"iiWuW", q1, q2, (uint16_t) 1, atoi(uid + 4), typing);

	return 0;
}

void icq_handle_disconnect(session_t *s, const char *reason, int type) {
	icq_private_t *j;

	if (!s || !(j = s->priv))
		return;

	if (!s->connected && !s->connecting)
		return;

	timer_remove_session(s, "ping");
	protocol_disconnected_emit(s, reason, type);

	if (j->fd != -1) {
		ekg_close(j->fd);
		j->fd = -1;
	}

	if (j->fd2 != -1) {
		ekg_close(j->fd2);
		j->fd2 = -1;
	}
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
	int len, ret, start_len;

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

	string_remove(j->stream_buf, start_len - j->stream_buf->len);

	switch (ret) {		/* XXX, magic values */
		case 0:
			/* OK */
			break;

		case -1:
			debug_white("icq_flap_loop() NEED MORE DATA\n");
			break;

		case -2:
			debug_error("icq_flap_loop() DISCONNECT\n");
			return -1;

		default:
			debug_error("icq_flap_loop() == %d ???\n", ret);
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
	uint32_t uin;

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

	if (!(uin = icq_get_uid(session, target))) {
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

	/* sent packet */
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


		icq_send_snac(session, 0x13, 0x11, 0, 0, "");		// Contacts edit start (begin transaction)

		icq_pack_append(data, "T", icq_pack_tlv_str(0x131, nickname));
		icq_pack_append(data, "T", icq_pack_tlv(0x66, NULL, 0)); //  XXX we are awaiting authorization for this buddy
		if (email)
			icq_pack_append(data, "T", icq_pack_tlv_str(0x137, email));	// locally assigned mail address
		if (phone)
			icq_pack_append(data, "T", icq_pack_tlv_str(0x13a, phone));	// locally assigned SMS number
		if (comment)
			icq_pack_append(data, "T", icq_pack_tlv_str(0x13c, comment));	// buddy comment

		icq_send_snac(session, 0x13, 0x08, 0, 0,		// SSI edit: add item(s)
				"U WWW WA",
				target+4,				// item name (uin)

				(uint16_t) j->user_default_group,	// Group#
				(uint16_t) iid,				// Item#
				(uint16_t) 0,				// Buddy record

				(uint16_t) data->len,			// Length of the additional data
				data
				);
		
		icq_send_snac(session, 0x13, 0x09, 0, 0,		// SSI edit: update group header
				"U WWWW T",
				"ekg2",					// default group name
				(uint16_t) j->user_default_group,	// Group#
				(uint16_t) 0,				// Item#
				(uint16_t) 1,				// Group record
				(uint16_t) buddies->len + 4,		// Length of the additional data
				icq_pack_tlv(0xc8, buddies->str, buddies->len)	// TLV(0xC8) contains the buddy ID#s of all buddies in the group
				);

		icq_send_snac(session, 0x13, 0x12, 0, 0, "");	// Contacts edit end (finish transaction)

		/* XXX ?wo? add user to contact list after serwer response (OK) */
		if ( (u = userlist_add(session, target, nickname)) ) {
			query_emit_id(NULL, USERLIST_ADDED, &u->uid, &u->nickname, &quiet);
			query_emit_id(NULL, ADD_NOTIFY, &session->uid, &u->uid);
			user_private_item_set_int(u, "iid", iid);
			user_private_item_set_int(u, "gid", j->user_default_group);
			/* XXX ?wo? ask for authorizarion flag */
		}

		string_free(data, 1);
		string_free(buddies, 1);
	}

	xfree(comment);
	xfree(email);
	xfree(nickname);
	xfree(phone);

	return 0;
}

static COMMAND(icq_command_msg) {
	uint32_t uin;
	char *uid;

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

	/* XXX, recode, and do other magic stuff :( */
	/* sent message */
	{
		/* for now params[1] should be US-ASCII encoded message.
		 * sorry */
		string_t pkt;
		string_t tlv_2, tlv_101;
		string_t recode;

	/* TLV(101) */
		/* XXX ?wo? check if we can send utf message */
		recode = icq_convert_to_ucs2be((char *) params[1]);
		tlv_101 = icq_pack("WW", 0x02, 0x00);		// Unicode
		string_append_raw(tlv_101, recode->str, recode->len);
		string_free(recode, 1);

		/* send ASCII message
		tlv_101 = icq_pack("WW", 0x03, 0x00);		// codepage, encoding, copied from ICQ lite
		string_append(tlv_101, params[1]);
		*/

	/* TLV(2) */
		tlv_2 = icq_pack("tcT",
					icq_pack_tlv_char(0x501, 0x1),			/* TLV(501) features, meaning unknown, duplicated from ICQ Lite */
					icq_pack_tlv(0x0101, tlv_101->str, tlv_101->len)/* TLV(101) text TLV. */
				);
		string_free(tlv_101, 1);

	/* main packet */
		pkt = icq_pack("iiWu", (uint32_t) rand(), (uint32_t) rand(), 0x01, (uint32_t) uin);
		icq_pack_append(pkt, "TTT", 
					icq_pack_tlv(0x02, tlv_2->str, tlv_2->len),	/* TLV(2) message-block */
					icq_pack_tlv(0x03, NULL, 0),			/* TLV(3) server-ack */
					icq_pack_tlv(0x06, NULL, 0)			/* TLV(6) received-offline */
					);

	/* message-header */
		icq_makesnac(session, pkt, 0x04, 0x06, 0, 0);
		icq_send_pkt(session, pkt);

		string_free(tlv_2, 1);
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

/* XXX, jak user podal adres serwera, to lacz sie z serwerem */
	if ((session_get(session, "server"))) {
		/* XXX */
	}

/* hubserver */
	if (!(hubserver = session_get(session, "hubserver"))) 
		hubserver = ICQ_HUB_SERVER;

	session->connecting = 1;
	j->ssi = 1;	/* XXX */
	j->aim = 1;	/* XXX */

	if (ekg_resolver2(&icq_plugin, hubserver, icq_handle_hubresolver, xstrdup(session->uid)) == NULL) {
		print("generic_error", strerror(errno));
		session->connecting = 0;
		return -1;
	}

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
		icq_handle_disconnect(session, NULL, EKG_DISCONNECT_USER);

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

	if (!(number = icq_get_uid(session, target))) {
		printq("invalid_uid", target);
		return -1;
	}

	/* XXX xookie */

	pkt = icq_pack("i", number);
	icq_makemetasnac(session, pkt, 2000, (minimal_req == 0) ? 1202 : 1210, 0);

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

	icq_makemetasnac(session, pkt, 2000, 0x0569, 0);	/* META_SEARCH_UIN */
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

	icq_makemetasnac(session, pkt, 2000, 0x055F, 0);	/* META_SEARCH_GENERIC */
	icq_send_pkt(session, pkt);

	array_free(argv);

	return 0;
#undef wo_idnhtni
}

static COMMAND(icq_command_auth) {
	uint32_t number;
	const char *reason;

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

	reason = params[2];

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


static COMMAND(icq_command_register) {
	printq("generic_error", "Create a new ICQ account on http://lite.icq.com/register");
	return 0;
}

static QUERY(icq_userlist_info_handle) {
	userlist_t *u	= *va_arg(ap, userlist_t **);
	int quiet	= *va_arg(ap, int *);
	const char *tmp;

	if (!u || valid_plugin_uid(&icq_plugin, u->uid) != 1)
		return 1;

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

	format_add("icq_userinfo_start",	"%g,+=%G----- %g%3%G info for: %Ticq:%2%n", 1);

	format_add("icq_userinfo_affilations",	"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_basic",	"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_email",	"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_short",	"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_more",		"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_work",		"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_interests",	"%g|| %n  %T%3:%n %4", 1);
	format_add("icq_userinfo_notes",	"%g|| %n  %T%3:%n %4", 1);

	format_add("icq_userinfo_end",		"%g`+=%G----- End%n", 1);

	format_add("icq_user_info_generic", "%K| %n%1: %T%2%n\n", 1);

#endif
	return 0;
}

static plugins_params_t icq_plugin_vars[] = {
	PLUGIN_VAR_ADD("alias",			VAR_STR, NULL, 0, NULL), 
	PLUGIN_VAR_ADD("auto_connect",		VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("auto_reconnect",	VAR_INT,  "0", 0, NULL),
	PLUGIN_VAR_ADD("log_formats",		VAR_STR, "xml,simple,sqlite", 0, NULL),
	PLUGIN_VAR_ADD("password",		VAR_STR, NULL, 1, NULL),
	PLUGIN_VAR_ADD("plaintext_passwd",	VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_END()
};

EXPORT int icq_plugin_init(int prio) {
#define ICQ_ONLY		SESSION_MUSTBELONG | SESSION_MUSTHASPRIVATE 
#define ICQ_FLAGS		ICQ_ONLY | SESSION_MUSTBECONNECTED
#define ICQ_FLAGS_TARGET	ICQ_FLAGS | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET
#define ICQ_FLAGS_MSG		ICQ_ONLY | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET

	PLUGIN_CHECK_VER("icq");

	icq_convert_string_init();

	icq_plugin.params = icq_plugin_vars;

	plugin_register(&icq_plugin, prio);

	query_connect_id(&icq_plugin, PROTOCOL_VALIDATE_UID, icq_validate_uid, NULL);
	query_connect_id(&icq_plugin, PLUGIN_PRINT_VERSION, icq_print_version, NULL);
	query_connect_id(&icq_plugin, SESSION_ADDED, icq_session_init, NULL);
	query_connect_id(&icq_plugin, SESSION_REMOVED, icq_session_deinit, NULL);
	query_connect_id(&icq_plugin, USERLIST_INFO, icq_userlist_info_handle, NULL);
	query_connect_id(&icq_plugin, PROTOCOL_TYPING_OUT, icq_typing_out, NULL);

	command_add(&icq_plugin, "icq:", "?", icq_command_inline_msg, ICQ_ONLY, NULL);
	command_add(&icq_plugin, "icq:msg", "!uU !", icq_command_msg, ICQ_FLAGS_MSG, NULL);

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
	command_add(&icq_plugin, "icq:disconnect", NULL,icq_command_disconnect,	ICQ_ONLY, NULL);
	command_add(&icq_plugin, "icq:reconnect", NULL,	icq_command_reconnect,	ICQ_ONLY, NULL);

	return 0;
}

static int icq_plugin_destroy() {
	icq_convert_string_destroy();
	plugin_unregister(&icq_plugin);
	return 0;
}
