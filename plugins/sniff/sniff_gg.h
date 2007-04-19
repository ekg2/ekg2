/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Woźny <speedy@ziew.org>
 *                          Arkadiusz Miśkiewicz <arekm@pld-linux.org>
 *                          Tomasz Chiliński <chilek@chilan.com>
 *                          Piotr Wysocki <wysek@linux.bydg.org>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *
 *			& many others look at: http://ekg.chmurka.net/docs/protocol.html
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

/* struct data && magic constants copied from libgadu (http://toxygen.net/libgadu) && 
 * 	gg protocol documentation (http://ekg.chmurka.net/docs/protocol.html)
 * 	great job guys! thx.
 */

#include <sys/types.h>

#define GG_PACKED __attribute__ ((packed))

typedef struct {
	uint32_t type;			/* typ pakietu */
	uint32_t len;		/* długość reszty pakietu */
	char data[];
} GG_PACKED gg_header;

#define GG_RECV_MSG 0x000a
typedef struct {
	uint32_t sender;
	uint32_t seq;
	uint32_t time;
	uint32_t msgclass;
	char msg_data[];
} GG_PACKED gg_recv_msg;

#define GG_SEND_MSG 0x000b
typedef struct {
	uint32_t recipient;
	uint32_t seq;
	uint32_t msgclass;
	char msg_data[];
} GG_PACKED gg_send_msg;

#define GG_WELCOME 0x0001
typedef struct {
	uint32_t key;
} GG_PACKED gg_welcome;

#define GG_SEND_MSG_ACK 0x0005
typedef struct {
	uint32_t status;
	uint32_t recipient;
	uint32_t seq;
} GG_PACKED gg_send_msg_ack;

#define GG_PING 0x0008
#define GG_PONG 0x0007

#define GG_STATUS 0x0002
typedef struct {
	uint32_t uin;			/* numerek */
	uint32_t status;		/* nowy stan */
	char status_data[];
} GG_PACKED gg_status;

#define GG_NEW_STATUS 0x0002
typedef struct {
	uint32_t status;			/* na jaki zmienić? */
	char status_data[];
} GG_PACKED gg_new_status;

#define GG_LOGIN_OK 0x0003
#define GG_LIST_EMPTY 0x0012

#define GG_STATUS60 0x000f
	
typedef struct {
	uint32_t uin;			/* numerek plus flagi w MSB */
	uint8_t status;			/* status danej osoby */
	uint32_t remote_ip;		/* adres ip delikwenta */
	uint16_t remote_port;		/* port, na którym słucha klient */
	uint8_t version;		/* wersja klienta */
	uint8_t image_size;		/* maksymalny rozmiar grafiki w KiB */
	uint8_t dunno1;			/* 0x00 */
	char status_data[];
} GG_PACKED gg_status60;

#define GG_NEED_EMAIL 0x0014

#define GG_LOGIN60 0x0015

typedef struct {
	uint32_t uin;			/* mój numerek */
	uint32_t hash;			/* hash hasła */
	uint32_t status;		/* status na dzień dobry */
	uint32_t version;		/* moja wersja klienta */
	uint8_t dunno1;			/* 0x00 */
	uint32_t local_ip;		/* mój adres ip */
	uint16_t local_port;		/* port, na którym słucham */
	uint32_t external_ip;		/* zewnętrzny adres ip */
	uint16_t external_port;		/* zewnętrzny port */
	uint8_t image_size;		/* maksymalny rozmiar grafiki w KiB */
	uint8_t dunno2;			/* 0xbe */
	char status_data[];
} GG_PACKED gg_login60;

#define GG_ADD_NOTIFY 0x000d
#define GG_REMOVE_NOTIFY 0x000e

typedef struct {
	uint32_t uin;			/* numerek */
	uint8_t dunno1;			/* bitmapa */
} GG_PACKED gg_add_remove;

#define GG_NOTIFY_REPLY60 0x0011
typedef struct {
	uint32_t uin;			/* numerek plus flagi w MSB */
	uint8_t status;			/* status danej osoby */
	uint32_t remote_ip;		/* adres ip delikwenta */
	uint16_t remote_port;		/* port, na którym słucha klient */
	uint8_t version;		/* wersja klienta */
	uint8_t image_size;		/* maksymalny rozmiar grafiki w KiB */
	uint8_t dunno1;			/* 0x00 */
	unsigned char next[];		/* nastepny, lub DLUGOSC_OPISU+OPIS */
} GG_PACKED gg_notify_reply60;

#define GG_NOTIFY_FIRST 0x000f
#define GG_NOTIFY_LAST 0x0010

#define GG_NOTIFY 0x0010
	
struct gg_notify {
	uint32_t uin;				/* numerek danej osoby */
	uint8_t dunno1;				/* rodzaj wpisu w liście */
} GG_PACKED;

#define GG_LOGIN70 0x19
#define GG_LOGIN_HASH_GG32 0x01
#define GG_LOGIN_HASH_SHA1 0x02

typedef struct {
	uint32_t uin;			/* mój numerek */
	uint8_t hash_type;		/* rodzaj hashowania hasła */
	uint8_t hash[64];		/* hash hasła dopełniony zerami */
	uint32_t status;		/* status na dzień dobry */
	uint32_t version;		/* moja wersja klienta */
	uint8_t dunno1;			/* 0x00 */
	uint32_t local_ip;		/* mój adres ip */
	uint16_t local_port;		/* port, na którym słucham */
	uint32_t external_ip;		/* zewnętrzny adres ip (???) */
	uint16_t external_port;		/* zewnętrzny port (???) */
	uint8_t image_size;		/* maksymalny rozmiar grafiki w KiB */
	uint8_t dunno2;			/* 0xbe */
	char status_data[];
} GG_PACKED gg_login70;

