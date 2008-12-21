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
/* orginal version of SHA-1 in C by Steve Reid <steve@edmweb.com> */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

static const char digit[] = "\0abcdefghijklmnoprstuwxyz";	/* bo kto tak naprawde korzysta z trudnych hasel? */
// static const char digit[] = "\0abcdefghijklmnoprstuwxyzABCDEFGHIJKLMOPRSTUWXYZ1234567890";

#define MAX_PASS_LEN 15	/* dlugosc hasla, tak naprawde to jest+1, nie przejmowac sie. */

#define ULTRA_DEBUG	0	/* sprawdza czy dobrze generujemy hasla (w/g digit, b. niepotrzebne i b. wolne) */
#define ULTRA_VERBOSE	1	/* rysuje kropki */
#define ULTRA_SAFE	0	/* sprawdza czy nie bedziemy rysowac po pamieci jesli haslo zacznie miec wiecej niz MAX_PASS_LEN znakow */

#define NOT_STOP_ON_FIRST 0

/* sample, smb has this seed/hash. 
 *	change it to your data.
 *
 *	you can benchmark && check if you gg-keygen.c generate good data with it.
 *	tested under athlon-xp 2500 XP+
 */

#if 0	/* kasza */	/*   0.115s */
#define SEED 0xe2cf3809
#define HASH 0x4d940c10
#endif

#if 0	/* agakrht */	/* 7.834s */
#define SEED 0xd3c742b6
#define HASH 0x9f9b9205
#endif

/* with apended 'q2' to digit: [static const char digit[] = "\0abcdefghijklmnoprstuwxyzq2"] */
#if 0	/* qwerty2 */	/* */
#define SEED 0xb2b9eec8
#define HASH_SHA1 "a266db74a7289913ec30a7872b7384ecc119e4ec"
#endif

#if 0		/* 5.154s */
#define TEXT "alamaka"		/* algo test */
#endif

#define DIGIT_SIZE (sizeof(digit)-2)	/* glupie gcc, i konczenie stringow \0 :( */ 

static unsigned char pass[MAX_PASS_LEN];
static unsigned char realpass[MAX_PASS_LEN+1];
static size_t pass_pos = 0;

#ifdef HASH_SHA1	/* SHA-1 STUFF */

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xFF00FF00) \
    |(rol(block->l[i],8)&0x00FF00FF))
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] \
    ^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);

/* S* is like R* but it don't get block values (we assume there're 00's) */
#define S0(v,w,x,y,z,i) z+=((w&(x^y))^y)+0x5A827999+rol(v,5);w=rol(w,30);
#define S1(v,w,x,y,z,i) z+=((w&(x^y))^y)+0x5A827999+rol(v,5);w=rol(w,30);
#define S2(v,w,x,y,z,i) z+=(w^x^y)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define S3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define S4(v,w,x,y,z,i) z+=(w^x^y)+0xCA62C1D6+rol(v,5);w=rol(w,30);

/* XXX, ?SHA-1 Broken?, XXX */
static inline int gg_login_sha1hash(const unsigned char *password, const size_t passlen, const uint32_t seed, const uint32_t *dig) {
#define SHA_STATE0 0x67452301
#define SHA_STATE1 0xEFCDAB89
#define SHA_STATE2 0x98BADCFE
#define SHA_STATE3 0x10325476
#define SHA_STATE4 0xC3D2E1F0
	int i;

	unsigned char buffer[64];

/* SHA1Init() */
    /* SHA1 initialization constants */
	uint32_t a = SHA_STATE0;
	uint32_t b = SHA_STATE1;
	uint32_t c = SHA_STATE2;
	uint32_t d = SHA_STATE3;
	uint32_t e = SHA_STATE4;

	/* XXX, it's optimized but it'll work only for short passwords, shorter than 63-4-7 */
	{
		for (i = 0; i < passlen; i++) 
			buffer[i] = digit[password[i]];

		memcpy(&buffer[passlen], &seed, 4);
	}

/* SHA1Final() */
	/* Add padding and return the message digest. */
	{
	/* pad */
		buffer[passlen+4] = '\200';
		for (i = passlen+5; i < 63-7; i++)
			buffer[i] = '\0';
			
	/* finalcount */
		for (i = 63-7; i < 63; i++)
			buffer[i] = '\0';

		buffer[63] = (unsigned char) (((passlen+4) << 3) & 0xff);
	}
/* SHA1Transform() */
	/* Hash a single 512-bit block. This is the core of the algorithm. */
	{
		typedef union {
			unsigned char c[64];
			uint32_t l[16];
		} CHAR64LONG16;

		CHAR64LONG16* block = (CHAR64LONG16*) buffer;
		/* We assume here you don't need more than 2 blocks for password (2*4=8 chars) + 1 block for seed */
		/* if you need more replace S0()'s with R0()'s */

		/* 4 rounds of 20 operations each. Loop unrolled. */
		R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); S0(c,d,e,a,b, 3);
		S0(b,c,d,e,a, 4); S0(a,b,c,d,e, 5); S0(e,a,b,c,d, 6); S0(d,e,a,b,c, 7);
		S0(c,d,e,a,b, 8); S0(b,c,d,e,a, 9); S0(a,b,c,d,e,10); S0(e,a,b,c,d,11);
		S0(d,e,a,b,c,12); S0(c,d,e,a,b,13); S0(b,c,d,e,a,14); R0(a,b,c,d,e,15);

		R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);

		R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
		R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
		R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
		R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
		R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);

		R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
		R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
		R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
		R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
		R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);

		R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
		R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
		R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
		R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
		R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);
	}

#if ULTRA_DEBUG
	printf("%s -> %.8x%.8x%.8x%.8x%.8x\n", realpass, a, b, c, d, e);
#endif

/* it returns 0 if digest match, 1 if not */
	if (dig[0] != a) return 1;
	if (dig[1] != b) return 1;
	if (dig[2] != c) return 1;
	if (dig[3] != d) return 1;
	if (dig[4] != e) return 1;

	return 0;
}

#endif

#ifdef HASH

/* stolen from libgadu, cache created by me. */

static unsigned int gg_login_hash() {
	static unsigned int saved_y;
	static unsigned char *saved_pos;
	static unsigned char saved_zn;

	unsigned int y;
	unsigned char *password;

	if (saved_pos && *saved_pos == saved_zn) {
		y = saved_y;
		password = saved_pos + 1;
	} else {
		y = SEED;
		password = realpass;
	}

/* I have no idea, how to crack/optimize this algo. Maybe someone? */
	do {
		register unsigned char zn = *password;
		register unsigned char z;

		y ^= zn;
		y += zn;

		y ^= (zn << 8);
		y -= (zn << 16);
		y ^= (zn << 24);

		z = y & 0x1F;
		y = (y << z) | (y >> (32 - z));

		if (*(++password)) {
			y ^= (zn << 24);
			y += (zn << 24);

			if (password[1] == '\0') {
				saved_y = y;
				saved_zn = zn;
				saved_pos = &password[-1];
			}
		}
	} while (*password);

#if ULTRA_DEBUG
	printf("%s -> 0x%x\n", realpass, y);
#endif
	return y;
}

#endif

#ifdef TEXT

static inline int check_text() {
	unsigned char *text = TEXT;
	unsigned char *password;

#if ULTRA_DEBUG
	printf("%s\n", realpass);
#endif
	for (password = realpass; *password && *text; password++, text++) {
		if (*text != *password)
			return 1;
	}

	return !(*password == '\0' && *text == '\0');
}

#endif

static inline void bonce(size_t i) {
	for (; i < pass_pos + 1; i++) {
		pass[i] = 1;	realpass[i] = digit[1];
	}
}

static inline void incr() {
	int i;

	for (i = pass_pos; i >= 0; i--) {
		if (pass[i] < DIGIT_SIZE) {
#if ULTRA_VERBOSE
			/* jesli ktos bardzo lubi kropki, lepiej jest nie lubiec. */
			if (i == 0) {
				putchar('.');
				fflush(stdout);
			}
#endif
			pass[i]++;	realpass[i] = digit[pass[i]];

			bonce(i+1);
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

	bonce(0);
}

int main() {
#ifdef HASH_SHA1
	unsigned char digest[20];
	uint32_t digstate[5];
	int i;

/* HASH w SHA1 najpierw z 40 znakowego, ascii-printable znakow od [0-f] zamieniamy na binarna 20 znakow tablice.. */

	for (i = 0; i < 40; i++) {
		uint8_t znak;

		if (HASH_SHA1[i] == '\0') { fprintf(stderr, "BAD SHA1 hash: %s\n", HASH_SHA1);	return 1; }

		if (tolower(HASH_SHA1[i]) >= 'a' && tolower(HASH_SHA1[i]) <= 'f')
			znak = 10 + tolower(HASH_SHA1[i]-'a');
		else if (HASH_SHA1[i] >= '0' && HASH_SHA1[i] <= '9')
			znak = HASH_SHA1[i]-'0';
		else { fprintf(stderr, "BAD SHA1 char!\n"); return 1; }

		if (i % 2)
			digest[i / 2] |= znak;
		else	digest[i / 2] = znak << 4;

	}
	if (HASH_SHA1[40] != '\0') { fprintf(stderr, "BAD SHA1 hash: %s\n", HASH_SHA1);  return 1; }

	printf("%s == ", HASH_SHA1);
	for (i = 0; i < 20; i++) 
		printf("%.2x", digest[i]);

	printf("\n");

/* a teraz zamienmy na tablice 32bitowych liczb.. */
	digstate[0] = digest[ 0] << 24 | digest[ 1] << 16 | digest[ 2] << 8 | digest[ 3];
	digstate[1] = digest[ 4] << 24 | digest[ 5] << 16 | digest[ 6] << 8 | digest[ 7];
	digstate[2] = digest[ 8] << 24 | digest[ 9] << 16 | digest[10] << 8 | digest[11];
	digstate[3] = digest[12] << 24 | digest[13] << 16 | digest[14] << 8 | digest[15];
	digstate[4] = digest[16] << 24 | digest[17] << 16 | digest[18] << 8 | digest[19];

/* substract SHA1 initial vectors, to speedup computing @ loop... 4 add instruction less per loop */
	digstate[0] -= SHA_STATE0;
	digstate[1] -= SHA_STATE1;
	digstate[2] -= SHA_STATE2;
	digstate[3] -= SHA_STATE3;
	digstate[4] -= SHA_STATE4;
#endif
	
	do {
		do {
			incr();
		} while 
#if defined(HASH_SHA1)		/* (HAS_SHA1) */
#warning "You'd chosen SHA-1 cracking"
			(gg_login_sha1hash(pass, pass_pos+1, SEED, digstate));
#elif defined(HASH)		/* !(HAS_SHA1) && HASH */
#warning "You'd chosen old HASH cracking"
			(gg_login_hash() != HASH);
#elif defined(TEXT)		/* !(HAS_SHA1) && !(HASH) && TEXT */
#warning "You didn't defined HASH_SHA1 nor HASH, generator test?"
			(check_text());
#else				/* !(HASH_SHA1) && !(HASH) && !(TEXT) */
#warning "You didn't defined HASH_SHA1 nor HASH nor TEXT, generator test?"
			(1);
#endif
		printf("Collision found: %s\n", realpass);
	} while(NOT_STOP_ON_FIRST);
	return 0;
}

