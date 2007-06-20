/* structs and magic values copied from http://akolacz.googlepages.com/RivChat-specyfikacja.PDF
 * copyright by Arkadiusz Kolacz
 */

typedef struct  {
	char header[11];
	int size;
	int fromid;
	int toid; 
	char nick[30];	
	int type;	
	char data[256]; 		/* or RCINFO */
	unsigned char format[10];
} rivchat_packet;

typedef struct {
	char host[50];
	char os[20];
	char prog[18];
	char version[2];
	char away;
	char master;
	int slowa;
	char user[32];
	char kod;
	char plec;
	int online;
	char filetransfer;
	char pisze;
} rivchat_packet_rcinfo;

#define RIVCHAT_MESSAGE 	0x00
#define RIVCHAT_INIT		0x01
#define RIVCHAT_QUIT		0x03
#define RIVCHAT_ME		0x04
#define RIVCHAT_PING		0x05
#define RIVCHAT_AWAY		0x09
#define RIVCHAT_PINGAWAY	0x13
