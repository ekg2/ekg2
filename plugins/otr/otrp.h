/*
 *  (C) Copyright 2015 Maciej Pasternacki <maciej@pasternacki.net>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <libotr/proto.h>
#include <libotr/userstate.h>
#include <libotr/message.h>
#include <libotr/privkey.h>
#include <gcrypt.h>

#include "ekg2.h"

/* EKG2 stuff */

extern plugin_t otr_plugin;

/* Macros and types */

#define OTRP_ERR_SOURCE GPG_ERR_SOURCE_USER_2
#define OTRP_OK GPG_ERR_NO_ERROR
#define OTRP_ERR_NO_PRIVKEY GPG_ERR_NO_SECKEY
#define OTRP_ERR_ALREADY_TRUSTED 2049
#define OTRP_ERR_ALREADY_UNTRUSTED 2050

typedef char OtrpFingerprint[OTRL_PRIVKEY_FPRINT_HUMAN_LEN];

typedef enum {
	OTRP_SESSION_ANY,
	OTRP_SESSION_XMPP,
	OTRP_SESSION_IRC
} OtrpSessionType;

typedef struct OtrpSession_s {
	struct OtrpSession_s *next;
	OtrpSessionType type;
	session_t *session;
	const char *protocol;
	const char *accountname;

	/* Internal */
	const char *_query_msg;
} OtrpSession;

typedef enum {
	OTRP_SMP_NONE,
	OTRP_SMP_INITIATED,
	OTRP_SMP_RECEIVED,
	OTRP_SMP_RESPONDED
} OtrpSMPState;

extern gcry_error_t otrp_init(void);
extern void otrp_deinit(void);

extern OtrpSession *otrp_session(session_t *session);
extern OtrpSession *otrp_session_find(const char *uid);

extern gcry_error_t otrp_privkey_generate(OtrpSession *os);
extern char *otrp_privkey_fingerprint(OtrpSession *os, OtrpFingerprint fingerprint);
extern gcry_error_t otrp_privkey_forget(OtrpSession *os);

extern ConnContext *otrp_context(OtrpSession *os, const char *username, const char *protocol, gboolean create);
extern ConnContext *otrp_context_uid(OtrpSession *os, const char *uid, gboolean create);

extern gcry_error_t otrp_initiate(OtrpSession *os, ConnContext *context);
extern gcry_error_t otrp_trust(ConnContext *context);
extern gcry_error_t otrp_distrust(ConnContext *context);
extern void otrp_smp(OtrpSession *os, ConnContext *context, const char *secret, const char *question);
extern void otrp_smp_abort(OtrpSession *os, ConnContext *context);
extern OtrpSMPState otrp_smp_state(ConnContext *context);
extern void otrp_disconnect(OtrpSession *os, ConnContext *context);

extern gcry_error_t otrp_message_sending(OtrpSession *os, const char *recipient, const char *message, char **messagep);
extern int otrp_message_receiving(OtrpSession *os, const char *sender, const char *message, char **newmessagep);

extern gcry_error_t otrp_debug(gcry_error_t err, const char *msg);

#define OTRP_DEBUG(fn, args...) (otrp_debug(fn(args), #fn) != OTRP_OK)

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
