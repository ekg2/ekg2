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

