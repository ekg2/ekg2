/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __SIMLITE_H
#define __SIMLITE_H

#ifndef __AC_STDINT_H
#include <stdint.h>
#endif 

extern char *sim_key_path;
extern int sim_errno;

typedef enum {
	SIM_ERROR_SUCCESS,	/* uda³o siê */
	SIM_ERROR_PUBLIC,	/* b³±d klucza publicznego */
	SIM_ERROR_PRIVATE,	/* b³±d klucza prywatnego */
	SIM_ERROR_RSA,		/* nie uda³o siê odszyfrowaæ RSA */
	SIM_ERROR_BF,		/* nie uda³o siê odszyfrowaæ BF */
	SIM_ERROR_RAND,		/* entropia posz³a na piwo */
	SIM_ERROR_MEMORY,	/* brak pamiêci */
	SIM_ERROR_INVALID,	/* niew³a¶ciwa wiadomo¶æ (za krótka) */
	SIM_ERROR_MAGIC		/* niew³a¶ciwy magic */
} sim_errno_t;

#define SIM_MAGIC_V1 0x2391
#define SIM_MAGIC_V1_BE 0x9123

typedef struct {
	unsigned char init[8];
	uint16_t magic;
	uint8_t flags;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
sim_message_header;

char *sim_message_decrypt(const unsigned char *message, uint32_t uin);
char *sim_message_encrypt(const unsigned char *message, uint32_t uin);
int sim_key_generate(uint32_t uin);
char *sim_key_fingerprint(uint32_t uin);

const char *sim_strerror(int error);

#endif /* __SIMLITE_H */
