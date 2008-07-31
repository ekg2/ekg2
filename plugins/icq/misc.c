#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/dynstuff_inline.h>
#include <ekg/xmalloc.h>

#include <ekg/userlist.h>

#include "misc.h"

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
		len 	-= display_len;
	}
}

static void icq_pack_common(string_t str, char *format, va_list ap) {
	if (!format)
		return;

	while (*format) {
		switch (*format) {
			case 'c':	/* uint8_t */
			case 'C':
			{
				uint32_t src = va_arg(ap, uint32_t);
				unsigned char buf[1];

				buf[0] = src;

				string_append_raw(str, (char *) buf, 1);
				break;
			}

			case 'W':	/* uint16_t BE */
			{
				uint32_t src = va_arg(ap, uint32_t);
				unsigned char buf[2];

				buf[0] = (src & 0xff00) >> 8;
				buf[1] = (src & 0x00ff);

				string_append_raw(str, (char *) buf, 2);
				break;
			}

			case 'w':	/* uint16_t LE */
			{
				uint32_t src = va_arg(ap, uint32_t);
				unsigned char buf[2];

				buf[0] = (src & 0x00ff);
				buf[1] = (src & 0xff00) >> 8;

				string_append_raw(str, (char *) buf, 2);
				break;
			}

			case 'I':	/* uint32_t BE */
			{
				uint32_t src = va_arg(ap, uint32_t);
				unsigned char buf[4];

				buf[0] = (src & 0xff000000) >> 24;
				buf[1] = (src & 0x00ff0000) >> 16;
				buf[2] = (src & 0x0000ff00) >> 8;
				buf[3] = (src & 0x000000ff);

				string_append_raw(str, (char *) buf, 4);
				break;
			}

			case 'i':	/* uint32_t LE */
			{
				uint32_t src = va_arg(ap, uint32_t);
				unsigned char buf[4];

				buf[3] = (src & 0xff000000) >> 24;
				buf[2] = (src & 0x00ff0000) >> 16;
				buf[1] = (src & 0x0000ff00) >> 8;
				buf[0] = (src & 0x000000ff);

				string_append_raw(str, (char *) buf, 4);
				break;
			}

			case 'T':	/* TLV */		/* uint32_t type, uint32_t len, uint8_t *buf (buflen = len) */
			{
				uint32_t t_type = va_arg(ap, uint32_t);
				uint32_t t_len  = va_arg(ap, uint32_t);
				uint8_t *t_buf  = va_arg(ap, uint8_t *);

				icq_pack_append(str, "WW", t_type, t_len);
				string_append_raw(str, (char *) t_buf, t_len);

				break;
			}

			case 't':	/* tlv */		/* uint32_t type, uint32_t len */
			{
				uint32_t t_type = va_arg(ap, uint32_t);
				uint32_t t_len  = va_arg(ap, uint32_t);

				icq_pack_append(str, "WW", t_type, t_len);
				break;
			}

			default:
				debug_error("icq_pack() unknown format: %c\n", *format);
				break;
		}
		format++;
	}
}

string_t icq_pack_append(string_t str, char *format, ...) {
	va_list ap;

	va_start(ap, format);
	icq_pack_common(str, format, ap);
	va_end(ap);

	return str;
}

string_t icq_pack(char *format, ...) {
	string_t str = string_init(NULL);
	va_list ap;

	va_start(ap, format);
	icq_pack_common(str, format, ap);
	va_end(ap);

	return str;
}

uint32_t icq_string_to_BE(unsigned char *buf, int len) {
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
		switch (*format) {
			case 'c':	/* uint8_t */
			case 'C':
			{
				uint8_t *dest = va_arg(ap, uint8_t *);

				if (len < 1)
					goto err;

				*dest = *buf;
				buf++; len--;
				break;
			}

			case 'W':	/* uint16_t BE */
			{
				uint16_t *dest = va_arg(ap, uint16_t *);

				if (len < 2)
					goto err;

				*dest = buf[0] << 8 | buf[1];
				buf += 2; len -= 2;
				break;
			}

			case 'w':	/* uint16_t LE */
			{
				uint16_t *dest = va_arg(ap, uint16_t *);

				if (len < 2)
					goto err;

				*dest = buf[1] << 8 | buf[0];
				buf += 2; len -= 2;
				break;
			}

			case 'I':	/* uint32_t BE */
			{
				uint32_t *dest = va_arg(ap, uint32_t *);

				if (len < 4)
					goto err;

				*dest = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
				buf += 4; len -= 4;
				break;
			}

			case 'i':	/* uint32_t LE */
			{
				uint32_t *dest = va_arg(ap, uint32_t *);

				if (len < 4)
					goto err;

				*dest = buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0];
				buf += 4; len -= 4;
				break;
			}

			case 'u':
			{
				uint8_t ulen;
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
				uint16_t Ulen;
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

icq_tlv_t *icq_tlv_get(struct icq_tlv_list *l, uint16_t type) {
	for (; l; l = l->next) {
		if (l->type == type) 
			return l;
	}
	return NULL;
}

struct icq_tlv_list *icq_unpack_tlvs(unsigned char *str, int maxlen, unsigned int maxcount) {
	struct icq_tlv_list *ret = NULL;
	int count = 0;

	while (maxlen >= 4) {
		uint16_t type, len;
		icq_tlv_t *ptlv;

		if (!icq_unpack(str, &str, &maxlen, "WW", &type, &len))
			return ret;
		
		debug("str_readtlvs(%d) NEXTTLV type: 0x%x len: %d (maxlen: %d maxcount: %d)\n", count, type, len, maxlen, maxcount ? maxcount-count : 0);

		if (maxlen < len) {
			debug("str_readtlvs() 1897 Incomplete TLV %d, len %ld of %ld - ignoring.\n", type, len, maxlen);
			return ret;
		}

		ptlv = xmalloc(sizeof(icq_tlv_t));

		ptlv->type = type;
		ptlv->len = len;

		ptlv->buf = str;
		ptlv->nr = icq_string_to_BE(ptlv->buf, ptlv->len);

		maxlen -= len;
		str += (len);			/* go to next TLV */

		icq_tlvs_add(&ret, ptlv);
		count++;

		if (maxcount && maxcount == count) 
			break;
	}
	return ret;
}

#include "miscicq.h"

uint16_t icq_status(int status) {
	switch (status) {
		case EKG_STATUS_NA:
			debug_error("icq_status(EKG_STATUS_NA)\n");
			return 0;

		case EKG_STATUS_AVAIL:		return ICQ_STATUS_ONLINE;
		case EKG_STATUS_AWAY:		return ICQ_STATUS_AWAY;
		case EKG_STATUS_DND:		return ICQ_STATUS_DND;
		case EKG_STATUS_FFC:		return ICQ_STATUS_FFC;
		case EKG_STATUS_INVISIBLE:	return ICQ_STATUS_INVISIBLE;
		/* XXX, ICQ_STATUS_OCCUPIED */

		default:			return STATUS_ICQONLINE;
	}
}

/* hash password, ripped from micq */
char *icq_encryptpw(const char *pw) {
	uint8_t tb[] = { 0xf3, 0x26, 0x81, 0xc4, 0x39, 0x86, 0xdb, 0x92, 0x71, 0xa3, 0xb9, 0xe6, 0x53, 0x7a, 0x95, 0x7c };

	char *cpw = xstrdup(pw), *p;
	int i = 0;

	for (p = cpw; *p; p++, i++)
		*p ^= tb[i % 16];
	return cpw;
}

