#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/vars.h>
#include <ekg/userlist.h>
#include <ekg/sessions.h>
#include <ekg/xmalloc.h>

#include <ekg/stuff.h>
#include <ekg/themes.h>

#include "simlite.h"

static int config_encryption = 0;
static int sim_theme_init();

PLUGIN_DEFINE(sim, PLUGIN_CRYPT, sim_theme_init);

static QUERY(message_encrypt)
{
	char **session, **recipient, **message, *result;
	int *encrypted;

	session = va_arg(ap, char**);
	recipient = va_arg(ap, char**);
	message = va_arg(ap, char**);
	encrypted = va_arg(ap, int*);

	if (!session || !message || !encrypted)
		return 0;

	debug("[sim] message-encrypt: %s -> %s\n", *session, *recipient);

	if (!config_encryption)
		return 0;

	if (!*session || !*recipient)
		return 0;

	if (!xstrncmp(*message, "-----BEGIN RSA PUBLIC KEY-----", 20)) {
		return 0;
	}

	result = sim_message_encrypt(*message, *recipient);

	if (!result) {
		debug("[sim] encryption failed: %s\n", sim_strerror(sim_errno));
		return 0;
	}

	if (xstrlen(result) > 1989) {
		debug("[sim] encrypted message too long - truncated\n");
		result[1989] = 0;
	}

	xfree(*message);
	*message = result;
	*encrypted = 1;

	return 0;
}

static QUERY(message_decrypt)
{
	char **session, **sender, **message, *result;
	int *decrypted;

	session = va_arg(ap, char**);
	sender = va_arg(ap, char**);
	message = va_arg(ap, char**);
	decrypted = va_arg(ap, int*);

	if (!session || !message || !decrypted)
		return 0;

	if (!config_encryption)
		return 0;

	if (!*session || !*sender)
		return 0;

	if (!xstrncmp(*message, "-----BEGIN RSA PUBLIC KEY-----", 20)) {
		char *name;
		FILE *f;

		print("key_public_received", format_user(session_find(*session), *sender));

		if (mkdir(prepare_path("keys", 1), 0700) && errno != EEXIST) {
			print("key_public_write_failed", strerror(errno));
			return 0;
		}

		name = saprintf("%s/%s.pem", prepare_path("keys", 0), *sender);

		if (!(f = fopen(name, "w"))) {
			print("key_public_write_failed", strerror(errno));
			xfree(name);
			return 0;
		}

		fprintf(f, "%s", *message);
		fclose(f);
		xfree(name);

		return 1;
	}

	result = sim_message_decrypt(*message, *session);

	if (!result) {
		debug("[sim] decryption failed: %s\n", sim_strerror(sim_errno));
		return 0;
	}

	xfree(*message);
	*message = result;
	*decrypted = 1;

	return 0;
}

/*
 * command_key()
 *
 * obs³uga komendy /key.
 */
static COMMAND(command_key)
{
	PARUNI
	if (match_arg(params[0], 'g', TEXT("generate"), 2)) {
		char *tmp, *tmp2;
		struct stat st;
		const char *uid;

		if (!session) 
			return -1;
		uid = session_uid_get(session);

		if (mkdir(prepare_path("keys", 1), 0700) && errno != EEXIST) {
			printq("key_generating_error", strerror(errno));
			return -1;
		}

		tmp = saprintf("%s/%s.pem", prepare_path("keys", 0), uid);
		tmp2 = saprintf("%s/private-%s.pem", prepare_path("keys", 0), uid);

		if (!stat(tmp, &st) && !stat(tmp2, &st)) {
			printq("key_private_exist");
			xfree(tmp);
			xfree(tmp2);
			return -1;
		}

		xfree(tmp);
		xfree(tmp2);

		printq("key_generating");

		if (sim_key_generate(uid)) {
			printq("key_generating_error", "sim_key_generate()");
			return -1;
		}

		printq("key_generating_success");

		return 0;
	}

	if (match_arg(params[0], 's', TEXT("send"), 2)) {
		wcs_string_t s = NULL;
		char *tmp, buf[128];
		FILE *f;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!session) 
			return -1;

		tmp = saprintf("%s/%s.pem", prepare_path("keys", 0), session_uid_get(session));
		f = fopen(tmp, "r");
		xfree(tmp);

		if (!f) {
			printq("key_public_not_found", format_user(session, session_uid_get(session)));
			return -1;
		}

		s = wcs_string_init(TEXT("/ "));

		while (fgets(buf, sizeof(buf), f)) {
			CHAR_T *sbuf = normal_to_wcs(buf);
			wcs_string_append(s, sbuf);
			free_utf(sbuf);
		}

		fclose(f);

		command_exec(wcs_to_normal(params[1]), session, s->str, quiet);

		printq("key_send_success", format_user(session, get_uid(session, wcs_to_normal(params[1]))));
		wcs_string_free(s, 1);

		return 0;
	}

	if (match_arg(params[0], 'd', TEXT("delete"), 2)) {
		char *tmp;
		char *uid;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!(uid = get_uid(session_current, wcs_to_normal(params[1])))) {
			printq("user_not_found", params[1]);
			return -1;
		}

		if (uid == session_uid_get(session_current)) {
			char *tmp = saprintf("%s/private-%s.pem", prepare_path("keys", 0), uid);
			unlink(tmp);
			xfree(tmp);
		}

		tmp = saprintf("%s/%s.pem", prepare_path("keys", 0), uid);

		if (unlink(tmp))
			printq("key_public_not_found", format_user(session_current, uid));
		else
			printq("key_public_deleted", format_user(session_current, uid));

		xfree(tmp);

		return 0;
	}

	if (!params[0] || match_arg(params[0], 'l', TEXT("list"), 2) || params[0][0] != '-') {
		DIR *dir;
		struct dirent *d;
		int count = 0;
		const char *path = prepare_path("keys", 0);
		const CHAR_T *x = NULL;
		const char *list_uid = NULL;

		if (!(dir = opendir(path))) {
			printq("key_public_noexist");
			return 0;
		}

		if (params[0] && params[0][0] != '-')
			x = params[0];
		else if (params[0] && match_arg(params[0], 'l', TEXT("list"), 2))
			x = params[1];

		if (x && !(list_uid = get_uid(session, wcs_to_normal(x)))) {
			printq("user_not_found", x);
			closedir(dir);
			return -1;
		}

		while ((d = readdir(dir))) {
			struct stat st;
			char *name = saprintf("%s/%s", path, d->d_name);
			struct tm *tm;
			const char *tmp;

			if ((tmp = xstrstr(d->d_name, ".pem")) && !tmp[4] && !stat(name, &st) && S_ISREG(st.st_mode)) {
				char *uid = xstrndup(d->d_name, xstrlen(d->d_name) - 4);

				if (list_uid && xstrcmp(uid, list_uid))
					continue;

				if (uid) {
					char *fp = sim_key_fingerprint(uid);
					char ts[100];

					tm = localtime(&st.st_mtime);
					strftime(ts, sizeof(ts), format_find("key_list_timestamp"), tm);

					print("key_list", format_user(session, uid), (fp) ? fp : format_string(format_find("value_none")), ts);
					count++;

					xfree(fp);
					xfree(uid);
				}
			}

			xfree(name);
		}

		closedir(dir);

		if (!count)
			printq("key_public_noexist");

		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

/* 
 * sim_theme_init()
 * 
 * themes initialization
 */
static int sim_theme_init()
{
	format_add("key_generating", _("%> Please wait, generating keys...\n"), 1);
	format_add("key_generating_success", _("%> Keys generated and saved\n"), 1);
	format_add("key_generating_error", _("%! Error while generating keys: %1\n"), 1);
	format_add("key_private_exist", _("%! You already own a key pair\n"), 1);
	format_add("key_public_deleted", _("%) Public key %1 removew\n"), 1);
	format_add("key_public_not_found", _("%! Can find %1's public key\n"), 1);
	format_add("key_public_noexist", _("%! No public keys\n"), 1);
	format_add("key_public_received", _("%> Received public key from %1\n"), 1);
	format_add("key_public_write_failed", _("%! Error while saving public key: %1\n"), 1);
	format_add("key_send_success", _("%> Sent public key to %1\n"), 1);
	format_add("key_send_error", _("%! Error sending public key\n"), 1);
	format_add("key_list", "%> %r%1%n (%3)\n%) fingerprint: %y%2\n", 1);
	format_add("key_list_timestamp", "%Y-%m-%d %H:%M", 1);

	return 0;
}

/*
 * sim_plugin_init()
 *
 * inicjalizacja pluginu.
 */
int sim_plugin_init(int prio)
{
	plugin_register(&sim_plugin, prio);

	query_connect(&sim_plugin, TEXT("message-encrypt"), message_encrypt, NULL);
	query_connect(&sim_plugin, TEXT("message-decrypt"), message_decrypt, NULL);

	command_add(&sim_plugin, TEXT("sim:key"), TEXT("puUC uUC"), command_key, 0,
			"-g --generate -s --send -d --delete -l --list");

	variable_add(&sim_plugin, TEXT("encryption"), VAR_BOOL, 1, &config_encryption, NULL, NULL, NULL);

	sim_key_path = xstrdup(prepare_path("keys/", 0));

	return 0;
}

/*
 * sim_plugin_destroy()
 *
 * usuniêcie pluginu z pamiêci.
 */
static int sim_plugin_destroy()
{
	plugin_unregister(&sim_plugin);

	xfree(sim_key_path);

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
