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

#define EKG_ACK_DELIVERED "delivered"	/* wiadomo¶æ dostarczono */
#define EKG_ACK_QUEUED "queued"		/* wiadomo¶æ zakolejkowano */
#define EKG_ACK_DROPPED "dropped"	/* wiadomo¶æ odrzucono */
#define EKG_ACK_UNKNOWN "unknown"	/* nie wiadomo, co siê z ni± sta³o */

#define EKG_DISCONNECT_USER 0		/* u¿ytkownik wpisa³ /disconnect */
#define EKG_DISCONNECT_NETWORK 1	/* problemy z sieci± */
#define EKG_DISCONNECT_FORCED 2		/* serwer kaza³ siê roz³±czyæ */
#define EKG_DISCONNECT_FAILURE 3	/* b³±d ³±czenia siê z serwerem */

enum msgclass_t {
	EKG_MSGCLASS_MESSAGE = 0,	/* pojedyncza wiadomo¶æ */
	EKG_MSGCLASS_CHAT,		/* wiadomo¶æ w ramach rozmowy */
	EKG_MSGCLASS_SENT,		/* wiadomo¶æ, któr± sami wysy³amy */
	EKG_MSGCLASS_SYSTEM		/* wiadomo¶æ systemowa */
};

void protocol_init();

int protocol_connected(void *data, va_list ap);
int protocol_failed(void *data, va_list ap);
int protocol_disconnected(void *data, va_list ap);
int protocol_status(void *data, va_list ap);
int protocol_status(void *data, va_list ap);
int protocol_message(void *data, va_list ap);
int protocol_message_ack(void *data, va_list ap);

void message_print(const char *session, const char *sender, const char **rcpts, const char *text, const uint32_t *format, time_t sent, int class, const char *seq);

typedef enum {
	DCC_NONE = 0,
	DCC_SEND,
	DCC_GET,
	DCC_VOICE
} dcc_type_t;

struct dcc_s;

typedef void (*dcc_close_handler_t)(struct dcc_s *);

typedef struct dcc_s {
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

dcc_t *dcc_add(const char *uid, dcc_type_t type, void *priv);
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

list_t dccs;

#endif /* __EKG_PROTOCOL_H */
