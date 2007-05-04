/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
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

#ifndef __EKG_PROTOCOL_H
#define __EKG_PROTOCOL_H

#include "ekg2-config.h"

#include <ekg/dynstuff.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>

#define EKG_FORMAT_RGB_MASK 0x00ffffffL	/* 0x00BBGGRR */
#define EKG_FORMAT_R_MASK 0x00ff0000L
#define EKG_FORMAT_G_MASK 0x0000ff00L
#define EKG_FORMAT_B_MASK 0x000000ffL
#define EKG_FORMAT_COLOR 0x01000000L
#define EKG_FORMAT_BOLD 0x02000000L
#define EKG_FORMAT_ITALIC 0x04000000L
#define EKG_FORMAT_UNDERLINE 0x08000000L
#define EKG_FORMAT_REVERSE 0x10000000L

#define EKG_NO_THEMEBIT	256

enum msgack_t {
	EKG_ACK_DELIVERED = 0,		/* message delivered successfully */
	EKG_ACK_QUEUED,			/* message queued for delivery */
	EKG_ACK_DROPPED,		/* message rejected 'permamently' */
	EKG_ACK_TEMPFAIL,		/* temporary delivery failure */
	EKG_ACK_UNKNOWN,		/* delivery status unknown */
	
	EKG_ACK_MAX			/* we don't want to read after array */
};

enum disconnect_t {
	EKG_DISCONNECT_USER = 0,	/* user-engaged disconnect */
	EKG_DISCONNECT_NETWORK,		/* network problems */
	EKG_DISCONNECT_FORCED,		/* server forced to disconnect */
	EKG_DISCONNECT_FAILURE,		/* connecting failed */
	EKG_DISCONNECT_STOPPED		/* connecting canceled */
};

#define EKG_NO_BEEP 0
#define EKG_TRY_BEEP 1

enum msgclass_t {
	/* recv */
	EKG_MSGCLASS_MESSAGE = 0,	/* pojedyncza wiadomo¶æ */
	EKG_MSGCLASS_CHAT,		/* wiadomo¶æ w ramach rozmowy */
	EKG_MSGCLASS_SYSTEM,		/* wiadomo¶æ systemowa */
	/* sent */
	EKG_MSGCLASS_SENT = 32,		/* wiadomo¶æ, któr± sami wysy³amy */
	EKG_MSGCLASS_SENT_CHAT,		/* wiadomo¶c, któr± sam wysylamy w ramach rozmowy */
};

#ifndef EKG2_WIN32_NOFUNCTION
void protocol_init();

char *message_print(const char *session, const char *sender, const char **rcpts, const char *text, const uint32_t *format, time_t sent, int class, const char *seq, int dobeep, int secure);
#endif

typedef enum {
	DCC_NONE = 0,
	DCC_SEND,
	DCC_GET,
	DCC_VOICE
} dcc_type_t;

struct dcc_s;

typedef void (*dcc_close_handler_t)(struct dcc_s *);

typedef struct dcc_s {
	session_t *session;		/* ktora sesja? */
	char *uid;			/* z kim po³±czenie */
	dcc_type_t type;		/* rodzaj po³±czenia */
	int id;				/* numer po³±czenia */
	void *priv;			/* dane prywatne pluginu */
	dcc_close_handler_t close_handler;	/* obs³uga /dcc close */
	int active;			/* czy po³±czono? */
	time_t started;			/* kiedy utworzono? */
	
	char *filename;			/* nazwa pliku */
	int size;			/* rozmiar pliku */
	int offset;			/* ile ju¿ wykonano */
} dcc_t;

#ifndef EKG2_WIN32_NOFUNCTION
dcc_t *dcc_add(session_t *session, const char *uid, dcc_type_t type, void *priv);
int dcc_close(dcc_t *d);

int dcc_private_set(dcc_t *, void *);
void *dcc_private_get(dcc_t *);
int dcc_close_handler_set(dcc_t *, dcc_close_handler_t);
dcc_close_handler_t dcc_close_handler_get(dcc_t *);
const char *dcc_uid_get(dcc_t *);
int dcc_id_get(dcc_t *);
time_t dcc_started_get(dcc_t *);
int dcc_active_set(dcc_t *, int);
int dcc_active_get(dcc_t *);
int dcc_offset_set(dcc_t *, int);
int dcc_offset_get(dcc_t *);
int dcc_size_set(dcc_t *, int);
int dcc_size_get(dcc_t *);
int dcc_filename_set(dcc_t *, const char *);
const char *dcc_filename_get(dcc_t *);
dcc_type_t dcc_type_get(dcc_t *);

extern list_t dccs;

#endif

#endif /* __EKG_PROTOCOL_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
