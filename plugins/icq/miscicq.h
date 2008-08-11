#define ICQ_STATUS_ONLINE           0x0000
#define ICQ_STATUS_AWAY             0x0001
#define ICQ_STATUS_NA               0x0005
#define ICQ_STATUS_OCCUPIED         0x0011
#define ICQ_STATUS_DND              0x0013
#define ICQ_STATUS_FFC              0x0020
#define ICQ_STATUS_INVISIBLE        0x0100

// Status FLAGS (used to determine status of other users)
#define ICQ_STATUS_FLAG_ONLINE          0x0000
#define ICQ_STATUS_FLAG_AWAY            0x0001
#define ICQ_STATUS_FLAG_DND             0x0002
#define ICQ_STATUS_FLAG_NA              0x0004
#define ICQ_STATUS_FLAG_OCCUPIED        0x0010
#define ICQ_STATUS_FLAG_FFC             0x0020
#define ICQ_STATUS_FLAG_INVISIBLE       0x0100


#define MTYPE_AUTOAWAY              0xE8 // Auto away message
#define MTYPE_AUTOBUSY              0xE9 // Auto occupied message
#define MTYPE_AUTONA                0xEA // Auto not available message
#define MTYPE_AUTODND               0xEB // Auto do not disturb message
#define MTYPE_AUTOFFC               0xEC // Auto free for chat message

#define STATUSF_WEBAWARE	0x00010000	/* The user is web-aware. */
#define STATUSF_DCAUTH		0x10000000	/* The user allows direct connections only upon authorization. */
#define STATUSF_DCCONTACT	0x20000000	/* The user allows direct connections only with contacts. */

#define STATUS_ICQONLINE	0x00000000
#define STATUS_ICQFFC		0x00000020
#define STATUS_ICQAWAY		0x00000001
#define STATUS_ICQDND		0x00000013
#define STATUS_INVISIBLE	0x00000100

#define STATUS_ICQOFFLINE    0xffffffff
#define STATUSF_ICQOCC       0x00000010
#define STATUSF_ICQDND       0x00000002
#define STATUSF_ICQNA        0x00000004

