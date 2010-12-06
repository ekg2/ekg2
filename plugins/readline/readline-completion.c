#include "ekg2-config.h"

#include <stdio.h>
#include <strings.h>

#ifndef HAVE_STRLCPY
#  include <compat/strlcpy.h>
#endif

#ifdef HAVE_READLINE_READLINE_H
#	include <readline/readline.h>
#else
#	include <readline.h>
#endif

#include <ekg/dynstuff.h>
#include <ekg/events.h>
#include <ekg/metacontacts.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

#include "ui-readline.h"

static void *userlist;

/* podstawmy ewentualnie brakujące funkcje i definicje readline */
// extern void rl_extend_line_buffer(int len);
extern char **completion_matches();

#define GENERATOR(x) static char *x##_generator(char *text, int state) 


GENERATOR(possibilities) {
	static int len;

#warning "GENERATOR: possibilities TODO"
	return NULL;
}

GENERATOR(plugin) {
	static int len;
	static plugin_t *p;

	p = state ? (p?p->next:NULL) : plugins;
	len = xstrlen(text);

	while (p) {
		if (!xstrncasecmp(text, p->name, len)) 
			return xstrdup(p->name);

		if ((text[0] == '+' || text[0] == '-') && !xstrncasecmp(text + 1, p->name, len - 1))
			return saprintf("%c%s", text[0], p->name);

		p = p->next;
	}
	return NULL;
}

GENERATOR(events) {
	static int len;
	static int i;

	if (!events_all) return NULL;

	if (!state) 	i = 0;
	else if (events_all[i]) i++;

	len = xstrlen(text);

	while (events_all[i]) {
		if (!xstrncasecmp(text, events_all[i], len)) 
			return xstrdup(events_all[i++]);
		i++;
	}
	return NULL;
}

GENERATOR(sessions_var) {
	static int len;
	static session_t *el;
#warning "GENERATOR: sessions_var TODO"
	return NULL;

	if (!state) {
		len = xstrlen(text);
		el = sessions;
	}
}

GENERATOR(ignorelevels) {
	static int len;
	static int index;

	const char *tmp = NULL;
	char *pre = NULL;
	char *ble = NULL;

	if (!state) 	index = 0;
	else if (ignore_labels[index].name) index++;

	len = xstrlen(text);

	if ((tmp = xstrrchr(text, '|')) || (tmp = xstrrchr(text, ','))) {
		char *foo;

		pre = xstrdup(text);
		foo = xstrrchr(pre, *tmp);
		*(foo + 1) = 0;

		len -= tmp - text + 1;
		tmp = tmp + 1;
	} else
		tmp = text;

	while (ignore_labels[index].name) {
		if (!xstrncasecmp(tmp, ignore_labels[index].name, len)) {
			ble = (tmp == text) ?	xstrdup(ignore_labels[index].name) : 
						saprintf("%s%s", pre, ignore_labels[index].name);
			xfree(pre);
			return ble;
		}
		index++;
	}


	return NULL;
}

GENERATOR(dir) {
#warning "GENERATOR: dir TODO"
	return NULL;
}

GENERATOR(metacontacts) {
	static int len;
	static metacontact_t *m;

	if (!state)
		m = metacontacts;

	len = xstrlen(text);

	while (m) {
		char *mname = m->name;
		m = m->next;
		if (!xstrncasecmp(text, mname, len)) 
			return xstrdup(mname);
	}
	return NULL;
}

GENERATOR(theme) {
#warning "GENERATOR: theme TODO"
	return NULL;
}

GENERATOR(command) {
	static int len, plen = 0;
	static command_t *c;
	const char *slash = "", *dash = "";
	session_t *session;

	c = state ? (c?c->next:NULL) : commands;

	if (*text == '/') {
		slash = "/";
		text++;
	}

	if (*text == '^') {
		dash = "^";
		text++;
	}

	len = xstrlen(text);

	if (window_current->target) slash = "/";

	session = session_current;

	if (session && session->uid)
		plen =	(int)(xstrchr(session->uid, ':') - session->uid) + 1;

	while (c) {
		char *without_sess_id = NULL;

		if (session && !xstrncasecmp(c->name, session->uid, plen))
			without_sess_id = xstrchr(c->name, ':') + 1;

		if (!xstrncasecmp(text, c->name, len))
			return saprintf("%s%s%s", slash, dash, c->name);
		else if (without_sess_id && !xstrncasecmp(text, without_sess_id, len)) 
			return saprintf("%s%s%s", slash, dash, without_sess_id);
		c = c->next;
	}
	return NULL;
}

GENERATOR(conference) {
	static struct conference *c;
	static int len;

	if (!state)
		c = conferences;

	len = xstrlen(text);

	while (c) {
		c = c->next;
		
		if (!xstrncasecmp(text, c->name, len))
			return xstrdup(c->name);

	}
	return NULL;
}

GENERATOR(known_uin) {
	static userlist_t *el;
	static int len;
	static session_t *s;

	if (!session_current) return NULL;

	if (!state) {
		char *tmp;

		s = session_current;

		if ((tmp == xstrrchr(text, '/')) && tmp+1) {
			/* XXX, find session */

		}

		el = s->userlist;
	}

	len = xstrlen(text);

/* XXX, search window_current->userlist && conference */

	while (el) {
		userlist_t *u = el;

		el = el->next;

		if (!xstrncasecmp(text, u->nickname, len))
			return (session_current != s) ?
				saprintf("%s/%s", s->uid, u->nickname) : 
				xstrdup(u->nickname);


		if (!xstrncasecmp(text, u->uid, len))
			return (session_current != s) ?
				saprintf("%s/%s", s->uid, u->uid) : 
				xstrdup(u->uid);

	}

	return NULL;
}

GENERATOR(unknown_uin) {
	static int index, len;

	if (!state) 	index = 0;
	else		index ++;

	len = xstrlen(text);

	while (index < send_nicks_count) {
		if (send_nicks[index] && xstrchr(send_nicks[index], ':') && xisdigit(xstrchr(send_nicks[index], ':')[1]) && !xstrncasecmp(text, send_nicks[index], len))
			return xstrdup(send_nicks[index++]);
		index++;
	}

	return NULL;
}

GENERATOR(variable) {
	static variable_t *v;
	static int len;

	v = state ? (v?v->next:NULL) : variables;

	len = xstrlen(text);

	while (v) {
		if (*text == '-') {
			if (!xstrncasecmp(text + 1, v->name, len - 1))
				return saprintf("-%s", v->name);
		} else {
			if (!xstrncasecmp(text, v->name, len))
				return xstrdup(v->name);
		}

		v = v->next;
	}

	return NULL;
}

GENERATOR(ignored_uin) {
#warning "GENERATOR: ignored uin NOT COPIED"
	static list_t l;
	static int len;

	if (!session_current) 
		return NULL;

	if (!state)
		l = userlist;

	len = xstrlen(text);

	while (l) {
		userlist_t *u = l->data;

		l = l->next;

		if (!ignored_check(session_current, u->uid))
			continue;
#if 0 /* dark */
		if (!u->display) {
			if (!xstrncasecmp(text, itoa(u->uin), len))
				return xstrdup(itoa(u->uin));
		} else {
			if (!xstrncasecmp(text, u->display, len))
				return ((xstrchr(u->display, ' ')) ? saprintf("\"%s\"", u->display) : xstrdup(u->display));
		}
#endif
	}

	return NULL;
}

GENERATOR(blocked_uin) {
	static userlist_t *el;
	static int len;
	session_t *s = session_current;

	if (!s) 
		return NULL;

	if (!state)
		el = s->userlist;

	len = xstrlen(text);

	while (el) {
		userlist_t *u = el;

		el = el->next;

		if (!ekg_group_member(u, "__blocked"))
			continue;

		if (!xstrncasecmp(text, u->nickname, len)) 
			return (xstrchr(u->nickname, ' ')) ?	saprintf("\"%s\"", u->nickname) :
								xstrdup(u->nickname);

		if (!xstrncasecmp(text, u->uid, len)) 
			return xstrdup(u->uid);
	}

	return NULL;
}

GENERATOR(window) {
	static window_t *w;
	static int len;

	w = state ? (w?w->next:NULL) : windows;

	len = xstrlen(text);

	while (w) {
		if (!xstrncmp(text, w->target, len))
			return xstrdup(w->target);

		w = w->next;
	}
	return NULL;
}

GENERATOR(reason) {
	session_t *s = session_current;

	if (!s || !s->descr) return NULL;

	if (!xstrncmp(text, s->descr, xstrlen(text)))
		return xstrdup(s->descr);

	return NULL;
}

GENERATOR(session) {
	static session_t *s;
	static int len;

	s = state ? (s?s->next:NULL) : sessions;

	len = xstrlen(text);

	while (s) {
		if (*text == '-') {
			if (!xstrncasecmp(text+1, s->uid, len-1))
				return saprintf("-%s", s->uid);
			if (!xstrncasecmp(text+1, s->alias, len-1))
				return saprintf("-%s", s->alias);
		} else {
			if (!xstrncasecmp(text, s->uid, len)) 
				return xstrdup(s->uid);
			if (!xstrncasecmp(text, s->alias, len)) 
				return xstrdup(s->alias);
		}
		s = s->next;
	} /* XXX: shouldn't we iterate or do sth here? I think can get into deadlock */
	return NULL;
}

char *empty_generator(char *text, int state) {
	return NULL;
}

static char *wiechu_params;

GENERATOR(wiechu) {
	static int i;
	char *ret;

	if (!state) 	i = 0;

	while (wiechu_params[i]) {
		switch (wiechu_params[i]) {
			case 'u': ret = known_uin_generator(text, state);	break;
			case 'C': ret = conference_generator(text, state);	break;
			case 'U': ret = unknown_uin_generator(text, state);	break;
			case 'c': ret = command_generator(text, state);		break;
			case 'i': ret = ignored_uin_generator(text, state);	break;
			case 'b': ret = blocked_uin_generator(text, state);	break;
			case 'v': ret = variable_generator(text, state);	break;
			case 'p': ret = possibilities_generator(text, state);	break;
			case 'P': ret = plugin_generator(text, state);		break;
			case 'w': ret = window_generator(text, state);		break;
			case 'f': ret = rl_filename_completion_function(text, state); break;
			case 'e': ret = events_generator(text, state);		break;
			case 's': ret = session_generator(text, state);		break;
			case 'S': ret = sessions_var_generator(text, state);	break;
			case 'I': ret = ignorelevels_generator(text, state);	break;
			case 'r': ret = reason_generator(text, state);		break;
			case 't': ret = theme_generator(text, state);		break;
			case 'o': ret = dir_generator(text, state);		break;
			case 'm': ret = metacontacts_generator(text, state);	break;
			case 'x':
			default:  ret = NULL;					break;
		}
		if (ret)
			return ret;
		i++;
	}
	return NULL;
}

char **my_completion(char *text, int start, int end) {
	char **params = NULL;
	int word = 0, i, abbrs = 0;
	CPFunction *func = known_uin_generator;
	command_t *c;
	static int my_send_nicks_count = 0;

	if (start == 0)
		return completion_matches(text, command_generator);

	char *tmp = rl_line_buffer, *cmd = (config_tab_command) ? config_tab_command : "chat";
	int slash = 0;

	if (*tmp == '/') {
		slash = 1;
		tmp++;
	}

	if (!xstrncasecmp(tmp, cmd, xstrlen(cmd)) && tmp[xstrlen(cmd)] == ' ') {
		int in_quote = 0;
		word = 0;
		for (i = 0; i < xstrlen(rl_line_buffer); i++) {
			if (rl_line_buffer[i] == '"')
				in_quote = !in_quote;

			if (xisspace(rl_line_buffer[i]) && !in_quote)
				word++;
		}
		if (word == 2 && xisspace(rl_line_buffer[xstrlen(rl_line_buffer) - 1])) {
			if (send_nicks_count != my_send_nicks_count) {
				my_send_nicks_count = send_nicks_count;
				send_nicks_index = 0;
			}

			if (send_nicks_count > 0) {
				char buf[100], *tmp;

				tmp = ((xstrchr(send_nicks[send_nicks_index], ' ')) ? saprintf("\"%s\"", send_nicks[send_nicks_index]) : xstrdup(send_nicks[send_nicks_index]));
				snprintf(buf, sizeof(buf), "%s%s %s ", (slash) ? "/" : "", cmd, tmp);
				xfree(tmp);
				send_nicks_index++;
				rl_extend_line_buffer(xstrlen(buf));
				strlcpy(rl_line_buffer, buf, xstrlen(buf) + 1);
				rl_end = xstrlen(buf);
				rl_point = rl_end;
				rl_redisplay();
			}

			if (send_nicks_index == send_nicks_count)
				send_nicks_index = 0;
					
			return NULL;
		}
		word = 0;
	}


	int in_quote = 0;

	for (i = 1; i <= start; i++) {
		if (rl_line_buffer[i] == '"')
			in_quote = !in_quote;

		if (xisspace(rl_line_buffer[i]) && !xisspace(rl_line_buffer[i - 1]) && !in_quote)
			word++;
	}
	word--;

	for (c = commands; c; c = c->next) {
		int len = xstrlen(c->name);
		char *cmd = (*rl_line_buffer == '/') ? rl_line_buffer + 1 : rl_line_buffer;

		if (!xstrncasecmp(cmd, c->name, len) && xisspace(cmd[len])) {
			params = c->params;
			abbrs = 1;
			break;
		}
			
		for (len = 0; cmd[len] && cmd[len] != ' '; len++);

		if (!xstrncasecmp(cmd, c->name, len)) {
			params = c->params;
			abbrs++;
		} else
			if (params && abbrs == 1)
				break;
	}

	if (params && abbrs == 1) {
		if (word >= /* xstrlen(params) */ array_count(params))
			func = empty_generator;
		else {
			wiechu_params = params[word];
			func = wiechu_generator;
		}
	}

	return completion_matches(text, func);
}
