/*
 * Messages taken from http://iserverd.khstu.ru/oscar/families.html
 *
 */

#if ICQ_SNAC_NAMES_DEBUG

typedef struct cmdnames_t {
	int code;
	char *text;
} cmdnames_t;

// Family 0x0001 Generic service controls 
struct cmdnames_t snac_names_01[] = {
	{0x01, "Client / server error"},
	{0x02, "Client is now online and ready for normal function"},
	{0x03, "Server supported snac families list"},
	{0x04, "Request for new service"},
	{0x05, "Redirect (for 0x0004 subtype)"},
	{0x06, "Request rate limits information"},
	{0x07, "Rate limits information response"},
	{0x08, "Add rate limits group"},
	{0x09, "Delete rate limits group"},
	{0x0a, "Rate information changed / rate limit warning"},
	{0x0b, "Server pause command"},
	{0x0c, "Client pause ack"},
	{0x0d, "Server resume command"},
	{0x0e, "Request own online information"},
	{0x0f, "Requested online info response"},
	{0x10, "Evil notification"},
	{0x11, "Set idle time"},
	{0x12, "Migration notice and info"},
	{0x13, "Message of the day (MOTD)"},
	{0x14, "Set privacy flags"},
	{0x15, "Well known urls"},
	{0x16, "No operation (NOP)"},
	{0x17, "Request server services versions"},
	{0x18, "Server services versions"},
	{0x1e, "Set status (set location info)"},
	{0x1f, "Client verification request"},
	{0x20, "Client verification reply"},
	{0x21, "Client's extended status from server"},
	{ -1, NULL}};

// Family 0x0002 Location services
struct cmdnames_t snac_names_02[] = {
	{0x01, "Client / server error"},
	{0x02, "Request limitations/params"},
	{0x03, "Limitations/params response"},
	{0x04, "Set user information"},
	{0x05, "Request user info"},
	{0x06, "User information response"},
	{0x07, "Watcher sub request"},
	{0x08, "Watcher notification"},
	{0x09, "Update directory info request"},
	{0x0A, "Update directory info reply"},
	{0x0B, "Query for SNAC(02,0C)"},
	{0x0C, "Reply to SNAC(02,0B)"},
	{0x0F, "Update user directory interests"},
	{0x10, "Update user directory interests reply"},
	{0x15, "User info query"},
	{ -1, NULL}};

// Family 0x0003 Buddy List management service
struct cmdnames_t snac_names_03[] = {
	{0x01, "Client / server error"},
	{0x02, "Request limitations/params"},
	{0x03, "Limitations/params response"},
	{0x04, "Add buddy(s) to contact list"},
	{0x05, "Remove buddy(ies) from contact"},
	{0x06, "Query for list of watchers"},
	{0x07, "Requested watchers list"},
	{0x08, "Watcher sub request"},
	{0x09, "Watcher notification"},
	{0x0a, "Notification rejected"},
	{0x0b, "User online notification"},
	{0x0c, "User offline notification"},
	{ -1, NULL}};

// Family 0x0004 ICBM service
struct cmdnames_t snac_names_04[] = {
	{0x01, "Client / server error"},
	{0x02, "Set ICBM parameters"},
	{0x03, "Reset ICBM parameters"},
	{0x04, "Request parameters info"},
	{0x05, "Requested parameters info response"},
	{0x06, "Send message thru server"},
	{0x07, "Message for client from server"},
	{0x08, "Evil request"},
	{0x09, "Server evil ack"},
	{0x0a, "Missed call (msg not delivered)"},
	{0x0b, "Client/server message error or data"},
	{0x0c, "Server message ack"},
	{0x10, "Get offline messages request"},
	{0x14, "Mini typing notifications (MTN)"},
	{0x17, "Get offline messages reply"},
	{ -1, NULL}};

// Family 0x0005 (depricated) Advertisements service
struct cmdnames_t snac_names_05[] = {
	{0x01, "Client / server error"},
	{0x02, "Client request ads"},
	{0x03, "Server returned ads (GIFs)"},
	{ -1, NULL}};

// Family 0x0006 Invitation service
struct cmdnames_t snac_names_06[] = {
	{0x01, "Client server error"},
	{0x02, "Invite a friend to join AIM"},
	{0x03, "Invitation server ack"},
	{ -1, NULL}};

// Family 0x0007 Administrative service
struct cmdnames_t snac_names_07[] = {
	{0x01, "Client / server error"},
	{0x02, "Request account info"},
	{0x03, "Requested account info"},
	{0x04, "Change account info (screenname, password) request"},
	{0x05, "Change account info ack"},
	{0x06, "Account confirm request"},
	{0x07, "Account confirm ack"},
	{0x08, "Account delete request"},
	{0x09, "Account delete ack"},
	{ -1, NULL}};

// Family 0x0008 Popup notices service
struct cmdnames_t snac_names_08[] = {
	{0x01, "Client server error"},
	{0x02, "Display popup message server command"},
	{ -1, NULL}};

// Family 0x0009 Privacy management service 
struct cmdnames_t snac_names_09[] = {
	{0x01, "Client / server error"},
	{0x02, "Request service parameters"},
	{0x03, "Requested service parameters"},
	{0x04, "Set group permissions mask"},
	{0x05, "Add to visible list"},
	{0x06, "Delete from visible list"},
	{0x07, "Add to invisible list"},
	{0x08, "Delete from invisible list"},
	{0x09, "Service error"},
	{0x0A, "Add to visible list (?)"},
	{0x0B, "Delete from visible list (?)"},
	{ -1, NULL}};

// Family 0x000A User lookup service
struct cmdnames_t snac_names_0a[] = {
	{0x01, "Client / server error"},
	{0x02, "Search user by email"},
	{0x03, "Search response"},
	{ -1, NULL}};

// Family 0x000B Usage stats service
struct cmdnames_t snac_names_0b[] = {
	{0x01, "Client / server error"},
	{0x02, "Set minimum report interval"},
	{0x03, "Usage stats report"},
	{0x04, "Usage stats report ack"},
	{ -1, NULL}};

// Family 0x000c (depricated) Translation service
struct cmdnames_t snac_names_0c[] = {
	{0x01, "Client / server error"},
	{0x02, "Client translate request"},
	{0x03, "Server translate reply"},
	{ -1, NULL}};

// Family 0x000D Chat navigation service
struct cmdnames_t snac_names_0d[] = {
	{0x01, "Client / server error"},
	{0x02, "Request limits"},
	{0x03, "Request exchange information"},
	{0x04, "Request room information"},
	{0x05, "Request extended room information"},
	{0x06, "Request member list"},
	{0x07, "Search for room"},
	{0x08, "Create room"},
	{0x09, "Requested information response"},
	{ -1, NULL}};

// Family 0x000E Chat service
struct cmdnames_t snac_names_0e[] = {
	{0x01, "Client / server error"},
	{0x02, "Room information update"},
	{0x03, "Users joined notification"},
	{0x04, "Users left notification"},
	{0x05, "Channel message from client"},
	{0x06, "Channel message to client"},
	{0x07, "Evil request"},
	{0x08, "Evil reply"},
	{0x09, "Chat error or data"},
	{ -1, NULL}};

// Family 0x000f Directory search (aim new search)
struct cmdnames_t snac_names_0f[] = {
	{0x01, "Client / server error"},
	{0x02, "Client search user request"},
	{0x03, "Server reply for search request (found users)"},
	{0x04, "Request interests list from server"},
	{0x05, "Server response to interests list request"},
	{ -1, NULL}};

// Family 0x0010 Server-stored buddy icons
struct cmdnames_t snac_names_10[] = {
	{0x01, "Client / server error"},
	{0x02, "Upload your icon to server"},
	{0x03, "Server ack for icon upload"},
	{0x04, "Request buddy icon from server (AIM only)"},
	{0x05, "Server response to a buddy icon request (AIM only)"},
	{0x06, "Request buddy icon from server (ICQ)"},
	{0x07, "Server response to a buddy icon request (ICQ)"},
	{ -1, NULL}};

// Family 0x0013 Server side information service
struct cmdnames_t snac_names_13[] = {
	{0x01, "Client / server error"},
	{0x02, "Request service parameters"},
	{0x03, "Service parameters reply"},
	{0x04, "Request contact list (first time)"},
	{0x05, "Contact list checkout"},
	{0x06, "Server contact list reply"},
	{0x07, "Load server contact list (after login)"},
	{0x08, "SSI edit: add item(s)"},
	{0x09, "SSI edit: update group header"},
	{0x0a, "SSI edit: remove item"},
	{0x0e, "SSI edit server ack"},
	{0x0f, "client local SSI is up-to-date"},
	{0x11, "Contacts edit start (begin transaction)"},
	{0x12, "Contacts edit end (finish transaction)"},
	{0x14, "Grant future authorization to client"},
	{0x15, "Future authorization granted"},
	{0x16, "Delete yourself from another client server contact"},
	{0x18, "Send authorization request"},
	{0x19, "Authorization request"},
	{0x1a, "Send authorization reply"},
	{0x1b, "Authorization reply"},
	{0x1c, "\"You were added\" message"},
	{ -1, NULL}};

// Family 0x0015 ICQ specific extensions service
struct cmdnames_t snac_names_15[] = {
	{0x01, "Client / server error"},
	{0x02, "Meta information request"},
	{0x03, "Meta information response"},
	{ -1, NULL}};

// Family 0x0017 Authorization/registration service
struct cmdnames_t snac_names_17[] = {
	{0x01, "Server error (registration refused)"},
	{0x02, "Client login request (md5 login sequence)"},
	{0x03, "Server login reply / error reply"},
	{0x04, "Request new uin"},
	{0x05, "New uin response"},
	{0x06, "Request md5 authkey"},
	{0x07, "Server md5 authkey response"},
	{0x0a, "Server SecureID request"},
	{0x0b, "Client SecureID reply"},
	{ -1, NULL}};

// Family 0x0085  - IServerd extension Broadcast service
struct cmdnames_t snac_names_85[] = {
	{0x01, "Server error"},
	{0x02, "Send broadcast message to server"},
	{0x03, "Server broadcast reply"},
	{ -1, NULL}};

typedef struct cmdtable_t {
	int code;
	struct cmdnames_t *table;
} cmdtable_t;

struct cmdtable_t snac_names_table[] = {
	{0x01, snac_names_01},
	{0x02, snac_names_02},
	{0x03, snac_names_03},
	{0x04, snac_names_04},
	{0x05, snac_names_05},
	{0x06, snac_names_06},
	{0x07, snac_names_07},
	{0x08, snac_names_08},
	{0x09, snac_names_09},
	{0x0a, snac_names_0a},
	{0x0b, snac_names_0b},
	{0x0c, snac_names_0c},
	{0x0d, snac_names_0d},
	{0x0e, snac_names_0e},
	{0x0f, snac_names_0f},
	{0x10, snac_names_10},
	{0x13, snac_names_13},
	{0x15, snac_names_15},
	{0x17, snac_names_17},
	{0x85, snac_names_85},
	{-1,   NULL}};

#endif

/*
 * Other names
 */

struct fieldnames_t caps_names[]={
	{0x1341, "Client supports voice chat."},
	{0x1342, "Client supports direct play service."},
	{0x1343, "Client supports file transfer (can send files)."},
	{0x1344, "Something called \"route finder\"."},
	{0x1345, "Client supports DirectIM/IMImage."},
	{0x1346, "Client supports avatar service."},
	{0x1347, "Client supports stocks (add-ins)."},
	{0x1348, "Client supports filetransfers (can receive files)."},
	{0x1349, "Client supports channel 2 extended, TLV(0x2711) based messages."},
	{0x134A, "Client supports games."},
	{0x134B, "Client supports buddy lists transfer."},
	{0x134D, "Client supports from/to AIM and ICQ."},
	{0x134E, "Client supports UTF-8 messages."},
	{-1, NULL}};

struct fieldnames_t snac_families[]={
	{0x01, "Generic service controls"},
	{0x02, "Location services"},
	{0x03, "Buddy List management service"},
	{0x04, "ICBM (messages) service"},
	{0x05, "Advertisements service"},
	{0x06, "Invitation service"},
	{0x07, "Administrative service"},
	{0x08, "Popup notices service"},
	{0x09, "Privacy management service"},
	{0x0a, "User lookup service"},
	{0x0b, "Usage stats service"},
	{0x0c, "Translation service"},
	{0x0d, "Chat navigation service"},
	{0x0e, "Chat service"},
	{0x0f, "Directory user search"},
	{0x10, "Server-stored buddy icons (SSBI) service"},
	{0x13, "Server Side Information (SSI) service"},
	{0x15, "ICQ specific extensions service"},
	{0x17, "Authorization/registration service"},
	{0x85, "Broadcast service - IServerd extension"},
	{-1, NULL}};

#if ICQ_SNAC_NAMES_DEBUG
const char *icq_snac_name(int family, int cmd) {

	cmdnames_t *cmsg = NULL;
	cmdtable_t *f = snac_names_table;

	for( ; f->code != -1 && f->table; f++) {
		if  (f->code != family)
		    continue;
		cmsg = f->table;
	}

	if (cmsg) {

		for ( ; (cmsg->code != -1) && cmsg->text; cmsg++) {
			if (cmsg->code ==  cmd) {
				return cmsg->text;
			}
		}
	}

	debug_error("icq_snac_name() Unknown SNAC(0x%x, 0x%x) name.\n", family, cmd);
	return NULL;
}
#endif

// vim:syn=c
