#include <sys/types.h>

typedef struct {
	uint32_t type;			/* typ pakietu */
	uint32_t len;		/* długość reszty pakietu */
	char data[];
} gg_header;

#define GG_RECV_MSG 0x000a
typedef struct {
	uint32_t sender;
	uint32_t seq;
	uint32_t time;
	uint32_t msgclass;
	char msg_data[];
} gg_recv_msg;

#define GG_SEND_MSG 0x000b
typedef struct {
	uint32_t recipient;
	uint32_t seq;
	uint32_t msgclass;
	char msg_data[];
} gg_send_msg;

#define GG_WELCOME 0x0001
typedef struct {
	uint32_t key;
} gg_welcome;

#define GG_SEND_MSG_ACK 0x0005
typedef struct {
	uint32_t status;
	uint32_t recipient;
	uint32_t seq;
} gg_send_msg_ack;

#define GG_PING 0x0008
#define GG_PONG 0x0007

#define GG_STATUS 0x0002
typedef struct {
	uint32_t uin;			/* numerek */
	uint32_t status;		/* nowy stan */
} gg_status;

#define GG_NEW_STATUS 0x0002
typedef struct {
	uint32_t status;			/* na jaki zmienić? */
} gg_new_status;

#define GG_LOGIN_OK 0x0003

#define GG_LIST_EMPTY 0x0012
