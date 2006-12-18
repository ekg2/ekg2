/*
 *  Code ported from mcabber 0.9.0 to ekg2
 *  	Copyright (C) 2006 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *
 *  Orginal code:
 *  	Copyright (C) 2006 Mikael Berthe <bmikael@lists.lilotux.net>
 *  	Some parts inspired by centericq (impgp.cc)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2-config.h"

#include <stdlib.h>
#include <unistd.h>

#include <ekg/debug.h>
#include <ekg/plugins.h>

#include <ekg/sessions.h>
#include <ekg/queries.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include <gpgme.h>

static int gpg_theme_init();
PLUGIN_DEFINE(gpg, PLUGIN_CRYPT, gpg_theme_init);

typedef struct {
	char *uid;
	char *keyid;
	char *status;
	char *password;
	int keysetup;
} egpg_key_t;

static list_t gpg_keydb;

/* XXX, multiresource. */

static egpg_key_t *gpg_keydb_add(const char *uid, const char *keyid, const char *fpr) {
	egpg_key_t *a = xmalloc(sizeof(egpg_key_t));

	a->uid		= xstrdup(uid);
	a->keyid 	= xstrdup(keyid);
/*	a->password	= NULL; */
/*	a->keysetup	= 0; */
	
	list_add(&gpg_keydb, a, 0);

	return a;
}

static egpg_key_t *gpg_keydb_find_uid(const char *uid) {
	list_t l;
	for (l = gpg_keydb; l; l = l->next) {
		egpg_key_t *k = l->data;

		if (!xstrcmp(k->uid, uid))
			return k;
	}

	return NULL;
}

static gpgme_error_t gpg_passphrase_cb(void *data, const char *uid_hint, const char *passphrase_info, int prev_was_bad, int fd) {
	size_t len;

	if (!data) {	/* no password ! */
		write(fd, "\n", 1);
		return gpg_error(GPG_ERR_CANCELED);
	}

	len = xstrlen((char *) data);

	if (write(fd, (char *) data, len) != len)	return gpg_error(GPG_ERR_CANCELED);
	if (write(fd, "\n", 1) != 1) 			return gpg_error(GPG_ERR_CANCELED);

	return 0;	/* success */
}

static const char *gpg_find_keyid(const char *uid, const char **password, char **error) {
	const char *key	= NULL;
	session_t *s;

	*password = NULL;

	if ((s = session_find(uid))) {				/* if we have that session */
		key = session_get(s, "gpg_key");			/* get value from session */
		*password = session_get(s, "gpg_password");		/* get password from session too */
	}
	if (!key) {				/* if we still have no key... than try our keydatabase */
		egpg_key_t *k = gpg_keydb_find_uid(uid);
		if (k) {
			key = k->uid;
			*password = k->password;
		}
	}
	if (!key) key = uid;					/* otherwise use uid */

	if (!key) {
		*error = saprintf("GPG INTERNAL ERROR: @ [%s:%d] key == NULL", __FILE__, __LINE__);
		return NULL;
	}

/* XXX here, we need to search gpg key db for search of key = 'key' coz we can have here everything... uid, name, etc, etc...
 * 	and if we have multiiple choices we should allow user to select proper one. 
 */

	return key;
}

#define GPGME_GENERROR(x) saprintf(x": %s", gpgme_strerror(err));

static QUERY(gpg_message_encrypt) {
	char *uid	= *(va_arg(ap, char **));		/* uid */
	char **message	= va_arg(ap, char **);			/* message to encrypt */
	char **error 	= va_arg(ap, char **);			/* place to put errormsg */

	const char *key  = NULL;
	const char *pass = NULL;

	char *gpg_data	= *message;

	*error = NULL;

	if (!(key = gpg_find_keyid(uid, &pass, error))) 
		return 1;

	if (!pass) {
		*error = saprintf("GPG: NO PASSPHRASE FOR KEY: %s SET PASSWORD AND TRY AGAIN (/sesion -s gpg_password \"[PASSWORD]\")\n", key);
		/* XXX, here if we don't have password. Allow user to type it. XXX */
		return 1;
	}

	*error = NULL;

	do {
		gpgme_ctx_t ctx;
		gpgme_data_t in, out;
		size_t nread;
		gpgme_key_t gpg_key;
		gpgme_error_t err;

		if ((err = gpgme_new(&ctx))) { *error = GPGME_GENERROR("GPGME error"); break; }

		gpgme_set_protocol(ctx, GPGME_PROTOCOL_OpenPGP);
		gpgme_set_textmode(ctx, 0);
		gpgme_set_armor(ctx, 1);

		err = gpgme_get_key(ctx, key, &gpg_key, 0);
		if (!err && gpg_key) {
			gpgme_key_t keys[] = { gpg_key, 0 };
			err = gpgme_data_new_from_mem(&in, gpg_data, xstrlen(gpg_data), 0);
			if (!err) {
				err = gpgme_data_new(&out);
				if (!err) {
					err = gpgme_op_encrypt(ctx, keys, GPGME_ENCRYPT_ALWAYS_TRUST, in, out);
					if (!err) {
						char *encrypted_data = gpgme_data_release_and_get_mem(out, &nread);
						xfree(*message);
						*message = xstrndup(encrypted_data, nread);
						xfree(encrypted_data);
					} else
						gpgme_data_release(out);
				}
				gpgme_data_release(in);
			}
			gpgme_key_release(gpg_key);
		} else {
			*error = saprintf("GPGME encryption error: key not found");
		}
		if (!*error && err /* && err != GPG_ERR_CANCELED */)
			*error = GPGME_GENERROR("GPGME encryption error");

		gpgme_release(ctx);
	} while(0);

	if (*error) return 1;
	return 0;
}

static QUERY(gpg_message_decrypt) {
	char *uid	= *(va_arg(ap, char **));		/* uid */
	char **message	= va_arg(ap, char **);			/* message to decrypt */
	char **error 	= va_arg(ap, char **);			/* place to put errormsg */

	char *gpg_data	= saprintf(data, *message);

	*error = NULL;

	do {
		gpgme_ctx_t ctx;
		gpgme_error_t err;
		gpgme_data_t in, out;
		char *p;

		if ((err = gpgme_new(&ctx))) { *error = GPGME_GENERROR("GPGME error"); break; }

		gpgme_set_protocol(ctx, GPGME_PROTOCOL_OpenPGP);

		p = getenv("GPG_AGENT_INFO");
		if (!(p && xstrchr(p, ':')))
			gpgme_set_passphrase_cb(ctx, gpg_passphrase_cb, 0);

		err = gpgme_data_new_from_mem(&in, gpg_data, xstrlen(gpg_data), 0);
		if (!err) {
			err = gpgme_data_new(&out);
			if (!err) {
				err = gpgme_op_decrypt(ctx, in, out);
				if (!err) {
					size_t nread;
					char *decrypted_data = gpgme_data_release_and_get_mem(out, &nread);

					xfree(*message);
					*message = xstrndup(decrypted_data, nread);
					xfree(decrypted_data);
				} else {
					gpgme_data_release(out);
				}
			}
			gpgme_data_release(in);
		}
/*		if (err && err != GPG_ERR_CANCELED) */
		if (err) 
			*error = GPGME_GENERROR("GPGME decryption error");

		gpgme_release(ctx);
	} while (0);
	xfree(gpg_data);

	if (*error) return 1;
	return 0;
}

static QUERY(gpg_sign) {
	char *uid	= *(va_arg(ap, char **));		/* uid */
	char **message	= va_arg(ap, char **);			/* message to sign */
	char **error 	= va_arg(ap, char **);			/* place to put errormsg */

	const char *key  = NULL;
	const char *pass = NULL;

	char *gpg_data	= *message;

	*error = NULL;

	if (!(key = gpg_find_keyid(uid, &pass, error))) 
		return 1;

	if (!pass) {
		*error = saprintf("GPG: NO PASSPHRASE FOR KEY: %s SET PASSWORD AND TRY AGAIN (/sesion -s gpg_password \"[PASSWORD]\")\n", key);
		/* XXX, here if we don't have password. Allow user to type it. XXX */
		return 1;
	}

	do {
		gpgme_error_t err;
		gpgme_ctx_t ctx;
		gpgme_key_t gpg_key;
		gpgme_data_t in, out;
		char *p;

		if ((err = gpgme_new(&ctx))) { *error = GPGME_GENERROR("GPGME error"); break; }

		gpgme_set_protocol(ctx, GPGME_PROTOCOL_OpenPGP);
		gpgme_set_textmode(ctx, 0);
		gpgme_set_armor(ctx, 1);
		
		p = getenv("GPG_AGENT_INFO");
		if (!(p && xstrchr(p, ':')))
			gpgme_set_passphrase_cb(ctx, gpg_passphrase_cb, (void *) pass);	/* last param -> data, .. in callback 1st param */
		
		if ((err = gpgme_get_key(ctx, key, &gpg_key, 1)) || !gpg_key) {
			*error = saprintf("GPGME error: private key not found");
			gpgme_release(ctx);
			break;
		}
		
		gpgme_signers_clear(ctx);
		gpgme_signers_add(ctx, gpg_key);
		gpgme_key_release(gpg_key);
		err = gpgme_data_new_from_mem(&in, gpg_data, xstrlen(gpg_data), 0);
		if (!err) {
			err = gpgme_data_new(&out);
			if (!err) {
				err = gpgme_op_sign(ctx, in, out, GPGME_SIG_MODE_DETACH);
				if (!err) {
					char *signed_data;
					size_t nread;

					xfree(*message);
					signed_data = gpgme_data_release_and_get_mem(out, &nread);

					*message = xstrndup(signed_data, nread);
					xfree(signed_data);
				} else {
					gpgme_data_release(out);
				}
			}
			gpgme_data_release(in);
		}
/*		if (err && err != GPG_ERR_CANCELED) */
		if (err) 
			*error = GPGME_GENERROR("GPGME signature error");

		gpgme_release(ctx);
	} while(0);

	if (*error) return 1;
	return 0;
}

static QUERY(gpg_verify) {
	char *uid	= *(va_arg(ap, char **));		/* uid */
	char *message	= *(va_arg(ap, char **));		/* message to verify WITHOUT HEADER! */
	char **keydata	= va_arg(ap, char **);			/* key data, after key-id  */
	char **error	= va_arg(ap, char **);			/* key verification status */

	char *gpg_data	= saprintf(data, *keydata);

	*error = NULL;
	
	do {
		gpgme_ctx_t ctx;
		gpgme_error_t err;
		gpgme_key_t key;

		gpgme_data_t data_sign, data_text;

		if ((err = gpgme_new(&ctx))) { *error = GPGME_GENERROR("GPGME error"); break; }
		
		gpgme_set_protocol(ctx, GPGME_PROTOCOL_OpenPGP);

		err = gpgme_data_new_from_mem(&data_sign, gpg_data, xstrlen(gpg_data), 0);
	/* too much if... too much... use goto? XXX */
		if (!err) {
			err = gpgme_data_new_from_mem(&data_text, message, xstrlen(message), 0);
			if (!err) {
				err = gpgme_op_verify(ctx, data_sign, data_text, 0);
				if (!err) {
					gpgme_verify_result_t vr = gpgme_op_verify_result(ctx);

					if (vr && vr->signatures) {
						char *fpr	= vr->signatures->fpr;
						char *keyid	= NULL;
						egpg_key_t *k;

						/* FINGERPRINT -> KEY_ID */
						if (!gpgme_get_key(ctx, fpr, &key, 0) && key) {
							keyid = key->subkeys->keyid;
							gpgme_key_release(key);
						}

						xfree(*keydata);
						*keydata = xstrdup(keyid);

						if ((k = gpg_keydb_find_uid(uid))) {
							xfree(k->status);
							if (xstrcmp(k->keyid, keyid)) {
								if (k->keysetup)
									k->status = xstrdup("Warning: The KeyId doesn't match the key you set up");
								else {
									xfree(k->keyid);
									k->keyid  = xstrdup(keyid);
									k->status = NULL;
								}
							} else		k->status = NULL;
						} else k = gpg_keydb_add(uid, keyid, fpr);

						if (!vr->signatures->summary && !vr->signatures->status) /* summary = 0, status = 0 -> signature valid */
							*error	= xstrdup("Signature ok");
						else if (vr->signatures->summary & GPGME_SIGSUM_RED)
							*error	= xstrdup("Signature bad");
						else if (vr->signatures->summary & GPGME_SIGSUM_GREEN)
							*error	= xstrdup("Signature ok");
						else	*error	= xstrdup("Signature ?!?!");

						if (!k->status) k->status = xstrdup(*error);

					} else {
						xfree(*keydata);
						*keydata = NULL;
					}
				}
				gpgme_data_release(data_text);
			}
			gpgme_data_release(data_sign);
		}
		if (err)
			*error = GPGME_GENERROR("GPGME verification error");
		gpgme_release(ctx);

	} while(0);
	xfree(gpg_data);

	if (*error) return 1;
	return 0;
}

static QUERY(gpg_user_keyinfo) {
/* HERE, we display info about gpg support for user 'u' 
 * 	query emited by /list 
 */
	userlist_t *u	= *va_arg(ap, userlist_t **);
	int quiet	= *va_arg(ap, int *);

	egpg_key_t *k;
	
	if (!u)
		return 0;

	if (xstrncmp(u->uid, "jid:", 4)) return 0; /* only jabber for now... */

	if ((k = gpg_keydb_find_uid(u->uid))) {
		printq("user_info_gpg_key", k->keyid, k->status);
	}

	return 0;
}

#define MIN_GPGME_VERSION "1.0.0"

int gpg_plugin_init(int prio) {
	gpgme_error_t err;

	if (!gpgme_check_version(MIN_GPGME_VERSION)) {
		debug_error("GPGME initialization error: Bad library version");
		return -1;
	}

	if ((err = gpgme_engine_check_version(GPGME_PROTOCOL_OpenPGP))) {
		debug_error("GPGME initialization error: %s", gpgme_strerror(err));
		return -1;
	}

#if 0
/* XXX Set the locale information. ? */
	gpgme_set_locale(NULL, LC_CTYPE, setlocale(LC_CTYPE, NULL));
	gpgme_set_locale(NULL, LC_MESSAGES, setlocale(LC_MESSAGES, NULL));
#endif

	plugin_register(&gpg_plugin, prio);

	query_connect_id(&gpg_plugin, GPG_MESSAGE_ENCRYPT, 	gpg_message_encrypt, NULL);
	query_connect_id(&gpg_plugin, GPG_MESSAGE_DECRYPT, 	gpg_message_decrypt, 
						"-----BEGIN PGP MESSAGE-----\n\n"
						"%s\n"
						"-----END PGP MESSAGE-----\n");

	query_connect_id(&gpg_plugin, GPG_SIGN,			gpg_sign, NULL);
	query_connect_id(&gpg_plugin, GPG_VERIFY,		gpg_verify, 
						"-----BEGIN PGP SIGNATURE-----\n\n"
						"%s\n"
						"-----END PGP SIGNATURE-----\n");

	query_connect_id(&gpg_plugin, USERLIST_INFO,		gpg_user_keyinfo, NULL);

	return 0;
}

static int gpg_theme_init() {
#ifndef NO_DEFAULT_THEME
	format_add("user_info_gpg_key", 	_("%K| %nGPGKEY: %T%1%n (%2) %n"), 1);	/* keyid, status */
#endif
	return 0;
}

static int gpg_plugin_destroy() {
	plugin_unregister(&gpg_plugin);
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
