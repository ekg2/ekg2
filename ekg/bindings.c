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
