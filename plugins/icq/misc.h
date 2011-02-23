#ifndef __ICQ_MISC_H
#define __ICQ_MISC_H

typedef struct icq_tlv_list {
	struct icq_tlv_list *next;

	guint16 type;
	guint16 len;

	guint32 nr;
	unsigned char *buf;
} icq_tlv_t;

struct fieldnames_t {
	int code;
	char *text;
};

extern struct fieldnames_t snac_families[];

/* pack, unpack */
int icq_unpack(unsigned char *buf, unsigned char **endbuf, int *l, char *format, ...);
int icq_unpack_nc(unsigned char *buf, int len, char *format, ...);
#define icq_unpack_tlv_word(tlv, val) \
	do {										\
		val = 0;								\
		icq_unpack_nc(tlv ? tlv->buf : NULL, tlv ? tlv->len : 0, "W", &val);	\
	} while(0);


GString *icq_pack(char *format, ...);
GString *icq_pack_append(GString *str, char *format, ...);

#define icq_pack_tlv(type, data, datalen)	(guint32) type, (guint32) datalen, (guint8 *) data
#define icq_pack_tlv_char(type, data)		(guint32) type, (guint32) 1, (guint32) data
#define icq_pack_tlv_word(type, data)		(guint32) type, (guint32) 2, (guint32) data
#define icq_pack_tlv_dword(type, data)		(guint32) type, (guint32) 4, (guint32) data
#define icq_pack_tlv_str(type, str)		icq_pack_tlv(type, str, xstrlen(str))

struct icq_tlv_list *icq_unpack_tlvs(unsigned char **str, int *maxlen, unsigned int maxcount);
struct icq_tlv_list *icq_unpack_tlvs_nc(unsigned char *str, int maxlen, unsigned int maxcount);
icq_tlv_t *icq_tlv_get(struct icq_tlv_list *l, guint16 type);
void icq_tlvs_destroy(struct icq_tlv_list **list);

void icq_hexdump(int level, unsigned char *p, size_t len);
char *icq_encryptpw(const char *pw);
guint16 icq_status(int status);

#define ICQ_UNPACK(endbuf, args...) (icq_unpack(buf, endbuf, &len, args))

status_t icq2ekg_status(int icq_status);
status_t icq2ekg_status2(int nMsgType);

/* misc */
int tlv_length_check(char *name, icq_tlv_t *t, int length);

#define ICQ_SNAC_NAMES_DEBUG 1

#if ICQ_SNAC_NAMES_DEBUG
const char *icq_snac_name(int family, int cmd);
#endif

const char *icq_lookuptable(struct fieldnames_t *table, int code);

void icq_pack_append_client_identification(GString *pkt);

void icq_convert_string_init();
void icq_convert_string_destroy();

char *icq_convert_from_ucs2be(char *buf, int len);
GString *icq_convert_to_ucs2be(char *text);
char *icq_convert_from_utf8(char *text);

void icq_send_snac(session_t *s, guint16 family, guint16 cmd, private_data_t *data, snac_subhandler_t subhandler, char *format, ...);

void icq_rates_destroy(session_t *s);
void icq_rates_init(session_t *s, int n_rates);

#endif
