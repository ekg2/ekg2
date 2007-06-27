void *mempcpy(void *dest, const void *src, size_t n);	/* forward */


/* Copyrights notes: */

/* symbols: digits[], special(), printable(), ns_name_unpack(), ns_name_ntop(), ns_name_uncompress()
 * 	copied from glibc resolv/res_data.c
 * under licence: */

/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* symbol: dn_expand() 
 * 	copied from glibc resolv/res_comp.c 
 * under licences: */

/*
 * Copyright (c) 1985, 1993
 *    The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* all struct and magic values
 * 	copied from: glibc resolv/arpa/nameser.h && resolv/arpa/nameser_compat.h
 * under licences: */

/*
 * Copyright (c) 1983, 1989, 1993
 *    The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

typedef struct {
	unsigned        id :16;         /* query identification number */
	/* fields in third byte */
	unsigned        rd :1;          /* recursion desired */
	unsigned        tc :1;          /* truncated message */
	unsigned        aa :1;          /* authoritive answer */
	unsigned        opcode :4;      /* purpose of message */
	unsigned        qr :1;          /* response flag */
	/* fields in fourth byte */
	unsigned        rcode :4;       /* response code */
	unsigned        cd: 1;          /* checking disabled by resolver */
	unsigned        ad: 1;          /* authentic data from named */
	unsigned        unused :1;      /* unused bits (MBZ as of 4.9.3a3) */
	unsigned        ra :1;          /* recursion available */
	/* remaining bytes */
	unsigned        qdcount :16;    /* number of question entries */
	unsigned        ancount :16;    /* number of answer entries */
	unsigned        nscount :16;    /* number of authority entries */
	unsigned        arcount :16;    /* number of resource entries */
} DNS_HEADER;


#define DNS_HFIXEDSZ	12		/* #/bytes of fixed data in header */
#define DNS_QFIXEDSZ	 4		/* #/bytes of fixed data in query */

#define NOERROR		0		/* No error occurred */
#define FORMERR		1		/* format error */
#define SERVFAIL	2		/* Server failure */
#define NXDOMAIN	3		/* Name error */
#define NOTIMP		4		/* Unimplemented */
#define REFUSED		5		/* Operation refused */

#define T_A		1		/* (IN) A */
#define T_CNAME		5		/* (IN) CNAME */
#define T_PTR		12		/* (IN) PTR */
#define T_MX		15		/* (IN) MX */
#define T_AAAA		28		/* (IN) AAAA */
#define T_SRV		33		/* (IN) SRV */

#define NS_MAXCDNAME	255		/* maximum compressed domain name */

#define NS_CMPRSFLGS	0xc0		/* Flag bits indicating name compression. */

static const char	digits[] = "0123456789";

static int special(int ch) {
	switch (ch) {
		case 0x22: /* '"' */
		case 0x2E: /* '.' */
		case 0x3B: /* ';' */
		case 0x5C: /* '\\' */
			/* Special modifiers in zone files. */
		case 0x40: /* '@' */
		case 0x24: /* '$' */
			return (1);
		default:
			return (0);
	}
}

static int printable(int ch) {
	return (ch > 0x20 && ch < 0x7f);
}

int ns_name_unpack(const u_char *msg, const u_char *eom, const u_char *src, u_char *dst, size_t dstsiz)
{
	const u_char *srcp, *dstlim;
	u_char *dstp;
	int n, len, checked;

	len = -1;
	checked = 0;
	dstp = dst;
	srcp = src;
	dstlim = dst + dstsiz;
	if (srcp < msg || srcp >= eom) {
		errno = EMSGSIZE;
		return (-1);
	}
	/* Fetch next label in domain name. */
	while ((n = *srcp++) != 0) {
		/* Check for indirection. */
		switch (n & NS_CMPRSFLGS) {
		case 0x40:
			if (n == 0x41) {
				if (dstp + 1 >= dstlim) {
					errno = EMSGSIZE;
					return (-1);
			  	}
				*dstp++ = 0x41;
				n = *srcp++ / 8;
				++checked;
			} else {
				errno = EMSGSIZE;
				return (-1);		/* flag error */
			}
			/* FALLTHROUGH */
		case 0:
			/* Limit checks. */
			if (dstp + n + 1 >= dstlim || srcp + n >= eom) {
				errno = EMSGSIZE;
				return (-1);
			}
			checked += n + 1;
			dstp = mempcpy(dstp, srcp - 1, n + 1);
			srcp += n;
			break;

		case NS_CMPRSFLGS:
			if (srcp >= eom) {
				errno = EMSGSIZE;
				return (-1);
			}
			if (len < 0)
				len = srcp - src + 1;
			srcp = msg + (((n & 0x3f) << 8) | (*srcp & 0xff));
			if (srcp < msg || srcp >= eom) {  /* Out of range. */
				errno = EMSGSIZE;
				return (-1);
			}
			checked += 2;
			/*
			 * Check for loops in the compressed name;
			 * if we've looked at the whole message,
			 * there must be a loop.
			 */
			if (checked >= eom - msg) {
				errno = EMSGSIZE;
				return (-1);
			}
			break;

		default:
			errno = EMSGSIZE;
			return (-1);			/* flag error */
		}
	}
	*dstp = '\0';
	if (len < 0)
		len = srcp - src;
	return (len);
}

int ns_name_ntop(const u_char *src, char *dst, size_t dstsiz) {
	const u_char *cp;
	char *dn, *eom;
	u_char c;
	u_int n;

	cp = src;
	dn = dst;
	eom = dst + dstsiz;

	while ((n = *cp++) != 0) {
		if ((n & NS_CMPRSFLGS) != 0 && n != 0x41) {
			/* Some kind of compression pointer. */
			errno = EMSGSIZE;
			return (-1);
		}
		if (dn != dst) {
			if (dn >= eom) {
				errno = EMSGSIZE;
				return (-1);
			}
			*dn++ = '.';
		}

		if (n == 0x41) {
			n = *cp++ / 8;
			if (dn + n * 2 + 4 >= eom) {
				errno = EMSGSIZE;
				return (-1);
			}
			*dn++ = '\\';
			*dn++ = '[';
			*dn++ = 'x';

			while (n-- > 0) {
				c = *cp++;
				unsigned u = c >> 4;
				*dn++ = u > 9 ? 'a' + u - 10 : '0' + u;
				u = c & 0xf;
				*dn++ = u > 9 ? 'a' + u - 10 : '0' + u;
			}

			*dn++ = ']';
			continue;
		}

		if (dn + n >= eom) {
			errno = EMSGSIZE;
			return (-1);
		}
		for ((void)NULL; n > 0; n--) {
			c = *cp++;
			if (special(c)) {
				if (dn + 1 >= eom) {
					errno = EMSGSIZE;
					return (-1);
				}
				*dn++ = '\\';
				*dn++ = (char)c;
			} else if (!printable(c)) {
				if (dn + 3 >= eom) {
					errno = EMSGSIZE;
					return (-1);
				}
				*dn++ = '\\';
				*dn++ = digits[c / 100];
				*dn++ = digits[(c % 100) / 10];
				*dn++ = digits[c % 10];
			} else {
				if (dn >= eom) {
					errno = EMSGSIZE;
					return (-1);
				}
				*dn++ = (char)c;
			}
		}
	}
	if (dn == dst) {
		if (dn >= eom) {
			errno = EMSGSIZE;
			return (-1);
		}
		*dn++ = '.';
	}
	if (dn >= eom) {
		errno = EMSGSIZE;
		return (-1);
	}
	*dn++ = '\0';
	return (dn - dst);
}

int ns_name_uncompress(const u_char *msg, const u_char *eom, const u_char *src, char *dst, size_t dstsiz) {
	u_char tmp[NS_MAXCDNAME];
	int n;

	if ((n = ns_name_unpack(msg, eom, src, tmp, sizeof tmp)) == -1)
		return (-1);
	if (ns_name_ntop(tmp, dst, dstsiz) == -1)
		return (-1);
	return (n);
}

int dn_expand(const u_char *msg, const u_char *eom, const u_char *src, char *dst, int dstsiz) {
	int n = ns_name_uncompress(msg, eom, src, dst, (size_t)dstsiz);

	if (n > 0 && dst[0] == '.')
		dst[0] = '\0';
	return (n);
}
