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

static int otr_theme_init();

PLUGIN_DEFINE(otr, PLUGIN_GENERIC, otr_theme_init);

static QUERY(message_decrypt_handler) {
	char **sessionp = va_arg(ap, char**);
	char **senderp = va_arg(ap, char**);
	char **messagep = va_arg(ap, char**);
	int *decryptedp = va_arg(ap, int*);

	if (!sessionp || !*sessionp || !senderp || !*senderp || !messagep || !*messagep) {
		return -1;
	}

	OtrpSession *os = otrp_session_find(*sessionp);

	if (!os) {
		return -1;
	}

	char *newmessage = NULL;

	if (otrp_message_receiving(os, *senderp, *messagep, &newmessage)) {
		debug("[otr] internal: %s\n", *messagep);
		g_free(*messagep);
		*messagep = NULL;
	} else if (newmessage) {
		g_free(*messagep);
		*messagep = g_strdup(newmessage);
		otrl_message_free(newmessage);
		*decryptedp = 1;
	}

	return 0;
}

static QUERY(message_encrypt_handler)
{
	char **sessionp = va_arg(ap, char**);
	char **recipient = va_arg(ap, char**);
	char **message = va_arg(ap, char**);
	int *encrypted = va_arg(ap, int*);

	char *newmessage = NULL;

	if (!sessionp || !*sessionp || !recipient || !*recipient || !message || !*message) {
		return -1;
	}

	if (otrl_proto_message_type(*message) != OTRL_MSGTYPE_NOTOTR) {
		/* This is an OTR message, which was injected by us
		 * (most likely), so we don't touch it. Is there any
		 * better logic we could use? What about when somebody
		 * actually types "?OTR" characters in their
		 * message, while in an OTR chat? */
		return 0;
	}


	OtrpSession *os = otrp_session_find(*sessionp);
	if (!os) {
		return -1;
	}

	if (OTRP_DEBUG(otrp_message_sending, os, *recipient, *message, &newmessage)) {
		g_free(*message);
		*message = g_strdup("[OTR send failure]");
		*encrypted = 1;
		return -1;
	} else if (newmessage) {
		g_free(*message);
		*encrypted = (otrl_proto_message_type(newmessage) != OTRL_MSGTYPE_TAGGEDPLAINTEXT);
		*message = g_strdup(newmessage);
		otrl_message_free(newmessage);
	}

	return 0;
}

COMMAND(cmd_status)
{
	OtrpFingerprint fingerprint;
	OtrpSession *os = otrp_session(session);
	g_assert(os);

	const char *fmtuser = format_user(session, target);

	ConnContext *context = otrp_context_uid(os, target, false);
	if (!context) {
		print("otr_status_plaintext", fmtuser);
		return 0;
	}

	switch (context->msgstate) {
	case OTRL_MSGSTATE_PLAINTEXT:
		print("otr_status_plaintext", fmtuser);
		return 0;
	case OTRL_MSGSTATE_FINISHED:
		print("otr_status_finished", fmtuser);
		return 0;
	case OTRL_MSGSTATE_ENCRYPTED:
		otrl_privkey_hash_to_human(fingerprint, context->active_fingerprint->fingerprint);
		if (otrl_context_is_fingerprint_trusted(context->active_fingerprint)) {
			print("otr_status_encrypted_trusted", fmtuser, fingerprint);
		} else {
			print("otr_status_encrypted_untrusted", fmtuser, fingerprint);
		}
		switch (otrp_smp_state(context)) {
		case OTRP_SMP_INITIATED:
			print("otr_status_smp_sent", fmtuser);
			break;
		case OTRP_SMP_RECEIVED:
			print("otr_status_smp_received", fmtuser);
			break;
		case OTRP_SMP_RESPONDED:
			print("otr_status_smp_responded", fmtuser);
			break;
		case OTRP_SMP_NONE: ; /* pass */
		}
		return 0;
	default:
		debug("[otr] msgstate=%d?\n", context->msgstate);
	}
	return 0;
}

COMMAND(cmd_init)
{
	OtrpSession *os = otrp_session(session);
	g_assert(os);

	ConnContext *context = otrp_context_uid(os, target, true);
	if (!context) {
		debug("[otr] cannot create context for %s\n", target);
		return -1;
	}

	printq("otr_requesting", format_user(session, target));
	if (OTRP_DEBUG(otrp_initiate, os, context)) {
		return -1;
	}
	return 0;
}

COMMAND(cmd_finish)
{
	OtrpSession *os = otrp_session(session);
	g_assert(os);

	ConnContext *context = otrp_context_uid(os, target, false);
	if (!context) {
		print("otr_no_session", format_user(session, target));
		return -1;
	}

	printq("otr_finishing", format_user(session, target));
	otrp_disconnect(os, context);
	return 0;
}

COMMAND(cmd_trust)
{
	OtrpSession *os = otrp_session(session);
	g_assert(os);

	ConnContext *context = otrp_context_uid(os, target, false);
	if (!context || context->msgstate != OTRL_MSGSTATE_ENCRYPTED) {
		print("otr_no_session", format_user(session, target));
		return -1;
	}

	OtrpFingerprint fingerprint;
	otrl_privkey_hash_to_human(fingerprint, context->active_fingerprint->fingerprint);

	gcry_error_t err;
	if ((err = otrp_debug(otrp_trust(context), "otrp_trust")) != OTRP_OK) {
		if (gpg_err_code(err) == OTRP_ERR_ALREADY_TRUSTED) {
			printq("otr_key_already_trusted", context->username, fingerprint);
			return 0;
		} else  {
			print("generic_error", gcry_strerror(err));
			return -1;
		}
	} 

	printq("otr_trusted_key", context->username, fingerprint);		
	return 0;
}

COMMAND(cmd_distrust)
{
	OtrpSession *os = otrp_session(session);
	g_assert(os);

	ConnContext *context = otrp_context_uid(os, target, false);
	if (!context || context->msgstate != OTRL_MSGSTATE_ENCRYPTED) {
		return -1;
	}

	OtrpFingerprint fingerprint;
	otrl_privkey_hash_to_human(fingerprint, context->active_fingerprint->fingerprint);

	gcry_error_t err;
	if ((err = otrp_debug(otrp_distrust(context), "otrp_distrust")) != OTRP_OK) {
		if (gpg_err_code(err) == OTRP_ERR_ALREADY_UNTRUSTED) {
			printq("otr_key_already_untrusted", context->username, fingerprint);
			return 0;
		} else  {
			print("generic_error", gcry_strerror(err));
			return -1;
		}
	} 

	printq("otr_untrusted_key", context->username, fingerprint);		
	return 0;
}

COMMAND(cmd_auth)
{
	if (!params || !params[0] || !target) {
		command_exec_params(target, session, 0, "/help", "otr:auth", NULL);
		return -1;
	}

	OtrpSession *os = otrp_session(session);
	g_assert(os);

	ConnContext *context = otrp_context_uid(os, target, false);
	if (!context || context->msgstate != OTRL_MSGSTATE_ENCRYPTED) {
		print("otr_no_session", format_user(session, target));
		return -1;
	}

	if (match_arg(params[0], 'a', "abort", 5)) {
		otrp_smp_abort(os, context);
	} else {
		otrp_smp(os, context, params[0], params[1]);
	}
	return 0;
}

static void privkey_show(session_t *session)
{
	OtrpSession *os = otrp_session(session);
	g_assert(os);

	OtrpFingerprint fingerprint;
	if (otrp_privkey_fingerprint(os, fingerprint)) {
		print("otr_privkey", session_format(session), fingerprint);
	} else {
		print("otr_privkey_none", session_format(session));
	}
}

COMMAND(cmd_key)
{
	session_t *s = session;
	const char *op = "-l";
	gcry_error_t err;

	if (!params || !params[0]) {
		/* Show current session key */
		privkey_show(s);
		return 0;
	}

	if (params[0][0] == '-') {
		op = params[0];
		params++;
	}

	if (params[0]) {
		if (!(s = session_find(params[0]))) {
			print("session_doesnt_exist", params[0]);
			return -1;
		}		
	}

	if (match_arg(op, 'l', "list", 4)) {
		if (!params[0]) {
			/* List all sessions' keys */
			for (s = sessions; s; s = s->next) {
				privkey_show(s);
			}
			return 0;
		}

		/* show session key for params[0] */
		privkey_show(s);
		return 0;
	}

	OtrpSession *os = otrp_session(s);
	g_assert(os);

	if (match_arg(op, 'g', "generate", 3)) {
		OtrpFingerprint fingerprint;

		if (otrp_privkey_fingerprint(os, fingerprint)) {
			print("otr_has_privkey", session_format(s), fingerprint);
			return -1;
		}
		printq("otr_privkey_generating", session_format(s));
		if (OTRP_DEBUG(otrp_privkey_generate, os)) {
			return -1;
		}
		privkey_show(s);
		return 0;
	}

	if (match_arg(op, 'f', "forget", 3)) {
		if ((err = otrp_privkey_forget(os)) != OTRP_OK) {
			if (gcry_err_code(err) == GPG_ERR_NO_SECKEY) {
				print("otr_no_privkey", session_format(s));
			} else {
				print("generic_error", gcry_strerror(err));
			}
			return -1;
		}
		print("otr_privkey_deleted", session_format(s));
		return 0;
	}

	command_exec_params(target, s, 0, "/help", "otr:key", NULL);
	return -1;
}

#define OTR_COMMAND_UID SESSION_MUSTBECONNECTED|COMMAND_ENABLEREQPARAMS|COMMAND_PARAMASTARGET|COMMAND_TARGET_VALID_UID

int otr_plugin_init(int prio)
{
	PLUGIN_CHECK_VER("otr");

	if (OTRP_DEBUG(otrp_init)) {
		return -1;
	}

	plugin_register(&otr_plugin, prio);

	command_add(&otr_plugin, "otr:status",		"!u",	cmd_status,	OTR_COMMAND_UID, NULL);
	command_add(&otr_plugin, "otr:init",		"!u",	cmd_init,	OTR_COMMAND_UID, NULL);
	command_add(&otr_plugin, "otr:finish",		"!u",	cmd_finish,	OTR_COMMAND_UID, NULL);
	command_add(&otr_plugin, "otr:trust",		"!u",	cmd_trust,	OTR_COMMAND_UID, NULL);
	command_add(&otr_plugin, "otr:distrust",	"!u",	cmd_distrust,	OTR_COMMAND_UID, NULL);
	command_add(&otr_plugin, "otr:auth",		"! ?",	cmd_auth,	COMMAND_TARGET_VALID_UID|SESSION_MUSTBECONNECTED, NULL);
	command_add(&otr_plugin, "otr:key",		"p s",	cmd_key,	0, "-l --list -g --generate -f --forget");

	query_connect(&otr_plugin, "message-decrypt", message_decrypt_handler, NULL);
	query_connect(&otr_plugin, "message-encrypt", message_encrypt_handler, NULL);

	debug("[otr] registered\n");

	return 0;
}

static int otr_plugin_destroy()
{
	plugin_unregister(&otr_plugin);

	otrp_deinit();
	
	debug("[otr] unregistered\n");

	return 0;
}


#define _tag		 "%y[%YOTR%y]%n"
#define _info		"%> " _tag " "
#define _err		"%! " _tag " "
#define _uid(n)		"%y%" n "%n"
#define _uid1		_uid("1")
#define _fingerprint(n)	"%c%U%" n "%n"
#define _fingerprint2	_fingerprint("2")
#define _cmd(cmd)	"%T/" cmd "%n"

#define _finish_or_init \
	"If you want to continue talking, use " _cmd("otr:finish") " for plaintext, or " _cmd("otr:init") " to restart.\n"

#define _using_trusted_key \
	", using %gtrusted%n key " _fingerprint2 ".\n"

#define _using_untrusted_key \
	", using %runtrusted%n key " _fingerprint2 ".\n" \
	"%! %|To make sure you are talking to the right person, please use the " _cmd("otr:auth SECRET [QUESTION]") " command to authenticate, or compare your fingerprints using a separate channel (e.g. over telephone or GPG-signed e-mail) and use the " _cmd("otr:trust") " command.\n"

#define _smp_update(state) \
	"Authentication with " _uid1 " is " state " (%2%%).\n"

static int otr_theme_init() {
#ifndef NO_DEFAULT_THEME
	format_add("otr_ev_CONNECTION_ENDED", 		_err	"Message has not been sent because our buddy has ended the private conversation.\n" _finish_or_init, 1);
	format_add("otr_ev_ENCRYPTION_ERROR",		_err	"An error occured while encrypting a message and the message was not sent.\n", 1);
	format_add("otr_ev_ENCRYPTION_REQUIRED", 	_err	"Our policy requires encryption but we are trying to send an unencrypted message out.\n", 1);
	format_add("otr_ev_MSG_REFLECTED", 		_err	"Received our own OTR messages.\n", 1);
	format_add("otr_ev_MSG_RESENT", 		_err	"The previous message was resent.\n", 1);
	format_add("otr_ev_RCVDMSG_GENERAL_ERR",	_err	"%1\n", 1);
	format_add("otr_ev_RCVDMSG_MALFORMED",		_err	"The message received contains malformed data.\n", 1);
	format_add("otr_ev_RCVDMSG_NOT_IN_PRIVATE",	_err	"Received an encrypted message but cannot read it because no private connection is established yet.\n" _finish_or_init, 1);
	format_add("otr_ev_RCVDMSG_UNREADABLE",		_err	"Cannot read the received message.\n", 1);
	format_add("otr_ev_SETUP_ERROR",		_err	"A private conversation could not be set up: %1\n", 1);
	format_add("otr_gone_secure_trusted",		_info	"Secure chat with " _uid1 " established" _using_trusted_key, 1);
	format_add("otr_gone_secure_untrusted",		_info	"Secure chat with " _uid1 " established" _using_untrusted_key, 1);
	format_add("otr_gone_insecure",			_info	"Secure chat with " _uid1 " lost.\n", 1);
	format_add("otr_still_secure_trusted",		_info	"Chat with " _uid1 " is still secure" _using_trusted_key, 1);
	format_add("otr_still_secure_untrusted",	_info	"Chat with " _uid1 " is still secure" _using_untrusted_key, 1);
	format_add("otr_new_fingerprint",		_info	"Received new fingerprint for " _uid1 ": " _fingerprint2 ".\n", 1);
	format_add("otr_key_already_trusted",		_err	"Key " _fingerprint2 " for " _uid1 " is %galready trusted%n.\n", 1);
	format_add("otr_key_already_untrusted",		_err	"Key " _fingerprint2 " for " _uid1 " is %ralready untrusted%n.\n", 1);
	format_add("otr_trusted_key",			_info	"Key " _fingerprint2 " for " _uid1 " is now %Gtrusted%n.\n", 1);
	format_add("otr_untrusted_key",			_info	"Key " _fingerprint2 " for " _uid1 " is now %Runtrusted%n.\n", 1);
	format_add("otr_requesting",			_info	"Requesting secure chat from %1\n", 1);
	format_add("otr_finishing",			_info	"Finishing secure chat with %1\n", 1);
	format_add("otr_invalid_uid",			_err	"Invalid UID: " _uid1 "\n", 1);
	format_add("otr_disconnected",			_info	"" _uid1 " has finished secure chat.\n" _finish_or_init, 1);
	format_add("otr_privkey",			_info	"Private key for %1: " _fingerprint2 "\n", 1);
	format_add("otr_privkey_none",			_info	"Private key for %1: %rNONE%n\n", 1);
	format_add("otr_has_privkey",			_err	"Session %1 already has key: " _fingerprint2 "\n", 1);
	format_add("otr_no_privkey",			_err	"Session %1 does not have a private key.\n", 1);
	format_add("otr_privkey_generating",		_info	"Generating private key for %1...\n", 1);
	format_add("otr_privkey_deleted",		_info	"Deleted private key for %1.\n", 1);
	format_add("otr_privkey_generate_error",	_err	"Could not generate private key for %1: %2\n", 1);
	format_add("otr_smp_ask_for_secret",		_info	"" _uid1 " wants you to authenticate. Provide the secret with the " _cmd("otr:auth \"SECRET\"") " command to complete.\n", 1);
	format_add("otr_smp_ask_for_answer",		_info	"" _uid1 " wants you to authenticate. Answer the question %c\"%U%2%n%c\"%n with the " _cmd("otr:auth \"ANSWER\"") " command to complete.\n", 1);
	format_add("otr_smp_in_progress",		_info	"" _smp_update("%Bin progress%n"), 1);
	format_add("otr_smp_success",			_info	"" _smp_update("%Gsuccessful%n"), 1);
	format_add("otr_smp_failure",			_info	"" _smp_update("%Rfailed%n"), 1);
	format_add("otr_smp_abort",			_info	"" _smp_update("%Raborted%n"), 1);
	format_add("otr_smp_already_in_progress",	_err	"Authentication with " _uid1 " is already in progress. Use " _cmd("otr:auth --abort") " to abort and start new authentication.\n", 1);
	format_add("otr_no_session",			_err	"No secure chat with %1\n", 1);
	format_add("otr_requesting_auth",		_info	"Requesting authentication from " _uid1 ".\n", 1);
	format_add("otr_requesting_auth_q",		_info	"Requesting authentication from " _uid1 ", asking question %c\"%U%2%n%c\"%n.\n", 1);
	format_add("otr_aborting_auth",			_info	"Aborting authentication with " _uid1 ".\n", 1);
	format_add("otr_status_plaintext",		_info	"Conversation with %1 is %rnot encrypted%n. Use " _cmd("otr:init") " to initiate encrypted conversation.\n", 1);
	format_add("otr_status_finished",		_info	"Encrypted conversation with %1 is %Rfinished%n. No further messages will be sent.\n" _finish_or_init, 1);
	format_add("otr_status_encrypted_trusted",	_info	"Conversation with %1 is %gencrypted%n" _using_trusted_key, 1);
	format_add("otr_status_encrypted_untrusted",	_info	"Conversation with %1 is %gencrypted%n" _using_untrusted_key, 1);
	format_add("otr_status_smp_sent",		_info	"Authentication request has been sent, awaiting response.\n", 1);
	format_add("otr_status_smp_received",		_info	"Authentication request has been received, respond with " _cmd("otr:auth \"SECRET\"") " .\n", 1);
	format_add("otr_status_smp_responded",		_info	"Authentication request has been received and response is being verified.\n", 1);
#endif
	return 0;
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
