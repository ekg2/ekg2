/* $Id: stuff.c 4597 2008-09-03 21:10:01Z darkjames $ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Robert J. Wo¼ny <speedy@ziew.org>
 *			    Pawe³ Maziarz <drg@o2.pl>
 *			    Dawid Jarosz <dawjar@poczta.onet.pl>
 *			    Piotr Domagalski <szalik@szalik.net>
 *			    Adam Mikuta <adammikuta@poczta.onet.pl>
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

#include <ctype.h>
#include <sys/time.h>
#include <time.h>

#include <stdarg.h>
#include <string.h>

#include "debug.h"
#include "dynstuff.h"
#include "plugins.h"
#include "stuff.h"
#include "xmalloc.h"

#include "dynstuff_inline.h"

EXPORTNOT struct timer *timers = NULL;

struct binding *bindings = NULL;
binding_added_t *bindings_added;

static LIST_FREE_ITEM(timer_free_item, struct timer *) { data->function(1, data->data); xfree(data->name); }

DYNSTUFF_LIST_DECLARE2(timers, struct timer, timer_free_item,
	static __DYNSTUFF_LIST_ADD,			/* timers_add() */
	__DYNSTUFF_LIST_REMOVE_SAFE,			/* timers_remove() */
	EXPORTNOT __DYNSTUFF_LIST_REMOVE_ITER,		/* timers_removei() */
	EXPORTNOT __DYNSTUFF_LIST_DESTROY)		/* timers_destroy() */

EXPORTNOT int old_stderr;

EXPORTNOT int config_make_window = 6;
EXPORTNOT int config_debug = 1;
EXPORTNOT char *server_console_charset;
EXPORTNOT int config_slash_messages = 0;
EXPORTNOT int config_default_status_window = 0;
EXPORTNOT int config_query_commands = 0;
EXPORTNOT int config_display_welcome = 1;
char *config_console_charset;
int config_sort_windows = 1;
char *config_timestamp = NULL;
int config_timestamp_show = 1;
int config_use_unicode;
int config_use_iso;
int config_display_color = 1;
int config_history_savedups = 1;
int config_display_pl_chars = 1;

char *config_tab_command = NULL;
int config_save_quit = 1;
int config_lastlog_noitems = 0;	
int config_lastlog_case = 0;
int config_lastlog_display_all = 0;
char *config_completion_char = NULL;

int config_changed = 0;
int in_autoexec = 0;

static LIST_FREE_ITEM(binding_free_item, struct binding *) {
	xfree(data->key);
	xfree(data->action);
	xfree(data->arg);
	xfree(data->default_action);
	xfree(data->default_arg);
}

static LIST_FREE_ITEM(binding_added_free_item, binding_added_t *) {
	xfree(data->sequence);
}

static __DYNSTUFF_LIST_DESTROY(bindings, struct binding, binding_free_item);				/* bindings_destroy() */
static __DYNSTUFF_LIST_DESTROY(bindings_added, binding_added_t, binding_added_free_item);		/* bindings_added_destroy() */

EXPORTNOT void binding_free() {
	bindings_destroy();
	bindings_added_destroy();
}

const char *compile_time() {
	return __DATE__ " " __TIME__;
}

void iso_to_ascii(unsigned char *buf) {
#if (USE_UNICODE || HAVE_GTK)
	if (config_use_unicode) return;
#endif
	if (!buf)
		return;

	while (*buf) {
		if (*buf == (unsigned char)'±') *buf = 'a';
		if (*buf == (unsigned char)'ê') *buf = 'e';
		if (*buf == (unsigned char)'æ') *buf = 'c';
		if (*buf == (unsigned char)'³') *buf = 'l';
		if (*buf == (unsigned char)'ñ') *buf = 'n';
		if (*buf == (unsigned char)'ó') *buf = 'o';
		if (*buf == (unsigned char)'¶') *buf = 's';
		if (*buf == (unsigned char)'¿') *buf = 'z';
		if (*buf == (unsigned char)'¼') *buf = 'z';

		if (*buf == (unsigned char)'¡') *buf = 'A';
		if (*buf == (unsigned char)'Ê') *buf = 'E';
		if (*buf == (unsigned char)'Æ') *buf = 'C';
		if (*buf == (unsigned char)'£') *buf = 'L';
		if (*buf == (unsigned char)'Ñ') *buf = 'N';
		if (*buf == (unsigned char)'Ó') *buf = 'O';
		if (*buf == (unsigned char)'¦') *buf = 'S';
		if (*buf == (unsigned char)'¯') *buf = 'Z';
		if (*buf == (unsigned char)'¬') *buf = 'Z';

		buf++;
	}
}

const char *timestamp(const char *format) {
	static char buf[100];
	time_t t;
	struct tm *tm;

	if (!format || format[0] == '\0')
		return "";

	t = time(NULL);
	tm = localtime(&t);
	if (!strftime(buf, sizeof(buf), format, tm))
		return "TOOLONG";
	return buf;
}

const char *timestamp_time(const char *format, time_t t) {
	struct tm *tm;
	static char buf[100];

	if (!format || format[0] == '\0')
		return itoa(t);

	tm = localtime(&t);

	if (!strftime(buf, sizeof(buf), format, tm))
		return "TOOLONG";
	return buf;
}

struct timer *timer_add_ms(plugin_t *plugin, const char *name, unsigned int period, int persist, int (*function)(int, void *), void *data) {
	struct timer *t;
	struct timeval tv;

	/* wylosuj now± nazwê, je¶li nie mamy */
	if (!name)
		debug_error("timer_add() without name\n");

	t = xmalloc(sizeof(struct timer));
	gettimeofday(&tv, NULL);
	tv.tv_sec += (period / 1000);
	tv.tv_usec += ((period % 1000) * 1000);
	if (tv.tv_usec >= 1000000) {
		tv.tv_usec -= 1000000;
		tv.tv_sec++;
	}
	memcpy(&(t->ends), &tv, sizeof(tv));
	t->name = xstrdup(name);
	t->period = period;
	t->persist = persist;
	t->function = function;
	t->data = data;
	t->plugin = plugin;

	timers_add(t);
	return t;
}

struct timer *timer_add(plugin_t *plugin, const char *name, unsigned int period, int persist, int (*function)(int, void *), void *data) {
	return timer_add(plugin, name, period * 1000, persist, function, data);
}

int timer_remove(plugin_t *plugin, const char *name)
{
	struct timer *t;
	int removed = 0;

	for (t = timers; t; t = t->next) {
		if (t->plugin == plugin && !xstrcasecmp(name, t->name)) {
			t = timers_removei(t);
			removed++;
		}
	}

	return ((removed) ? 0 : -1);
}

int isalpha_pl(unsigned char c)
{
    if(isalpha(c)) /* normalne znaki */
	return 1;
    else if(c == 177 || c == 230 || c == 234 || c == 179 || c == 241 || c == 243 || c == 182 || c == 191 || c == 188) /* polskie literki */
	return 1;
    else if(c == 161 || c == 198 || c == 202 || c == 209 || c == 163 || c == 211 || c == 166 || c == 175 || c == 172) /* wielka litery polskie */
	return 1;
    else
	return 0;
}

void debug_ext(debug_level_t level, const char *format, ...) {
	va_list ap;

	if (!config_debug)
		return;

	va_start(ap, format);
	ekg_debug_handler(level, format, ap);
	va_end(ap);
}

void debug(const char *format, ...) {
	va_list ap;

	if (!config_debug)
		return;

	va_start(ap, format);
	ekg_debug_handler(0, format, ap);
	va_end(ap);
}

const char *ekg_status_string(const int status, const int cmd) {
	if (cmd)
		debug_error("ekg_status_string() %d with cmd: %d\n", status, cmd);

	switch (status) {
		case EKG_STATUS_ERROR: 		return "error";
		case EKG_STATUS_BLOCKED: 	return "blocking";
		case EKG_STATUS_UNKNOWN:	return "unknown";
		case EKG_STATUS_NA:		return "notavail";
		case EKG_STATUS_INVISIBLE:	return "invisible";
		case EKG_STATUS_DND:		return "dnd";
		case EKG_STATUS_GONE:		return "gone";
		case EKG_STATUS_XA:		return "xa";
		case EKG_STATUS_AWAY:		return "away";
		case EKG_STATUS_AVAIL:		return "avail";
		case EKG_STATUS_FFC:		return "chat";
	}

	debug_error("ekg_status_string(): called with unexpected status: 0x%02x\n", status);
	return "unknown";
}

char *saprintf(const char *format, ...)
{
	va_list ap;
	char *res;

	va_start(ap, format);
	res = vsaprintf(format, ap);
	va_end(ap);

	return res;
}

EXPORTNOT int ekg_write(int fd, const char *buf, int len) {
	watch_t *wl = NULL;
	list_t l;

	if (fd == -1)
		return -1;

	/* first check if we have watch for this fd */
	for (l = watches; l; l = l->next) {
		watch_t *w = l->data;

		if (w && w->fd == fd && w->type == WATCH_WRITE && w->buf) {
			wl = w;
			break;
		}
	}
	if (!wl) {
		/* if we have no watch, let's create it. */	/* XXX, first try write() ? */
		wl = watch_add(NULL, fd, WATCH_WRITE_LINE, NULL, NULL);
	}

	return watch_write(wl, buf, len);
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: noet
 */
