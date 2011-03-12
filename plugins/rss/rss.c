/*
 *  (C) Copyright 2011 Paweł Zuzelski <pawelz@google.com>
 *  (C) Copyright 2006 Jakub Zawadzki <darkjames@darkjames.ath.cx>
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

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <netdb.h>

#include <string.h>

#ifdef __sun	  /* Solaris, thanks to Beeth */
#include <sys/filio.h>
#endif

#include <ekg/net.h>

#include <expat.h>

#include "ekg2.h"

#define feed_private(s) ((s && s->priv) ? ((feed_private_t *) s->priv)->priv_data : NULL)

extern plugin_t feed_plugin;

typedef struct {
	void *priv_data;
} rss_private_t;

#define RSS_DEFAULT_TIMEOUT 60

#define RSS_ONLY         SESSION_MUSTBELONG | SESSION_MUSTHASPRIVATE
#define RSS_FLAGS_TARGET RSS_ONLY | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET

#define rss_convert_string(text, encoding) \
	ekg_recode_to_core_dup(encoding ? encoding : "UTF-8", text)

typedef enum {
	RSS_PROTO_UNKNOWN = 0,
	RSS_PROTO_HTTP,
	RSS_PROTO_HTTPS,
	RSS_PROTO_FTP,
	RSS_PROTO_FILE,
	RSS_PROTO_EXEC,
} rss_proto_t;

typedef struct rss_item_list {
	struct rss_item_list *next;

	char *session;
	int new;		/* is new? */

	char *url;		/* url */
	int hash_url;		/* ekg_hash of url */
	char *title;		/* title */
	int hash_title;		/* ekg_hash of title */
	char *descr;		/* descr */
	int hash_descr;		/* ekg_hash of descr */

	string_t other_tags;	/* place for other headers saved in format: (tag: value\n)
				 * sample:
				 *	author: someone\n
				 *	pubDate: someday\n
				 */
} rss_item_t;

typedef struct rss_channel_list {
	struct rss_channel_list *next;

	char *session;
	int new;		/* is new? */

	char *url;		/* url */
	int hash_url;		/* ekg_hash of url */
	char *title;		/* title */
	int hash_title;		/* ekg_hash of title */
	char *descr;		/* descr */
	int hash_descr;		/* ekg_hash of descr */
	char *lang;		/* lang */
	int hash_lang;		/* ekg_hash of lang */

	struct rss_item_list *rss_items;	/* list of channel items */
} rss_channel_t;

typedef struct rss_rss_list {
	struct rss_rss_list *next;

	char *session;

	char *url;		/* url */
	char *uid;		/* rss:url */

	int resolving;		/* we are waiting for resolver ? */
	int connecting;		/* we are waiting for connect() ? */
	int getting;		/* we are waiting for read()	 ? */

	int headers_done;
	struct rss_channel_list *rss_channels;

/* XXX headers_* */
	string_t headers;	/* headers */
	string_t buf;		/* buf with requested file */

/* PROTOs: */
	rss_proto_t proto;
	char *host;	/* protos: RSS_PROTO_HTTP, RSS_PROTO_HTTPS, RSS_PROTO_FTP			hostname	*/
	char *ip;	/*		j/w								cached ip addr	*/
	int port;	/*		j/w								port		*/
	char *file;	/* protos:	j/w RSS_PROTO_FILE						file		*/
} rss_rss_t;

struct htmlent_t {
	int l;
	char *s;
	gunichar uni;
};

static const struct htmlent_t html_entities[] = {
	{ 4,	"nbsp",		0xa0	},	/* no-break space = non-breaking space  */
	{ 4,	"quot",		0x22	},	/* quotation mark = APL quote  */
	{ 3,	"amp",		0x26	},	/* ampersand  */
	{ 2,	"lt",		0x3c	},	/* less-than sign  */
	{ 2,	"gt",		0x3e	},	/* greater-than sign  */
	{ 5,	"iexcl",	0xa1	},	/* inverted exclamation mark  */
	{ 4,	"cent",		0xa2	},	/* cent sign  */
	{ 5,	"pound",	0xa3	},	/* pound sign  */
	{ 6,	"curren",	0xa4	},	/* currency sign  */
	{ 3,	"yen",		0xa5	},	/* yen sign = yuan sign  */
	{ 6,	"brvbar",	0xa6	},	/* broken bar = broken vertical bar  */
	{ 4,	"sect",		0xa7	},	/* section sign  */
	{ 3,	"uml",		0xa8	},	/* diaeresis = spacing diaeresis  */
	{ 4,	"copy",		0xa9	},	/* copyright sign  */
	{ 4,	"ordf",		0xaa	},	/* feminine ordinal indicator  */
	{ 5,	"laquo",	0xab	},	/* left-pointing double angle quotation mark = left pointing guillemet  */
	{ 3,	"not",		0xac	},	/* not sign  */
	{ 3,	"shy",		0xad	},	/* soft hyphen = discretionary hyphen  */
	{ 3,	"reg",		0xae	},	/* registered sign = registered trade mark sign  */
	{ 4,	"macr",		0xaf	},	/* macron = spacing macron = overline = APL overbar  */
	{ 3,	"deg",		0xb0	},	/* degree sign  */
	{ 6,	"plusmn",	0xb1	},	/* plus-minus sign = plus-or-minus sign  */
	{ 4,	"sup2",		0xb2	},	/* superscript two = superscript digit two = squared  */
	{ 4,	"sup3",		0xb3	},	/* superscript three = superscript digit three = cubed  */
	{ 5,	"acute",	0xb4	},	/* acute accent = spacing acute  */
	{ 5,	"micro",	0xb5	},	/* micro sign  */
	{ 4,	"para",		0xb6	},	/* pilcrow sign = paragraph sign  */
	{ 6,	"middot",	0xb7	},	/* middle dot = Georgian comma = Greek middle dot  */
	{ 5,	"cedil",	0xb8	},	/* cedilla = spacing cedilla  */
	{ 4,	"sup1",		0xb9	},	/* superscript one = superscript digit one  */
	{ 4,	"ordm",		0xba	},	/* masculine ordinal indicator  */
	{ 5,	"raquo",	0xbb	},	/* right-pointing double angle quotation mark = right pointing guillemet  */
	{ 6,	"frac14",	0xbc	},	/* vulgar fraction one quarter = fraction one quarter  */
	{ 6,	"frac12",	0xbd	},	/* vulgar fraction one half = fraction one half  */
	{ 6,	"frac34",	0xbe	},	/* vulgar fraction three quarters = fraction three quarters  */
	{ 6,	"iquest",	0xbf	},	/* inverted question mark = turned question mark  */
	{ 6,	"Agrave",	0xc0	},	/* Latin capital letter A with grave = Latin capital letter A grave  */
	{ 6,	"Aacute",	0xc1	},	/* Latin capital letter A with acute  */
	{ 5,	"Acirc",	0xc2	},	/* Latin capital letter A with circumflex  */
	{ 6,	"Atilde",	0xc3	},	/* Latin capital letter A with tilde  */
	{ 4,	"Auml",		0xc4	},	/* Latin capital letter A with diaeresis  */
	{ 5,	"Aring",	0xc5	},	/* Latin capital letter A with ring above = Latin capital letter A ring  */
	{ 5,	"AElig",	0xc6	},	/* Latin capital letter AE = Latin capital ligature AE  */
	{ 6,	"Ccedil",	0xc7	},	/* Latin capital letter C with cedilla  */
	{ 6,	"Egrave",	0xc8	},	/* Latin capital letter E with grave  */
	{ 6,	"Eacute",	0xc9	},	/* Latin capital letter E with acute  */
	{ 5,	"Ecirc",	0xca	},	/* Latin capital letter E with circumflex  */
	{ 4,	"Euml",		0xcb	},	/* Latin capital letter E with diaeresis  */
	{ 6,	"Igrave",	0xcc	},	/* Latin capital letter I with grave  */
	{ 6,	"Iacute",	0xcd	},	/* Latin capital letter I with acute  */
	{ 5,	"Icirc",	0xce	},	/* Latin capital letter I with circumflex  */
	{ 4,	"Iuml",		0xcf	},	/* Latin capital letter I with diaeresis  */
	{ 3,	"ETH",		0xd0	},	/* Latin capital letter ETH  */
	{ 6,	"Ntilde",	0xd1	},	/* Latin capital letter N with tilde  */
	{ 6,	"Ograve",	0xd2	},	/* Latin capital letter O with grave  */
	{ 6,	"Oacute",	0xd3	},	/* Latin capital letter O with acute  */
	{ 5,	"Ocirc",	0xd4	},	/* Latin capital letter O with circumflex  */
	{ 6,	"Otilde",	0xd5	},	/* Latin capital letter O with tilde  */
	{ 4,	"Ouml",		0xd6	},	/* Latin capital letter O with diaeresis  */
	{ 5,	"times",	0xd7	},	/* multiplication sign  */
	{ 6,	"Oslash",	0xd8	},	/* Latin capital letter O with stroke = Latin capital letter O slash  */
	{ 6,	"Ugrave",	0xd9	},	/* Latin capital letter U with grave  */
	{ 6,	"Uacute",	0xda	},	/* Latin capital letter U with acute  */
	{ 5,	"Ucirc",	0xdb	},	/* Latin capital letter U with circumflex  */
	{ 4,	"Uuml",		0xdc	},	/* Latin capital letter U with diaeresis  */
	{ 6,	"Yacute",	0xdd	},	/* Latin capital letter Y with acute  */
	{ 5,	"THORN",	0xde	},	/* Latin capital letter THORN  */
	{ 5,	"szlig",	0xdf	},	/* Latin small letter sharp s = ess-zed  */
	{ 6,	"agrave",	0xe0	},	/* Latin small letter a with grave = Latin small letter a grave  */
	{ 6,	"aacute",	0xe1	},	/* Latin small letter a with acute  */
	{ 5,	"acirc",	0xe2	},	/* Latin small letter a with circumflex  */
	{ 6,	"atilde",	0xe3	},	/* Latin small letter a with tilde  */
	{ 4,	"auml",		0xe4	},	/* Latin small letter a with diaeresis  */
	{ 5,	"aring",	0xe5	},	/* Latin small letter a with ring above = Latin small letter a ring  */
	{ 5,	"aelig",	0xe6	},	/* Latin small letter ae = Latin small ligature ae  */
	{ 6,	"ccedil",	0xe7	},	/* Latin small letter c with cedilla  */
	{ 6,	"egrave",	0xe8	},	/* Latin small letter e with grave  */
	{ 6,	"eacute",	0xe9	},	/* Latin small letter e with acute  */
	{ 5,	"ecirc",	0xea	},	/* Latin small letter e with circumflex  */
	{ 4,	"euml",		0xeb	},	/* Latin small letter e with diaeresis  */
	{ 6,	"igrave",	0xec	},	/* Latin small letter i with grave  */
	{ 6,	"iacute",	0xed	},	/* Latin small letter i with acute  */
	{ 5,	"icirc",	0xee	},	/* Latin small letter i with circumflex  */
	{ 4,	"iuml",		0xef	},	/* Latin small letter i with diaeresis  */
	{ 3,	"eth",		0xf0	},	/* Latin small letter eth  */
	{ 6,	"ntilde",	0xf1	},	/* Latin small letter n with tilde  */
	{ 6,	"ograve",	0xf2	},	/* Latin small letter o with grave  */
	{ 6,	"oacute",	0xf3	},	/* Latin small letter o with acute  */
	{ 5,	"ocirc",	0xf4	},	/* Latin small letter o with circumflex  */
	{ 6,	"otilde",	0xf5	},	/* Latin small letter o with tilde  */
	{ 4,	"ouml",		0xf6	},	/* Latin small letter o with diaeresis  */
	{ 6,	"divide",	0xf7	},	/* division sign  */
	{ 6,	"oslash",	0xf8	},	/* Latin small letter o with stroke = Latin small letter o slash  */
	{ 6,	"ugrave",	0xf9	},	/* Latin small letter u with grave  */
	{ 6,	"uacute",	0xfa	},	/* Latin small letter u with acute  */
	{ 5,	"ucirc",	0xfb	},	/* Latin small letter u with circumflex  */
	{ 4,	"uuml",		0xfc	},	/* Latin small letter u with diaeresis  */
	{ 6,	"yacute",	0xfd	},	/* Latin small letter y with acute  */
	{ 5,	"thorn",	0xfe	},	/* Latin small letter thorn  */
	{ 4,	"yuml",		0xff	},	/* Latin small letter y with diaeresis  */
	{ 4,	"fnof",		0x192	},	/* Latin small f with hook = function = florin  */
	{ 5,	"Alpha",	0x391	},	/* Greek capital letter alpha  */
	{ 4,	"Beta",		0x392	},	/* Greek capital letter beta  */
	{ 5,	"Gamma",	0x393	},	/* Greek capital letter gamma  */
	{ 5,	"Delta",	0x394	},	/* Greek capital letter delta  */
	{ 7,	"Epsilon",	0x395	},	/* Greek capital letter epsilon  */
	{ 4,	"Zeta",		0x396	},	/* Greek capital letter zeta  */
	{ 3,	"Eta",		0x397	},	/* Greek capital letter eta  */
	{ 5,	"Theta",	0x398	},	/* Greek capital letter theta  */
	{ 4,	"Iota",		0x399	},	/* Greek capital letter iota  */
	{ 5,	"Kappa",	0x39a	},	/* Greek capital letter kappa  */
	{ 6,	"Lambda",	0x39b	},	/* Greek capital letter lambda  */
	{ 2,	"Mu",		0x39c	},	/* Greek capital letter mu  */
	{ 2,	"Nu",		0x39d	},	/* Greek capital letter nu  */
	{ 2,	"Xi",		0x39e	},	/* Greek capital letter xi  */
	{ 7,	"Omicron",	0x39f	},	/* Greek capital letter omicron  */
	{ 2,	"Pi",		0x3a0	},	/* Greek capital letter pi  */
	{ 3,	"Rho",		0x3a1	},	/* Greek capital letter rho  */
	{ 5,	"Sigma",	0x3a3	},	/* Greek capital letter sigma  */
	{ 3,	"Tau",		0x3a4	},	/* Greek capital letter tau  */
	{ 7,	"Upsilon",	0x3a5	},	/* Greek capital letter upsilon  */
	{ 3,	"Phi",		0x3a6	},	/* Greek capital letter phi  */
	{ 3,	"Chi",		0x3a7	},	/* Greek capital letter chi  */
	{ 3,	"Psi",		0x3a8	},	/* Greek capital letter psi  */
	{ 5,	"Omega",	0x3a9	},	/* Greek capital letter omega  */
	{ 5,	"alpha",	0x3b1	},	/* Greek small letter alpha  */
	{ 4,	"beta",		0x3b2	},	/* Greek small letter beta  */
	{ 5,	"gamma",	0x3b3	},	/* Greek small letter gamma  */
	{ 5,	"delta",	0x3b4	},	/* Greek small letter delta  */
	{ 7,	"epsilon",	0x3b5	},	/* Greek small letter epsilon  */
	{ 4,	"zeta",		0x3b6	},	/* Greek small letter zeta  */
	{ 3,	"eta",		0x3b7	},	/* Greek small letter eta  */
	{ 5,	"theta",	0x3b8	},	/* Greek small letter theta  */
	{ 4,	"iota",		0x3b9	},	/* Greek small letter iota  */
	{ 5,	"kappa",	0x3ba	},	/* Greek small letter kappa  */
	{ 6,	"lambda",	0x3bb	},	/* Greek small letter lambda  */
	{ 2,	"mu",		0x3bc	},	/* Greek small letter mu  */
	{ 2,	"nu",		0x3bd	},	/* Greek small letter nu  */
	{ 2,	"xi",		0x3be	},	/* Greek small letter xi  */
	{ 7,	"omicron",	0x3bf	},	/* Greek small letter omicron  */
	{ 2,	"pi",		0x3c0	},	/* Greek small letter pi  */
	{ 3,	"rho",		0x3c1	},	/* Greek small letter rho  */
	{ 6,	"sigmaf",	0x3c2	},	/* Greek small letter final sigma  */
	{ 5,	"sigma",	0x3c3	},	/* Greek small letter sigma  */
	{ 3,	"tau",		0x3c4	},	/* Greek small letter tau  */
	{ 7,	"upsilon",	0x3c5	},	/* Greek small letter upsilon  */
	{ 3,	"phi",		0x3c6	},	/* Greek small letter phi  */
	{ 3,	"chi",		0x3c7	},	/* Greek small letter chi  */
	{ 3,	"psi",		0x3c8	},	/* Greek small letter psi  */
	{ 5,	"omega",	0x3c9	},	/* Greek small letter omega  */
	{ 8,	"thetasym",	0x3d1	},	/* Greek small letter theta symbol  */
	{ 5,	"upsih",	0x3d2	},	/* Greek upsilon with hook symbol  */
	{ 3,	"piv",		0x3d6	},	/* Greek pi symbol  */
	{ 4,	"bull",		0x2022	},	/* bullet = black small circle  */
	{ 6,	"hellip",	0x2026	},	/* horizontal ellipsis = three dot leader  */
	{ 5,	"prime",	0x2032	},	/* prime = minutes = feet  */
	{ 5,	"Prime",	0x2033	},	/* double prime = seconds = inches  */
	{ 5,	"oline",	0x203e	},	/* overline = spacing overscore  */
	{ 5,	"frasl",	0x2044	},	/* fraction slash  */
	{ 6,	"weierp",	0x2118	},	/* script capital P = power set = Weierstrass p  */
	{ 5,	"image",	0x2111	},	/* blackletter capital I = imaginary part  */
	{ 4,	"real",		0x211c	},	/* blackletter capital R = real part symbol  */
	{ 5,	"trade",	0x2122	},	/* trade mark sign  */
	{ 7,	"alefsym",	0x2135	},	/* alef symbol = first transfinite cardinal  */
	{ 4,	"larr",		0x2190	},	/* leftwards arrow  */
	{ 4,	"uarr",		0x2191	},	/* upwards arrow  */
	{ 4,	"rarr",		0x2192	},	/* rightwards arrow  */
	{ 4,	"darr",		0x2193	},	/* downwards arrow  */
	{ 4,	"harr",		0x2194	},	/* left right arrow  */
	{ 5,	"crarr",	0x21b5	},	/* downwards arrow with corner leftwards = carriage return  */
	{ 4,	"lArr",		0x21d0	},	/* leftwards double arrow  */
	{ 4,	"uArr",		0x21d1	},	/* upwards double arrow  */
	{ 4,	"rArr",		0x21d2	},	/* rightwards double arrow  */
	{ 4,	"dArr",		0x21d3	},	/* downwards double arrow  */
	{ 4,	"hArr",		0x21d4	},	/* left right double arrow  */
	{ 6,	"forall",	0x2200	},	/* for all  */
	{ 4,	"part",		0x2202	},	/* partial differential  */
	{ 5,	"exist",	0x2203	},	/* there exists  */
	{ 5,	"empty",	0x2205	},	/* empty set = null set = diameter  */
	{ 5,	"nabla",	0x2207	},	/* nabla = backward difference  */
	{ 4,	"isin",		0x2208	},	/* element of  */
	{ 5,	"notin",	0x2209	},	/* not an element of  */
	{ 2,	"ni",		0x220b	},	/* contains as member  */
	{ 4,	"prod",		0x220f	},	/* n-ary product = product sign  */
	{ 3,	"sum",		0x2211	},	/* n-ary sumation  */
	{ 5,	"minus",	0x2212	},	/* minus sign  */
	{ 6,	"lowast",	0x2217	},	/* asterisk operator  */
	{ 5,	"radic",	0x221a	},	/* square root = radical sign  */
	{ 4,	"prop",		0x221d	},	/* proportional to  */
	{ 5,	"infin",	0x221e	},	/* infinity  */
	{ 3,	"ang",		0x2220	},	/* angle  */
	{ 3,	"and",		0x2227	},	/* logical and = wedge  */
	{ 2,	"or",		0x2228	},	/* logical or = vee  */
	{ 3,	"cap",		0x2229	},	/* intersection = cap  */
	{ 3,	"cup",		0x222a	},	/* union = cup  */
	{ 3,	"int",		0x222b	},	/* integral  */
	{ 6,	"there4",	0x2234	},	/* therefore  */
	{ 3,	"sim",		0x223c	},	/* tilde operator = varies with = similar to  */
	{ 4,	"cong",		0x2245	},	/* approximately equal to  */
	{ 5,	"asymp",	0x2248	},	/* almost equal to = asymptotic to  */
	{ 2,	"ne",		0x2260	},	/* not equal to  */
	{ 5,	"equiv",	0x2261	},	/* identical to  */
	{ 2,	"le",		0x2264	},	/* less-than or equal to  */
	{ 2,	"ge",		0x2265	},	/* greater-than or equal to  */
	{ 3,	"sub",		0x2282	},	/* subset of  */
	{ 3,	"sup",		0x2283	},	/* superset of  */
	{ 4,	"nsub",		0x2284	},	/* not a subset of  */
	{ 4,	"sube",		0x2286	},	/* subset of or equal to  */
	{ 4,	"supe",		0x2287	},	/* superset of or equal to  */
	{ 5,	"oplus",	0x2295	},	/* circled plus = direct sum  */
	{ 6,	"otimes",	0x2297	},	/* circled times = vector product  */
	{ 4,	"perp",		0x22a5	},	/* up tack = orthogonal to = perpendicular  */
	{ 4,	"sdot",		0x22c5	},	/* dot operator  */
	{ 5,	"lceil",	0x2308	},	/* left ceiling = APL upstile  */
	{ 5,	"rceil",	0x2309	},	/* right ceiling  */
	{ 6,	"lfloor",	0x230a	},	/* left floor = APL downstile  */
	{ 6,	"rfloor",	0x230b	},	/* right floor  */
	{ 4,	"lang",		0x2329	},	/* left-pointing angle bracket = bra  */
	{ 4,	"rang",		0x232a	},	/* right-pointing angle bracket = ket  */
	{ 3,	"loz",		0x25ca	},	/* lozenge  */
	{ 6,	"spades",	0x2660	},	/* black spade suit  */
	{ 5,	"clubs",	0x2663	},	/* black club suit = shamrock  */
	{ 6,	"hearts",	0x2665	},	/* black heart suit = valentine  */
	{ 5,	"diams",	0x2666	},	/* black diamond suit  */
	{ 5,	"OElig",	0x152	},	/* Latin capital ligature OE  */
	{ 5,	"oelig",	0x153	},	/* Latin small ligature oe  */
	{ 6,	"Scaron",	0x160	},	/* Latin capital letter S with caron  */
	{ 6,	"scaron",	0x161	},	/* Latin small letter s with caron  */
	{ 4,	"Yuml",		0x178	},	/* Latin capital letter Y with diaeresis  */
	{ 4,	"circ",		0x2c6	},	/* modifier letter circumflex accent  */
	{ 5,	"tilde",	0x2dc	},	/* small tilde  */
	{ 4,	"ensp",		0x2002	},	/* en space  */
	{ 4,	"emsp",		0x2003	},	/* em space  */
	{ 6,	"thinsp",	0x2009	},	/* thin space  */
	{ 4,	"zwnj",		0x200c	},	/* zero width non-joiner  */
	{ 3,	"zwj",		0x200d	},	/* zero width joiner  */
	{ 3,	"lrm",		0x200e	},	/* left-to-right mark  */
	{ 3,	"rlm",		0x200f	},	/* right-to-left mark  */
	{ 5,	"ndash",	0x2013	},	/* en dash  */
	{ 5,	"mdash",	0x2014	},	/* em dash  */
	{ 5,	"lsquo",	0x2018	},	/* left single quotation mark  */
	{ 5,	"rsquo",	0x2019	},	/* right single quotation mark  */
	{ 5,	"sbquo",	0x201a	},	/* single low-9 quotation mark  */
	{ 5,	"ldquo",	0x201c	},	/* left double quotation mark  */
	{ 5,	"rdquo",	0x201d	},	/* right double quotation mark  */
	{ 5,	"bdquo",	0x201e	},	/* double low-9 quotation mark  */
	{ 6,	"dagger",	0x2020	},	/* dagger  */
	{ 6,	"Dagger",	0x2021	},	/* double dagger  */
	{ 6,	"permil",	0x2030	},	/* per mille sign  */
	{ 6,	"lsaquo",	0x2039	},	/* single left-pointing angle quotation mark  */
	{ 6,	"rsaquo",	0x203a	},	/* single right-pointing angle quotation mark  */
	{ 4,	"euro",		0x20ac	},	/* euro sign  */
	{ 0,	NULL,		0	}
};

static int rss_theme_init();
void rss_protocol_deinit(void *priv);
void *rss_protocol_init(session_t *session);
void rss_init();
void rss_deinit();

PLUGIN_DEFINE(rss, PLUGIN_PROTOCOL, rss_theme_init);

static QUERY(rss_validate_uid)
{
	char *uid = *(va_arg(ap, char **));
	int *valid = va_arg(ap, int *);

	if (!uid)
		return 0;

	if (!xstrncasecmp(uid, "rss:", 4)) {
		(*valid)++;
		return -1;
	}

	return 0;
}

static QUERY(rss_session_init) {
	char *session = *(va_arg(ap, char**));
	session_t *s = session_find(session);

	rss_private_t *j;

	if (!s || s->priv || s->plugin != &rss_plugin)
		return 1;

	j = xmalloc(sizeof(rss_private_t));
	j->priv_data = rss_protocol_init(s);

	s->priv = j;
	userlist_read(s);
	return 0;
}

static QUERY(rss_session_deinit) {
	char *session = *(va_arg(ap, char**));
	session_t *s = session_find(session);

	rss_private_t *j;

	if (!s || !(j = s->priv) || s->plugin != &rss_plugin)
		return 1;

	userlist_write(s);
	config_commit();
	s->priv			= NULL;
	rss_protocol_deinit(j->priv_data);

	xfree(j);

	return 0;
}

// #define EKG_WINACT_RSS EKG_WINACT_MSG // till 4616
#define EKG_WINACT_RSS EKG_WINACT_IMPORTANT

	/* new:
	 *	0x0 - old
	 *	0x1 - new
	 *	0x2 - modified
	 */

	/* mtags: (by default rss_message() won't display any messages if new == 0, but if user want to display again (?) news, we must allow him)
	 *	0x0 - none
	 *	0x8 - display all headers / sheaders
	 */

static QUERY(rss_message) {
	char *session	= *(va_arg(ap, char **));
	char *uid	= *(va_arg(ap, char **));
	char *sheaders	= *(va_arg(ap, char **));
	char *headers	= *(va_arg(ap, char **));
	char *title	= *(va_arg(ap, char **));
	char *url	= *(va_arg(ap, char **));
	char *body	= *(va_arg(ap, char **));

	int *new	= va_arg(ap, int *);		/* 0 - old; 1 - new; 2 - modified */
	int mtags	= *(va_arg(ap, int *));

	session_t *s	= session_find(session);
	char *tmp;

	const char *dheaders	= session_get(s, "display_headers");
	const char *dsheaders	= session_get(s, "display_server_headers");
	int dmode		= session_int_get(s, "display_mode");
	int mw			= session_int_get(s, "make_window");

	const char *target	= NULL;
	window_t *targetwnd	= NULL;

	if (*new == 0 && !mtags) return 0;

	if (mtags)	/* XXX */
		dmode = mtags;

	switch (mw) {			/* XXX, __current ? */
		case 0:
			target = "__status";
			targetwnd = window_status;
			break;
		case 1:
			target = session;
			break;
		case 2:
		default:
			if (!(target = get_nickname(s, uid)))
				target = uid;
			break;
	}

	if (mw)
		targetwnd = window_new(target, s, 0);

	switch (dmode) {
		case 0:	 print_window_w(targetwnd, EKG_WINACT_RSS, "rss_message_new", title, url);	/* only notify */
		case -1: return 0;							/* do nothing */

		case 2:	body		= NULL;					/* only headers */
		case 1:	if (dmode == 1) headers = NULL;				/* only body */
		default:							/* default: 3 (body+headers) */
		case 3:	sheaders = NULL;					/* headers+body */
		case 4:	break;							/* shreaders+headers+body */
	}

	print_window_w(targetwnd, EKG_WINACT_RSS, "rss_message_header", title, url);

	if (sheaders) {
		char *str = xstrdup(sheaders);
		char *formated = NULL;
		while ((tmp = split_line(&str))) {
			char *value = NULL;
			char *formatka;

			if ((value = xstrchr(tmp, ' '))) *value = 0;
			if (dsheaders && !xstrstr(dsheaders, tmp)) {
/*				debug("DSHEADER: %s=%s skipping..\n", tmp, value+1); */
				continue;	/* jesli mamy display_server_headers a tego nie mamy na liscie to pomijamy */
			}

			formatka = saprintf("rss_server_header_%s", tmp);
			if (!format_exists(formatka)) { xfree(formatka); formatka = NULL; }

			formated = format_string(format_find(formatka ? formatka : "rss_server_header_generic"), tmp, value ? value+1 : "");
			print_window_w(targetwnd, EKG_WINACT_RSS, "rss_message_body", formated ? formated : tmp);

			xfree(formatka);
		}
		if (headers || body) print_window_w(targetwnd, EKG_WINACT_RSS, "rss_message_body", "");	/* rozdziel */
	}
	if (headers) {
		char *str, *org;
		str = org = xstrdup(headers);
		char *formated = NULL;
		while ((tmp = split_line(&str))) {
			char *value = NULL;
			char *formatka;

			if ((value = xstrchr(tmp, ' '))) *value = 0;
			if (dheaders && !xstrstr(dheaders, tmp)) {
				if (value)
					debug("DHEADER: %s=%s skipping...\n", tmp, value+1);
				else	debug("DHEADER: %s skipping.. (tag without value?\n", tmp);
				continue;	/* jesli mamy display_headers a tego nie mamy na liscie to pomijamy */
			}

			formatka = saprintf("rss_message_header_%s", tmp);
			if (!format_exists(formatka)) { xfree(formatka); formatka = NULL; }

			formated = format_string(format_find(formatka ? formatka : "rss_message_header_generic"), tmp, value ? value+1 : "");
			print_window_w(targetwnd, EKG_WINACT_RSS, "rss_message_body", formated ? formated : tmp);

			xfree(formated);
			xfree(formatka);
		}
		if (body) print_window_w(targetwnd, EKG_WINACT_RSS, "rss_message_body", "");	/* rozdziel */
		xfree(org);
	}
	if (body) {
		print_window_w(targetwnd, EKG_WINACT_RSS, "rss_message_body", body);
	}

	print_window_w(targetwnd, EKG_WINACT_RSS, "rss_message_footer");

	*new = 0;
	return 0;
}

static plugins_params_t rss_plugin_vars[] = {
/* common vars. */
	PLUGIN_VAR_ADD("alias",			VAR_STR, NULL, 0, NULL),
	/* (-1 - nothing; 0 - only notify; 1 - only body; 2 - only headers; 3 - headers+body 4 - sheaders+headers+ body)  default+else: 3 */
	PLUGIN_VAR_ADD("display_mode",		VAR_INT, "3", 0, NULL),
	PLUGIN_VAR_ADD("display_headers",	VAR_STR, "pubDate: author: dc:creator: dc:date:", 0, NULL),
	PLUGIN_VAR_ADD("display_server_headers", VAR_STR,
	/* display some basic server headers */
		"HTTP/1.1 "	/* rcode? */
		"Server: "
		"Date: ",
		0, NULL),
	/* [common var again] 0 - status; 1 - all in one window (s->uid) 2 - seperate windows per rss / group. default+else: 2 */
	PLUGIN_VAR_ADD("make_window",		VAR_INT, "2", 0, NULL),
	PLUGIN_VAR_END()
};

EXPORT int rss_plugin_init(int prio) {
	PLUGIN_CHECK_VER("rss");

	rss_plugin.params = rss_plugin_vars;
	plugin_register(&rss_plugin, prio);

	query_register("rss-message",
				QUERY_ARG_CHARP,		/* session uid */
				QUERY_ARG_CHARP,		/* uid */
				QUERY_ARG_CHARP,		/* proto headers */
				QUERY_ARG_CHARP,		/* headers */
				QUERY_ARG_CHARP,		/* title */
				QUERY_ARG_CHARP,		/* url */
				QUERY_ARG_CHARP,		/* descr */
				QUERY_ARG_INT,			/* new */
				QUERY_ARG_INT,			/* modify */
				QUERY_ARG_END);

	query_connect(&rss_plugin, "session-added", rss_session_init, NULL);
	query_connect(&rss_plugin, "session-removed", rss_session_deinit, NULL);
	query_connect(&rss_plugin, "protocol-validate-uid", rss_validate_uid, NULL);
	query_connect(&rss_plugin, "rss-message", rss_message, NULL);

	rss_init();
	return 0;
}

static int rss_plugin_destroy() {
	plugin_unregister(&rss_plugin);
	rss_deinit();
	return 0;
}

static int rss_theme_init() {
#ifndef NO_DEFAULT_THEME
	/* url - %1; title - %2; descr - %3; lang: %4 */
	format_add("rss_user_info_channel_unread",	_("%K| %g[unread]%n %2 (%1)"), 1);
	format_add("rss_user_info_channel_read",	_("%K| %R[read]%n %2 (%1)"), 1);

	/* same, but without lang (%4) */
	format_add("rss_user_info_item_unread",		_("%K|   %g[unread]%n %2 (%1)"), 1);
	format_add("rss_user_info_item_read",		_("%K|   %R[read]%n %2 (%1)"), 1);

	format_add("rss_status",		_("%> Newstatus: %1 (%2) %3"), 1);	/* XXX */

	format_add("rss_added",		_("%> (%2) Added %T%1%n to subscription\n"), 1);
	format_add("rss_exists_other",		_("%! (%3) %T%1%n already subscribed as %2\n"), 1);
	format_add("rss_not_found",		_("%) Subscription %1 not found, cannot unsubscribe"), 1);
	format_add("rss_deleted",		_("%) (%2) Removed from subscription %T%1%n\n"), 1);

	format_add("rss_message_new",		_("%) New message: %Y%1%n (%W%2%n)"), 1);

	format_add("rss_message_header",	_("%g,+=%G-----%y  %1 %n(ID: %W%2%n)"), 1);
	format_add("rss_message_body",		_("%g||%n %|%1"), 1);
	format_add("rss_message_footer",	_("%g|+=%G----- End of message...%n\n"), 1);

		/* %1 - tag %2 - value */
/* rss: */
	format_add("rss_message_header_generic",	_("%r %1 %W%2"), 1);
	format_add("rss_message_header_pubDate:",	_("%r Napisano: %W%2"), 1);
	format_add("rss_message_header_author:",	_("%r Autor: %W%2"), 1);
/* rdf: */
	format_add("rss_message_header_dc:date:",	_("%r Napisano: %W%2"), 1);
	format_add("rss_message_header_dc:creator:",	_("%r Autor: %W%2"), 1);

	format_add("rss_server_header_generic",	_("%m %1 %W%2"), 1);
#endif

	return 0;
}

static LIST_FREE_ITEM(rss_item_free_item, rss_item_t *) {
	xfree(data->session);
	xfree(data->url);
	xfree(data->title);
	xfree(data->descr);
}

DYNSTUFF_LIST_DECLARE_WC(rss_items, rss_item_t, rss_item_free_item,
	static __DYNSTUFF_ADD,			/* rss_items_add() */
	__DYNSTUFF_NOREMOVE,
	static __DYNSTUFF_DESTROY,		/* rss_items_destroy() */
	static __DYNSTUFF_COUNT)		/* rss_items_count() */

static LIST_FREE_ITEM(rss_channel_free_item, rss_channel_t *) {
	xfree(data->session);
	xfree(data->url);
	xfree(data->title);
	xfree(data->descr);
	xfree(data->lang);
	rss_items_destroy(&data->rss_items);
}

DYNSTUFF_LIST_DECLARE_WC(rss_channels, rss_channel_t, rss_channel_free_item,
	static __DYNSTUFF_ADD,			/* rss_channels_add() */
	__DYNSTUFF_NOREMOVE,
	static __DYNSTUFF_DESTROY,		/* rss_channels_destroy() */
	static __DYNSTUFF_COUNT)		/* rss_channels_count() */

static rss_rss_t *rsss;

static LIST_FREE_ITEM(rsss_free_item, rss_rss_t *) {
	xfree(data->session);
	xfree(data->url);
	xfree(data->uid);
	rss_channels_destroy(&data->rss_channels);
	string_free(data->buf, 1);
	string_free(data->headers, 1);
	xfree(data->host);
	xfree(data->ip);
	xfree(data->file);
}

DYNSTUFF_LIST_DECLARE(rsss, rss_rss_t, rsss_free_item,
	static __DYNSTUFF_LIST_ADD,			/* rsss_add() */
	__DYNSTUFF_NOREMOVE,
	static __DYNSTUFF_LIST_DESTROY)			/* rsss_destroy() */


static void rss_string_append(rss_rss_t *f, const char *str) {
	string_t buf		= f->buf;

	if (!buf) buf = f->buf =	string_init(str);
	else				string_append(buf, str);
	string_append_c(buf, '\n');
}

static void rss_set_status(const char *uid, int status) {
	session_t *s;

	for (s = sessions; s; s = s->next) {
		userlist_t *u = userlist_find(s, uid);
		if (u) u->status = status;
	}
}

static void rss_set_descr(const char *uid, char *descr) {
	session_t *s;

	for (s = sessions; s; s = s->next) {
		userlist_t *u = userlist_find(s, uid);
		if (u) {
			char *tmp;
			tmp = u->descr;
			u->descr = descr;
			xfree(tmp);
		}
	}
}

static void rss_set_statusdescr(const char *uid, int status, char *descr) {
	session_t *s;

	for (s = sessions; s; s = s->next) {
		if (!xstrncmp(s->uid, "rss:", 4)) {
			rss_set_status(uid, status);
			rss_set_descr(uid, descr);
		}
	}
}

static rss_item_t *rss_item_find(rss_channel_t *c, const char *url, const char *title, const char *descr) {
	session_t *s	= session_find(c->session);

	int hash_url	= url	? ekg_hash(url)   : 0;
	int hash_title	= title ? ekg_hash(title) : 0;
	int hash_descr	= descr ? ekg_hash(descr) : 0;

	struct rss_item_list *l;
	rss_item_t *item;

	for (l = c->rss_items; l; l = l->next) {
		item = l;

		if (item->hash_url != hash_url || xstrcmp(url, item->url)) continue;
		if (session_int_get(s, "item_enable_title_checking") == 1 && (item->hash_title != hash_title || xstrcmp(title, item->title))) continue;
		if (session_int_get(s, "item_enable_descr_checking") == 1 && (item->hash_descr != hash_descr || xstrcmp(descr, item->descr))) continue;

		return item;
	}

	item		= xmalloc(sizeof(rss_item_t));
	item->url	= xstrdup(url);
	item->hash_url	= hash_url;
	item->title	= xstrdup(title);
	item->hash_title= hash_title;
	item->descr	= xstrdup(descr);
	item->hash_descr= hash_descr;

	item->other_tags= string_init(NULL);
	item->new	= 1;

	rss_items_add(&(c->rss_items), item);
	return item;
}

static rss_channel_t *rss_channel_find(rss_rss_t *f, const char *url, const char *title, const char *descr, const char *lang) {
	session_t *s	= session_find(f->session);

	int hash_url	= url	? ekg_hash(url)   : 0;
	int hash_title	= title ? ekg_hash(title) : 0;
	int hash_descr	= descr ? ekg_hash(descr) : 0;
	int hash_lang	= lang	? ekg_hash(lang)  : 0;

	struct rss_channel_list *l;
	rss_channel_t *channel;

	for (l = f->rss_channels; l; l = l->next) {
		channel = l;

		if (channel->hash_url != hash_url || xstrcmp(url, channel->url)) continue;
		if (session_int_get(s, "channel_enable_title_checking") == 1 && (channel->hash_title != hash_title || xstrcmp(title, channel->title))) continue;
		if (session_int_get(s, "channel_enable_descr_checking") == 1 && (channel->hash_descr != hash_descr || xstrcmp(descr, channel->descr))) continue;
		if (session_int_get(s, "channel_enable_lang_checking")	== 1 && (channel->hash_lang  != hash_lang  || xstrcmp(lang, channel->lang))) continue;

		return channel;
	}

	channel			= xmalloc(sizeof(rss_channel_t));
	channel->session	= xstrdup(f->session);
	channel->url		= xstrdup(url);
	channel->hash_url	= hash_url;
	channel->title		= xstrdup(title);
	channel->hash_title	= hash_title;
	channel->descr		= xstrdup(descr);
	channel->hash_descr	= hash_descr;
	channel->lang		= xstrdup(lang);
	channel->hash_lang	= hash_lang;

	channel->new	= 1;

	rss_channels_add(&(f->rss_channels), channel);
	return channel;
}

static rss_rss_t *rss_rss_find(session_t *s, const char *url) {
	struct rss_rss_list *l;
	rss_rss_t *rss;

	if (!xstrncmp(url, "rss:", 4)) url += 4;

	for (l = rsss; l; l = l->next) {
		rss = l;

		if (!xstrcmp(rss->url, url))
			return rss;
	}

	rss		= xmalloc(sizeof(rss_rss_t));
	rss->session	= xstrdup(s->uid);
	rss->uid	= saprintf("rss:%s", url);
	rss->url	= xstrdup(url);

/*  URI: ^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))? */

	if (!xstrncmp(url, "https://", 8)) {
		url += 8;				/* skip https:// */
		rss->proto = RSS_PROTO_HTTPS;
	} else if (!xstrncmp(url, "ftp://", 6)) {
		url += 6;				/* skip ftp://	*/
		rss->proto = RSS_PROTO_FTP;
	} else if (!xstrncmp(url, "file://", 7)) {
		url += 7;				/* skip file:// */
		rss->proto = RSS_PROTO_FILE;
	} else if (!xstrncmp(url, "http://", 7)) {
		url += 7;				/* skip http:// */
		rss->proto = RSS_PROTO_HTTP;
	} else if (!xstrncmp(url, "exec:", 5)) {
		url += 5;
		rss->proto = RSS_PROTO_EXEC;
	} else {
		rss->proto = RSS_PROTO_HTTP;
	}

	if (rss->proto == RSS_PROTO_HTTP || rss->proto == RSS_PROTO_HTTPS || rss->proto == RSS_PROTO_FTP) {
		const char *req;
		char *host = NULL, *tmp;

		if ((req = xstrchr(url, '/')))	rss->host = xstrndup(url, req - url);
		else				rss->host = xstrdup(url);

		if ((tmp = xstrchr(host, ':'))) {	/* port http://www.cos:1234 */
			rss->port = atoi(tmp+1);
			*tmp = 0;
		} else {
			if (rss->proto == RSS_PROTO_FTP)	rss->port = 21;
			if (rss->proto == RSS_PROTO_HTTP)	rss->port = 80;
			if (rss->proto == RSS_PROTO_HTTPS)	rss->port = 443;
		}
		url = req;
	}
	if (rss->proto == RSS_PROTO_HTTP || rss->proto == RSS_PROTO_HTTPS || rss->proto == RSS_PROTO_FTP || rss->proto == RSS_PROTO_FILE || rss->proto == RSS_PROTO_EXEC)
		rss->file = xstrdup(url);

	debug_white("[rss] proto: %d url: %s port: %d url: %s file: %s\n", rss->proto, rss->url, rss->port, rss->url, rss->file);

	rsss_add(rss);
	return rss;
}

typedef struct xmlnode_s {
	char *name;
	string_t data;
	char **atts;

	struct xmlnode_s *parent;
	struct xmlnode_s *children;

	struct xmlnode_s *next;
} xmlnode_t;

typedef struct {
	rss_rss_t *f;
	xmlnode_t *node;
	char *no_unicode;
} rss_fetch_process_t;

static void rss_fetch_error(rss_rss_t *f, const char *str) {
	debug_error("rss_fetch_error() %s\n", str);
	rss_set_statusdescr(f->uid, EKG_STATUS_ERROR, xstrdup(str));
}

/* ripped from jabber plugin */
static void rss_handle_start(void *data, const char *name, const char **atts) {
	rss_fetch_process_t *j = data;
	xmlnode_t *n, *newnode;
	int arrcount;
	int i;

	if (!data || !name) {
		debug_error("[rss] rss_handle_start() invalid parameters\n");
		return;
	}

	newnode = xmalloc(sizeof(xmlnode_t));
	newnode->name = xstrdup(name);
	newnode->data = string_init(NULL);

	if ((n = j->node)) {
		newnode->parent = n;

		if (!n->children) {
			n->children = newnode;
		} else {
			xmlnode_t *m = n->children;

			while (m->next)
				m = m->next;

			m->next = newnode;
			newnode->parent = n;
		}
	}
	arrcount = g_strv_length((char **) atts);

	if (arrcount > 0) {
		newnode->atts = xmalloc((arrcount + 1) * sizeof(char *));
		for (i = 0; i < arrcount; i++)
			newnode->atts[i] = rss_convert_string(atts[i], j->no_unicode);
	} else	newnode->atts = NULL;

	j->node = newnode;
}

static void rss_handle_end(void *data, const char *name) {
	rss_fetch_process_t *j = data;
	xmlnode_t *n;
	string_t recode;
	char *text, *end;

	if (!data || !name) {
		debug_error("[rss] rss_handle_end() invalid parameters\n");
		return;
	}
	if (!(n = j->node)) return;

	if (n->parent) j->node = n->parent;

	recode = string_init(NULL);

	end = n->data->str + n->data->len;

	for (text = n->data->str; text < end; text++) {
		int n;
		gunichar unichar;
		gchar buffer[6];

		if (*text != '&') {
			string_append_c(recode, *text);
			continue;
		}

		text++;

		if ('#' == *text) {
			if (sscanf(text, "#%d;", &unichar) || sscanf(text, "#x%x;", &unichar)) {
				n = g_unichar_to_utf8(unichar, buffer);
				string_append_raw(recode, buffer, n);
				text = xstrchr(text, ';');
				continue;
			}
		} else {
			const struct htmlent_t *e;
			for (e = html_entities; e->l; e++) {
				if (!xstrncmp(text, e->s, e->l) && (';' == text[e->l])) {
					n = g_unichar_to_utf8(e->uni, buffer);
					string_append_raw(recode, buffer, n);
					text += e->l;
					break;
				}
			}
			if (e->l)
				continue;
		}

		text--;
		string_append_c(recode, '&');
	}

	string_free(n->data, 1);
	n->data = string_init(rss_convert_string(recode->str, j->no_unicode));
	string_free(recode, 1);
}

static void rss_handle_cdata(void *data, const char *text, int len) {
	rss_fetch_process_t *j = data;
	xmlnode_t *n;

	if (!j || !text) {
		debug_error("[rss] rss_handle_cdata() invalid parameters\n");
		return;
	}

	if (!(n = j->node)) return;

	string_append_n(n->data, text, len);
}

static int rss_handle_encoding(void *data, const char *name, XML_Encoding *info) {
	rss_fetch_process_t	 *j = data;
	int i;

	debug_function("rss_handle_encoding() %s\n", name);

	for(i=0; i<256; i++)
		info->map[i] = i;

	info->convert	= NULL;
	info->data	= NULL;
	info->release	= NULL;
	j->no_unicode	= xstrdup(name);
	return 1;
}

static void rss_parsexml_atom(rss_rss_t *f, xmlnode_t *node) {
	debug_error("rss_parsexml_atom() sorry, atom not implemented\n");
}

static void rss_parsexml_rdf(rss_rss_t *f, xmlnode_t *node) {
	rss_channel_t *chan;

	debug("rss_parsexml_rdf (channels oldcount: %d)\n", rss_channels_count(f->rss_channels));
	debug_error("XXX http://web.resource.org/rss/1.0/");

	chan = rss_channel_find(f, /* chanlink, chantitle, chandescr, chanlang */ "", "", "", "");

	for (; node; node = node->next) {
		if (!xstrcmp(node->name, "channel")) {
			/* DUZE XXX */

		} else if (!xstrcmp(node->name, "item")) {
			const char *itemtitle	= NULL;
			const char *itemdescr	= NULL;
			const char *itemlink	= NULL;

			xmlnode_t *subnode;
			rss_item_t *item;
			string_t    tmp		= string_init(NULL);

			for (subnode = node->children; subnode; subnode = subnode->next) {
				if (!xstrcmp(subnode->name, "title"))		itemtitle	= subnode->data->str;
				else if (!xstrcmp(subnode->name, "link"))	itemlink	= subnode->data->str;
				else if (!xstrcmp(subnode->name, "content:encoded") || !xstrcmp(subnode->name, "description")) {
					if (!itemdescr)
						itemdescr = subnode->data->str;
					else	debug_error("rss_parsexml_rdf: ignoring %s\n", subnode->name);

				} else {  /* other, format tag: value\n */
/*					debug_error("rss_parsexml_rdf RDF->ITEMS: %s\n", subnode->name); */
					string_append(tmp, subnode->name);
					string_append(tmp, ": ");
					string_append(tmp, subnode->data->str);
					string_append_c(tmp, '\n');
				}
			}
			item = rss_item_find(chan, itemlink, itemtitle, itemdescr);

			string_free(item->other_tags, 1);
			item->other_tags = tmp;
		} else debug_error("rss_parsexml_rdf RSS: %s\n", node->name);
	}
}

static void rss_parsexml_rss(rss_rss_t *f, xmlnode_t *node) {
	debug("rss_parsexml_rss (channels oldcount: %d)\n", rss_channels_count(f->rss_channels));

	for (; node; node = node->next) {
		if (!xstrcmp(node->name, "channel")) {
			const char *chantitle	= NULL;
			const char *chanlink	= NULL;
			const char *chandescr	= NULL;
			const char *chanlang	= NULL;
			rss_channel_t *chan;

			xmlnode_t *subnode;

			for (subnode = node->children; subnode; subnode = subnode->next) {
				if (!xstrcmp(subnode->name, "title"))		chantitle	= subnode->data->str;
				else if (!xstrcmp(subnode->name, "link"))	chanlink	= subnode->data->str;
				else if (!xstrcmp(subnode->name, "description"))chandescr	= subnode->data->str;
				else if (!xstrcmp(subnode->name, "language"))	chanlang	= subnode->data->str;
				else if (!xstrcmp(subnode->name, "item"))	; /* later */
				else debug("rss_parsexml_rss RSS->CHANNELS: %s\n", subnode->name);
			}

			chan = rss_channel_find(f, chanlink, chantitle, chandescr, chanlang);
			debug("rss_parsexml_rss (items oldcount: %d)\n", rss_items_count(chan->rss_items));

			for (subnode = node->children; subnode; subnode = subnode->next) {
				if (!xstrcmp(subnode->name, "item")) {
					const char *itemtitle	= NULL;
					const char *itemdescr	= NULL;
					const char *itemlink	= NULL;
					rss_item_t *item;
					string_t    tmp		= string_init(NULL);

					xmlnode_t *items;
					for (items = subnode->children; items; items = items->next) {
						if (!xstrcmp(items->name, "title"))		itemtitle = items->data->str;
						else if (!xstrcmp(items->name, "description"))	itemdescr = items->data->str;
						else if (!xstrcmp(items->name, "link"))		itemlink  = items->data->str;
						else {	/* other, format tag: value\n */
							string_append(tmp, items->name);
							string_append(tmp, ": ");
							string_append(tmp, items->data->str);
							string_append_c(tmp, '\n');
						}
					}
					item = rss_item_find(chan, itemlink, itemtitle, itemdescr);

					string_free(item->other_tags, 1);
					item->other_tags = tmp;
				}
			}
		} else debug("rss_parsexml_rss RSS: %s\n", node->name);
	}
}

static void xmlnode_free(xmlnode_t *n) {
	xmlnode_t *m;

	if (!n)
		return;

	for (m = n->children; m;) {
		xmlnode_t *cur = m;
		m = m->next;
		xmlnode_free(cur);
	}

	xfree(n->name);
	string_free(n->data, 1);
	g_strfreev(n->atts);
	xfree(n);
}

static void rss_fetch_process(rss_rss_t *f, const char *str) {
	int new_items = 0;
	struct rss_channel_list *l;

	rss_fetch_process_t *priv = xmalloc(sizeof(rss_fetch_process_t));
	xmlnode_t *node;
	XML_Parser parser = XML_ParserCreate(NULL);

	XML_SetUserData(parser, (void*) priv);
	XML_SetElementHandler(parser, (XML_StartElementHandler) rss_handle_start, (XML_EndElementHandler) rss_handle_end);
	XML_SetCharacterDataHandler(parser, (XML_CharacterDataHandler) rss_handle_cdata);

//	XML_SetParamEntityParsing(parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
	XML_SetUnknownEncodingHandler(parser, (XML_UnknownEncodingHandler) rss_handle_encoding, priv);

	rss_set_descr(f->uid, xstrdup("Parsing..."));

	if (XML_Parse(parser, str, xstrlen(str), 1) == XML_STATUS_OK) {
		for (node = priv->node; node; node = node->next) {
			if (!xstrcmp(node->name, "rss")) rss_parsexml_rss(f, node->children);
			else if (!xstrcmp(node->name, "rss")) rss_parsexml_atom(f, node->children); /* xmlns */
			else if (!xstrcmp(node->name, "rdf:RDF")) rss_parsexml_rdf(f, node->children);
			else {
				debug("UNKNOWN node->name: %s\n", node->name);
				goto fail;
			}
		}
	} else {
		char *tmp = saprintf("XML_Parse: %s", XML_ErrorString(XML_GetErrorCode(parser)));
		rss_fetch_error(f, tmp);
		xfree(tmp);
		goto fail;
			//		for (node = priv->node; node; node = node->parent); /* going up on error */
	}

	for (l = f->rss_channels; l; l = l->next) {
		rss_channel_t *channel = l;
		struct rss_item_list *k;

		for (k = channel->rss_items; k; k = k->next) {
			rss_item_t *item	= k;
			char *proto_headers	= f->headers->len	? f->headers->str	: NULL;
			char *headers		= item->other_tags->len	? item->other_tags->str : NULL;
			int modify		= 0;			/* XXX */

//			if (channel->new)	item->new = 0;
			if (item->new)		new_items++;

			query_emit(NULL, "rss-message",
				&(f->session), &(f->uid), &proto_headers, &headers, &(item->title),
				&(item->url),  &(item->descr), &(item->new), &modify);
		}
		channel->new = 0;
	}

	if (!new_items)
		rss_set_statusdescr(f->uid, EKG_STATUS_DND, xstrdup("Done, no new messages"));
	else	rss_set_statusdescr(f->uid, EKG_STATUS_AVAIL, saprintf("Done, %d new messages", new_items));
fail:
	xmlnode_free(priv->node);
	XML_ParserFree(parser);
	return;
}

static WATCHER_LINE(rss_fetch_handler) {
	rss_rss_t	*f = data;

	if (type) {
		if (f->buf)
			rss_fetch_process(f, f->buf->str);
		else	rss_fetch_error(f, "[INTERNAL ERROR] Null f->buf");
		f->getting = 0;
		f->headers_done = 0;
		return 0;
	}

	if (f->headers_done) {
		rss_set_descr(f->uid, xstrdup("Getting data..."));
		if (xstrcmp(watch, ""))
			rss_string_append(f, watch);
	} else {
		if (!xstrcmp(watch, "\r")) {
			f->headers_done = 1;
			return 1;
		}
	/* append headers */
		if (!f->headers)	f->headers = string_init(watch);
		else			string_append(f->headers, watch);
		string_append_c(f->headers, '\n');

		/* XXX, parse some headers */
	}
	return 0;
}

/* handluje polaczenie, wysyla to co ma wyslac, dodaje łocza do odczytu */
static WATCHER(rss_fetch_handler_connect) {
	int		res = 0;
	socklen_t	res_size = sizeof(res);
	rss_rss_t	*f = data;

	f->connecting = 0;

	string_clear(f->headers);
	string_clear(f->buf);

	if (type == 1)
		return 0;

	if (type || getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
		if (type)
			debug("[rss] handle_connect(): SO_ERROR %s\n", strerror(res));
		if (type == 2); /* connection timeout */
		close(fd);
		return -1; /* ? */
	}

	if (f->proto == RSS_PROTO_HTTP) {
		rss_set_descr(f->uid, xstrdup("Requesting..."));
		char *request = saprintf(
			"GET %s HTTP/1.0\r\n"
			"Host: %s\r\n"
			"User-Agent: Ekg2 - evilny klient gnu (ssacz rssuff)\r\n"
			/* XXX, other headers */
			"Connection: close\r\n"
			"\r\n", f->file, f->host);
		write(fd, request, xstrlen(request));
		xfree(request);
	} else {	/* unknown proto here ? */
		close(fd);
		return -1;
	}
	f->getting = 1;
	f->headers_done = 0;
	watch_add_line(&rss_plugin, fd, WATCH_READ_LINE, rss_fetch_handler, f);
	return -1;
}

typedef struct {
	char *session;
	char *uid;
} rss_resolver_t;

static int rss_url_fetch(rss_rss_t *f, int quiet);

static WATCHER(rss_url_fetch_resolver) {
	rss_resolver_t *b = data;
	rss_rss_t *f;

	struct in_addr a;
	int len;

	debug("rss_url_fetch_resolver() fd: %d type: %d\n", fd, type);

	f = rss_rss_find(session_find(b->session), b->uid);

	if (type) {
		f->resolving = 0;
		if (f->ip)
			rss_url_fetch(f, 0);

		if (type == 2)
			rss_set_statusdescr(b->uid, EKG_STATUS_ERROR, saprintf("Resolver tiemout..."));

		xfree(b->session);
		xfree(b->uid);
		xfree(b);

		close(fd);
		return 0;
	}

	len = read(fd, &a, sizeof(a));

	if ((len != sizeof(a)) || (len && a.s_addr == INADDR_NONE /* INADDR_NONE kiedy NXDOMAIN */)) {
		rss_set_statusdescr(b->uid, EKG_STATUS_ERROR,
			saprintf("Resolver ERROR read: %d bytes (%s)", len, len == -1 ? strerror(errno) : ""));

		return -1;
	}

	f->ip = xstrdup(inet_ntoa(a));
	rss_set_descr(b->uid, saprintf("Resolved to: %s", f->ip));

	return -1;
}

static int rss_url_fetch(rss_rss_t *f, int quiet) {
	int fd = -1;

	debug_function("rss_url_fetch() f: 0x%x\n", f);

	if (f->connecting || f->resolving) {
		printq("rss_during_connect", session_name(session_find(f->session)), f->url);
		return -1;
	}

	if (f->getting) {
		printq("rss_during_getting", session_name(session_find(f->session)), f->url);
		return -1;
	}

	if (f->proto == RSS_PROTO_HTTPS) {
		printq("generic_error", "Currently we don't support https protocol, sorry");
		return -1;
	}

	if (f->proto == RSS_PROTO_FTP) {
		printq("generic_error", "Currently we don't support ftp protocol, sorry");
		return -1;
	}

	if (f->proto == RSS_PROTO_FILE) {
		fd = open(f->file, O_RDONLY);

		if (fd == -1) {
			debug_error("rss_url_fetch FILE: %s (error: %s,%d)", f->file, strerror(errno), errno);
			return -1;
		}
	}

	if (f->proto == RSS_PROTO_EXEC) {
		int fds[2];
		int pid;
		f->headers_done = 1;

		pipe(fds);

		if (!(pid = fork())) {

			dup2(open("/dev/null", O_RDONLY), 0);
			dup2(fds[1], 1);
			dup2(fds[1], 2);

			close(fds[0]);
			close(fds[1]);

			execl("/bin/sh", "sh", "-c", f->file, (void *) NULL);
			exit(1);
		}

		if (pid < 1) {
			close(fds[0]);
			close(fds[1]);
			return -1;
		}

		close(fds[1]);

		fd = fds[0];
		watch_add_line(&rss_plugin, fd, WATCH_READ_LINE, rss_fetch_handler, f);
	}

	if (f->proto == RSS_PROTO_HTTP) {
		debug("rss_url_fetch HTTP: host: %s port: %d file: %s\n", f->host, f->port, f->file);

		if (f->port <= 0 || f->port >= 65535) return -1;

		if (!f->ip) {	/* if we don't have ip, maybe it's v4 address? */
			if (inet_addr(f->host) != INADDR_NONE)
				f->ip = xstrdup(f->host);
		}

		if (f->ip) {
			struct sockaddr_in sin;
			int ret;
			int one = 1;

			debug("rss_url_fetch %s using previously cached IP address: %s\n", f->host, f->ip);

			fd = socket(AF_INET, SOCK_STREAM, 0);

			sin.sin_addr.s_addr	= inet_addr(f->ip);
			sin.sin_port		= g_htons(f->port);
			sin.sin_family		= AF_INET;

			rss_set_descr(f->uid, saprintf("Connecting to: %s (%s)", f->host, f->ip));
			f->connecting = 1;

			ioctl(fd, FIONBIO, &one);

			ret = connect(fd, (struct sockaddr *) &sin, sizeof(sin));

			watch_add(&rss_plugin, fd, WATCH_WRITE, rss_fetch_handler_connect, f);
		} else {
			watch_t *w;
			rss_resolver_t *b;

			if (!(w = ekg_resolver2(&rss_plugin, f->host, rss_url_fetch_resolver, NULL))) {
				rss_set_statusdescr(f->uid, EKG_STATUS_ERROR, saprintf("Resolver error: %s\n", strerror(errno)));
				return -1;
			}
			w->data = b = xmalloc(sizeof(rss_resolver_t));
			b->session	= xstrdup(f->session);
			b->uid		= saprintf("rss:%s", f->url);

			rss_set_descr(f->uid, xstrdup("Resolving..."));
			watch_timeout_set(w, 10);	/* 10 sec resolver timeout */
		}
		return fd;
	}
	return -1;
}


static COMMAND(rss_command_check) {
	userlist_t *ul;

	if (params[0]) {
		userlist_t *u = userlist_find(session, params[0]);

		if (!u) {
			printq("user_not_found", params[0]);
			/* && try /rss:get ? */
			return -1;
		}

		return rss_url_fetch(rss_rss_find(session, u->uid), quiet);
	}

	/* if param not given, check all */
	for (ul = session->userlist; ul; ul = ul->next) {
		userlist_t *u = ul;
		rss_rss_t *f = rss_rss_find(session, u->uid);

		rss_url_fetch(f, quiet);
	}
	return 0;
}

static COMMAND(rss_command_get) {
	return rss_url_fetch(rss_rss_find(session, target), quiet);
}

static COMMAND(rss_command_show) {
	rss_rss_t *rss;

	for (rss = rsss; rss; rss = rss->next) {
		/* if (!xstrcmp(rss->uid, XXX)); */
		rss_channel_t *chan;

		for (chan = rss->rss_channels; chan; chan = chan->next) {
			rss_item_t *item;
			/* if (!xstrcmp(chan->url, XXX)); */

			for (item = chan->rss_items; item; item = item->next) {

				if (!xstrcmp(item->url, params[0])) {
					char *proto_headers	= rss->headers->len	? rss->headers->str	: NULL;
					char *headers		= item->other_tags->len	? item->other_tags->str : NULL;
					int modify		= 0x04;			/* XXX */

					query_emit(NULL, "rss-message",
							&(rss->session), &(rss->uid), &proto_headers, &headers, &(item->title),
							&(item->url),  &(item->descr), &(item->new), &modify);
				}

			}
		}
	}

	return 0;
}

static COMMAND(rss_command_subscribe) {
	const char *nick;
	char *fulluid;
	userlist_t *u;

	if ((u = userlist_find(session, target))) {
		printq("rss_exists_other", target, format_user(session, u->uid), session_name(session));
		return -1;
	}

	if (!xstrncmp(target, "rss:", 4)) {
		fulluid = g_strdup(target);
	} else {
		fulluid = g_malloc(strlen(target)+ 1 + 4);
		strcpy(fulluid, "rss:");
		strcpy(fulluid+4, target);
	}

	if (params[0] && params[1]) {
		nick = params[1];
	} else {
		if ( (nick=xstrstr(fulluid + 4, "://")) )
			nick += 3;
		else if (!xstrncmp(fulluid + 4, "exec:", 5))
			nick = fulluid + 9;
		else
			nick = fulluid;
	}

	if (userlist_find(session, nick) || !(u = userlist_add(session, fulluid, nick))) {
		debug_error("rss_command_subscribe() userlist_add(%s, %s, %s) failed\n", session->uid, fulluid, nick);
		printq("generic_error", "IE, userlist_add() failed.");
		g_free(fulluid);
		return -1;
	}

	printq("rss_added", format_user(session, fulluid), session_name(session));
	query_emit(NULL, "userlist-refresh");
	g_free(fulluid);
	return 0;
}

static COMMAND(rss_command_unsubscribe) {
	userlist_t *u;
	if (!(u = userlist_find(session, target))) {
		printq("rss_not_found", target);
		return -1;
	}

	printq("rss_deleted", target, session_name(session));
	userlist_remove(session, u);
	query_emit(NULL, "userlist-refresh");
	return 0;
}

void rss_protocol_deinit(void *priv) {
	return;
}

void *rss_protocol_init(session_t *session) {
	session_connected_set(session, 1);
	session->status = EKG_STATUS_AVAIL;
	protocol_connected_emit(session);
	return NULL;
}

void rss_deinit() {
	rsss_destroy();
}

static QUERY(rss_userlist_info) {
	userlist_t *u	= *va_arg(ap, userlist_t **);
	int quiet	= *va_arg(ap, int *);

	rss_rss_t *rss;

	if (!u || valid_plugin_uid(&rss_plugin, u->uid) != 1 || u->uid[0] != 'r')
		return 1;

	for (rss = rsss; rss; rss = rss->next) {
		if (!xstrcmp(rss->uid, u->uid)) {
			rss_channel_t *chan;

			for (chan = rss->rss_channels; chan; chan = chan->next) {
				rss_item_t *item;

				printq(chan->new ? "rss_user_info_channel_unread" : "rss_user_info_channel_read",
					chan->url, chan->title, chan->descr, chan->lang);

				for (item = chan->rss_items; item; item = item->next) {
					printq(item->new ? "rss_user_info_item_unread" : "rss_user_info_item_read",
						item->url, item->title, item->descr);
				}
			}
			return 0;
		}
	}

	return 1;
}

void rss_init() {
	command_add(&rss_plugin, ("rss:check"), "u", rss_command_check, RSS_ONLY, NULL);
	command_add(&rss_plugin, ("rss:get"), "!u", rss_command_get, RSS_FLAGS_TARGET, NULL);

	command_add(&rss_plugin, ("rss:show"), "!", rss_command_show, RSS_ONLY | COMMAND_ENABLEREQPARAMS, NULL);

	command_add(&rss_plugin, ("rss:subscribe"), "! ?",	rss_command_subscribe, RSS_FLAGS_TARGET, NULL);
	command_add(&rss_plugin, ("rss:unsubscribe"), "!u",rss_command_unsubscribe, RSS_FLAGS_TARGET, NULL);

	query_connect(&rss_plugin, "userlist-info", rss_userlist_info, NULL);
}
