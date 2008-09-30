#ifndef __ICQ_CONST_H
#define __ICQ_CONST_H

/*
 * Message types.
 * Each OSCAR message has type. It can be just plain message, url message, contact list, wwp, email express or another.
 * Only one byte used for message type. Here is the list of known message types:
 */

#define MTYPE_PLAIN	0x01	/* Plain text (simple) message */
#define MTYPE_CHAT	0x02	/* Chat request message */
#define MTYPE_FILEREQ	0x03	/* File request / file ok message */
#define MTYPE_URL	0x04	/* URL message (0xFE formatted) */
#define MTYPE_AUTHREQ	0x06	/* Authorization request message (0xFE formatted) */
#define MTYPE_AUTHDENY	0x07	/* Authorization denied message (0xFE formatted) */
#define MTYPE_AUTHOK	0x08	/* Authorization given message (empty) */
#define MTYPE_SERVER	0x09	/* Message from OSCAR server (0xFE formatted) */
#define MTYPE_ADDED	0x0C	/* "You-were-added" message (0xFE formatted) */
#define MTYPE_WWP	0x0D	/* Web pager message (0xFE formatted) */
#define MTYPE_EEXPRESS	0x0E	/* Email express message (0xFE formatted) */
#define MTYPE_CONTACTS	0x13	/* Contact list message */
#define MTYPE_PLUGIN	0x1A	/* Plugin message described by text string */
#define MTYPE_AUTOAWAY	0xE8	/* Auto away message */
#define MTYPE_AUTOBUSY	0xE9	/* Auto occupied message */
#define MTYPE_AUTONA	0xEA	/* Auto not available message */
#define MTYPE_AUTODND	0xEB	/* Auto do not disturb message */
#define MTYPE_AUTOFFC	0xEC	/* Auto free for chat message */
/*
 *Internal Message types
 */
#define MTYPE_UNKNOWN               0x00 // Unknown message

#define MTYPE_GREETINGCARD          0x101 // Greeting Card
#define MTYPE_REQUESTCONTACTS       0x102 // Request for Contacts
#define MTYPE_MESSAGE               0x103 // Message+
#define MTYPE_STATUSMSGEXT          0x104 // StatusMsgExt (2003b)
#define MTYPE_SMS_MESSAGE           0x110 // SMS message from Mobile
#define MTYPE_SCRIPT_INVITATION     0x201 // Xtraz Invitation
#define MTYPE_SCRIPT_DATA           0x202 // Xtraz Message
#define MTYPE_SCRIPT_NOTIFY         0x208 // Xtraz Response
#define MTYPE_REVERSE_REQUEST       0x401 // Reverse DC request


/*
 * Message flag used to indicate additional message  properties. like auto message, multiple recipients message, etc.
 */
#define MFLAG_NORMAL	0x01	/* Normal message */
#define MFLAG_AUTO	0x03	/* Auto-message flag */
#define MFLAG_MULTI	0x80	/* This is multiple recipients message */

/*
 *
 */
#define ACKTYPE_MESSAGE    0
#define ACKTYPE_URL        1
#define ACKTYPE_FILE       2
#define ACKTYPE_CHAT       3
#define ACKTYPE_AWAYMSG    4
#define ACKTYPE_AUTHREQ    5
#define ACKTYPE_ADDED      6
#define ACKTYPE_GETINFO    7
#define ACKTYPE_SETINFO    8
#define ACKTYPE_LOGIN      9
#define ACKTYPE_SEARCH     10
#define ACKTYPE_NEWUSER    11
#define ACKTYPE_STATUS     12
#define ACKTYPE_CONTACTS   13	//send/recv of contacts
#define ACKTYPE_AVATAR	   14 //send/recv of avatars from a protocol
#define ACKTYPE_EMAIL      15	//notify if the unread emails changed


/*
 * Server response types to client meta request.
 */
#define SRV_OFFLINE_MESSAGE		0x0041
#define SRV_END_OF_OFFLINE_MSGS		0x0042
#define SRV_META_INFO_REPLY		0x07da

/*
 *  Server response subtypes to client meta info request
 * 	SNAC(15,03)/07da
 */
#define META_SET_HOMEINFO_ACK		0x0064
#define META_SET_WORKINFO_ACK		0x006e
#define META_SET_MOREINFO_ACK		0x0078
#define META_SET_NOTES_ACK		0x0082
#define META_SET_EMAILINFO_ACK		0x0087
#define META_SET_INTINFO_ACK		0x008c
#define META_SET_AFFINFO_ACK		0x0096
#define META_SMS_DELIVERY_RECEIPT	0x0096
#define META_SET_PERMS_ACK		0x00a0
#define META_SET_PASSWORD_ACK		0x00aa
#define META_UNREGISTER_ACK		0x00b4
#define META_SET_HPAGECAT_ACK		0x00be

#define META_BASIC_USERINFO		0x00c8
#define META_WORK_USERINFO		0x00d2
#define META_MORE_USERINFO		0x00dc
#define META_NOTES_USERINFO		0x00e6
#define META_EMAIL_USERINFO		0x00eb
#define META_INTERESTS_USERINFO		0x00f0
#define META_AFFILATIONS_USERINFO	0x00fa
#define META_SHORT_USERINFO		0x0104
#define META_HPAGECAT_USERINFO		0x010e

#define SRV_USER_FOUND			0x01a4
#define SRV_LAST_USER_FOUND		0x01ae

#define META_REGISTRATION_STATS_ACK	0x0302
#define SRV_RANDOM_FOUND		0x0366
#define META_XML_INFO			0x08a2
#define META_SET_FULLINFO_ACK		0x0c3f
#define META_SPAM_REPORT_ACK		0x2012


#define ACKTYPE_MESSAGE    0
#define ACKTYPE_URL        1
#define ACKTYPE_FILE       2
#define ACKTYPE_CHAT       3
#define ACKTYPE_AWAYMSG    4
#define ACKTYPE_AUTHREQ    5
#define ACKTYPE_ADDED      6
#define ACKTYPE_GETINFO    7
#define ACKTYPE_SETINFO    8
#define ACKTYPE_LOGIN      9
#define ACKTYPE_SEARCH     10
#define ACKTYPE_NEWUSER    11
#define ACKTYPE_STATUS     12
#define ACKTYPE_CONTACTS   13	//send/recv of contacts
#define ACKTYPE_AVATAR	   14 //send/recv of avatars from a protocol
#define ACKTYPE_EMAIL      15	//notify if the unread emails changed

/*
 * Status flags
 */
#define STATUS_WEBAWARE             0x0001 // Status webaware flag
#define STATUS_SHOWIP               0x0002 // Status show ip flag
#define STATUS_BIRTHDAY             0x0008 // User birthday flag
#define STATUS_WEBFRONT             0x0020 // User active webfront flag
#define STATUS_DCDISABLED           0x0100 // Direct connection not supported
#define STATUS_DCAUTH               0x1000 // Direct connection upon authorization
#define STATUS_DCCONT               0x2000 // DC only with contact users

/*
 * DC types
 */
#define DC_DISABLED                 0x0000 // Direct connection disabled / auth required
#define DC_HTTPS                    0x0001 // Direct connection thru firewall or https proxy
#define DC_SOCKS                    0x0002 // Direct connection thru socks4/5 proxy server
#define DC_NORMAL                   0x0004 // Normal direct connection (without proxy/firewall)
#define DC_WEB                      0x0006 // Web client - no direct connection


/*
 * Internal constants
 */
#define ICQ_VERSION		8         /* Protocol version */
#define CLIENTFEATURES		0x3
#define WEBFRONTPORT		0x50


#endif
