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

