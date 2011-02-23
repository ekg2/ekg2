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

#include "ekg2.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "icq.h"
#include "icq_snac_handlers.h"
#include "misc.h"

void *ucs2be_conv_in = (void*) -1;
void *ucs2be_conv_out = (void*) -1;

void icq_hexdump(int level, unsigned char *p, size_t len) {
	#define MAX_BYTES_PER_LINE 16
	unsigned char *payload = (unsigned char *) p;
	int offset = 0;

	while (len) {
		int display_len;
		int i;

		if (len > MAX_BYTES_PER_LINE)
			display_len = MAX_BYTES_PER_LINE;
		else	display_len = len;

	/* offset */
		debug_ext(level, "\t0x%.4x  ", offset);
	/* hexdump */
		for(i = 0; i < MAX_BYTES_PER_LINE; i++) {
			if (i < display_len)
				debug_ext(level, "%.2x ", payload[i]);
			else	debug_ext(level, "   ");
		}
	/* seperate */
		debug_ext(level, "   ");

	/* asciidump if printable, else '.' */
		for(i = 0; i < display_len; i++)
			debug_ext(level, "%c", isprint(payload[i]) ? payload[i] : '.');
		debug_ext(level, "\n");

		payload	+= display_len;
		offset	+= display_len;
		len	-= display_len;
	}
}

static void icq_pack_common(GString *str, char *format, va_list ap) {
	if (!format)
		return;

	while (*format) {
		switch (*format) {
			case 'c':	/* guint8 */
			case 'C':
			{
				guint32 src = va_arg(ap, guint32);
				unsigned char buf[1];

				buf[0] = src;

				g_string_append_len(str, (char *) buf, 1);
				break;
			}

			case 'W':	/* guint16 BE */
			{
				guint32 src = va_arg(ap, guint32);
				unsigned char buf[2];

				buf[0] = (src & 0xff00) >> 8;
				buf[1] = (src & 0x00ff);

				g_string_append_len(str, (char *) buf, 2);
				break;
			}

			case 'w':	/* guint16 LE */
			{
				guint32 src = va_arg(ap, guint32);
				unsigned char buf[2];

				buf[0] = (src & 0x00ff);
				buf[1] = (src & 0xff00) >> 8;

				g_string_append_len(str, (char *) buf, 2);
				break;
			}

			case 'I':	/* guint32 BE */
			{
				guint32 src = va_arg(ap, guint32);
				unsigned char buf[4];

				buf[0] = (src & 0xff000000) >> 24;
				buf[1] = (src & 0x00ff0000) >> 16;
				buf[2] = (src & 0x0000ff00) >> 8;
				buf[3] = (src & 0x000000ff);

				g_string_append_len(str, (char *) buf, 4);
				break;
			}

			case 'i':	/* guint32 LE */
			{
				guint32 src = va_arg(ap, guint32);
				unsigned char buf[4];

				buf[3] = (src & 0xff000000) >> 24;
				buf[2] = (src & 0x00ff0000) >> 16;
				buf[1] = (src & 0x0000ff00) >> 8;
				buf[0] = (src & 0x000000ff);

				g_string_append_len(str, (char *) buf, 4);
				break;
			}

			case 'T':	/* TLV */		/* guint32 type, guint32 len, guint8 *buf (buflen = len) */
			{
				guint32 t_type = va_arg(ap, guint32);
				guint32 t_len	= va_arg(ap, guint32);
				guint8 *t_buf	= va_arg(ap, guint8 *);

				icq_pack_append(str, "WW", t_type, t_len);
				g_string_append_len(str, (char *) t_buf, t_len);

				break;
			}

			case 't':	/* tlv */		/* guint32 type, guint32 len */
			{
				guint32 t_type = va_arg(ap, guint32);
				guint32 t_len	= va_arg(ap, guint32);

				icq_pack_append(str, "WW", t_type, t_len);
				break;
			}

			case 'U':
			{
				char *buf = va_arg(ap, char *);

				icq_pack_append(str, "W", (guint32) xstrlen(buf));
				g_string_append(str, buf);
				break;
			}

			case 'u':	/* uid */
			{
				guint32 uin = va_arg(ap, guint32);
				const char *buf = ekg_itoa(uin);	/* XXX, enough? */

				icq_pack_append(str, "C", (guint32) xstrlen(buf));
				g_string_append(str, buf);
				break;
			}

			case 's':
			{
				char *buf = va_arg(ap, char *);

				icq_pack_append(str, "C", (guint32) xstrlen(buf));
				g_string_append(str, buf);
				break;
			}
			case 'P':	/* caps */
			{
				guint32 t_new = 0x09460000 | va_arg(ap, guint32);

				icq_pack_append(str, "IIII", (guint32) t_new, (guint32) 0x4c7f11d1, (guint32) 0x82224445, (guint32) 0x53540000);
				break;
			}

			case 'A':	/* append GString* */
			{
				GString *buf = va_arg(ap, GString *);

				g_string_append_len(str, buf->str, buf->len);

				break;
			}

			case ' ':	/* skip whitespaces */
			case 0x09:
			case 0x0A:
			case 0x0D:
				break;

			default:
				debug_error("icq_pack() unknown format: %c\n", *format);
				break;
		}
		format++;
	}
}

GString *icq_pack_append(GString *str, char *format, ...) {
	va_list ap;

	va_start(ap, format);
	icq_pack_common(str, format, ap);
	va_end(ap);

	return str;
}

GString *icq_pack(char *format, ...) {
	GString *str = g_string_new(NULL);
	va_list ap;

	va_start(ap, format);
	icq_pack_common(str, format, ap);
	va_end(ap);

	return str;
}

guint32 icq_string_to_BE(unsigned char *buf, int len) {
	switch (len) {
		case 1:	return buf[0];
		case 2:	return buf[0] << 8 | buf[1];
		case 4:	return buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
	}

	/* debug("icq_string_to_BE() len: %d\n", len); */
	return 0;
}

int icq_unpack_common(unsigned char *buf, unsigned char **endbuf, int *l, char *format, va_list ap) {
	static char ubuf[0xFF+1]; int used_ubuf = 0;
	static char Ubuf[0xFFFF+1]; int used_Ubuf = 0;

	int len = *l;

	if (!len || !format)
		return 0;

	while (*format) {
		if (*format >= '0' && *format <= '9') {
			long int skip = strtol(format, &format, 10);

			if (len < skip) {
				debug_error("icq_unpack() len: %d skiplen: %ld\n", len, skip);
				goto err2;
			}

			buf += skip; len -= skip;
			continue;
		}

		switch (*format) {
			case 'c':	/* guint8 */
			case 'C':
			{
				guint8 *dest = va_arg(ap, guint8 *);

				if (len < 1)
					goto err;

				*dest = *buf;
				buf++; len--;
				break;
			}

			case 'W':	/* guint16 BE */
			{
				guint16 *dest = va_arg(ap, guint16 *);

				if (len < 2)
					goto err;

				*dest = buf[0] << 8 | buf[1];
				buf += 2; len -= 2;
				break;
			}

			case 'w':	/* guint16 LE */
			{
				guint16 *dest = va_arg(ap, guint16 *);

				if (len < 2)
					goto err;

				*dest = buf[1] << 8 | buf[0];
				buf += 2; len -= 2;
				break;
			}

			case 'I':	/* guint32 BE */
			{
				guint32 *dest = va_arg(ap, guint32 *);

				if (len < 4)
					goto err;

				*dest = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
				buf += 4; len -= 4;
				break;
			}

			case 'i':	/* guint32 LE */
			{
				guint32 *dest = va_arg(ap, guint32 *);

				if (len < 4)
					goto err;

				*dest = buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0];
				buf += 4; len -= 4;
				break;
			}

			case 'u':
			{
				guint8 ulen;
				char **dest = va_arg(ap, char **);

				if (used_ubuf) {
					debug_error("icq_unpack() ubuf used!\n");
					goto err2;
				}

				if (len < 1)
					goto err;

				ulen = buf[0];
				buf += 1; len -= 1;

				if (len < ulen)
					goto err;

				*dest = memcpy(ubuf, buf, ulen);
				ubuf[ulen] = '\0';
				buf += ulen; len -= ulen;

				used_ubuf = 1;
				break;
			}

			case 'U':
			case 'S':
			{
				guint16 Ulen;
				char **dest = va_arg(ap, char **);

				if (used_Ubuf) {
					debug_error("icq_unpack() Ubuf used!\n");
					goto err2;
				}

				if (len < 2)
					goto err;

				if (*format == 'S')
					Ulen = buf[1] << 8 | buf[0];
				else	Ulen = buf[0] << 8 | buf[1];

				buf += 2; len -= 2;

				if (len < Ulen)
					goto err;

				*dest = memcpy(Ubuf, buf, Ulen);
				Ubuf[Ulen] = '\0';
				buf += Ulen; len -= Ulen;

				used_Ubuf = 1;
				break;
			}

			case 'x':	/* skip this byte */
				buf += 1;
				len -= 1;
				break;

			case 'X':	/* skip this byte */
				buf += 2;
				len -= 2;
				break;

			case ' ':	/* skip whitespaces */
			case 0x09:
			case 0x0A:
			case 0x0D:
				break;

			default:
				debug_error("icq_unpack() unknown format: %c\n", *format);
				goto err2;
		}
		format++;
	}

	*endbuf = buf;
	*l = len;

	return 1;

err:
	debug_error("icq_unpack() len: %d format: %c\n", len, *format);
err2:
	return 0;
}

int icq_unpack(unsigned char *buf, unsigned char **endbuf, int *l, char *format, ...) {
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = icq_unpack_common(buf, endbuf, l, format, ap);
	va_end(ap);
	return ret;
}

int icq_unpack_nc(unsigned char *buf, int len, char *format, ...) {
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = icq_unpack_common(buf, &buf, &len, format, ap);
	va_end(ap);
	return ret;
}

static LIST_FREE_ITEM(tlv_free_do_nothing, icq_tlv_t *) { }
DYNSTUFF_LIST_DECLARE(icq_tlvs, icq_tlv_t, tlv_free_do_nothing,
	static __DYNSTUFF_ADD,			/* icq_tlvs_add() */
	__DYNSTUFF_NOREMOVE,
	__DYNSTUFF_DESTROY)			/* icq_tlvs_destroy() */

icq_tlv_t *icq_tlv_get(struct icq_tlv_list *l, guint16 type) {
	for (; l; l = l->next) {
		if (l->type == type)
			return l;
	}
	return NULL;
}

struct icq_tlv_list *icq_unpack_tlvs(unsigned char **str, int *maxlen, unsigned int maxcount) {
	struct icq_tlv_list *ret = NULL;
	int count = 0;

	while (*maxlen >= 4) {
		guint16 type, len;
		icq_tlv_t *ptlv;

		if (!icq_unpack(*str, str, maxlen, "WW", &type, &len))
			return ret;

		debug("str_readtlvs(%d) NEXTTLV type: 0x%x len: %d (maxlen: %d maxcount: %d)\n", count, type, len, *maxlen, maxcount ? maxcount-count : 0);

		if (*maxlen < len) {
			debug("str_readtlvs() 1897 Incomplete TLV %d, len %ld of %ld - ignoring.\n", type, len, *maxlen);
			return ret;
		}

		ptlv = xmalloc(sizeof(icq_tlv_t));

		ptlv->type = type;
		ptlv->len = len;

		ptlv->buf = *str;
		ptlv->nr = icq_string_to_BE(ptlv->buf, ptlv->len);

		*maxlen -= len;
		*str += (len);			/* go to next TLV */

		icq_tlvs_add(&ret, ptlv);
		count++;

		if (maxcount && maxcount == count)
			break;
	}
	return ret;
}

struct icq_tlv_list *icq_unpack_tlvs_nc(unsigned char *str, int maxlen, unsigned int maxcount) {
	return icq_unpack_tlvs(&str, &maxlen, maxcount);
}

#include "miscicq.h"

guint16 icq_status(int status) {
	switch (status) {
		case EKG_STATUS_NA:
			debug_error("icq_status(EKG_STATUS_NA)\n");
			return 0;

		case EKG_STATUS_AVAIL:		return ICQ_STATUS_ONLINE;
		case EKG_STATUS_AWAY:		return ICQ_STATUS_AWAY;
		case EKG_STATUS_DND:		return ICQ_STATUS_DND;
		case EKG_STATUS_FFC:		return ICQ_STATUS_FFC;
		case EKG_STATUS_INVISIBLE:	return ICQ_STATUS_INVISIBLE;
		case EKG_STATUS_XA:		return ICQ_STATUS_OCCUPIED;	/* XXX good choice? */
		case EKG_STATUS_GONE:		return ICQ_STATUS_NA;

		default:			return STATUS_ICQONLINE;
	}
}

status_t icq2ekg_status2(int nMsgType) {
	switch (nMsgType) {
		case MTYPE_AUTOAWAY:	return EKG_STATUS_AWAY;
		case MTYPE_AUTOBUSY:	return EKG_STATUS_XA;	/* XXX good choice? */
		case MTYPE_AUTONA:	return EKG_STATUS_GONE;
		case MTYPE_AUTODND:	return EKG_STATUS_DND;
		case MTYPE_AUTOFFC:	return EKG_STATUS_FFC;

		default:		return EKG_STATUS_UNKNOWN;
	}
}

status_t icq2ekg_status(int icq_status) {
	/*
	 * idea from IcqStatusToMiranda()
	 *
	 * Maps the ICQ status flag (as seen in the status change SNACS) and returns a EKG2 style status.
	 *
	 * NOTE: The order in which the flags are compared are important!
	 */
	if (icq_status & ICQ_STATUS_FLAG_INVISIBLE)
		return EKG_STATUS_INVISIBLE;
	if (icq_status & ICQ_STATUS_FLAG_DND)
		return EKG_STATUS_DND;
	if (icq_status & ICQ_STATUS_FLAG_OCCUPIED)
		return EKG_STATUS_XA;	/* XXX good choice? */
	if (icq_status & ICQ_STATUS_FLAG_NA)
		return EKG_STATUS_GONE;
	if (icq_status & ICQ_STATUS_FLAG_AWAY)
		return EKG_STATUS_AWAY;
	if (icq_status & ICQ_STATUS_FLAG_FFC)
		return EKG_STATUS_FFC;

	return EKG_STATUS_AVAIL;

}

int tlv_length_check(char *name, icq_tlv_t *t, int length) {
	if (t->len == length)
		return 0;

	debug_error("%s Incorrect TVL type=0x%02x. Length=%d, should be %d.\n", name, t->type, t->len, length);
	return 1;
}

/* hash password, ripped from micq */
char *icq_encryptpw(const char *pw) {
	/* Passwords are roasted when sent to the host. This is done so they aren't
	 * sent in "clear text" over the wire, although they are still trivial to
	 * decode. Roasting is performed by first xoring each byte in the password
	 * with the equivalent modulo byte in the roasting array.
	 */
	guint8 tb[] = { 0xf3, 0x26, 0x81, 0xc4, 0x39, 0x86, 0xdb, 0x92, 0x71, 0xa3, 0xb9, 0xe6, 0x53, 0x7a, 0x95, 0x7c };

	char *cpw = xstrdup(pw), *p;
	int i = 0;

	for (p = cpw; *p; p++, i++)
		*p ^= tb[i % 16];
	return cpw;
}

#include "icq_debug.inc"

const char *icq_lookuptable(struct fieldnames_t *table, int code) {
	int i;

	if (code == 0)
		return NULL;

	for(i = 0; table[i].code != -1 && table[i].text; i++) {
		if (table[i].code == code)
			return table[i].text;
	}

	debug_error("icq_lookuptable() invalid lookup: %x\n", code);
	return NULL;
}

void icq_pack_append_client_identification(GString *pkt) {
	/*
	 * Pack client identification details.
	 */
	icq_pack_append(pkt, "T",  icq_pack_tlv_str(0x03, CLIENT_ID_STRING));		// TLV(0x03) - client id string (name, version)
	icq_pack_append(pkt, "tW", icq_pack_tlv_word(0x16, CLIENT_ID_CODE));		// TLV(0x16) - client id number
	icq_pack_append(pkt, "tW", icq_pack_tlv_word(0x17, CLIENT_VERSION_MAJOR));	// TLV(0x17) - client major version
	icq_pack_append(pkt, "tW", icq_pack_tlv_word(0x18, CLIENT_VERSION_MINOR));	// TLV(0x18) - client minor version
	icq_pack_append(pkt, "tW", icq_pack_tlv_word(0x19, CLIENT_VERSION_LESSER));	// TLV(0x19) - client lesser version
	icq_pack_append(pkt, "tW", icq_pack_tlv_word(0x1a, CLIENT_VERSION_BUILD));	// TLV(0x1A) - client build number
	icq_pack_append(pkt, "tI", icq_pack_tlv_dword(0x14, CLIENT_DISTRIBUTION));	// TLV(0x14) - distribution number
	icq_pack_append(pkt, "T",  icq_pack_tlv_str(0x0f, CLIENT_LANGUAGE));		// TLV(0x0F) - client language (2 symbols)
	icq_pack_append(pkt, "T",  icq_pack_tlv_str(0x0e, CLIENT_COUNTRY));		// TLV(0x0E) - client country (2 symbols)
}

void icq_convert_string_init() {
	ucs2be_conv_in = ekg_convert_string_init("UCS-2BE", NULL, &ucs2be_conv_out);
}

void icq_convert_string_destroy() {
	if (ucs2be_conv_in != (void*) -1) {
		ekg_convert_string_destroy(ucs2be_conv_in);
		ekg_convert_string_destroy(ucs2be_conv_out);
	}
}

char *icq_convert_from_ucs2be(char *buf, int len) {
	GString *text, *ret;

	if (!buf || !len)
		return NULL;

	text = g_string_new(NULL);
	g_string_append_len(text, buf, len);

	ret = ekg_convert_string_t_p(text, ucs2be_conv_in);

	g_string_free(text, TRUE);

	if (ret)
		return g_string_free(ret, FALSE);

	return NULL;
}

GString *icq_convert_to_ucs2be(char *text) {
	GString *ret, *s;

	if (!text || !*text)
		return NULL;

	s = g_string_new(text);
	ret = ekg_convert_string_t_p(s, ucs2be_conv_out);
	/* XXX, ret == NULL */
	g_string_free(s, TRUE);

	return ret;
}

void icq_send_snac(session_t *s, guint16 family, guint16 cmd, private_data_t *data, snac_subhandler_t subhandler, char *format, ...) {
	va_list ap;
	GString *pkt = g_string_new(NULL);

	if (format && *format) {
		va_start(ap, format);
		icq_pack_common(pkt, format, ap);
		va_end(ap);
	}

	icq_makesnac(s, pkt, family, cmd, data, subhandler);
	icq_send_pkt(s, pkt);
}

/*
 * rate limit handle
 */
void icq_rates_destroy(session_t *s) {
	icq_private_t *j;
	int i;

	if (!s || !(j = s->priv))
		return;

	for (i=0; i<j->n_rates; i++) {
		xfree(j->rates[i]->groups);
		xfree(j->rates[i]);
	}
	xfree(j->rates);
	j->rates = NULL;
	j->n_rates = 0;
}

void icq_rates_init(session_t *s, int n_rates) {
	icq_private_t *j;
	int i;

	if (!s || !(j = s->priv))
		return;

	if (j->rates)
		icq_rates_destroy(s);

	if (n_rates<=0)
		return;

	j->n_rates = n_rates;
	j->rates = xmalloc(sizeof(icq_rate_t *) * n_rates);

	for (i=0; i<j->n_rates; i++)
		j->rates[i] = xmalloc(sizeof(icq_rate_t));
}
