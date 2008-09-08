#ifndef __ICQ_CAPS_H
#define __ICQ_CAPS_H

#include <ekg/dynstuff.h>

typedef enum {
	CAP_HTML = 0,
	CAP_NEWCAPS,		/* Client understands new format of caps */
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
	CAP_DEVILS,		/* Client supports devils */
	CAP_INTEROPERATE,	/* Setting this lets AIM users receive messages from ICQ users, and ICQ users receive messages from AIM users */
	CAP_UTF,		/* Client supports UTF-8 messages */
	CAP_XTRAZ,
	CAP_TYPING,		/* Client supports mini typing notifications */
	CAP_CHAT,		/* Client supports chat service */
	CAP_RTF,		/* Client supports RTF messages */
	CAP_UNKNOWN
} capabilities_t;

const char *icq_capability_name(int id);

int icq_cap_id(unsigned char *buf);
int icq_short_cap_id(unsigned char *buf);

const unsigned char *icq_cap_str(int id);

void icq_pack_append_cap(string_t pkt, int cap_id);


/*
 * xStatuses
 *
 */

#define XSTATUS_COUNT 32

const char *icq_xstatus_name(int id);

int icq_xstatus_id(unsigned char *buf);

void icq_pack_append_xstatus(string_t pkt, int x_id);

#endif
