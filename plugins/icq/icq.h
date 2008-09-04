#ifndef __ICQ_ICQ_H
#define __ICQ_ICQ_H

#include <ekg/dynstuff.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>

#define SNAC_HANDLER(x) static int x(session_t *s, uint16_t cmd, unsigned char *buf, int len, private_data_t *data)
typedef int (*snac_handler_t) (session_t *, uint16_t cmd, unsigned char *, int, private_data_t * );

#define SNAC_SUBHANDLER(x) int x(session_t *s, unsigned char *buf, int len, private_data_t *data)
typedef int (*snac_subhandler_t) (session_t *s, unsigned char *, int, private_data_t * );

typedef struct {
	int win_size;		// Window size
	int clear_lvl;		// Clear level
	int alert_lvl;		// Alert level
	int limit_lvl;		// Limit level
	int discn_lvl;		// Disconnect level
	int curr_lvl;		// Current level
	int max_lvl;		// Max level
	int n_groups;
	uint32_t *groups;
} icq_rate_t;

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
	int default_group_id;	/* XXX ?wo? TEMP! We should support list of groups */
	char *default_group_name;
	string_t cookie;	/* connection login cookie */
	string_t stream_buf;
	icq_snac_reference_list_t *snac_ref_list;
	int n_rates;
	icq_rate_t **rates;
} icq_private_t;

typedef enum {
	CAP_HTML = 0,
	CAP_VOICE,		/* Client supports voice chat */
	CAP_AIMDIRPLAY,		/* Client supports direct play service */
	CAP_SENDFILE,		/* Client supports file transfer (can send files) */
	CAP_ICQDIRECT,		/* Something called "route finder" (ICQ2K only) */
	CAP_IMIMAGE,		/* Client supports DirectIM/IMImage */
	CAP_BUDDYICON,		/* Client supports avatar service. */
	CAP_SAVESTOCKS,		/* Client supports stocks (add-ins) */
	CAP_GETFILE,		/* Client supports filetransfers (can receive files) */
	CAP_SRV_RELAY,		/* Client supports channel 2 extended, TLV(0x2711) based messages */
	CAP_GAMES2,		/* Client supports games */
	CAP_GAMES,		/* Client supports games */
	CAP_CONTACTS,		/* Client supports buddy lists transfer */
	CAP_INTEROPERATE,	/* Setting this lets AIM users receive messages from ICQ users, and ICQ users receive messages from AIM users */
	CAP_UTF,		/* Client supports UTF-8 messages */
	CAP_XTRAZ,
	CAP_TYPING,		/* Client supports mini typing notifications */
	CAP_CHAT,		/* Client supports chat service */
	CAP_RTF,		/* Client supports RTF messages */
	CAP_UNKNOWN
} capabilities_t;

int icq_send_pkt(session_t *s, string_t buf);

void icq_session_connected(session_t *s);
int icq_write_status(session_t *s);
void icq_handle_disconnect(session_t *s, const char *reason, int type);

#define icq_uid(target) protocol_uid("icq", target)

#define MIRANDAOK 1
#define MIRANDA_COMPILANT_CLIENT 1

#define ICQ_DEBUG_UNUSED_INFORMATIONS 1

#endif
