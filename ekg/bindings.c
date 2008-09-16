#include <string.h>

#include "commands.h"
#include "dynstuff.h"
#include "themes.h"
#include "plugins.h"
#include "queries.h"
#include "stuff.h"
#include "xmalloc.h"

#include "bindings.h"

#include "dynstuff_inline.h"

struct binding *bindings = NULL;
binding_added_t *bindings_added;

/*
 * binding_list()
 *
 * wyświetla listę przypisanych komend.
 */
void binding_list(int quiet, const char *name, int all) 
{
	struct binding *b;
	int found = 0;

	if (!bindings)
		printq("bind_seq_list_empty");

	for (b = bindings; b; b = b->next) {
		if (name) {
			if (xstrcasestr(b->key, name)) {
				printq("bind_seq_list", b->key, b->action);
				found = 1;
			}
			continue;
		}

		if (!b->internal || (all && b->internal)) 
			printq("bind_seq_list", b->key, b->action);
	}

	if (name && !found) {
		for (b = bindings; b; b = b->next) {
			if (xstrcasestr(b->action, name))
				printq("bind_seq_list", b->key, b->action);
		}
	}
}

/*
 * binding_quick_list()
 *
 * wyświetla krótką i zwięzła listę dostępnych, zajętych i niewidocznych
 * ludzi z listy kontaktów.
 */
int binding_quick_list(int a, int b)
{
	string_t list = string_init(NULL);
	userlist_t *ul;
	session_t *s;

	for (s = sessions; s; s = s->next) {
		for (ul = s->userlist; ul; ul = ul->next) {
			userlist_t *u = ul;
			const char *format;

			if (!u->nickname)
				continue;
		
			format = format_find(ekg_status_label(u->status, NULL, "quick_list_"));

			if (format_ok(format)) {
				char *tmp = format_string(format, u->nickname);
				string_append(list, tmp);

				xfree(tmp);
			}
		}
	}

	if (list->len > 0)
		print("quick_list", list->str);

	string_free(list, 1);

	return 0;
}

int binding_help(int a, int b)	
{
	print("help_quick");  

	return 0;  
}

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

/**
 * binding_free()
 *
 * Free memory allocated for key bindings.
 */

void binding_free() {
	bindings_destroy();
	bindings_added_destroy();
}

COMMAND(cmd_bind) {
	if (match_arg(params[0], 'a', ("add"), 2)) {
		if (!params[1] || !params[2]) {
			printq("not_enough_params", name);
			return -1;
		}
		query_emit_id(NULL, BINDING_COMMAND, (int) 1, params[1], params[2], quiet);
/*		ncurses_binding_add(p2, p3, 0, quiet); */
		return 0;
	}
	if (match_arg(params[0], 'd', ("delete"), 2)) {
		if (!params[1]) {
			printq("not_enough_params", ("bind"));
			return -1;
		}

		query_emit_id(NULL, BINDING_COMMAND, (int) 0, params[1], NULL, quiet);
/*		ncurses_binding_delete(p2, quiet); */
		return 0;
	} 
	if (match_arg(params[0], 'L', ("list-default"), 5)) {
		binding_list(quiet, params[1], 1);
		return 0;
	} 
	if (match_arg(params[0], 'S', ("set"), 2)) {
		window_lock_dec(window_find_s(session, target)); /* this is interactive command. XXX, what about window_current? */

		query_emit_id(NULL, BINDING_SET, params[1], NULL, quiet);
		return 0;
	}
	if (match_arg(params[0], 'l', ("list"), 2)) {
		binding_list(quiet, params[1], 0);
		return 0;
	}
	binding_list(quiet, params[0], 0);

	return 0;
}

static int binding_key(struct binding *b, const char *key, int add)
{
	/* debug("Key: %s\n", key); */
	if (!xstrncasecmp(key, ("Alt-"), 4)) {
		unsigned char ch;

		if (!xstrcasecmp(key + 4, ("Enter"))) {
			b->key = xstrdup(("Alt-Enter"));
			if (add)
				ncurses_binding_map_meta[13] = LIST_ADD2(&bindings, xmemdup(b, sizeof(struct binding)));
			return 0;
		}

		if (!xstrcasecmp(key + 4, ("Backspace"))) {
			b->key = xstrdup(("Alt-Backspace"));
			if (add) {
				ncurses_binding_map_meta[KEY_BACKSPACE] = LIST_ADD2(&bindings, xmemdup(b, sizeof(struct binding)));
				ncurses_binding_map_meta[127] = ncurses_binding_map_meta[KEY_BACKSPACE];
			}
			return 0;
		}

		if (xstrlen(key) != 5)
			return -1;
	
		ch = xtoupper(key[4]);

		b->key = saprintf(("Alt-%c"), ch);

		if (add) {
			ncurses_binding_map_meta[ch] = LIST_ADD2(&bindings, xmemdup(b, sizeof(struct binding)));
			if (xisalpha(ch))
				ncurses_binding_map_meta[xtolower(ch)] = ncurses_binding_map_meta[ch];
		}

		return 0;
	}

	if (!xstrncasecmp(key, ("Ctrl-"), 5)) {
		unsigned char ch;
		
//		if (xstrlen(key) != 6)
//			return -1;
#define __key(x, y, z) \
	if (!xstrcasecmp(key + 5, (x))) { \
		b->key = xstrdup(key); \
		if (add) { \
			ncurses_binding_map[y] = LIST_ADD2(&bindings, xmemdup(b, sizeof(struct binding))); \
			if (z) \
				ncurses_binding_map[z] = ncurses_binding_map[y]; \
		} \
		return 0; \
	}

	__key("Enter", KEY_CTRL_ENTER, 0);
	__key("Escape", KEY_CTRL_ESCAPE, 0);
	__key("Home", KEY_CTRL_HOME, 0);
	__key("End", KEY_CTRL_END, 0);
	__key("Delete", KEY_CTRL_DC, 0);
	__key("Backspace", KEY_CTRL_BACKSPACE, 0);
	__key("Tab", KEY_CTRL_TAB, 0);

#undef __key
	
		ch = xtoupper(key[5]);
		b->key = saprintf(("Ctrl-%c"), ch);

		if (add) {
			if (xisalpha(ch))
				ncurses_binding_map[ch - 64] = LIST_ADD2(&bindings, xmemdup(b, sizeof(struct binding)));
			else
				return -1;
		}
		
		return 0;
	}

	if (xtoupper(key[0]) == 'F' && atoi(key + 1)) {
		int f = atoi(key + 1);

		if (f < 1 || f > 24)
			return -1;

		b->key = saprintf(("F%d"), f);
		
		if (add)
			ncurses_binding_map[KEY_F(f)] = LIST_ADD2(&bindings, xmemdup(b, sizeof(struct binding)));
		
		return 0;
	}

#define __key(x, y, z) \
	if (!xstrcasecmp(key, (x))) { \
		b->key = xstrdup((x)); \
		if (add) { \
			ncurses_binding_map[y] = LIST_ADD2(&bindings, xmemdup(b, sizeof(struct binding))); \
			if (z) \
				ncurses_binding_map[z] = ncurses_binding_map[y]; \
		} \
		return 0; \
	}

	__key("Enter", 13, 0);
	__key("Escape", 27, 0);
	__key("Home", KEY_HOME, KEY_FIND);
	__key("End", KEY_END, KEY_SELECT);
	__key("Delete", KEY_DC, 0);
	__key("Backspace", KEY_BACKSPACE, 127);
	__key("Tab", 9, 0);
	__key("Left", KEY_LEFT, 0);
	__key("Right", KEY_RIGHT, 0);
	__key("Up", KEY_UP, 0);
	__key("Down", KEY_DOWN, 0);
	__key("PageUp", KEY_PPAGE, 0);
	__key("PageDown", KEY_NPAGE, 0);

#undef __key

	return -1;
}

void ncurses_binding_add(const char *key, const char *action, int internal, int quiet)
{
	struct binding b, *c = NULL, *d;
	
	if (!key || !action)
		return;

	memset(&b, 0, sizeof(b));

	b.internal = internal;
	
	for (d = bindings; d; d = d->next) {
		if (!xstrcasecmp(key, d->key)) {
			if (d->internal) {
				c = d;
				break;
			}
			printq("bind_seq_exist", d->key);
			return;
		}
	}

	binding_parse(&b, action);

	if (internal) {
		b.default_action	= xstrdup(b.action);
		b.default_function	= b.function;
		b.default_arg		= xstrdup(b.arg);
	}

	if (binding_key(&b, key, (c) ? 0 : 1)) {
		printq("bind_seq_incorrect", key);
		xfree(b.action);
		xfree(b.arg);
		xfree(b.default_action);
		xfree(b.default_arg);
		xfree(b.key);
	} else {
		printq("bind_seq_add", b.key);

		if (c) {
			xfree(c->action);
			c->action = b.action;
			xfree(c->arg);
			c->arg = b.arg;
			c->function = b.function;
			xfree(b.default_action);
			xfree(b.default_arg);
			xfree(b.key);
			c->internal = 0;
		}

		if (!in_autoexec)
			config_changed = 1;
	}
}

void bindings_default() {
	/* ncurses bindings */

	ncurses_binding_add("Alt-`", "/window switch 0", 1, 1);
	ncurses_binding_add("Alt-1", "/window switch 1", 1, 1);
	ncurses_binding_add("Alt-2", "/window switch 2", 1, 1);
	ncurses_binding_add("Alt-3", "/window switch 3", 1, 1);
	ncurses_binding_add("Alt-4", "/window switch 4", 1, 1);
	ncurses_binding_add("Alt-5", "/window switch 5", 1, 1);
	ncurses_binding_add("Alt-6", "/window switch 6", 1, 1);
	ncurses_binding_add("Alt-7", "/window switch 7", 1, 1);
	ncurses_binding_add("Alt-8", "/window switch 8", 1, 1);
	ncurses_binding_add("Alt-9", "/window switch 9", 1, 1);
	ncurses_binding_add("Alt-0", "/window switch 10", 1, 1);
	ncurses_binding_add("Alt-Q", "/window switch 11", 1, 1);
	ncurses_binding_add("Alt-W", "/window switch 12", 1, 1);
	ncurses_binding_add("Alt-E", "/window switch 13", 1, 1);
	ncurses_binding_add("Alt-R", "/window switch 14", 1, 1);
	ncurses_binding_add("Alt-T", "/window switch 15", 1, 1);
	ncurses_binding_add("Alt-Y", "/window switch 16", 1, 1);
	ncurses_binding_add("Alt-U", "/window switch 17", 1, 1);
	ncurses_binding_add("Alt-I", "/window switch 18", 1, 1);
	ncurses_binding_add("Alt-O", "/window switch 19", 1, 1);
	ncurses_binding_add("Alt-P", "/window switch 20", 1, 1);
	ncurses_binding_add("Alt-K", "window-kill", 1, 1);
	ncurses_binding_add("Alt-N", "/window new", 1, 1);
	ncurses_binding_add("Alt-A", "/window active", 1, 1);
	ncurses_binding_add("Alt-G", "ignore-query", 1, 1);
	ncurses_binding_add("Alt-B", "backward-word", 1, 1);
	ncurses_binding_add("Alt-F", "forward-word", 1, 1);
	ncurses_binding_add("Alt-D", "kill-word", 1, 1);
	ncurses_binding_add("Alt-Enter", "toggle-input", 1, 1);
	ncurses_binding_add("Escape", "cancel-input", 1, 1);
	ncurses_binding_add("Ctrl-N", "/window next", 1, 1);
	ncurses_binding_add("Ctrl-P", "/window prev", 1, 1);
	ncurses_binding_add("Backspace", "backward-delete-char", 1, 1);
	ncurses_binding_add("Ctrl-H", "backward-delete-char", 1, 1);
	ncurses_binding_add("Ctrl-A", "beginning-of-line", 1, 1);
	ncurses_binding_add("Home", "beginning-of-line", 1, 1);
	ncurses_binding_add("Ctrl-D", "delete-char", 1, 1);
	ncurses_binding_add("Delete", "delete-char", 1, 1);
	ncurses_binding_add("Ctrl-E", "end-of-line", 1, 1);
	ncurses_binding_add("End", "end-of-line", 1, 1);
	ncurses_binding_add("Ctrl-K", "kill-line", 1, 1);
	ncurses_binding_add("Ctrl-Y", "yank", 1, 1);
	ncurses_binding_add("Enter", "accept-line", 1, 1);
	ncurses_binding_add("Ctrl-M", "accept-line", 1, 1);
	ncurses_binding_add("Ctrl-U", "line-discard", 1, 1);
	ncurses_binding_add("Ctrl-V", "quoted-insert", 1, 1);
	ncurses_binding_add("Ctrl-W", "word-rubout", 1, 1);
	ncurses_binding_add("Alt-Backspace", "word-rubout", 1, 1);
	ncurses_binding_add("Ctrl-L", "/window refresh", 1, 1);
	ncurses_binding_add("Tab", "complete", 1, 1);
	ncurses_binding_add("Right", "forward-char", 1, 1);
	ncurses_binding_add("Left", "backward-char", 1, 1);
	ncurses_binding_add("Up", "previous-history", 1, 1);
	ncurses_binding_add("Down", "next-history", 1, 1);
	ncurses_binding_add("PageUp", "backward-page", 1, 1);
	ncurses_binding_add("Ctrl-F", "backward-page", 1, 1);
	ncurses_binding_add("PageDown", "forward-page", 1, 1);
	ncurses_binding_add("Ctrl-G", "forward-page", 1, 1);
	ncurses_binding_add("Ctrl-X", "cycle-sessions", 1, 1);
	ncurses_binding_add("F1", "/help", 1, 1);
	ncurses_binding_add("F2", "quick-list", 1, 1);
	ncurses_binding_add("F3", "toggle-contacts", 1, 1);
	ncurses_binding_add("F4", "next-contacts-group", 1, 1);
	ncurses_binding_add("F12", "/window switch 0", 1, 1);
	ncurses_binding_add("F11", "ui-ncurses-debug-toggle", 1, 1);
	/* ncurses_binding_add("Ctrl-Down", "forward-contacts-page", 1, 1); 
	ncurses_binding_add("Ctrl-Up", "backward-contacts-page", 1, 1); */

	query_emit_id(NULL, BINDING_DEFAULT);		/* allow ui's to process binding-list */		/* XXX, change name */
}

void bindings_init() {
	bindings_default();
}

