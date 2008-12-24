/* $Id$ */

/*
 * This is work is derived from material Copyright RSA Data Security, Inc.
 */

/* MD5C.C - RSA Data Security, Inc., MD5 message-digest algorithm
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 * rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 *
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

/*
SHA-1 in C
By Steve Reid <steve@edmweb.com>
100% Public Domain

Test Vectors (from FIPS PUB 180-1)
"abc"
  A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
  84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
A million repetitions of "a"
  34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
*/
#include "ekg2-config.h"
#include <ekg/win32.h>
#include <ekg/debug.h>
#include <ekg/recode.h>

#include <stdint.h>

#include <ekg/stuff.h>
#include <ekg/xmalloc.h>

#include "jabber.h"

#if !defined(WORDS_BIGENDIAN) && !defined(LITTLE_ENDIAN)
#  define LITTLE_ENDIAN
#endif

#define SHA1HANDSOFF

#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} EKG2_SHA1_CTX, EKG2_MD5_CTX;

static void Init(EKG2_SHA1_CTX* context, int usesha);
static void Transform(uint32_t state[5], unsigned char buffer[64], int usesha);
static void Update(EKG2_SHA1_CTX* context, unsigned char* data, unsigned int len, int usesha);
static void Final(unsigned char digest[20], EKG2_SHA1_CTX* context, int usesha);

#define SHA1Init(ctx)			Init(ctx, 1)
#define SHA1Transform(state, buffer)	Transform(state, buffer, 1)
#define SHA1Update(ctx, data, len)	Update(ctx, (unsigned char *) data, len, 1) 
#define SHA1Final(digest, ctx)		Final(digest, ctx, 1) 

#define MD5Init(ctx)			Init(ctx, 0)
#define MD5Transform(state, buffer)	Transform(state, buffer, 0)
#define MD5Update(ctx, data, len)	Update(ctx, (unsigned char *) data, len, 0)
#define MD5Final(digest, ctx)		Final(digest, ctx, 0)

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
#ifdef LITTLE_ENDIAN
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xFF00FF00) \
    |(rol(block->l[i],8)&0x00FF00FF))
#else
#define blk0(i) block->l[i]
#endif
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] \
    ^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);

#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

#define FF(a, b, c, d, x, s, ac) { (a) += F ((b), (c), (d)) + (x) + (uint32_t)(ac); (a) = rol((a), (s)); (a) += (b); }
#define GG(a, b, c, d, x, s, ac) { (a) += G ((b), (c), (d)) + (x) + (uint32_t)(ac); (a) = rol((a), (s)); (a) += (b); }
#define HH(a, b, c, d, x, s, ac) { (a) += H ((b), (c), (d)) + (x) + (uint32_t)(ac); (a) = rol((a), (s)); (a) += (b); }
#define II(a, b, c, d, x, s, ac) { (a) += I ((b), (c), (d)) + (x) + (uint32_t)(ac); (a) = rol((a), (s)); (a) += (b); }

/* Hash a single 512-bit block. This is the core of the algorithm. */

static void Transform(uint32_t state[5], unsigned char buffer[64], int usesha)
{
uint32_t a, b, c, d, e;
typedef union {
    unsigned char c[64];
    uint32_t l[16];
} CHAR64LONG16;
CHAR64LONG16* block;
#ifdef SHA1HANDSOFF
static unsigned char workspace[64];
    block = (CHAR64LONG16*)workspace;
    memcpy(block, buffer, 64);
#else
    block = (CHAR64LONG16*)buffer;
#endif
    /* Copy context->state[] to working vars */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];

    if (usesha) {
	    /* 4 rounds of 20 operations each. Loop unrolled. */
	    R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
	    R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
	    R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
	    R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
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
    } else {
	    uint32_t *block = (uint32_t *) &workspace[0];
	    /* Round 1 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
	    FF (a, b, c, d, block[ 0], S11, 0xd76aa478); /* 1 */
	    FF (d, a, b, c, block[ 1], S12, 0xe8c7b756); /* 2 */
	    FF (c, d, a, b, block[ 2], S13, 0x242070db); /* 3 */
	    FF (b, c, d, a, block[ 3], S14, 0xc1bdceee); /* 4 */
	    FF (a, b, c, d, block[ 4], S11, 0xf57c0faf); /* 5 */
	    FF (d, a, b, c, block[ 5], S12, 0x4787c62a); /* 6 */
	    FF (c, d, a, b, block[ 6], S13, 0xa8304613); /* 7 */
	    FF (b, c, d, a, block[ 7], S14, 0xfd469501); /* 8 */
	    FF (a, b, c, d, block[ 8], S11, 0x698098d8); /* 9 */
	    FF (d, a, b, c, block[ 9], S12, 0x8b44f7af); /* 10 */
	    FF (c, d, a, b, block[10], S13, 0xffff5bb1); /* 11 */
	    FF (b, c, d, a, block[11], S14, 0x895cd7be); /* 12 */
	    FF (a, b, c, d, block[12], S11, 0x6b901122); /* 13 */
	    FF (d, a, b, c, block[13], S12, 0xfd987193); /* 14 */
	    FF (c, d, a, b, block[14], S13, 0xa679438e); /* 15 */
	    FF (b, c, d, a, block[15], S14, 0x49b40821); /* 16 */

	    /* Round 2 */
#define S21 5
#define S22 9
#define S23 14
#define S24 20
	    GG (a, b, c, d, block[ 1], S21, 0xf61e2562); /* 17 */
	    GG (d, a, b, c, block[ 6], S22, 0xc040b340); /* 18 */
	    GG (c, d, a, b, block[11], S23, 0x265e5a51); /* 19 */
	    GG (b, c, d, a, block[ 0], S24, 0xe9b6c7aa); /* 20 */
	    GG (a, b, c, d, block[ 5], S21, 0xd62f105d); /* 21 */
	    GG (d, a, b, c, block[10], S22,  0x2441453); /* 22 */
	    GG (c, d, a, b, block[15], S23, 0xd8a1e681); /* 23 */
	    GG (b, c, d, a, block[ 4], S24, 0xe7d3fbc8); /* 24 */
	    GG (a, b, c, d, block[ 9], S21, 0x21e1cde6); /* 25 */
	    GG (d, a, b, c, block[14], S22, 0xc33707d6); /* 26 */
	    GG (c, d, a, b, block[ 3], S23, 0xf4d50d87); /* 27 */

	    GG (b, c, d, a, block[ 8], S24, 0x455a14ed); /* 28 */
	    GG (a, b, c, d, block[13], S21, 0xa9e3e905); /* 29 */
	    GG (d, a, b, c, block[ 2], S22, 0xfcefa3f8); /* 30 */
	    GG (c, d, a, b, block[ 7], S23, 0x676f02d9); /* 31 */
	    GG (b, c, d, a, block[12], S24, 0x8d2a4c8a); /* 32 */

	    /* Round 3 */
#define S31 4
#define S32 11
#define S33 16
#define S34 23
	    HH (a, b, c, d, block[ 5], S31, 0xfffa3942); /* 33 */
	    HH (d, a, b, c, block[ 8], S32, 0x8771f681); /* 34 */
	    HH (c, d, a, b, block[11], S33, 0x6d9d6122); /* 35 */
	    HH (b, c, d, a, block[14], S34, 0xfde5380c); /* 36 */
	    HH (a, b, c, d, block[ 1], S31, 0xa4beea44); /* 37 */
	    HH (d, a, b, c, block[ 4], S32, 0x4bdecfa9); /* 38 */
	    HH (c, d, a, b, block[ 7], S33, 0xf6bb4b60); /* 39 */
	    HH (b, c, d, a, block[10], S34, 0xbebfbc70); /* 40 */
	    HH (a, b, c, d, block[13], S31, 0x289b7ec6); /* 41 */
	    HH (d, a, b, c, block[ 0], S32, 0xeaa127fa); /* 42 */
	    HH (c, d, a, b, block[ 3], S33, 0xd4ef3085); /* 43 */
	    HH (b, c, d, a, block[ 6], S34,  0x4881d05); /* 44 */
	    HH (a, b, c, d, block[ 9], S31, 0xd9d4d039); /* 45 */
	    HH (d, a, b, c, block[12], S32, 0xe6db99e5); /* 46 */
	    HH (c, d, a, b, block[15], S33, 0x1fa27cf8); /* 47 */
	    HH (b, c, d, a, block[ 2], S34, 0xc4ac5665); /* 48 */

	    /* Round 4 */
#define S41 6
#define S42 10
#define S43 15
#define S44 21
	    II (a, b, c, d, block[ 0], S41, 0xf4292244); /* 49 */
	    II (d, a, b, c, block[ 7], S42, 0x432aff97); /* 50 */
	    II (c, d, a, b, block[14], S43, 0xab9423a7); /* 51 */
	    II (b, c, d, a, block[ 5], S44, 0xfc93a039); /* 52 */
	    II (a, b, c, d, block[12], S41, 0x655b59c3); /* 53 */
	    II (d, a, b, c, block[ 3], S42, 0x8f0ccc92); /* 54 */
	    II (c, d, a, b, block[10], S43, 0xffeff47d); /* 55 */
	    II (b, c, d, a, block[ 1], S44, 0x85845dd1); /* 56 */
	    II (a, b, c, d, block[ 8], S41, 0x6fa87e4f); /* 57 */
	    II (d, a, b, c, block[15], S42, 0xfe2ce6e0); /* 58 */
	    II (c, d, a, b, block[ 6], S43, 0xa3014314); /* 59 */
	    II (b, c, d, a, block[13], S44, 0x4e0811a1); /* 60 */
	    II (a, b, c, d, block[ 4], S41, 0xf7537e82); /* 61 */
	    II (d, a, b, c, block[11], S42, 0xbd3af235); /* 62 */
	    II (c, d, a, b, block[ 2], S43, 0x2ad7d2bb); /* 63 */
	    II (b, c, d, a, block[ 9], S44, 0xeb86d391); /* 64 */
    }
    /* Add the working vars back into context.state[] */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    /* Wipe variables */
    a = b = c = d = e = 0;
}


/* SHA1Init - Initialize new context */

static void Init(EKG2_SHA1_CTX* context, int usesha)
{
    /* SHA1 initialization constants */
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = usesha ? 0xC3D2E1F0 : 0x00000000;
    context->count[0] = context->count[1] = 0;
}

/* Run your data through this. */

static void Update(EKG2_SHA1_CTX* context, unsigned char* data, unsigned int len, int usesha)
{
unsigned int i, j;

    j = (context->count[0] >> 3) & 63;
    if ((context->count[0] += len << 3) < (len << 3)) context->count[1]++;
    context->count[1] += (len >> 29);
    if ((j + len) > 63) {
	memcpy(&context->buffer[j], data, (i = 64-j));
	Transform(context->state, context->buffer, usesha);
	for ( ; i + 63 < len; i += 64) {
	    Transform(context->state, &data[i], usesha);
	}
	j = 0;
    }
    else i = 0;
    memcpy(&context->buffer[j], &data[i], len - i);
}

static void Encode (unsigned char *output, uint32_t *input, unsigned int len, int usesha) {
	unsigned int i, j;

	if (usesha) {
		if (len == 8) for (i = 0; i < 8; i++)
			output[i] = (unsigned char) ((input[(i >= 4 ? 0 : 1)] >> ((3-(i & 3)) * 8) ) & 255);  /* Endian independent */

		if (len == 20) for (i = 0; i < 20; i++)
			output[i] = (unsigned char) ((input[i>>2] >> ((3-(i & 3)) * 8) ) & 255);

	} else {
		for (i = 0, j = 0; j < len; i++, j += 4) {
			output[j]   = (unsigned char)	((input[i]	) & 0xff);
			output[j+1] = (unsigned char)	((input[i] >>  8) & 0xff);
			output[j+2] = (unsigned char)	((input[i] >> 16) & 0xff);
			output[j+3] = (unsigned char)	((input[i] >> 24) & 0xff);
		}
	}
}

/* Add padding and return the message digest. */

static void Final(unsigned char digest[20], EKG2_SHA1_CTX* context, int usesha)
{
    unsigned char finalcount[8];
    uint32_t i;

    Encode(finalcount, context->count, 8, usesha);

    Update(context, (unsigned char *)"\200", 1, usesha);
#if 0
    else {		/* ORGINAL MD5Final() code from rfc1321 (http://tools.ietf.org/html/rfc1321) XXX, do we need it? \200 == 0x80 */
	    static unsigned char PADDING[64] = {
		    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	    };

	    unsigned int index, padLen;
	    index = (unsigned int)((context->count[0] >> 3) & 0x3f);
	    padLen = (index < 56) ? (56 - index) : (120 - index);
	    MD5Update (context, PADDING, padLen);
    }
#endif

    while ((context->count[0] & 504) != 448) {
	Update(context, (unsigned char *)"\0", 1, usesha);
    }
    Update(context, finalcount, 8, usesha);  /* Should cause a SHA1Transform() */

    Encode(digest, context->state, usesha ? 20 : 16, usesha);

    /* Wipe variables */
    i = 0;
    memset(context->buffer, 0, 64);
    memset(context->state, 0, 20);
    memset(context->count, 0, 8);
    memset(&finalcount, 0, 8);
#ifdef SHA1HANDSOFF  /* make SHA1Transform overwrite it's own static vars */
    Transform(context->state, context->buffer, usesha);
#endif
}

/* EKG2 STUFF */

extern char *config_console_charset;							/* ekg/stuff.h */

/**
 * base16_encode()
 *
 * Return base16 hash of @a data
 *
 * @return <b>static</b> with 32 digit BASE16 HASH + NUL char.
 */

static char *base16_encode(const unsigned char *data) {
	static char result[33];
	int i;

	if (!data) return NULL;

	for (i = 0; i < 16; i++)
		snprintf(&result[i * 2], 3, "%02hhx", data[i]);

	result[32] = 0;
	return result;
}

/**
 * jabber_challange_digest()
 *
 * Return base16 encoded hash for SASL MD5 CHALLANGE
 *
 * @todo MD5Update() on NULL params will fail. XXX, no idea what to do.
 *
 * @return <b>static</b> buffer with 32 digit BASE16 HASH + NUL char
 */

char *jabber_challange_digest(const char *sid, const char *password, const char *nonce, const char *cnonce, const char *xmpp_temp, const char *realm) {
	EKG2_MD5_CTX ctx;
	unsigned char digest[20];

	const char *convnode, *convpasswd;	/* sid && password encoded in UTF-8 */
	char *ha1, *ha2;
	char *kd;

/* ZERO STEP -> recode */
	convnode = ekg_locale_to_utf8_use(sid);
	convpasswd = ekg_locale_to_utf8_use(password);

/* FIRST STEP */
	kd = saprintf("%s:%s:%s", convnode, realm, convpasswd);

	recode_xfree(sid, convnode);
	recode_xfree(password, convpasswd);

	MD5Init(&ctx);
	MD5Update(&ctx, kd, xstrlen(kd));
	MD5Final(digest, &ctx);

	xfree(kd);

/* SECOND STEP */
	kd = saprintf("xxxxxxxxxxxxxxxx:%s:%s", nonce, cnonce);
	memcpy(kd, digest, 16);

	MD5Init(&ctx);
	MD5Update(&ctx, kd, 16 + 1 + xstrlen(nonce) + 1 + xstrlen(cnonce));
	MD5Final(digest, &ctx);

	xfree(kd);

/* 3a) DATA */
	ha1 = xstrdup(base16_encode(digest));

	MD5Init(&ctx);
	MD5Update(&ctx, xmpp_temp, xstrlen(xmpp_temp));
	MD5Final(digest, &ctx);
/* 3b) DATA */
	ha2 = xstrdup(base16_encode(digest));

/* THIRD STEP */
	kd = saprintf("%s:%s:00000001:%s:auth:%s", ha1, nonce, cnonce, ha2);

	xfree(ha1);
	xfree(ha2);

	MD5Init(&ctx);
	MD5Update(&ctx, kd, xstrlen(kd));
	MD5Final(digest, &ctx);

	xfree(kd);

/* FINAL */
	return base16_encode(digest);
}

/** [XXX] SOME TIME AGO, I had idea to connect jabber_dcc_digest() and jabber_digest()
 *	with one function, and use va_list for it... i don't know.
 */

/**
 * jabber_dcc_digest()
 *
 * Return SHA1 hash for SOCKS5 Bytestreams connections [DCC]<br>
 * Make SHA1Update()'s on (@a uid, @a initiator and @a target)
 *
 * @todo SHA1Update() on NULL params will fail. XXX, no idea what to do.
 *
 * @todo We don't reencode params here to utf-8.
 *
 * @return <b>static</b> buffer, with 40 digit SHA1 hash + NUL char
 */

char *jabber_dcc_digest(char *sid, char *initiator, char *target) {
	EKG2_SHA1_CTX ctx;
	unsigned char digest[20];
	static char result[41];
	int i;

	SHA1Init(&ctx);
	SHA1Update(&ctx, sid, xstrlen(sid));
	SHA1Update(&ctx, initiator, xstrlen(initiator));
	SHA1Update(&ctx, target, xstrlen(target));
	SHA1Final(digest, &ctx);

	for (i = 0; i < 20; i++)
		sprintf(result + i * 2, "%.2x", digest[i]);

	return result;
}

/**
 * jabber_digest()
 *
 * Return SHA1 hash for jabber:iq:auth<br>
 * Make SHA1Update()'s on recoded to utf-8 (@a sid and @a password)
 *
 * @todo SHA1Update() on NULL params will fail. XXX, no idea what to do.
 *
 * @return <b>static</b> buffer, with 40 digit SHA1 hash + NUL char
 */

char *jabber_digest(const char *sid, const char *password, int istlen) {
	EKG2_SHA1_CTX ctx;
	unsigned char digest[20];
	static char result[41];
	const char *tmp;
	int i;

	SHA1Init(&ctx);

	tmp = (istlen) ? ekg_locale_to_iso2_use(sid) : ekg_locale_to_utf8_use(sid);
	SHA1Update(&ctx, tmp, xstrlen(tmp));
	recode_xfree(sid, tmp);

	tmp = (istlen) ? ekg_locale_to_iso2_use(password) : ekg_locale_to_utf8_use(password);
	SHA1Update(&ctx, tmp, xstrlen(tmp));
	recode_xfree(password, tmp);

	SHA1Final(digest, &ctx);

	for (i = 0; i < 20; i++)
		sprintf(result + i * 2, "%.2x", digest[i]);

	return result;
}

char *jabber_sha1_generic(char *buf, int len) {
	EKG2_SHA1_CTX ctx;
	unsigned char digest[20];
	static char result[41];
	int i;

	SHA1Init(&ctx);
	SHA1Update(&ctx, buf, len);
	SHA1Final(digest, &ctx);

	for (i = 0; i < 20; i++)
		sprintf(result + i * 2, "%.2x", digest[i]);
	
	return result;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
