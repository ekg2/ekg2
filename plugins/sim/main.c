#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include <ekg/plugins.h>
#include <ekg/commands.h>
#include <ekg/vars.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/xmalloc.h>
#include <ekg/userlist.h>

#include "simlite.h"

static int config_encryption = 0;
static int sim_plugin_destroy();

static plugin_t sim_plugin = {
	name: "sim",
	pclass: PLUGIN_CRYPT,
	destroy: sim_plugin_destroy,
};

static int message_encrypt(void *data, va_list ap)
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

	if (!*session || strncmp(*session, "gg:", 3) || !*recipient || strncmp(*recipient, "gg:", 3))
		return 0;
	
	result = sim_message_encrypt(*message, atoi(*recipient + 3));

	if (!result) {
		debug("[sim] encryption failed: %s\n", sim_strerror(sim_errno));
		return 0;
	}

	if (strlen(result) > 1989) {
		debug("[sim] encrypted message too long - truncated\n");
		result[1989] = 0;
	}

	xfree(*message);
	*message = result;
	*encrypted++;
	
	return 0;
}

static int message_decrypt(void *data, va_list ap)
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

	if (!*session || strncmp(*session, "gg:", 3) || !*sender || strncmp(*sender, "gg:", 3))
		return 0;

	result = sim_message_decrypt(*message, atoi(*session) + 3);

	if (!result) {
		debug("[sim] decryption failed: %s\n", sim_strerror(sim_errno));
		return 0;
	}
	
	xfree(*message);
	*message = result;
	*decrypted++;

	return 0;
}

/*
 * command_key()
 *
 * obs³uga komendy /key.
 */
static COMMAND(command_key)
{
	if (match_arg(params[0], 'g', "generate", 2)) {
		char *tmp, *tmp2;
		struct stat st;
		int uin;

		if (!session || strncasecmp(session_uid_get(session), "gg:", 3)) {
			printq("generic", "Mo¿na generowaæ klucze tylko dla sesji Gadu-Gadu");
			return -1;
		}

		uin = atoi(session_uid_get(session) + 3);

		if (mkdir(prepare_path("keys", 1), 0700) && errno != EEXIST) {
			printq("key_generating_error", strerror(errno));
			return -1;
		}

		tmp = saprintf("%s/%d.pem", prepare_path("keys", 0), uin);
		tmp2 = saprintf("%s/private.pem", prepare_path("keys", 0));

		if (!stat(tmp, &st) && !stat(tmp2, &st)) {
			printq("key_private_exist");
			xfree(tmp);
			xfree(tmp2);
			return -1;
		} 

		xfree(tmp);
		xfree(tmp2);

		printq("key_generating");

		if (sim_key_generate(uin)) {
			printq("key_generating_error", "sim_key_generate()");
			return -1;
		}

		printq("key_generating_success");

		return 0;
	}

	if (match_arg(params[0], 's', "send", 2)) {
		string_t s = NULL;
		char *tmp, buf[128];
		FILE *f;
		
		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!session || strncasecmp(session_uid_get(session), "gg:", 3)) {
			printq("generic", "Mo¿na wysy³aæ klucze tylko dla sesji Gadu-Gadu");
			return -1;
		}

		tmp = saprintf("%s/%d.pem", prepare_path("keys", 0), atoi(session_uid_get(session) + 3));
		f = fopen(tmp, "r");
		xfree(tmp);

		if (!f) {
			printq("key_public_not_found", format_user(session, session_uid_get(session)));
			return -1;
		}

		s = string_init("/ ");

		while (fgets(buf, sizeof(buf), f))
			string_append(s, buf);

		fclose(f);

		command_exec(params[1], session, s->str, quiet);
		
		printq("key_send_success", format_user(session, params[1]));
		string_free(s, 1);

		return 0;
	}

#if 0
 	if (match_arg(params[0], 'd', "delete", 2)) {
		char *tmp;
		int uin;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!(uin = get_uin(params[1]))) {
			printq("user_not_found", params[1]);
			return -1;
		}

		if (uin == config_uin) {
			char *tmp = saprintf("%s/private.pem", prepare_path("keys", 0));
			unlink(tmp);
			xfree(tmp);
		}

		tmp = saprintf("%s/%d.pem", prepare_path("keys", 0), uin);
		
		if (unlink(tmp))
			printq("key_public_not_found", format_user(uin));
		else
			printq("key_public_deleted", format_user(uin));
		
		xfree(tmp);

		return 0;
	}
#endif

	if (!params[0] || match_arg(params[0], 'l', "list", 2) || params[0][0] != '-') {
		DIR *dir;
		struct dirent *d;
		int count = 0, list_uin = 0;
		const char *path = prepare_path("keys", 0);
		const char *x = NULL;

		if (!(dir = opendir(path))) {
			printq("key_public_noexist");
			return 0;
		}

#if 0
		if (params[0] && params[0][0] != '-')
			x = params[0];
		else if (params[0] && match_arg(params[0], 'l', "list", 2))
			x = params[1];

		if (x && !(list_uin = get_uin(x))) {
			printq("user_not_found", x);
			closedir(dir);
			return -1;
		}
		
		while ((d = readdir(dir))) {
			struct stat st;
			char *name = saprintf("%s/%s", path, d->d_name);
			struct tm *tm;
			const char *tmp;

			if ((tmp = strstr(d->d_name, ".pem")) && !tmp[4] && !stat(name, &st) && S_ISREG(st.st_mode)) {
				int uin = atoi(d->d_name);

				if (list_uin && uin != list_uin)
					continue;

				if (uin) {
					char *fp = sim_key_fingerprint(uin);
					char ts[100];

					tm = localtime(&st.st_mtime);
					strftime(ts, sizeof(ts), format_find("key_list_timestamp"), tm);

					print("key_list", format_user(uin), (fp) ? fp : "", ts);
					count++;

					xfree(fp);
				}
			}

			xfree(name);
		}

		closedir(dir);

		if (!count)
			printq("key_public_noexist");
#endif
		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

/*
 * sim_plugin_init()
 *
 * inicjalizacja pluginu.
 */
int sim_plugin_init()
{
	plugin_register(&sim_plugin);

	query_connect(&sim_plugin, "message-encrypt", message_encrypt, NULL);
	query_connect(&sim_plugin, "message-decrypt", message_decrypt, NULL);
	
	command_add(&sim_plugin, "key", "uu", command_key, 0,
	  " [opcje]", "zarz±dzanie kluczami dla SIM",
	  "\n"
	  "  -g, --generate              generuje parê kluczy u¿ytkownika\n"
	  "  -s, --send <numer/alias>    wysy³a nasz klucz publiczny\n"
	  "  -d, --delete <numer/alias>  usuwa klucz publiczny\n"
	  "  [-l, --list] [numer/alias]  wy¶wietla posiadane klucze publiczne");
	
	variable_add(&sim_plugin, "encryption", VAR_BOOL, 1, &config_encryption, NULL, NULL, NULL);

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
	query_disconnect(&sim_plugin, "message-encrypt");
	query_disconnect(&sim_plugin, "message-decrypt");
	
	command_remove(&sim_plugin, "key");

	variable_remove(&sim_plugin, "encryption");
	
	plugin_unregister(&sim_plugin);

	xfree(sim_key_path);

	return 0;
}
