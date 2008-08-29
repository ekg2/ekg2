#ifndef __ICQ_ICQ_H
#define __ICQ_ICQ_H

#include <ekg/dynstuff.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>

#define SNAC_HANDLER(x) static int x(session_t *s, uint16_t cmd, unsigned char *buf, int len, private_data_t *data)
typedef int (*snac_handler_t) (session_t *, uint16_t cmd, unsigned char *, int, private_data_t * );

#define SNAC_SUBHANDLER(x) int x(session_t *s, unsigned char *buf, int len, private_data_t *data)
typedef int (*snac_subhandler_t) (session_t *s, unsigned char *, int, private_data_t * );


typedef struct icq_snac_reference_list_s {
	struct icq_snac_reference_list_s *next;
	int ref;
	time_t timestamp;
	snac_subhandler_t subhandler;
	private_data_t *list;
} icq_snac_reference_list_t;

typedef struct {
	int fd;
	int fd2;

	int flap_seq;		/* FLAP seq id */
	uint16_t snac_seq;	/* SNAC seq id */
	int snacmeta_seq;	/* META SNAC seq id */

	int ssi;		/* server-side-userlist? */
	int aim;		/* aim-ok? */
	int user_default_group; /* XXX ?wo? TEMP! We should support list of groups */
	string_t cookie;	/* connection login cookie */
	string_t stream_buf;
	icq_snac_reference_list_t *snac_ref_list;
} icq_private_t;

int icq_send_pkt(session_t *s, string_t buf);

void icq_session_connected(session_t *s);
int icq_write_status(session_t *s);
void icq_handle_disconnect(session_t *s, const char *reason, int type);

#define icq_uid(target) protocol_uid("icq", target)

#define MIRANDAOK 1
#define MIRANDA_COMPILANT_CLIENT 1

#define ICQ_DEBUG_UNUSED_INFORMATIONS 1

#endif
