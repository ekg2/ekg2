#ifndef __ICQ_MISC_H
#define __ICQ_MISC_H

#include <stdint.h>

#include <ekg/dynstuff.h>

typedef struct icq_tlv_list {
	struct icq_tlv_list *next;

	uint16_t type;
	uint16_t len;
	
	uint32_t nr;
	unsigned char *buf;
} icq_tlv_t;

/* pack, unpack */
int icq_unpack(unsigned char *buf, unsigned char **endbuf, int *l, char *format, ...);
int icq_unpack_nc(unsigned char *buf, int len, char *format, ...);
#define icq_unpack_tlv_word(tlv, val) \
	do {										\
		val = 0;								\
		icq_unpack_nc(tlv ? tlv->buf : NULL, tlv ? tlv->len : 0, "W", &val);	\
	} while(0);


string_t icq_pack(char *format, ...);
string_t icq_pack_append(string_t str, char *format, ...);

#define icq_pack_tlv(type, data, datalen)	(uint32_t) type, (uint32_t) datalen, (uint8_t *) data
#define icq_pack_tlv_char(type, data)		(uint32_t) type, (uint32_t) 1, (uint32_t) data
#define icq_pack_tlv_word(type, data)		(uint32_t) type, (uint32_t) 2, (uint32_t) data
#define icq_pack_tlv_dword(type, data)		(uint32_t) type, (uint32_t) 4, (uint32_t) data
#define icq_pack_tlv_str(type, str)		icq_pack_tlv(type, str, xstrlen(str))

struct icq_tlv_list *icq_unpack_tlvs(unsigned char *str, int maxlen, unsigned int maxcount);
icq_tlv_t *icq_tlv_get(struct icq_tlv_list *l, uint16_t type);
void icq_tlvs_destroy(struct icq_tlv_list **list);

void icq_hexdump(int level, unsigned char *p, size_t len);
char *icq_encryptpw(const char *pw);
uint16_t icq_status(int status);

#define ICQ_UNPACK(endbuf, args...) (icq_unpack(buf, endbuf, &len, args))

status_t icq2ekg_status(int icq_status);

/* misc */
int tlv_length_check(char *name, icq_tlv_t *t, int length);

#endif
