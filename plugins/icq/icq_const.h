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


#endif
