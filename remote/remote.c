/*
 *  (C) Copyright 2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
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

/* sprawdzac includy od: */
#include "ekg2-config.h"

#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/un.h>

#include <sys/ioctl.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>

#include <stdio.h>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "commands.h"
#include "debug.h"		/* ok */
#include "plugins.h"
#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif
#include "sessions.h"
#include "recode.h"
#include "stuff.h"
#include "themes.h"
#include "userlist.h"
#include "vars.h"
#include "windows.h"
#include "xmalloc.h"

#include "queries.h"
/* do */

#include "remote-ssl.h"

extern void ekg_loop();

static int login_OK;
static int commands_OK, variables_OK, formats_OK, plugins_OK, sessions_OK, windows_OK, userlist_OK;
static int ui_config_OK, backlog_OK;

static int remote_fd;

static unsigned int read_total, write_total;
int remote_mail_count;

#ifdef HAVE_ZLIB
static unsigned int zlib_read_total, zlib_write_total;
static int zlib_used;
#endif

#ifdef HAVE_SSL
static unsigned int ssl_read_total, ssl_write_total;
static int ssl_used;

SSL_SESSION ssl_session;
#ifdef WANT_GNUTLS
gnutls_certificate_credentials ssl_xcred;
#endif

/*
 * this function is based on some (print_info(), print_cert_info(), print_x509_info(), etc..) from gnutls-2.0.4, src/common.c
 * Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007 Free Software Foundation
 * Author: Nikos Mavrogiannopoulos
 *
 * under GPL-2 or later */
static void ssl_print_info(SSL_SESSION sesja) {
#ifdef WANT_GNUTLS
	gnutls_kx_algorithm_t kx;
#endif
	printf("---====== ssl_print_info() ======---\n");

#ifdef WANT_GNUTLS
	kx = gnutls_kx_get(sesja);

	if (gnutls_auth_get_type(sesja) == GNUTLS_CRD_CERTIFICATE) {
		switch (gnutls_certificate_type_get(sesja)) {
			case GNUTLS_CRT_X509:
				// break;

			/* XXX, case GNUTLS_CRT_OPENPGP: */
			case GNUTLS_CRT_UNKNOWN:
			default:
				printf("\tNieznany typ certyfikatu!\n");
				break;
		}
	}

	printf("\t   Wersja: %s\n", gnutls_protocol_get_name(gnutls_protocol_get_version(sesja)));
	printf("\t    Klucz: %s\n", gnutls_kx_get_name(kx));
	printf("\t    Szyfr: %s\n", gnutls_cipher_get_name(gnutls_cipher_get(sesja)));
	printf("\t      MAC: %s\n", gnutls_mac_get_name(gnutls_mac_get(sesja)));
	printf("\tKompresja: %s\n", gnutls_compression_get_name(gnutls_compression_get(sesja)));
#endif

#ifdef WANT_OPENSSL
	printf("\t    Szyfr: %s\n", SSL_CIPHER_get_name(SSL_get_current_cipher(sesja)));
	printf("\tKompresja: %s\n", SSL_COMP_get_name(SSL_get_current_compression(sesja)));		/* XXX, nie ma tego w man? */
#endif
	printf("------------------------------------\n");
}

#endif

#ifdef WANT_GNUTLS
static ssize_t ekg_gnutls_read(int fd, void *buf, size_t count) {
	int ret;

	if ((ret = read(fd, buf, count)) > 0)
		ssl_read_total += ret;
	return ret;
}
// #define ekg_gnutls_read read

static ssize_t ekg_gnutls_write(int fd, void *buf, size_t count) {
	int ret;

	if ((ret = write(fd, buf, count)) > 0)
		ssl_write_total += ret;
	return ret;
}
// #define ekg_gnutls_write write

#endif

#ifdef WANT_OPENSSL
long ekg_openssl_bio_dump(BIO *bio, int cmd, const char *argp, int argi, long argl, long ret) {
	if (cmd == (BIO_CB_READ|BIO_CB_RETURN))
		ssl_read_total += ret;
	else if (cmd == (BIO_CB_WRITE|BIO_CB_RETURN))
		ssl_write_total += ret;
	return ret;
}
#endif

static int rc_input_new_pipe(const char *path) {
	errno = ENOTSUP;
	return -1;
}

static int rc_input_new_inet(const char *path, int type) {
	char *s_addr, *s_port;
	uint32_t addr;
	int port;

	struct sockaddr_in sin;
	int fd;

	if (!(s_port = strchr(path, ':'))) {
		errno = EINVAL;
		return -1;
	}
	s_addr = xstrndup(path, s_port - path);
		addr = inet_addr(s_addr);
		port = atoi(s_port + 1);
	xfree(s_addr);

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = addr;

	if ((fd = socket(AF_INET, type, 0)) == -1)
		return -1;

	if (connect(fd, (struct sockaddr*) &sin, sizeof(sin))) {
		int err = errno;

		close(fd);
		errno = err;
		return -1;
	}

	return fd;
}

static int rc_input_new_inet_ssl(const char *path, int type) {
#ifdef HAVE_SSL
#ifdef WANT_GNUTLS
	/* Allow connections to servers that have OpenPGP keys as well. */
	const int cert_type_priority[3] = {GNUTLS_CRT_X509, GNUTLS_CRT_OPENPGP, 0};
	const int comp_type_priority[3] = {GNUTLS_COMP_ZLIB, GNUTLS_COMP_NULL, 0};

#endif
	SSL_SESSION ssl;
	int fd;
	int ret;

	if ((fd = rc_input_new_inet(path, type)) == -1)
		return -1;

	ssl_used = 1;
	SSL_GLOBAL_INIT();		/* inicjuj ssla */
#ifdef WANT_GNUTLS
	gnutls_certificate_allocate_credentials(&ssl_xcred);
#endif

	if (SSL_INIT(ssl)) {
		close(fd);
		errno = ENOMEM;		/* XXX? */
		return -1;
	}
#ifdef WANT_GNUTLS
	/* gnutls_record_set_max_size(ssl, 1024); */		/* XXX */

	gnutls_set_default_priority(ssl);
	gnutls_certificate_type_set_priority(ssl, cert_type_priority);
	gnutls_credentials_set(ssl, GNUTLS_CRD_CERTIFICATE, ssl_xcred);
	gnutls_compression_set_priority(ssl, comp_type_priority);

	/* we use read/write instead of recv/send */
	gnutls_transport_set_pull_function(ssl, (gnutls_pull_func) ekg_gnutls_read);
	gnutls_transport_set_push_function(ssl, (gnutls_push_func) ekg_gnutls_write);
#endif

	if (SSL_SET_FD(ssl, fd) == 0) {	/* gnutls never fail */
		SSL_DEINIT(ssl);
		close(fd);
		errno = ENOMEM;		/* XXX? */
		return -1;
	}

#ifdef WANT_OPENSSL
	BIO_set_callback(SSL_get_rbio(ssl), ekg_openssl_bio_dump);
	BIO_set_callback(SSL_get_wbio(ssl), ekg_openssl_bio_dump);
#endif

	do {
		ret = SSL_HELLO(ssl);

#ifdef WANT_OPENSSL
		if (ret != -1)
			goto handshake_ok;			/* ssl was ok */

		ret = SSL_get_error(ssl, ret);
#endif
	} while (SSL_E_AGAIN(ret));


#ifdef WANT_GNUTLS
	if (ret >= 0) 
		goto handshake_ok;	/* gnutls was ok */

	printf("rc_input_new_inet_ssl(): %s\n", SSL_ERROR(ret));

	SSL_DEINIT(ssl);
	close(fd);
	errno = EFAULT;
	return -1;
#endif

handshake_ok:
	ssl_print_info(ssl);
/*
 * XXX,
 * Key fingerprint is XX:YY:ZZ:AA:BB:CC:DD:EE:FF:GG:HH:II:JJ:KK:LL:MM.
 * Are you sure you want to continue connecting (yes/no)? 
 */
	ssl_session = ssl;
	return fd;

#else
	errno = ENOTSUP;
	return -1;
#endif
}

static int rc_input_new_unix(const char *path) {
	struct sockaddr_un beeth;
	int fd;

	beeth.sun_family = AF_UNIX;
	strlcpy(beeth.sun_path, path, sizeof(beeth.sun_path));

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		return -1;
	
	if (connect(fd, (struct sockaddr*) &beeth, sizeof(beeth))) {
		int err = errno;

		close(fd);
		errno = err;
		return -1;
	}

	return fd;
}

int remote_connect(const char *path) {
	if (!path) {
		errno = EINVAL;
		return -1;
	}

	if (!strncmp(path, "tcp:", 4)) {
		path = path + 4;
		return rc_input_new_inet(path, SOCK_STREAM);
	}

	if (!strncmp(path, "tcps:", 5)) {
		path = path + 5;
		return rc_input_new_inet_ssl(path, SOCK_STREAM);
	}

	if (!strncmp(path, "udp:", 4)) {
		path = path + 4;
		return rc_input_new_inet(path, SOCK_DGRAM);
	}
	
	if (!strncmp(path, "unix:", 5)) {
		path = path + 5;
		return rc_input_new_unix(path);
	}

	if (!strncmp(path, "pipe:", 5)) {
		path = path + 5;
		return rc_input_new_pipe(path);
	}

	debug_error("[rc] unknown input type: %s\n", path);

	errno = ENOTSUP;
	return -1;
}

static void xstrtr(char *text, char from, char to) {
	while (*text++) {
		if (*text == from) {
			*text = to;
		}
	}
}

static string_t remote_what_to_write(char *what, va_list ap) {
	string_t str;
	char *_str;

	str = string_init(what);

	while ((_str = va_arg(ap, char *))) {
		char *tmp;

		string_append_c(str, '\001');
		/* mozna tez rekodowac :) */
		tmp = remote_recode_to(xstrdup(_str));
		string_append(str, tmp);
		xfree(tmp);
	}
	/* zamieniamy \n na \008 */		/* XXX, nie powinno wystapic, albo przynajmniej rzadziej... */
	xstrtr(str->str, '\n', '\x8');
	string_append_c(str, '\n');

	debug_io("remote_what_to_write: %s\n", str->str);

	return str;
}

static int remote_writefd(int fd, char *what, ...) {
	string_t str;
	va_list ap;

	va_start(ap, what);
	str = remote_what_to_write(what, ap);
	va_end(ap);

	ekg_write(fd, str->str, str->len);

	string_free(str, 1);
	return 0;
}

EXPORTNOT int remote_request(char *what, ...) {
	string_t str;
	va_list ap;

	va_start(ap, what);
	str = remote_what_to_write(what, ap);
	va_end(ap);

	ekg_write(remote_fd, str->str, str->len);

	string_free(str, 1);
	return 0;
}

/* XXX, zrobic cos takiego ze zmienia tylko wystapienia stringu sep na \0 i wrzuca adresy do tablicy */
/*      i potem zamiast array_free() zwykle xfree() */
static char **array_make_fast(const char *string, char sep, int *arrcnt)
{
	const char *p;
	char **result = NULL;
	int items = 0;

	for (p = string; *p; p++) {
		const char *q;
		char *token;
		int len;

		for (q = p, len = 0; (*q && *q != sep); q++, len++);

		token = remote_recode_from(xstrndup(p, len));

		p = q;
		
		result = xrealloc(result, (items + 2) * sizeof(char*));
		result[items] = token;
		result[++items] = NULL;

		if (!*p)
			break;
	}
	*arrcnt = items;

	return result;
}

static void plugin_params_array_add(plugins_params_t **array, plugins_params_t *param) {
	plugins_params_t *tmp;
	int count;

	if (!(tmp = *array)) {
		count = 0;
		tmp = xmalloc2(2 * sizeof(plugins_params_t));
	} else {
		for (count = 0; tmp[count].key; count++)
			;
		tmp = xrealloc(tmp, (count + 2) * sizeof(plugins_params_t));
	}

	memcpy(&tmp[count], param, sizeof(plugins_params_t));
	memset(&tmp[count+1], 0, sizeof(plugins_params_t));

	*array = tmp;
}

/* note:
 *	jest pomysl zeby w tym miejscu nie emitowac zadnych zdarzen,
 *	tylko dodawac do listy ktora bedzie sie wykonywac w ekg_loop()
 *
 *	brzmi calkiem sensownie, bo zamiast emitowania 3 razy takiego UI_WINDOW_TARGET_CHANGED, &okno
 *	emitniemy tylko raz.
 *
 *	Problemem ewentualnie moga byc zdarzenia ktore polegaja na wskazniku na takie &okno, a jesli je wczesniej zniszczymy to bedzie zle..
 *	Ale oczywiscie wszystko da sie zrobic, wystarczy tylko przemyslec
 */

static WATCHER_LINE(remote_read_line) {
	char **arr;
	int arrcnt;
	char *cmd;
	int done;

	if (type) {
		remote_fd = -1;
		close(fd);
		/* XXX, wyswietlic jakis madry komunikat */
		return 0;
	}

	if (!watch || !watch[0])
		return 0;

	/* odsanityzujemy \008 na \n */
	xstrtr((char *) watch, '\x8', '\n');

	arr = array_make_fast(watch, '\002', &arrcnt);
	cmd = arr[0];

	if (cmd[0] == '+') {
		cmd++;
		done = 1;
	} else if (cmd[0] == '-') {
		cmd++;
		done = -1;
	} else
		done = 0;

	if (!strcmp(cmd, "LOGIN")) {
		if (done) {
			login_OK = done;
			/* debug_ok("LOGIN: DONE\n"); */
		}

	} else if (!strcmp(cmd, "CONFIG")) {
		if (done == 0 && (arrcnt == 3 || arrcnt == 2))
			remote_variable_add(arr[1], (arrcnt == 3) ? arr[2] : NULL);

		if (done == 1) {
			variables_OK = 1;
			debug_ok("VARIABLES: DONE\n");
		}
	} else if (!strcmp(cmd, "UICONFIG")) {
		if (done == 0 && (arrcnt == 3 || arrcnt == 2))
			remote_variable_add(arr[1], (arrcnt == 3) ? arr[2] : NULL);

		if (done == 1) {
			ui_config_OK = 1;
			debug_ok("UIVARIABLES: DONE\n");
		}

	} else if (!strcmp(cmd, "COMMAND")) {
		if (done == 0 && (arrcnt == 2 || arrcnt == 3))
			remote_command_add(arr[1], (arrcnt == 3) ? arr[2] : NULL);

		if (done == 1) {
			commands_OK = 1;
			debug_ok("COMMANDS: DONE\n");
		}

	} else if (!strcmp(cmd, "PLUGIN")) {
		if (done == 0 && arrcnt == 3)
			remote_plugin_load(arr[1], atoi(arr[2]));

		if (done == 1) {
			plugins_OK = 1;
			debug_ok("PLUGINS: DONE\n");
		}

	} else if (!strcmp(cmd, "PLUGINPARAM")) {
		if (arrcnt == 3) {
			plugin_t *p;

			if ((p = plugin_find(arr[1]))) {
				static plugins_params_t pa;

				pa.key = xstrdup(arr[2]);		/* should be enough */
				plugin_params_array_add(&(p->params), &pa);
			}
		}

	} else if (!strcmp(cmd, "FORMAT")) {
		if (done == 0 && arrcnt == 3)
			remote_format_add(arr[1], arr[2]);

		if (done == 1) {
			formats_OK = 1;
			debug_ok("FORMATS: DONE\n");
		}

	} else if (!strcmp(cmd, "SESSION")) {
		if (done == 0 && arrcnt == 3) {
			remote_session_add(arr[1], arr[2]);
		}

		if (done == 1) {
			sessions_OK = 1;
			debug_ok("SESSION: DONE\n");
		}

	} else if (!strcmp(cmd, "SESSIONINFO")) {
		if (done == 0 && arrcnt == 4) {
			session_t *s;

			if ((s = session_find(arr[1]))) {
				if (!strcmp(arr[2], "CONNECTED")) {
					int event;

					s->connected = (atoi(arr[3]) != 0);
					event = (s->connected) ? 1 : 2;

					query_emit_id(NULL, SESSION_EVENT, &s, &event);	/* Notify UI */

				} else if (!strcmp(arr[2], "STATUS")) {
					s->status = atoi(arr[3]);
					/* notify ui? */
				}
			}
			/* popsute */
		}

		if (done == 0 && (arrcnt == 3 || arrcnt == 4)) {
			session_t *s;

			if ((s = session_find(arr[1]))) {
				if (!strcmp(arr[2], "ALIAS")) {
					xfree(s->alias);
					s->alias = ((arrcnt == 4) ? xstrdup(arr[3]) : NULL);

					query_emit_id(NULL, SESSION_RENAMED, &(s->alias));	/* notify-ui */
				}
			}
			/* popsute */

		}

	} else if (!strcmp(cmd, "BACKLOG")) {
		if (done == 0 && arrcnt == 4) {
			int id = atoi(arr[1]);
			time_t ts = atoi(arr[2]);	/* XXX? atoi() */

			remote_print_window(id, ts, arr[3]);
		}

		if (done == 1) {
			backlog_OK = 1;
			debug_ok("BACKLOG: DONE\n");
		}

	} else if (!strcmp(cmd, "WINDOW") || !strcmp(cmd, "WINDOW_NEW")) {
		if (done == 0 && (arrcnt == 2 || arrcnt == 3)) {
			int id = atoi(arr[1]);

			remote_window_new(id, (arrcnt == 3) ? arr[2] : NULL);
		}

		if (done == 1) {
			windows_OK = 1;
			debug_ok("WINDOWS: DONE\n");
		}

	} else if (!strcmp(cmd, "WINDOWINFO")) {
		if (done == 0 && (arrcnt == 3 || arrcnt == 4)) {
			window_t *w;

			if ((w = window_exist(atoi(arr[1])))) {
				if (!strcmp(arr[2], "ALIAS")) {
					xfree(w->alias);
					w->alias = ((arrcnt == 4) ? xstrdup(arr[3]) : NULL);
					query_emit_id(NULL, UI_WINDOW_TARGET_CHANGED, &w);

				} else if (!strcmp(arr[2], "SESSION")) {
					window_session_set(w, ((arrcnt == 4) ? session_find(arr[3]) : NULL));
					/* window_session_set() notify ui */

				} else if (!strcmp(arr[2], "TARGET")) {
					xfree(w->target);
					w->target = ((arrcnt == 4) ? xstrdup(arr[3]) : NULL);
					query_emit_id(NULL, UI_WINDOW_TARGET_CHANGED, &w);

	/* IRCTOPIC/IRCTOPICBY/IRCTOPICMODE 
	 * XXX, informowac ui
	 * */

				} else if (!strcmp(arr[2], "IRCTOPIC")) {
					xfree(w->irctopic);
					w->irctopic = ((arrcnt == 4) ? xstrdup(arr[3]) : NULL);

				} else if (!strcmp(arr[2], "IRCTOPICBY")) {
					xfree(w->irctopicby);
					w->irctopicby = ((arrcnt == 4) ? xstrdup(arr[3]) : NULL);

				} else if (!strcmp(arr[2], "IRCTOPICMODE")) {
					xfree(w->ircmode);
					w->ircmode = ((arrcnt == 4) ? xstrdup(arr[3]) : NULL);

				} else if (!strcmp(arr[2], "ACTIVITY") && arrcnt == 4) {
					w->act = atoi(arr[3]);
					query_emit_id(NULL, UI_WINDOW_ACT_CHANGED, &w);
				}
			}
			/* popsute */
		}

		/* userlista */
	} else if (!strcmp(cmd, "USERLIST")) {		/* tylko potwierdzenie wyslania wszystkich WINDOWITEM, SESSIONITEM */
		if (done == 1) {
			userlist_OK = 1;
			debug_ok("USERLISTS: DONE\n");
		}

	} else if (!strcmp(cmd, "WINDOWITEM")) {
		if (done == 0 && (arrcnt > 2)) {
			window_t *w;

			if ((w = window_exist(atoi(arr[1])))) {
				remote_userlist_add_entry(&(w->userlist), &arr[2], arrcnt - 2);
				/* XXX, notify-ui */
			}
		}

	} else if (!strcmp(cmd, "SESSIONITEM")) {
		if (done == 0 && (arrcnt > 2)) {
			session_t *s;

			if ((s = session_find(arr[1]))) {
				remote_userlist_add_entry(&(s->userlist), &arr[2], arrcnt - 2);
				/* XXX, notify-ui */
			}
		}

	} else if (!strcmp(cmd, "USERINFO")) {
		if (done == 0 && (arrcnt == 4 || arrcnt == 5)) {
			userlist_t *u;

			if ((u = userlist_find(session_find(arr[1]), arr[2]))) {
				u->status = atoi(arr[3]);
				xfree(u->descr);
				u->descr = (arrcnt == 5) ? xstrdup(arr[4]) : NULL;

				query_emit_id(NULL, USERLIST_CHANGED, &(arr[1]), &(arr[2]));	/* notify-ui */
			}
		}

	} else if (!strcmp(cmd, "EXECUTE")) {		/* retvalue, don't care */
		windows_unlock_all();		/* XXX?! */

	} else if (!strcmp(cmd, "SESSION_CYCLE")) {	/* retvalue, don't care */

		/* === queries ==== */
	} else if (!strcmp(cmd, "WINDOW_CLEAR")) {
		if (arrcnt == 2) {
			int id = atoi(arr[1]);

			window_t *w;

			if ((w = window_exist(id))) {
				query_emit_id(NULL, UI_WINDOW_CLEAR, &w);

			} else {
				/* fucked */
			}
		}

	} else if (!strcmp(cmd, "WINDOW_PRINT")) {
		if (arrcnt == 4) {
			int id = atoi(arr[1]);
			time_t ts = atoi(arr[2]);	/* XXX? atoi() */
			char *val = arr[3];

			remote_print_window(id, ts, val);
		}


	} else if (!strcmp(cmd, "WINDOW_SWITCH")) {
		if (arrcnt == 2) {
			int id = atoi(arr[1]);

			remote_window_switch(id);
		}

	} else if (!strcmp(cmd, "WINDOW_KILL")) {
		if (arrcnt == 2) {
			int id = atoi(arr[1]);

			remote_window_kill(id);
		}

	} else if (!strcmp(cmd, "VARIABLE_CHANGED")) {		/* XXX, remote_variable_set() ? */
		if (arrcnt == 3 || arrcnt == 2)
			remote_variable_add(arr[1], (arrcnt == 3) ? arr[2] : NULL);


	} else if (!strcmp(cmd, "BEEP")) {
		if (arrcnt == 1) {
			query_emit_id(NULL, UI_BEEP, NULL);
		}

	} else if (!strcmp(cmd, "MAILCOUNT")) {
		if (arrcnt == 2)
			remote_mail_count = atoi(arr[1]);
	} else {
		debug_error("unknown request: %s\n", cmd);
	}

	array_free(arr);
	return 0;
}

#ifdef HAVE_SSL
static WATCHER(remote_read_ssl) {
	string_t str = (string_t) data;
	char buf[1024], *tmp;
	int ret, res = 0;

	if (type) {
		string_free(str, 1);
		remote_read_line(1, fd, NULL, NULL);
		return 0;
	}
again:
	ret = SSL_RECV(ssl_session, buf, sizeof(buf));

#ifdef WANT_OPENSSL
	if ((ret == 0 && SSL_get_error(ssl_session, ret) == SSL_ERROR_ZERO_RETURN)); /* connection shut down cleanly */
	else if (ret < 0) 
		ret = SSL_get_error(ssl_session, ret);
/* XXX, When an SSL_read() operation has to be repeated because of SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE, it must be repeated with the same arguments. */
#endif

	if (SSL_E_AGAIN(ret))
		return 0;

	if (ret > 0) {
		string_append_raw(str, buf, ret);
		read_total += ret;
	}

	if (ret == 0)
		string_append_c(str, '\n');

	while ((tmp = strchr(str->str, '\n'))) {
		size_t strlen = tmp - str->str;		/* get len of str from begining to \n char */
		char *line = xstrndup(str->str, strlen);	/* strndup() str with len == strlen */

		/* we strndup() str with len == strlen, so we don't need to call xstrlen() */
		if (strlen > 1 && line[strlen - 1] == '\r')
			line[strlen - 1] = 0;

		res = remote_read_line(0, fd, line, NULL);
		xfree(line);

		if (res == -1)
			break;

		string_remove(str, strlen + 1);
	}

	/* jeśli koniec strumienia, lub nie jest to ciągłe przeglądanie,
	 * zwolnij pamięć i usuń z listy */
	if (ret == 0 || (ret < 0))
		res = -1;

	if (res == -1)
		return res;

#ifdef WANT_GNUTLS
	if (gnutls_record_check_pending(ssl_session))
		goto again;
#endif
#ifdef WANT_OPENSSL
#warning "You're using openssl. This may be faulty!"
	if (ret == sizeof(buf))
		goto again;
#endif
	return 0;
}

static WATCHER_LINE(remote_write_ssl) {
	string_t str = (string_t) data;
	int res;

	if (type) {
		/* XXX */
		return 0;
	}

	res = SSL_SEND(ssl_session, watch, str->len);

#ifdef JABBER_HAVE_OPENSSL		/* OpenSSL */
	if ((res == 0 && SSL_get_error(j->ssl_session, res) == SSL_ERROR_ZERO_RETURN)); /* connection shut down cleanly */
	else if (res < 0) 
		res = SSL_get_error(j->ssl_session, res);
	/* XXX, When an SSL_write() operation has to be repeated because of SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE, it must be repeated with the same arguments. */
#endif

	if (SSL_E_AGAIN(res))
		return 0;

	if (res < 0)
		return -1;

	write_total += res;

	return res;
}
#endif

static WATCHER(remote_read) {
	string_t str = (string_t) data;
	char buf[1024], *tmp;
	int ret, res = 0;

	if (type) {
		string_free(str, 1);
		remote_read_line(1, fd, NULL, NULL);
		return 0;
	}

	ret = read(fd, buf, sizeof(buf));

	if (ret == -1 && (errno == EAGAIN))
		return 0;

	if (ret > 0) {
		string_append_raw(str, buf, ret);
		read_total += ret;
	}

	if (ret == 0)
		string_append_c(str, '\n');

	while ((tmp = strchr(str->str, '\n'))) {
		size_t strlen = tmp - str->str;		/* get len of str from begining to \n char */
		char *line = xstrndup(str->str, strlen);	/* strndup() str with len == strlen */

		/* we strndup() str with len == strlen, so we don't need to call xstrlen() */
		if (strlen > 1 && line[strlen - 1] == '\r')
			line[strlen - 1] = 0;

		res = remote_read_line(0, fd, line, NULL);
		xfree(line);

		if (res == -1)
			break;

		string_remove(str, strlen + 1);
	}

	/* jeśli koniec strumienia, lub nie jest to ciągłe przeglądanie,
	 * zwolnij pamięć i usuń z listy */
	if (ret == 0 || (ret < 0))
		res = -1;

	if (res == -1)
		return res;

	return 0;
}

static WATCHER_LINE(remote_write) {
	string_t str = (string_t) data;
	int res;

	if (type) {
		/* XXX */
		return 0;
	}

	res = write(fd, watch, str->len);

	if (res == -1 && (errno == EAGAIN))
		return 0;
	if (res < 0)
		return -1;

	write_total += res;

	return res;
}

int remote_connect2(int fd, const char *password) {
#ifdef HAVE_SSL
	if (ssl_session) {
		watch_t *w;

		watch_add(NULL, fd, WATCH_READ, remote_read_ssl, string_init(NULL));
		w = watch_add_line(NULL, fd, WATCH_WRITE_LINE, remote_write_ssl, NULL);
		w->data = w->buf;
	} else
#endif
	{
		watch_t *w;

		watch_add_line(NULL, fd, WATCH_READ, remote_read, string_init(NULL));
		w = watch_add_line(NULL, fd, WATCH_WRITE_LINE, remote_write, NULL);
		w->data = w->buf;
	}

	/* XXX, na poczatku polaczenia powinnismy dostac kodowanie serwera? */

	/* XXX, tutaj moga sie zaczac juz problemy z kodowaniem :) */

	remote_writefd(fd, "REQLOGIN", password, NULL);	/* Note: config_password can be NULL */

	while (!login_OK)
		ekg_loop();

	if (login_OK != 1) {
		close(fd);
		errno = EACCES;
		return 0;
	}

	remote_writefd(fd, "REQCONFIG", NULL);

	while (!variables_OK)
		ekg_loop();
	remote_recode_reinit();

	remote_writefd(fd, "REQCOMMANDS", NULL);
	remote_writefd(fd, "REQFORMATS", NULL);
	remote_writefd(fd, "REQPLUGINS", NULL);
	remote_writefd(fd, "REQSESSIONS", NULL);
	remote_writefd(fd, "REQWINDOWS", NULL);
	remote_writefd(fd, "REQUSERLISTS", NULL);

	while (!commands_OK || !formats_OK || !plugins_OK || !sessions_OK || !windows_OK || !userlist_OK)
		ekg_loop();

	remote_fd = fd;

	return 1;
}

int remote_connect3() {
	remote_writefd(remote_fd, "REQUICONFIG", ui_plugin->name, NULL);

	while (!ui_config_OK)
		ekg_loop();

	/* XXX, ostatnia szansa zeby wyswietlic jakis madry komunikat */

	/* Backlog dopiero po zaladowaniu UI-plugina */

	windows_lock_all();
	/* remote_writefd(remote_fd, "REQBACKLOGS", NULL); */
	/* remote_writefd(remote_fd, "REQBACKLOGS", "FROMTIME", "1221747609", NULL); */
	remote_writefd(remote_fd, "REQBACKLOGS", "LAST", "1000", NULL);

	while (!backlog_OK)
		ekg_loop();
	windows_unlock_all();

	return 1;
}

/* na szybko, human-readable */
static char *recalc(unsigned int i) {
	static char bufs[2][32];
	static int index = 0;
	char *tmp = bufs[index++];

	unsigned int reszta = 0;
	int j = 0;

	if (index > 2)
		index = 0;

	while (i >= 1000 && j < 3) {
		reszta = i % 1000;
		i /= 1000;
		j++;
	}

	if (reszta)
		snprintf(tmp, 30, "%u,%.3u ", i, reszta);
	else
		snprintf(tmp, 30, " %u ", i);

	if (j == 0)	strcat(tmp, " B");
	else if (j == 1)strcat(tmp, "KB");
	else if (j == 2)strcat(tmp, "MB");
	else		strcat(tmp, "GB");	/* lol? */

	return tmp;
}

void remote_print_stats() {
#ifdef HAVE_SSL
	if (ssl_used) {
	#ifdef WANT_GNUTLS
		if (ssl_xcred)
			gnutls_certificate_free_credentials(ssl_xcred);
	#endif
		SSL_GLOBAL_DEINIT();

		printf("SSL-recv: %10s (network: %10s) [ratio: %.2f%%]\n", recalc(read_total), recalc(ssl_read_total), 100.0 * (ssl_read_total / (float) read_total));
		printf("SSL-sent: %10s (network: %10s) [retio: %.2f%%]\n", recalc(write_total), recalc(ssl_write_total), 100.0 * (ssl_write_total / (float) write_total));
	} else
#endif

#ifdef HAVE_ZLIB
	if (zlib_used) { 
		/* deinit */

		printf("ZLIB-recv: %10s (network: %10s) [ratio: %.2f%%]\n", recalc(read_total), recalc(zlib_read_total), 100.0 * (zlib_read_total / (float) read_total));
		printf("ZLIB-sent: %10s (network: %10s) [retio: %.2f%%]\n", recalc(write_total), recalc(zlib_write_total), 100.0 * (zlib_write_total / (float) write_total));
	} else
#endif

	{
		printf("recv: %10s\n", recalc(read_total));
		printf("sent: %10s\n", recalc(write_total));
	}
}
