#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/vars.h>
#include <ekg/stuff.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

typedef struct {
	char *session;
	char *target;
	char *rot;
	char *drot;
	int prio;
} rot13_key_t;
static list_t keys;

static int config_encryption = 0;
static int config_default_rot;
static int config_default_drot;

static int rot13_theme_init();

PLUGIN_DEFINE(rot13, PLUGIN_CRYPT, rot13_theme_init);

/* main function \o/ */
static void do_foo(char *p, int rot, int deltarot) {	/* some code/idea gathered from: http://gophi.rotfl.pl/p/rot.c (c Adam Wysocki) */
	if (!p) return;
	rot		%= 26;
	deltarot	%= 26;

	while (*p) {
		int i;

		if (tolower(*p) < 'a' || tolower(*p) > 'z') goto end;

		for (i = 0; i < rot; i++) {
			if (*p == 'z') 		*p = 'a';
			else if (*p == 'Z') 	*p = 'A';
			else 			(*p)++;
		}
end:
		rot += deltarot;
		rot %= 26;
		p++;
	}
}

static rot13_key_t *rot13_find_key(char *session, char *target) {
	list_t l;
	for (l = keys; l; l = l->next) {
		rot13_key_t *k = l->data;
		const char *tmp;
		int len;

		if ((!k->session || !xstrcmp(session, k->session)) && (!k->target || !xstrcmp(target, k->target))) 
			return k;

	/* XXX, resource strip, only jabber if no resource passed */
		if (!((tmp = xstrchr(target, '/')) || xstrncmp(target, "jid:", 4) || xstrchr(k->target, '/'))) continue;

		len = (int)(tmp - k->target);

		if (len > 0 && (!k->session || !xstrcmp(session, k->session)) && (!k->target || !xstrncmp(target, k->target, len)))
			return k;

	}
	return NULL;
}

static int rot13_key_compare(void *data1, void *data2) {
	rot13_key_t *key1 = data1;
	rot13_key_t *key2 = data2;

	if (!key1->target && key2->target) return 1;
	if (!key2->target && key1->target) return -1;

	if (!key1->session && key2->session) return 1;
	if (!key2->session && key1->session) return -1;

	if (key1->prio || key2->prio) return key1->prio-key2->prio;

	if (!key1->rot && key2->rot) return 1;
	if (key1->rot && !key2->rot) return -1;

	return 0;
}

static QUERY(message_parse) {
	char *session	= *(va_arg(ap, char **));
	char *recipient = *(va_arg(ap, char **));
	char *message	= *(va_arg(ap, char **));
	int *encrypted = va_arg(ap, int *);
	rot13_key_t *key;

	if (!config_encryption)					return 0;
	if (!session || !recipient || !message || !encrypted)	return 0;
	if (*encrypted)						return 0;

	if (!(key = rot13_find_key(session, recipient)))	return 0;
	
	if (data) 	do_foo(message, key->rot ? atoi(key->rot) : config_default_rot, key->drot ? atoi(key->drot) : config_default_drot);
	else		do_foo(message, key->rot ? -atoi(key->rot): config_default_rot, key->drot ? -atoi(key->drot): config_default_drot);

	*encrypted = 1;
	return 0;
}

static rot13_key_t *rot13_key_parse(const char *target, const char *sesja, const char *offset, const char *offset2) {
	rot13_key_t *k = xmalloc(sizeof(rot13_key_t));

	if (!xstrcmp(target, "$")) 		k->target = xstrdup(get_uid(window_current->session, window_current->target));
	else if (!xstrcmp(target, "*")) 	k->target = NULL;
	else					k->target = xstrdup(target);

	if (!xstrcmp(sesja, "$"))		k->session = session_current ? xstrdup(session_current->uid) : NULL /* "*" */;
	else if (!xstrcmp(sesja, "*"))		k->session = NULL;
	else					k->session = xstrdup(sesja);

	if (!offset || !xstrncmp(offset, "def", 3))	k->rot = xstrdup("?");
	else						k->rot = xstrdup(offset);

	if (!offset2 || !xstrncmp(offset2, "def", 3))	k->drot	= xstrdup("?");
	else						k->drot	= xstrdup(offset2);

	return k;
}

static COMMAND(command_rot) {
	char *tmp = xstrdup(params[0]);
	do_foo(tmp, params[1] ? atoi(params[1]) : 0, params[1] && params[2] ? atoi(params[2]) : 0);
	print("rot_generic", params[0], tmp);
	xfree(tmp);
	return 0;
}

static COMMAND(command_key) {
	int type = 0;

	if (match_arg(params[0], 'a', "add", 2))	type = 1;
	if (match_arg(params[0], 'm', "modify", 2))	type = 2;
	if (match_arg(params[0], 'd', "delete", 2))	type = 3;

	if (type == 1) {
		char *sesja	= NULL;
		char *target	= NULL;
		char *offset	= NULL;
		char *offset2	= NULL;

		char **arr;
		int i;

		if (!params[1]) {
			printq("invalid_params", name);
			return -1;
		}

		arr = array_make(params[1], " ", 0, 1, 1);

		for (i = 0; arr[i]; i++) {
			if (!target)		target = arr[i];
			else if (!sesja)	sesja  = arr[i];
			else if (!offset)	offset = arr[i];
			else if (!offset2)	offset2= arr[i];
			else			debug("command_key() Nextarg? for what? %s\n", arr[i]);
		}

		if (!target) {
			printq("invalid_params", name);
			array_free(arr);
			return -1;
		}

		list_add_sorted(&keys, rot13_key_parse(target, sesja, offset, offset2), 0, rot13_key_compare);

		array_free(arr);
		return 0;
	}

	if (!params[0] || match_arg(params[0], 'l', ("list"), 2) || params[0][0] != '-') {
		list_t l;
		for (l = keys; l; l = l->next) {
			rot13_key_t *k = l->data;
			printq("rot_list", 
				k->session ? k->session : "*",
				k->target ? k->target : "*", 
				xstrcmp(k->rot, "?") ? k->rot : itoa(config_default_rot),
				xstrcmp(k->drot, "?") ? k->drot : itoa(config_default_drot));

		}
		return 0;
	}

	printq("invalid_params", name);
	return -1;
}

static QUERY(rot13_setvar_default) {
	char *path 	= saprintf("%s/rot13.keys", prepare_path("keys", 0));
	FILE *f;
	
	if ((f = fopen(path, "r"))) {
		char *tmp;
		while ((tmp = read_file(f))) {
			char **arr = array_make(tmp, " ", 0, 1, 1);
			
			if (arr[0] && arr[1] && arr[2] && arr[3] && !arr[4]) {
				list_add(&keys, rot13_key_parse(arr[0], arr[1], arr[2], arr[3]), 0);

				xfree(arr);
			} else {
				debug("rot13_setvar_default() failed to parse line: %s\n", tmp);
				array_free(arr);
			}
			xfree(tmp);
		}
		fclose(f);
	} else debug("rot13_setvar_default() failed to open: %s errno: %d\n", path, errno);
	xfree(path);

	config_encryption	= 0;
	config_default_rot	= 13;
	config_default_drot	= 0;
	return 0;
}

static int rot13_theme_init() {
#ifndef NO_DEFAULT_THEME
	format_add("rot_generic", _("%> Text: %1 roted: %2"), 1);
	format_add("rot_list", _("%> Sesja: %1 target: %2 (rot%3 +%4)"), 1);
#endif
	return 0;
}

int rot13_plugin_init(int prio) {
	plugin_register(&rot13_plugin, prio);

	rot13_setvar_default(NULL, NULL);

	query_connect(&rot13_plugin, ("set-vars-default"), rot13_setvar_default, NULL);
	query_connect(&rot13_plugin, "message-encrypt", message_parse, (void *) 0);
	query_connect(&rot13_plugin, "message-decrypt", message_parse, (void *) 1);

	command_add(&rot13_plugin, "rot13", "! ? ?", command_rot, 0, NULL);
	command_add(&rot13_plugin, "rot:key", ("puUC uUC"), command_key, 0, "-a --add -m --modify -d --delete -l --list");

	variable_add(&rot13_plugin, "encryption", VAR_BOOL, 1, &config_encryption, NULL, NULL, NULL);
	variable_add(&rot13_plugin, "default_rot", VAR_INT, 1, &config_default_rot, NULL, NULL, NULL);
	variable_add(&rot13_plugin, "default_drot", VAR_INT, 1, &config_default_drot, NULL, NULL, NULL);
	return 0;
}

static int rot13_plugin_destroy() {
	char *path = saprintf("%s/rot13.keys", prepare_path("keys", 0));
	FILE *f;
	if ((f = fopen(path, "w"))) {
		list_t l;

		for (l = keys; l; l = l->next) {
			rot13_key_t *k = l->data;

			fprintf(f, "\"%s\" \"%s\" \"%s\" \"%s\"\n",
					k->target ? k->target : "*",
					k->session ? k->session : "*", 
					k->rot ? k->rot : "?",
					k->drot ? k->drot : "?");
		}
		fclose(f);
	}
	xfree(path);

	plugin_unregister(&rot13_plugin);
	return 0;
}
