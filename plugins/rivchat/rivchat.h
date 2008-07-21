/* some rivchat-magic-stuff based on protocol: http://akolacz.googlepages.com/RivChat-specyfikacja.PDF */

#define RC_BROADCAST	0xFFFFFFFF

#define RC_TIMEOUT	30	/* soft-timeout */
#define RC_PING_TIMEOUT	60	/* hard-timeout */

#define RC_MAGICSIZE	 11
#define RC_SIZE		328
#define RC_DATASIZE	256
#define RC_INFOSIZE	140

#define RC_MESSAGE 	 0
#define RC_INIT 	 1
#define RC_NICKCHANGE	 2
#define RC_QUIT 	 3
#define RC_ME		 4
#define RC_PING		 5
#define RC_NICKPROTEST	 6
#define RC_TOPIC	 7
#define RC_NEWTOPIC	 8
#define RC_AWAY		 9
#define RC_REAWAY	10
#define RC_KICK		11
#define RC_POP		12
#define RC_REPOP	13
#define RC_KICKED	14
#define RC_IGNORE	15
#define RC_NOIGNORE	16
#define RC_REPOPIGNORED	17
#define RC_ECHOMSG	18
#define RC_PINGAWAY	19
#define RC_FILEPROPOSE	20
#define RC_FILEREQUEST	21
#define RC_FILECANCEL	22
#define RC_FILECANCEL2	23	/* XXX, nie w protokole */

static const char rivchat_magic[RC_MAGICSIZE] = { 'R', 'i', 'v', 'C', 'h', 'a', 't' /* here NULs */};	/* RivChat\0\0\0\0 */

#define RC_PACKED __attribute__ ((packed))

typedef struct {
	char host[50];
	char os[20];
	char prog[18];
	char version[2];
	char away;
	char master;
	uint32_t slowa;
	char user[32];
	char kod;
	char plec;
	char __pad1[2];
	uint32_t online;
	char filetransfer;
	char pisze;
        char __pad2[2];
} RC_PACKED rivchat_info_t;

typedef struct {
	char header[RC_MAGICSIZE];		/* rivchat_magic */
	char __pad1;
	uint32_t size;
	uint32_t fromid;
	uint32_t toid;
	char nick[30];
	char __pad2[2];
	uint32_t type;
	char data[RC_DATASIZE]; 		/* or RCINFO */
	uint8_t colors[3];			/* colors RGB values */
	uint8_t seq;				/* sequence */
/* these 8bytes, can be uint64_t -> filesize */
	uint8_t gender;				/* 1 - man, 2 - woman */
	uint8_t encrypted;			/* we support encryption? */
	uint8_t bold;				/* ? */
	uint8_t reserved[5];
} RC_PACKED rivchat_header_t; 

#define RC_FILETRANSFER 2
// #define RC_FILETRANSFER 0
#define RC_ENCRYPTED 0
