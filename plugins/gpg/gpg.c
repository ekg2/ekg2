/*
 *  Code ported from mcabber 0.9.0 to ekg2
 *	Copyright (C) 2006 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *
 *  Orginal code:
 *	Copyright (C) 2006 Mikael Berthe <bmikael@lists.lilotux.net>
 *	Some parts inspired by centericq (impgp.cc)
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
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/sessions.h>
#include <ekg/queries.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include <ekg/stuff.h>

#include <gpgme.h>

static int gpg_theme_init();
PLUGIN_DEFINE(gpg, PLUGIN_CRYPT, gpg_theme_init);

/* XXX, enums */
typedef struct {
	char *uid;
	char *keyid;
	char *password;
	int keysetup;		/*  0 - autoadded; 
				    1 - added by user; 
				    2 - forced by user 
				*/
	int keynotok;		/* -1 - keystatus unknown. 
				    0 - key ok;	
				    1 - key ver failed; 
				    2 - key mishmashed 
				 */
} egpg_key_t;

static list_t gpg_keydb;

/* XXX, multiresource. */

static egpg_key_t *gpg_keydb_add(const char *uid, const char *keyid, const char *fpr) {
	egpg_key_t *a = xmalloc(sizeof(egpg_key_t));

	a->uid		= xstrdup(uid);
	a->keyid	= xstrdup(keyid);
	a->keynotok	= -1;
	
	list_add(&gpg_keydb, a);

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
	if (write(fd, "\n", 1) != 1)			return gpg_error(GPG_ERR_CANCELED);

	return 0;	/* success */
}

static const char *gpg_find_keyid(const char *uid, const char **password, char **error) {
	const char *key	= NULL;
	session_t *s;

	if (password) *password = NULL;

	if ((s = session_find(uid))) {				/* if we have that session */
		key = session_get(s, "gpg_key");			/* get value from session */
									/* get password from session too */
		if (password) *password = session_get(s, "gpg_password");	
	}
	if (!key) {				/* if we still have no key... than try our keydatabase */
		egpg_key_t *k = gpg_keydb_find_uid(uid);
		if (k) {
			key = k->uid;
			if (password) *password = k->password;
		}
	}
	if (!key) key = uid;					/* otherwise use uid */

	if (!key) {
		*error = saprintf("GPG INTERNAL ERROR: @ [%s:%d] key == NULL", __FILE__, __LINE__);
		return NULL;
	}

/* XXX here, we need to search gpg key db for search of key = 'key' coz we can have here everything... uid, name, etc, etc...
 *	and if we have multiiple choices we should allow user to select proper one. 
 */

	return key;
}

#define GPGME_GENERROR(x) saprintf(x": %s", gpgme_strerror(err));

static QUERY(gpg_message_encrypt) {
	char *uid	= *(va_arg(ap, char **));		/* uid */
	char **message	= va_arg(ap, char **);			/* message to encrypt */
	char **error	= va_arg(ap, char **);			/* place to put errormsg */

	char *gpg_data	= *message;

	egpg_key_t *key;

	*error = NULL;

	if (!(key = gpg_keydb_find_uid(uid))) {
		*error = saprintf("GPG KEY FOR USER: %s NOT FOUND. TRY /gpg:key --setkey\n", uid);
		return 1;
	}

	if (key->keynotok) {
		if (key->keysetup != 2) {
			if (key->keynotok == -1)*error = xstrdup("Message not encrypted cause key verification status unknown");
			if (key->keynotok == 1) *error = xstrdup("Message not encrypted cause key failed verification");
			if (key->keynotok == 2) *error = xstrdup("Message not encrypted cause key mishmash, if you really want encrypt messages use: /gpg:key --forcekey");
			return 1;
		}
		/* key forced */
		debug_error("gpg_message_encrypt() USER FORCE KEY!!!!\n");
	}

	if (key->keysetup == 0 && 1 /* XXX, zmienna */) {
		*error = xstrdup("Message not encrypted, key is ok, but it was set up automagicly... you must [turn on global encryption with /set gpg:smth 1 (XXX) or] use /gpg:key --setkey");
		return 1;
	}

	do {
		gpgme_ctx_t ctx;
		gpgme_data_t in, out;
		gpgme_key_t gpg_key;
		gpgme_error_t err;

		if ((err = gpgme_new(&ctx))) { *error = GPGME_GENERROR("GPGME error"); break; }

		gpgme_set_protocol(ctx, GPGME_PROTOCOL_OpenPGP);
		gpgme_set_textmode(ctx, 0);
		gpgme_set_armor(ctx, 1);

		err = gpgme_get_key(ctx, key->keyid, &gpg_key, 0);
		if (!err && gpg_key) {
			gpgme_key_t keys[] = { gpg_key, 0 };
			err = gpgme_data_new_from_mem(&in, gpg_data, xstrlen(gpg_data), 0);
			if (!err) {
				err = gpgme_data_new(&out);
				if (!err) {
					err = gpgme_op_encrypt(ctx, keys, GPGME_ENCRYPT_ALWAYS_TRUST, in, out);
					if (!err) {
						size_t nread;
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
	char **error	= va_arg(ap, char **);			/* place to put errormsg */

	char *gpg_data	= saprintf(data, *message);
	const char *key  = NULL;
	const char *pass = NULL;

	*error = NULL;

	if (!(key = gpg_find_keyid(uid, &pass, error))) 
		return 1;

	if (!pass) {
		*error = saprintf("GPG: NO PASSPHRASE FOR KEY: %s SET PASSWORD AND TRY AGAIN (/sesion -s gpg_password \"[PASSWORD]\")\n", key);
		/* XXX, here if we don't have password. Allow user to type it. XXX */
		return 1;
	}

	do {
		gpgme_ctx_t ctx;
		gpgme_error_t err;
		gpgme_data_t in, out;
		char *p;

		if ((err = gpgme_new(&ctx))) { *error = GPGME_GENERROR("GPGME error"); break; }

		gpgme_set_protocol(ctx, GPGME_PROTOCOL_OpenPGP);

		p = getenv("GPG_AGENT_INFO");
		if (!(p && xstrchr(p, ':')))
			gpgme_set_passphrase_cb(ctx, gpg_passphrase_cb, (void *) pass);

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
	char **error	= va_arg(ap, char **);			/* place to put errormsg */

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
						int keynotok	= -1;
						gpgme_key_t key;
						egpg_key_t *k;

						/* FINGERPRINT -> KEY_ID */
						if (!gpgme_get_key(ctx, fpr, &key, 0) && key) {
							keyid = xstrdup(key->subkeys->keyid);
							gpgme_key_release(key);
						}

						if (!vr->signatures->summary && !vr->signatures->status) { /* summary = 0, status = 0 -> signature valid */
							*error	= xstrdup("Signature ok");
							keynotok = 0;				/* ok */
						} else if (vr->signatures->summary & GPGME_SIGSUM_RED) {
							*error	= xstrdup("Signature bad");
							keynotok = 1;				/* bad */
						} else if (vr->signatures->summary & GPGME_SIGSUM_GREEN) {
							*error	= xstrdup("Signature ok");
							keynotok = 0;				/* ok */
						} else	{ 
							*error	= xstrdup("Signature ?!?!");
							keynotok = -1;				/* bad, unknown */
						}

						if ((k = gpg_keydb_find_uid(uid))) {
							if (xstrcmp(k->keyid, keyid)) {
								if (k->keysetup == 0) {		/* if we don't setup our key... than replace it. */
									xfree(k->keyid);
									k->keyid  = xstrdup(keyid);
								} else	debug_error("[gpg] uid: %s is really using key: %s in our db: %s\n", uid, keyid, k->keyid);
								if (k->keysetup)	k->keynotok = 2;			/* key mishmash (if we set it up manually. */
								else			k->keynotok = keynotok;
							} else	
								k->keynotok = keynotok;
						} else {
							k = gpg_keydb_add(uid, keyid, fpr);
							k->keynotok	= keynotok;
						}

						xfree(*keydata);
						*keydata = keyid;
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

static char *gpg_key_status(egpg_key_t *k) {
	static char buf[123];

	if (!k) return NULL;	/* X */

	buf[0] = 0;

	if (k->keynotok == -1)		xstrcat(&buf[0], "Warning: Signature unknown status");
	if (k->keynotok == 0)		xstrcat(&buf[0], "Signature ok");
	if (k->keynotok == 1)		xstrcat(&buf[0], "Warning: Signature bad.");
	if (k->keynotok == 2)		xstrcat(&buf[0], "Warning: The KeyId doesn't match the key you set up.");

	if (k->keysetup == 2)
		xstrcat(&buf[0], " [ENCRPYTION FORCED]");

	if (k->keysetup == 1 && k->keynotok == 0)
		xstrcat(&buf[0], " [ENCRYPTED]");

	if (k->keysetup == 1 && k->keynotok != 0)
		xstrcat(&buf[0], " [NOTENCRYPTED]");

	if (k->keysetup == 0) 
		xstrcat(&buf[0], " [NOTENCRYPTED, NOTTRUSTED]");

	if (k->keysetup == 0 && k->keynotok == 0)
		xstrcat(&buf[0], " [If you trust that key use /gpg:key -s]");

	return &buf[0];
}

static QUERY(gpg_user_keyinfo) {
/* HERE, we display info about gpg support for user 'u' 
 *	query emited by /list 
 */
	userlist_t *u	= *va_arg(ap, userlist_t **);
	int quiet	= *va_arg(ap, int *);

	egpg_key_t *k;
	
	if (!u)
		return 0;

	if (xstrncmp(u->uid, "xmpp:", 5)) return 0; /* only jabber for now... */

	if ((k = gpg_keydb_find_uid(u->uid))) {
		printq("user_info_gpg_key", k->keyid, gpg_key_status(k));
	}

	return 0;
}

static COMMAND(gpg_command_key) {
	int fkey = 0;

	if ((!params[0]) || match_arg(params[0], 'l', "listkeys", 2)) {		/* DISPLAY SUMMARY OF ALL KEYS */
		list_t l;
		for (l = gpg_keydb; l; l = l->next) {
			egpg_key_t *k = l->data;

			printq("gpg_keys_list", k->uid, k->keyid, gpg_key_status(k));
		}

		return 0;
	}
#if 0
	if ((params[0] && !params[1]) || match_arg(params[0], 'i', "infokey", 2)) {		/* DISPLAY KEY INFO */
		
		return 0;
	}
#endif
	if ((fkey = match_arg(params[0], 'f', "forcekey", 2)) || match_arg(params[0], 's', "setkey", 2)) {
		egpg_key_t *k;
		if (!params[1] || !params[2]) {
			printq("not_enough_params", name);
			return -1;
		}

		if ((k = gpg_keydb_find_uid(params[1]))) {	/* szukaj klucza */
			if (xstrcmp(k->keyid, params[2])) {		/* jesli mamy usera w bazie i klucze mishmashuja */
/* XXX, keep user valid key? and check here if match? XXX */

				if (k->keynotok != 2 && k->keynotok != -1) {			/* XXX ? keynotok != 0 */
					printq(fkey ? "gpg_key_set_okfbutmish" : "gpg_key_set_okbutmish", k->uid, params[2]);
					k->keynotok = 2;
				} else {
					printq(fkey ? "gpg_key_set_okfbutunk" : "gpg_key_set_okbutunk", k->uid, params[2]);
					k->keynotok = -1;					/* unknown status of key */
				}

			/* replace keyid */
				xfree(k->keyid);
				k->keyid = xstrdup(params[2]);
			} else {
				if (fkey)  /* forced */
					printq(
						k->keynotok == 0 ? "gpg_key_set_okf" :
						k->keynotok == 1 ? "gpg_key_set_okfbutver" :
						k->keynotok == 2 ? "gpg_key_set_okfbutmish":
								   "gpg_key_set_okfbutunk",
							k->uid, k->keyid);
				else	printq(
						k->keynotok == 0 ? "gpg_key_set_ok" :
						k->keynotok == 1 ? "gpg_key_set_okbutver" :
						k->keynotok == 2 ? "gpg_key_set_okbutmish":
								   "gpg_key_set_okbutunk",
							k->uid, k->keyid);
			}
		} else {
			k = gpg_keydb_add(params[1], params[2], NULL);
			printq(fkey ? "gpg_key_set_newf" : "gpg_key_set_new", params[1], params[2]);
		}

		if (fkey)
			k->keysetup = 2;	/* forced */
		else	k->keysetup = 1;	/* normal */

		return 0;
	}

	if (match_arg(params[0], 'd', "delkey", 2)) {
		egpg_key_t *k;
		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!(k = gpg_keydb_find_uid(params[1]))) { 
			printq("gpg_key_not_found", params[1]);
			return -1;
		}

		k->keysetup = 0;
		k->keynotok = -1;

		printq("gpg_key_unset", params[1]);

		return 0;
	}
	
	printq("invalid_params", name);
	return -1;
}

static int gpg_theme_init() {
#ifndef NO_DEFAULT_THEME
	format_add("gpg_key_unset",	_("%) GPGKEY for uid: %W%1%n UNSET!"), 1);
	format_add("gpg_key_not_found", _("%> GPGKEY for uid: %W%1%n NOT FOUND!"), 1);

	format_add("gpg_key_set_new",	_("%) You've set up new key for uid: %W%1%n keyid: %W%2%n\n"
					"%) Encryption will be disabled until you force key (gpg:key --forcekey) NOT RECOMENDED or we verify key (signed presence is enough)"), 1);
	format_add("gpg_key_set_newf",	_("%) You've forced setting new key for uid: %W%1%n keyid: %W%2%n\n"
					"%! Forcing key is not good idea... Please rather use /gpg:key --setkey coz key will be verified before encryption..."), 1);

	format_add("gpg_key_set_ok",		_("%> Keys you've set up for uid: %W%1%n match with our internal DB. Happy encrypted talk. F**k echelon"), 1);
	format_add("gpg_key_set_okf",		_("%> Keys you've set up for uid: %W%1%n match with our internal DB. Happy encrypted talk. F**k echelon (Forcing key is not nessesary here!)"), 1);
	format_add("gpg_key_set_okbutver",	_("%! Keys matched, but lasttime we fail to verify key. Encryption won't work until forced."), 1);
	format_add("gpg_key_set_okfbutver",	_("%! Keys matched, but lasttime we fail to verify key. Encryption forced."), 1);
	format_add("gpg_key_set_okbutmish",	_("%! Keys mishmash. Encryption won't work until forced or user change his keyid."), 1);
	format_add("gpg_key_set_okfbutmish",	_("%! Keys mishmash. Encryption forced."), 1);
	format_add("gpg_key_set_okbutunk",	_("%! We didn't verify this key, if you're sure it's ok force key (gpg:key --forcekey) however it's NOT RECOMENDED.. or wait until we verify key"), 1);
	format_add("gpg_key_set_okfbutunk",	_("%! We didn't verify this key, You've forced encryption. NOT RECOMENDED."), 1);

	format_add("gpg_keys_list",		"%> %W%1%n/%W%2%n %3", 1);		/* uid, keyid, key status */

	format_add("user_info_gpg_key",		_("%K| %nGPGKEY: %T%1%n (%2)%n"), 1);	/* keyid, key status */
#endif
	return 0;
}

#define MIN_GPGME_VERSION "1.0.0"

EXPORT int gpg_plugin_init(int prio) {
	FILE *f;
	gpgme_error_t err;
	const char *dbfile = prepare_pathf("keys/gpgkeydb.txt");

	PLUGIN_CHECK_VER("gpg");

	if (mkdir_recursive(dbfile, 0)) {
		debug_error("Creating of directory keys failed, gpg plugin needs it!\n");	/* it's not 100% true.. but... */
		return -1;
	}

	if (!gpgme_check_version(MIN_GPGME_VERSION)) {
		debug_error("GPGME initialization error: Bad library version");
		return -1;
	}

	if ((err = gpgme_engine_check_version(GPGME_PROTOCOL_OpenPGP))) {
		debug_error("GPGME initialization error: %s", gpgme_strerror(err));
		return -1;
	}

	if ((f = fopen(dbfile, "r"))) {
		char *line;
		while ((line = read_file(f, 0))) {
			char **p = array_make(line, "\t", 3, 0, 0);

			if (p && p[0] && p[1] && p[2]) {
				egpg_key_t *k = gpg_keydb_add(p[0], p[1], NULL);

				k->keysetup = atoi(p[2]);
			} else debug_error("[GPG] INVALID LINE: %s\n", line);

			array_free(p);
		}
		fclose(f);
	} else debug_error("[GPG] Opening of %s failed: %d %s.\n", dbfile, errno, strerror(errno));

#if 0
/* XXX Set the locale information. ? */
	gpgme_set_locale(NULL, LC_CTYPE, setlocale(LC_CTYPE, NULL));
	gpgme_set_locale(NULL, LC_MESSAGES, setlocale(LC_MESSAGES, NULL));
#endif

	plugin_register(&gpg_plugin, prio);

	command_add(&gpg_plugin, "gpg:key", "p u ?", gpg_command_key, 0, 
		"-d --delkey -f --forcekey -i --infokey -l --listkeys -s --setkey");

	query_connect_id(&gpg_plugin, GPG_MESSAGE_ENCRYPT,	gpg_message_encrypt, NULL);
	query_connect_id(&gpg_plugin, GPG_MESSAGE_DECRYPT,	gpg_message_decrypt, 
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

static int gpg_plugin_destroy() {
	FILE *f = NULL;
	list_t l;
	const char *dbfile = prepare_pathf("keys/gpgkeydb.txt");

	if (mkdir_recursive(dbfile, 0) || !(f = fopen(dbfile, "w"))) {
		debug_error("[GPG] gpg db failed to save (%s)\n", strerror(errno));
	}

/* save our db to file, and cleanup memory... */
	for (l = gpg_keydb; l; l = l->next) {
		egpg_key_t *k = l->data;

		if (f) fprintf(f, "%s\t%s\t%d\n", k->uid, k->keyid, k->keysetup);

		xfree(k->uid);
		xfree(k->keyid);
		xfree(k->password);	/* WE DON'T SAVE PASSWORD ? XXX */
	}
	list_destroy(gpg_keydb, 1);
	gpg_keydb = NULL;

	if (f) fclose(f);

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
