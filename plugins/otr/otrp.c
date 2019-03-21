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

#include "otrp.h"

#include <stdarg.h>


/* 
 * forward declarations
 ****************************************************************************/

typedef struct {
	OtrpSMPState smp_state;
	const char *uid;
} OtrpAppData;

static void _session_free(OtrpSession *os);
static void _session_remove(OtrpSession *os);
static void _add_appdata(void *data, ConnContext *context);
static void _free_appdata(void *data);
static void _print(OtrpSession *os, ConnContext *context, const char *theme, ...);
static char *_username(OtrpSession *os, const char *target);
static gcry_error_t _err_make(gcry_err_code_t ec);
static gcry_error_t _privkey_write();
static gcry_error_t _fingerprints_write();
static gcry_error_t _instag_generate(const char *accountname, const char *protocol);
static gcry_error_t _send_message(OtrpSession *os, ConnContext *context, const char *message);
static OtrpAppData *_app_data(ConnContext *context);

/* UI ops */
static OtrlPolicy _ui_policy(void *opdata, ConnContext *context);
static void _ui_create_privkey(void *opdata, const char *accountname, const char *protocol);
static int _ui_is_logged_in(void *opdata, const char *accountname, const char *protocol, const char *recipient);
static void _ui_inject_message(void *opdata, const char *accountname, const char *protocol, const char *recipient, const char *message);
static void _ui_new_fingerprint(void *opdata, OtrlUserState us, const char *accountname, const char *protocol, const char *username, unsigned char fingerprint[20]);
static void _ui_write_fingerprints(void *opdata);
static void _ui_gone_secure(void *opdata, ConnContext *context);
static void _ui_gone_insecure(void *opdata, ConnContext *context);
static void _ui_still_secure(void *opdata, ConnContext *context, int is_reply);
static const char *_ui_otr_error_message(void *opdata, ConnContext *context, OtrlErrorCode err_code);
static void _ui_handle_smp_event(void *opdata, OtrlSMPEvent smp_event, ConnContext *context, unsigned short progress_percent, char *question);
static void _ui_handle_msg_event(void *opdata, OtrlMessageEvent msg_event, ConnContext *context, const char *message, gcry_error_t err);
static void _ui_create_instag(void *opdata, const char *accountname, const char *protocol);
static void _ui_timer_control(void *opdata, unsigned int interval);


/* 
 * global state
 ****************************************************************************/

static const char *_privkey_path, *_instag_path, *_fingerprints_path;
static OtrpSession *_sessions;
static OtrlUserState _userstate;
static OtrlMessageAppOps _ui_ops = {
	.policy=_ui_policy,
	.create_privkey=_ui_create_privkey,
	.is_logged_in=_ui_is_logged_in,
	.inject_message=_ui_inject_message,
	.update_context_list=NULL,
	.new_fingerprint=_ui_new_fingerprint,
	.write_fingerprints=_ui_write_fingerprints,
	.gone_secure=_ui_gone_secure,
	.gone_insecure=_ui_gone_insecure,
	.still_secure=_ui_still_secure,
	.max_message_size=NULL,
	.account_name=NULL,
	.account_name_free=NULL,
	.received_symkey=NULL,
	.otr_error_message=_ui_otr_error_message,
	.otr_error_message_free=NULL,
	.resent_msg_prefix=NULL,
	.resent_msg_prefix_free=NULL,
	.handle_smp_event=_ui_handle_smp_event,
	.handle_msg_event=_ui_handle_msg_event,
	.create_instag=_ui_create_instag,
	.convert_msg=NULL,
	.convert_free=NULL,
	.timer_control=_ui_timer_control,
};

#define _OTR_POLICY OTRL_POLICY_DEFAULT


/* 
 * initialization, state saving, cleanup
 ****************************************************************************/

/* libotr doesn't support saving private keys without generating a new
 * one, and we want to resave after forgetting a private key, so we
 * generate a dummy key we forget after load/save. */

#define _DUMMY_PRIVKEY ".dummy", ".internal"

static void _forget_dummy(void)
{
	OtrlPrivKey *dummy = otrl_privkey_find(_userstate, _DUMMY_PRIVKEY);
	if (dummy) {
		otrl_privkey_forget(dummy);
	}
}

gcry_error_t otrp_init()
{
	static gboolean otrl_initialized = false;
	gcry_error_t err;

	if (!otrl_initialized) {
		OTRL_INIT;
		otrl_sm_init();
		otrl_initialized = true;
	}

	_sessions = NULL;

	_userstate = otrl_userstate_create();

	_privkey_path = g_strdup(prepare_path("otr_keys", 0));
	if ((err = otrl_privkey_read(_userstate, _privkey_path)) != GPG_ERR_NO_ERROR) {
		if (gpg_err_code(err) != GPG_ERR_ENOENT) {
			return err;
		}
	}
	_forget_dummy();
	
	_fingerprints_path = g_strdup(prepare_path("otr_fingerprints", 0));
	if ((err = otrl_privkey_read_fingerprints(_userstate, _fingerprints_path, NULL, NULL)) != GPG_ERR_NO_ERROR) {
		if (gpg_err_code(err) != GPG_ERR_ENOENT) {
			return err;
		}
	}

	_instag_path = g_strdup(prepare_path("otr_instag", 0));
	if ((err = otrl_instag_read(_userstate, _instag_path)) != GPG_ERR_NO_ERROR) {
		if (gpg_err_code(err) != GPG_ERR_ENOENT) {
			return err;
		}
	}

	return OTRP_OK;
}

static gcry_error_t _fingerprints_write()
{
	return otrl_privkey_write_fingerprints(_userstate, _fingerprints_path);
}

static gcry_error_t _privkey_write()
{
	gcry_error_t err = otrl_privkey_generate(_userstate, _privkey_path, _DUMMY_PRIVKEY);
	_forget_dummy();
	return err;
}

void otrp_deinit()
{
	otrl_userstate_free(_userstate);
	_userstate = NULL;

	g_free((void*)_privkey_path);
	g_free((void*)_fingerprints_path);
	g_free((void*)_instag_path);

	_privkey_path = NULL;
	_fingerprints_path = NULL;
	_instag_path = NULL;

	while (_sessions) {
		_session_free(_sessions);
		_sessions = _sessions->next;
	}
}


/* 
 * session object
 ****************************************************************************/

#define _session_isproto(os, theprotocol) !xstrcmp(os->protocol, theprotocol)

OtrpSession *otrp_session(session_t *session)
{
	if (!session) {
		return NULL;
	}

	OtrpSession *os = _sessions;
	while (os) {
		if (os->session == session) {
			break;
		}
		os = os->next;
	}

	/* sanity check that session still exists on ekg2 side. We
	 * check now, so that we first find a possible stale
	 * OtrpSession to free. */
	if (!(session = session_find_ptr(session))) {
		_session_remove(os);
		return NULL;
	}

	if (!os) {
		char *colon = strchr(session->uid, ':');
		if (!colon) {
			return NULL; /* WAT */
		}

		os = g_malloc0(sizeof(OtrpSession));
		os->session = session;
		os->protocol = g_strndup(session->uid, colon - session->uid);

		colon++;   /* to point at beginning of account name */

		if (_session_isproto(os, "irc")) {
			os->type = OTRP_SESSION_IRC;

			/* IRC account name is session's nickname */
			const char *uid = session_get(session, "nickname");
			if (!uid) {
				uid = colon;
			}
			os->accountname = g_strdup(uid);
		} else if (_session_isproto(os, "xmpp") || _session_isproto(os, "tlen")) {
			os->type = OTRP_SESSION_XMPP;
			os->accountname = g_strdup(colon);
		} else {
			/* By default, whatever follows colon character is account name */
			os->accountname = g_strdup(colon);
		}

		if (_sessions) {
			OtrpSession *tail = _sessions;
			while (tail->next) {
				tail = tail->next;
			}
			tail->next = os;
		} else {
			_sessions = os;
		}
	}

	return os;
}

OtrpSession *otrp_session_find(const char *uid)
{
	return otrp_session(session_find(uid));
}

static void _session_remove(OtrpSession *os)
{
	if (!os) {
		return;
	}
	if (os == _sessions) {
		_sessions = os->next;
	} else {
		OtrpSession *prev = _sessions;
		while (prev) {
			if (prev->next == os) {
				prev->next = os->next;
				break;
			}
			prev = prev->next;
		}
	}
	_session_free(os);
}

static void _session_free(OtrpSession *os)
{
	if (!os) {
		return;
	}
	g_free((gpointer)os->protocol);
	g_free((gpointer)os->accountname);
	g_free(os);
}


/* 
 * private key
 ****************************************************************************/

gcry_error_t otrp_privkey_generate(OtrpSession *os)
{
	g_assert(os);
	return otrl_privkey_generate(_userstate, _privkey_path, os->accountname, os->protocol);
}

char *otrp_privkey_fingerprint(OtrpSession *os, OtrpFingerprint fingerprint)
{
	g_assert(os);
	return otrl_privkey_fingerprint(_userstate, fingerprint, os->accountname, os->protocol);
}

gcry_error_t otrp_privkey_forget(OtrpSession *os)
{
	g_assert(os);
	OtrlPrivKey *pk = otrl_privkey_find(_userstate, os->accountname, os->protocol);
	if (!pk) {
		return _err_make(OTRP_ERR_NO_PRIVKEY);
	}
	otrl_privkey_forget(pk);
	return _privkey_write();
}


/* 
 * connection context
 ****************************************************************************/

char *_username(OtrpSession *os, const char *uid)
{
	uid = get_uid(os->session, uid);
	
	if (!uid) {
		return NULL;
	}

	char *username = strchr(uid, ':');
	if (username) {
		username = g_strdup(username+1);
	} else {
		username = g_strdup(uid);
	}

	if (os->type == OTRP_SESSION_XMPP) {
		/* Strip resource */
		char *resource = strchr(username, '/');
		if (resource) {
			*resource = '\0';
		}
	}

	return username;
}

ConnContext *otrp_context(OtrpSession *os, const char *username, const char *protocol, gboolean create)
{
	g_assert(os);
	g_assert(username);
	g_assert(protocol);
	ConnContext *context = otrl_context_find(
		_userstate, username, os->accountname, protocol, OTRL_INSTAG_BEST,
		create, NULL, _add_appdata, NULL);
	if (create) {
		g_assert(context);
	}
	return context;
}

ConnContext *otrp_context_uid(OtrpSession *os, const char *uid, gboolean create)
{
	g_assert(os);
	g_assert(uid);

	char *username = _username(os, uid);
	if (!username) {
		return NULL;
	}

	ConnContext *ctx = otrp_context(os, username, os->protocol, create);

	g_free(username);
	return ctx;
}

static OtrpAppData *_app_data(ConnContext *context)
{
	OtrpAppData *ret = context->app_data;
	if (!ret) {
		_add_appdata(NULL, context);
		g_assert((ret = context->app_data));
	}
	return ret;
}

/* 
 * encrypted conversation
 ****************************************************************************/

static const char *_session_query_msg(OtrpSession *os)
{
	if (!os->_query_msg) {
		os->_query_msg = otrl_proto_default_query_msg(os->accountname, _OTR_POLICY);
	}
	return os->_query_msg;
}

gcry_error_t otrp_initiate(OtrpSession *os, ConnContext *context)
{
	return _send_message(os, context, _session_query_msg(os));
}

gcry_error_t otrp_trust(ConnContext *context)
{
	if (otrl_context_is_fingerprint_trusted(context->active_fingerprint)) {
		return _err_make(OTRP_ERR_ALREADY_TRUSTED);
	}
	otrl_context_set_trust(context->active_fingerprint, "manual");
	return _fingerprints_write();
}

gcry_error_t otrp_distrust(ConnContext *context)
{
	if (!otrl_context_is_fingerprint_trusted(context->active_fingerprint)) {
		return _err_make(OTRP_ERR_ALREADY_UNTRUSTED);
	}
	otrl_context_set_trust(context->active_fingerprint, "");
	return _fingerprints_write();
}

void otrp_smp(OtrpSession *os, ConnContext *context, const char *secret, const char *question)
{
	OtrpAppData *ad;
	g_assert(os);
	g_assert(context);
	g_assert(secret);
	ad = _app_data(context);

	switch (ad->smp_state) {
	case OTRP_SMP_INITIATED:
	case OTRP_SMP_RESPONDED:
		print("otr_smp_already_in_progress", context->username);
		break;
	case OTRP_SMP_NONE:
		if (question) {
			print("otr_requesting_auth_q", context->username, question);
			otrl_message_initiate_smp_q(_userstate, &_ui_ops, os, context, question, (unsigned char *) secret, xstrlen(secret));
		} else {
			print("otr_requesting_auth", context->username);
			otrl_message_initiate_smp(_userstate, &_ui_ops, os, context, (unsigned char *) secret, xstrlen(secret));
		}
		ad->smp_state = OTRP_SMP_INITIATED;
		break;
	case OTRP_SMP_RECEIVED:
		otrl_message_respond_smp(_userstate, &_ui_ops, os, context, (unsigned char *) secret, xstrlen(secret));
		ad->smp_state = OTRP_SMP_RESPONDED;
		break;
	}
}

void otrp_smp_abort(OtrpSession *os, ConnContext *context)
{
	OtrpAppData *ad;
	g_assert(os);
	g_assert(context);
	ad = _app_data(context);

	print("otr_aborting_auth", context->username);
	otrl_message_abort_smp(_userstate, &_ui_ops, os, context);
	ad->smp_state = OTRP_SMP_NONE;
}

OtrpSMPState otrp_smp_state(ConnContext *context)
{
	if (!context) {
		return OTRP_SMP_NONE;
	}
	return _app_data(context)->smp_state;
}

void otrp_disconnect(OtrpSession *os, ConnContext *context)
{
	otrl_message_disconnect_all_instances(_userstate, &_ui_ops, os, context->accountname, context->protocol, context->username);
}


/* 
 * libotr wrappers and callbacks
 ****************************************************************************/

static gcry_error_t _send_message(OtrpSession *os, ConnContext *context, const char *message)
{
	OtrpAppData *ad;
	g_assert(os);
	g_assert(context);
	ad = _app_data(context);
	if (command_exec_params(ad->uid, os->session, 1, "/msg", ad->uid, message, NULL)) {
		return _err_make(GPG_ERR_GENERAL);
	}
	return OTRP_OK;
}

static gcry_error_t _instag_generate(const char *accountname, const char *protocol)
{
	return otrl_instag_generate(_userstate, _instag_path, accountname, protocol);
}

int otrp_message_receiving(OtrpSession *os, const char *sender, const char *message, char **newmessagep)
{
	char *username = _username(os, sender);
	if (!username) {
		debug("[otr] no username %s / %s\n", os->session->uid, sender);
		return 0;
	}

	OtrlTLV *tlvs = NULL;
	ConnContext *context = NULL;
	
	int ret = otrl_message_receiving(
		_userstate, &_ui_ops, os,
		os->accountname, os->protocol, username,
		message, newmessagep, &tlvs, &context,
		_add_appdata, NULL);
	g_free(username);

	/* Check for disconnected message */
	OtrlTLV *tlv = otrl_tlv_find(tlvs, OTRL_TLV_DISCONNECTED);
	if (tlv) {
		_print(os, context, "otr_disconnected", context->username);
	}
	otrl_tlv_free(tlv);

	return ret;
}

gcry_error_t otrp_message_sending(OtrpSession *os, const char *recipient, const char *message, char **messagep)
{
	char *username = _username(os, recipient);
	if (!username) {
		return _err_make(GPG_ERR_INV_USER_ID);
	}

	gcry_error_t err = otrl_message_sending(
		_userstate, &_ui_ops, os,
		os->accountname, os->protocol, username, OTRL_INSTAG_BEST,
		message,
		NULL, messagep, OTRL_FRAGMENT_SEND_SKIP,
		NULL,
		_add_appdata, NULL);

	g_free(username);
	return err;
}


/* 
 * UI ops
 ****************************************************************************/


static OtrlPolicy _ui_policy(void *opdata, ConnContext *context) {
	return _OTR_POLICY;
}

static void _ui_create_privkey(void *opdata, const char *accountname, const char *protocol)
{
	ConnContext *context = otrp_context(opdata, accountname, protocol, true);
	OtrpAppData *ad = _app_data(context);

	command_exec_params(NULL, ((OtrpSession *)opdata)->session, 0, "/otr:key", "--generate", ad->uid, NULL);
}

static int _ui_is_logged_in(void *opdata, const char *accountname, const char *protocol, const char *recipient)
{
	ConnContext *context = otrp_context(opdata, accountname, protocol, true);
	OtrpAppData *ad = _app_data(context);

	userlist_t *u = userlist_find(((OtrpSession *)opdata)->session, ad->uid);
	return u && !EKG_STATUS_IS_NA(u->status);
}

static void _ui_inject_message(void *opdata, const char *accountname, const char *protocol, const char *recipient, const char *message)
{
	OTRP_DEBUG(_send_message, opdata, otrp_context(opdata, recipient, protocol, true), message);
}

static void _ui_new_fingerprint(void *opdata, OtrlUserState us, const char *accountname, const char *protocol, const char *username, unsigned char fingerprint[20])
{
	OtrpFingerprint human_fingerprint;
	otrl_privkey_hash_to_human(human_fingerprint, fingerprint);
	_print(opdata, otrp_context(opdata, username, protocol, true), "otr_new_fingerprint", username, human_fingerprint);
}

static void _ui_write_fingerprints(void *opdata)
{
	OTRP_DEBUG(_fingerprints_write);
}

static void _ui_gone_secure(void *opdata, ConnContext *context)
{
	OtrpFingerprint fingerprint;
	otrl_privkey_hash_to_human(fingerprint, context->active_fingerprint->fingerprint);
	if (otrl_context_is_fingerprint_trusted(context->active_fingerprint)) {
		_print(opdata, context, "otr_gone_secure_trusted", context->username, fingerprint);
	} else {
		_print(opdata, context, "otr_gone_secure_untrusted", context->username, fingerprint);
	}
}

static void _ui_gone_insecure(void *opdata, ConnContext *context)
{
	_print(opdata, context, "otr_gone_insecure", context->username);
}

static void _ui_still_secure(void *opdata, ConnContext *context, int is_reply)
{
	OtrpFingerprint fingerprint;
	otrl_privkey_hash_to_human(fingerprint, context->active_fingerprint->fingerprint);
	if (otrl_context_is_fingerprint_trusted(context->active_fingerprint)) {
		_print(opdata, context, "otr_still_secure_trusted", context->username, fingerprint);
	} else {
		_print(opdata, context, "otr_still_secure_untrusted", context->username, fingerprint);
	}
}

static const char *_ui_otr_error_message(void *opdata, ConnContext *context, OtrlErrorCode err_code)
{
	switch (err_code) {
	case OTRL_ERRCODE_ENCRYPTION_ERROR:
		return "error occured while encrypting a message";
	case OTRL_ERRCODE_MSG_NOT_IN_PRIVATE:
		return "sent encrypted message to somebody who is not in a mutual OTR session";
	case OTRL_ERRCODE_MSG_UNREADABLE:
		return "sent an unreadable encrypted message";
	case OTRL_ERRCODE_MSG_MALFORMED:
		return "message sent is malformed";
	default:
		debug("[otr] unkown err_code %d\n", err_code);
		return "(unrecognized error code)";
	}
}

static void _ui_handle_smp_event(void *opdata, OtrlSMPEvent smp_event, ConnContext *context, unsigned short progress_percent, char *question)
{
	OtrpAppData *ad = _app_data(context);

	char *percent = g_strdup_printf("%d", progress_percent);

	switch (smp_event) {
	case OTRL_SMPEVENT_ASK_FOR_SECRET:
		ad->smp_state = OTRP_SMP_RECEIVED;
		_print(opdata, context, "otr_smp_ask_for_secret", context->username);
		break;
	case OTRL_SMPEVENT_ASK_FOR_ANSWER:
		ad->smp_state = OTRP_SMP_RECEIVED;
		_print(opdata, context, "otr_smp_ask_for_answer", context->username, question);
		break;
	case OTRL_SMPEVENT_IN_PROGRESS:
		_print(opdata, context, "otr_smp_in_progress", context->username, percent);
		break;
	case OTRL_SMPEVENT_SUCCESS:
		ad->smp_state = OTRP_SMP_NONE;
		_print(opdata, context, "otr_smp_success", context->username, percent);
		break;
	case OTRL_SMPEVENT_FAILURE:
	case OTRL_SMPEVENT_CHEATED:
	case OTRL_SMPEVENT_ERROR:
		ad->smp_state = OTRP_SMP_NONE;
		_print(opdata, context, "otr_smp_failure", context->username, percent);
		break;
	case OTRL_SMPEVENT_ABORT:
		ad->smp_state = OTRP_SMP_NONE;
		_print(opdata, context, "otr_smp_abort", context->username, percent);
		break;
	default:
		debug("[otr] Ignoring unknown SMP event %d\n", smp_event);
	}
	g_free(percent);
}

static void _ui_handle_msg_event(void *opdata, OtrlMessageEvent msg_event, ConnContext *context, const char *message, gcry_error_t err)
{
	char *error_string;

	switch (msg_event) {
	case OTRL_MSGEVENT_CONNECTION_ENDED:
		/* Message has not been sent because our buddy has ended the private conversation. We should either close the connection, or refresh it. */
		_print(opdata, context, "otr_CONNECTION_ENDED");
		break;

	case OTRL_MSGEVENT_RCVDMSG_NOT_IN_PRIVATE:
		/* Received an encrypted message but cannot read it because no private connection is established yet. */
		_print(opdata, context, "otr_RCVDMSG_NOT_IN_PRIVATE");
		break;

	case OTRL_MSGEVENT_ENCRYPTION_REQUIRED:
		/* Our policy requires encryption but we are trying to send an unencrypted message out. */
		_print(opdata, context, "otr_ENCRYPTION_REQUIRED", context->username);
		break;

	case OTRL_MSGEVENT_ENCRYPTION_ERROR:
		/* An error occured while encrypting a message and the message was not sent. */
		_print(opdata, context, "otr_ENCRYPTION_ERROR", context->username);
		break;

	case OTRL_MSGEVENT_SETUP_ERROR:
		/* A private conversation could not be set up. A gcry_error_t will be passed. */
		error_string = g_strdup_printf("%s/%s", gcry_strsource(err), gcry_strerror(err));
		_print(opdata, context, "otr_SETUP_ERROR", context->username, error_string);
		g_free(error_string);
		break;

	case OTRL_MSGEVENT_MSG_REFLECTED:
		/* Received our own OTR messages. */
		_print(opdata, context, "otr_MSG_REFLECTED", context->username);
		break;

	case OTRL_MSGEVENT_MSG_RESENT:
		/* The previous message was resent. */
		_print(opdata, context, "otr_MSG_RESENT", context->username);
		break;

	case OTRL_MSGEVENT_RCVDMSG_UNREADABLE:
		/* Cannot read the received message. */
		_print(opdata, context, "otr_RCVDMSG_UNREADABLE", context->username);
		break;

	case OTRL_MSGEVENT_RCVDMSG_MALFORMED:
		/* The message received contains malformed data. */
		_print(opdata, context, "otr_RCVDMSG_MALFORMED", context->username);
		break;

	case OTRL_MSGEVENT_RCVDMSG_GENERAL_ERR:
		/* Received a general OTR error. The argument 'message' will also be passed and it will contain the OTR error message. */
		_print(opdata, context, "otr_RCVDMSG_GENERAL_ERR", context->username, message);
		break;

	case OTRL_MSGEVENT_LOG_HEARTBEAT_RCVD:
		/* Received a heartbeat. */
		debug("[otr] LOG_HEARTBEAT_RCVD\n");
		break;

	case OTRL_MSGEVENT_LOG_HEARTBEAT_SENT:
		/* Sent a heartbeat. */
		debug("[otr] LOG_HEARTBEAT_SENT\n");
		break;

	case OTRL_MSGEVENT_RCVDMSG_UNENCRYPTED:
		/* Received an unencrypted message. The argument 'message' will also be passed and it will contain the plaintext message. */
		/* XXX should we show it to the user? */
		debug("[otr] RCVDMSG_UNENCRYPTED: %s\n", message);
		break;

	case OTRL_MSGEVENT_RCVDMSG_UNRECOGNIZED:
		/* Cannot recognize the type of OTR message received. */
		debug("[otr] RCVDMSG_UNRECOGNIZED\n");
		break;

	case OTRL_MSGEVENT_RCVDMSG_FOR_OTHER_INSTANCE:
		/* Received and discarded a message intended for another instance. */
		debug("[otr] RCVDMSG_FOR_OTHER_INSTANCE\n");
		break;

	default:
		debug("[otr] Unrecognized msgevent: %d\n", msg_event);
	}
}

static void _ui_create_instag(void *opdata, const char *accountname, const char *protocol)
{
	OTRP_DEBUG(_instag_generate, accountname, protocol);
}

static gboolean _message_poll(void *data)
{
	otrl_message_poll(_userstate, &_ui_ops, data);
	return TRUE;
}

static void _ui_timer_control(void *opdata, unsigned int interval)
{
	(void) timer_remove_session(((OtrpSession *)opdata)->session, "otrp_message_poll");
	if (interval) {
		ekg_timer_add(&otr_plugin, "otrp_message_poll", interval*1000, _message_poll, ((OtrpSession *)opdata)->session, NULL);
	}
}


/* 
 * misc helpers
 ****************************************************************************/

static void _print(OtrpSession *os, ConnContext *context, const char *theme, ...)
{									
	va_list ap;
	OtrpAppData *ad = context->app_data;
	g_assert(ad);

	va_start(ap, theme);
	vprint_window(ad->uid, os->session, EKG_WINACT_JUNK, 1, theme, ap);
	va_end(ap);
}


gcry_error_t otrp_debug(gcry_error_t err, const char *msg) {
	if (err != OTRP_OK) {
		debug("[otr] %s: ERROR: %s: %s\n", msg, gcry_strsource(err), gcry_strerror(err));
	}
	return err;
}

static void _add_appdata(void *data, ConnContext *context)
{
	OtrpAppData *ad = g_malloc0(sizeof(OtrpAppData));
	ad->smp_state = OTRP_SMP_NONE;
	ad->uid = g_strdup_printf("%s:%s", context->protocol, context->username);
	context->app_data = ad;
	context->app_data_free = _free_appdata;
}

static void _free_appdata(void *data)
{
	if (data) {
		g_free((gpointer) ((OtrpAppData *)data)->uid);		
		g_free(data);
	}
}


static gcry_error_t _err_make(gcry_err_code_t err_code)
{
	return gpg_err_make(OTRP_ERR_SOURCE, err_code);
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
