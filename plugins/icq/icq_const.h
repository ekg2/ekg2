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


#endif
