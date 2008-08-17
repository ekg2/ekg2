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

#include "ekg2-config.h"
#include <ekg/win32.h>
#include <ekg/debug.h>

#include <stdint.h>

#include <ekg/stuff.h>
#include <ekg/xmalloc.h>

#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    unsigned char buffer[64];
} EKG2_MD5_CTX;

static void Init(EKG2_MD5_CTX* context);
static void Transform(uint32_t state[4], unsigned char buffer[64]);
static void Update(EKG2_MD5_CTX* context, unsigned char* data, unsigned int len);
static void Final(unsigned char digest[20], EKG2_MD5_CTX* context);

#define MD5Init(ctx)			Init(ctx)
#define MD5Transform(state, buffer)	Transform(state, buffer)
#define MD5Update(ctx, data, len)	Update(ctx, (unsigned char *) data, len)
#define MD5Final(digest, ctx)		Final(digest, ctx)

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

#define FF(a, b, c, d, x, s, ac) { (a) += F ((b), (c), (d)) + (x) + (uint32_t)(ac); (a) = rol((a), (s)); (a) += (b); }
#define GG(a, b, c, d, x, s, ac) { (a) += G ((b), (c), (d)) + (x) + (uint32_t)(ac); (a) = rol((a), (s)); (a) += (b); }
#define HH(a, b, c, d, x, s, ac) { (a) += H ((b), (c), (d)) + (x) + (uint32_t)(ac); (a) = rol((a), (s)); (a) += (b); }
#define II(a, b, c, d, x, s, ac) { (a) += I ((b), (c), (d)) + (x) + (uint32_t)(ac); (a) = rol((a), (s)); (a) += (b); }

/* Hash a single 512-bit block. This is the core of the algorithm. */

static void Transform(uint32_t state[4], unsigned char buffer[64]) {
	uint32_t *block = (uint32_t *) buffer;
	uint32_t a, b, c, d;

	/* Copy context->state[] to working vars */
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];

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

	/* Add the working vars back into context.state[] */
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	/* Wipe variables */
	a = b = c = d = 0;
}

/* MD5Init - Initialize new context */

static void Init(EKG2_MD5_CTX* context) {
	/* MD5 initialization constants */
	context->state[0] = 0x67452301;
	context->state[1] = 0xEFCDAB89;
	context->state[2] = 0x98BADCFE;
	context->state[3] = 0x10325476;
	context->count[0] = context->count[1] = 0;
}

/* Run your data through this. */

static void Update(EKG2_MD5_CTX* context, unsigned char* data, unsigned int len) {
	unsigned int i, j;

	j = (context->count[0] >> 3) & 63;
	if ((context->count[0] += len << 3) < (len << 3)) context->count[1]++;
	context->count[1] += (len >> 29);
	if ((j + len) > 63) {
		memcpy(&context->buffer[j], data, (i = 64-j));
		Transform(context->state, context->buffer);
		for ( ; i + 63 < len; i += 64) {
			Transform(context->state, &data[i]);
		}
		j = 0;
	}
	else i = 0;
	memcpy(&context->buffer[j], &data[i], len - i);
}

static void Encode (unsigned char *output, uint32_t *input, unsigned int len) {
	unsigned int i, j;

	for (i = 0, j = 0; j < len; i++, j += 4) {
		output[j]   = (unsigned char)	((input[i]	) & 0xff);
		output[j+1] = (unsigned char)	((input[i] >>  8) & 0xff);
		output[j+2] = (unsigned char)	((input[i] >> 16) & 0xff);
		output[j+3] = (unsigned char)	((input[i] >> 24) & 0xff);
	}
}

/* Add padding and return the message digest. */

static void Final(unsigned char digest[16], EKG2_MD5_CTX* context) {
	unsigned char finalcount[8];
	uint32_t i;

	Encode(finalcount, context->count, 8);

	Update(context, (unsigned char *)"\200", 1);

	while ((context->count[0] & 504) != 448) {
		Update(context, (unsigned char *)"\0", 1);
	}
	Update(context, finalcount, 8);  /* Should cause a Transform() */

	Encode(digest, context->state, 16);

	/* Wipe variables */
	i = 0;
	memset(context->buffer, 0, 64);
	memset(context->state, 0, 16);
	memset(context->count, 0, 8);
	memset(&finalcount, 0, 8);
}

char *icq_md5_digest(const char *password, const unsigned char *key, int key_len) {
	static unsigned char digest[16];
	EKG2_MD5_CTX ctx;

	MD5Init(&ctx);
	MD5Update(&ctx, password, xstrlen(password));
	MD5Final(digest, &ctx);

	MD5Init(&ctx);
	MD5Update(&ctx, key, key_len);
	MD5Update(&ctx, digest, sizeof(digest));
	MD5Update(&ctx, "AOL Instant Messenger (SM)", xstrlen("AOL Instant Messenger (SM)"));
	MD5Final(digest, &ctx);

	return (char *) digest;
}

