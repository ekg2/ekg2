/*
 *  (C) Copyright 2007 Jakub Zawadzki <darkjames@darkjames.ath.cx>
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

/* gg_login_hash() copied from libgadu copyrighted under LGPL-2.1 (C) libgadu developers */

#include <stdio.h>

#define DIGIT_SIZE (sizeof(digit)-2)	/* glupie gcc, i konczenie stringow \0 :( */ 
// static const char digit[] = "\0abcdefghijklmnoprstuwxyzABCDEFGHIJKLMOPRSTUWXYZ1234567890";
static const char digit[] = "\0abcdefghijklmnoprstuwxyz";	/* bo kto tak naprawde korzysta z trudnych hasel? */

#define MAX_PASS_LEN 100	/* dlugosc hasla, tak naprawde to jest+1, nie przejmowac sie. */

#define ULTRA_DEBUG 	0	/* sprawdza czy dobrze generujemy hasla (w/g digit, b. niepotrzebne i b. wolne) */
#define ULTRA_VERBOSE	0	/* rysuje kropki */
#define ULTRA_SAFE	0	/* sprawdza czy nie bedziemy rysowac po pamieci jesli haslo zacznie miec wiecej niz MAX_PASS_LEN znakow */
#define ULTRA_CACHE 	0	/* XXX keszuje wyniki, jak komus sie chce napisac. */

#if ULTRA_CACHE
typedef struct {
	unsigned int x;
	unsigned int y;
} last_t;

static last_t lasts[MAX_PASS_LEN];
static last_t *last_last;

#endif

static unsigned char pass[MAX_PASS_LEN];
static size_t pass_pos = 0;

/* stolen from libgadu */
static inline unsigned int gg_login_hash(const unsigned char *password, unsigned int y /* seed */
#if ULTRA_CACHE
, unsigned int x
#endif
) {
#if !ULTRA_CACHE
	unsigned int x = 0;
#endif
	unsigned int z;

/* I have no idea, how to crack/optimize this algo. Maybe someone? */
	for (; *password; password++) {
		x = (x/* & 0xffffff00 */) | digit[*password]; /* LE, po co & ? */
		y ^= x;
		y += x;
		x <<= 8;
		y ^= x;
		x <<= 8;
		y -= x;
		x <<= 8;
		y ^= x;

		z = y & 0x1F;
		y = (y << z) | (y >> (32 - z));
	}

#if ULTRA_DEBUG
	for (password = pass; *password; password++) {
		printf("%c", digit[*password]);
	}
	printf(" -> 0x%x\n", y);
#endif
	return y;
}

static void bonce(unsigned char *buf, size_t len) {
	size_t i;
	for (i = 0; i < len; i++) 
		buf[i] = 1;
}

static char *print_pass(unsigned char *pass) {
	printf("Collision found: ");
	while (*pass) {
		putchar(digit[*pass]);

		pass++;
	}
	printf("\n");
}

/* sample, smb has this seed/hash. 
 *	change it to your data.
 *
 *	you can benchmark && check if you gg-keygen.c generate good data with it.
 *	tested under athlon-xp 2500 XP+
 */

#if 0	/* kasza */	/*   0.193s */
#define SEED 0xe2cf3809
#define HASH 0x4d940c10
#endif

#if 0	/* agakrht */	/* 14.738s */
#define SEED 0xd3c742b6
#define HASH 0x9f9b9205
#endif

#define NOT_STOP_ON_FIRST 0

static inline void incr() {
	int i;

	for (i = pass_pos; i >= 0; i--) {
		if (pass[i] < DIGIT_SIZE) {
#if ULTRA_VERBOSE
			/* jesli ktos lubi kropki, lepiej jest nie lubiec. */
			if (i == 0) {
				putchar('.');
				fflush(stdout);
			}
#endif
			pass[i]++;
			bonce(&(pass[i+1]), pass_pos-i);
			return;
		}
	}
#if ULTRA_SAFE 
	/* po co to komu? */
	if (pass_pos == MAX_PASS_LEN) {
		fprintf(stderr, "%s:%d pass_pos == MAX_PASS_LEN, incr MAX_PASS_LEN?\n", __FILE__, __LINE__);
		exit(1);
	}
#endif
	pass_pos++;
	printf("Len: %d\n", pass_pos+1);

	bonce(pass, pass_pos+1);
}

int main() {
	unsigned int hash;
	
	memset(pass, 0, sizeof(pass));
	do {
		do {
			incr();
		} while ((hash = gg_login_hash(pass, SEED
#if ULTRA_CACHE
						,0
#endif
					      )) != HASH);
		print_pass(pass);
	} while(NOT_STOP_ON_FIRST);
	return 0;
}

