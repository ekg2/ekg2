#include <stdio.h>
#include <strings.h>

#ifndef HAVE_STRLCPY
#  include <compat/strlcpy.h>
#endif

#include <readline.h>

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
	static list_t el;

	if (!state) {
		len = xstrlen(text);
		el = plugins;
	}

	while (el) {
		plugin_t *p = el->data;

		el = el->next;

		if (!xstrncasecmp(text, p->name, len)) 
			return xstrdup(p->name);

		if ((text[0] == '+' || text[0] == '-') && !xstrncasecmp(text + 1, p->name, len - 1))
			return saprintf("%c%s", text[0], p->name);
	}
	return NULL;
}

GENERATOR(events) {
	static int len;
	static int i;

	if (!events_all) return NULL;

	if (!state) {
		len = xstrlen(text);
		i = 0;
	}

	while (events_all[i]) {
		if (!xstrncasecmp(text, events_all[i], len)) 
			return xstrdup(events_all[i++]);
		i++;
	}
	return NULL;
}

GENERATOR(sessions_var) {
	static int len;
	static list_t el;
#warning "GENERATOR: sessions_var TODO"
	return NULL;

	if (!state) {
		len = xstrlen(text);
		el = sessions;
	}

}

GENERATOR(ignorelevels) {
#warning "GENERATOR: ignorelevels TODO"
	return NULL;
}

GENERATOR(dir) {
#warning "GENERATOR: dir TODO"
	return NULL;
}

GENERATOR(metacontacts) {
	static int len;
	static list_t el;

	if (!state) {
		len = xstrlen(text);
		el = metacontacts;
	}

	while (el) {
		metacontact_t *m = el->data;

		el = el->next;

		if (!xstrncasecmp(text, m->name, len)) 
			return xstrdup(m->name);
	}
	return NULL;
}

GENERATOR(theme) {
#warning "GENERATOR: theme TODO"
	return NULL;
}

GENERATOR(command) {
	static int len;
	static list_t el;
	int slash = 0;
	int dash = 0;

	#warning "XXX TOTAJ JAK NIE MA NIC WPISANEGO XXX"
#if 0
	if (!state) {


		char *cmd = saprintf(("/%s "), (config_tab_command) ? config_tab_command : "chat");

		/* nietypowe dopełnienie nicków przy rozmowach */
		if (!xstrcmp(line, ("")) || (!xstrncasecmp(line, cmd, xstrlen(cmd)) && word == 2 && send_nicks_count > 0) || (!xstrcasecmp(line, cmd) && send_nicks_count > 0)) {
			if (send_nicks_index >= send_nicks_count)
				send_nicks_index = 0;

			if (send_nicks_count) {
				char *nick = send_nicks[send_nicks_index++];
				snprintf(line, LINE_MAXLEN, (xstrchr(nick, ' ')) ? "%s\"%s\" " : "%s%s ", cmd, nick);
			} else
				snprintf(line, LINE_MAXLEN, "%s", cmd);
			*line_start = 0;
			*line_index = xstrlen(line);

			array_free(completions);
			array_free(words);
			xfree(start);
			xfree(separators);
			xfree(cmd);
			return;
		}

		char *nick, *ret;
		if (state)
			return NULL;
		if (send_nicks_count < 1)
			return saprintf((window_current->target) ? "/%s" : "%s", (config_tab_command) ? config_tab_command : "msg");
		send_nicks_index = (send_nicks_count > 1) ? 1 : 0;

		nick = ((xstrchr(send_nicks[0], ' ')) ? saprintf("\"%s\"", send_nicks[0]) : xstrdup(send_nicks[0])); 
		ret = saprintf((window_current->target) ? "/%s %s" : "%s %s", (config_tab_command) ? config_tab_command : "chat", nick);
		xfree(nick);
		return ret;
	}
#endif

	if (!state) {
		el = commands;
		len = xstrlen(text);
	}

	if (*text == '/') {
		slash = 1;
		text++;
		len--;
	}

	if (*text == '^') {
		dash = 1;
		text++;
		len--;
	}

	if (window_current->target) slash = 1;

	while (el) {
		command_t *c = el->data;
		char *without_sess_id = NULL;
		int plen = 0;
		session_t *session = session_current;

		el = el->next;

		if (session && session->uid)
			plen =  (int)(xstrchr(session->uid, ':') - session->uid) + 1;

		if (session && !xstrncasecmp(c->name, session->uid, plen))
			without_sess_id = xstrchr(c->name, ':');

		if (!xstrncasecmp(text, c->name, len))
			return saprintf("%s%s%s", 
					slash ? "/" : "",
					dash ? "^" : "",
					c->name);
		else if (without_sess_id && !xstrncasecmp(text, without_sess_id + 1, len)) 
			return saprintf("%s%s%s",
					slash ? "/" : "",
					dash ? "^" : "",
					without_sess_id + 1);
	}

	return NULL;
}

GENERATOR(conference) {
	static list_t el;
	static int len;

	if (!state) {
		len = xstrlen(text);
		el = conferences;
	}

	while (el) {
		struct conference *c = el->data;
		el = el->next;
		
		if (!xstrncasecmp(text, c->name, len))
			return xstrdup(c->name);

	}
	return NULL;
}

GENERATOR(known_uin) {
	static list_t el;
	static int len;
	static session_t *s;

	if (!session_current) return NULL;


	if (!state) {
		char *tmp;

		len = xstrlen(text);
		s = session_current;

		if ((tmp == xstrrchr(text, '/')) && tmp+1) {
			/* XXX, find session */

		}

		el = s->userlist;
	}

/* XXX, search window_current->userlist && conference */

	while (el) {
		userlist_t *u = el->data;

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
	static int index = 0, len;

	if (!state) {
		index = 0;
		len = xstrlen(text);
	}

	while (index < send_nicks_count) {
		if (send_nicks[index] && xstrchr(send_nicks[index], ':') && xisdigit(xstrchr(send_nicks[index], ':')[1]) && !xstrncasecmp(text, send_nicks[index], len))
			return xstrdup(send_nicks[index++]);
		index++;
	}

	return NULL;
}

GENERATOR(variable) {
	static list_t el;
	static int len;

	if (!state) {
		el = variables;
		len = xstrlen(text);
	}

	while (el) {
		variable_t *v = el->data;
		
		el = el->next;
		
		if (v->type == VAR_FOREIGN)
			continue;

		if (*text == '-') {
			if (!xstrncasecmp(text + 1, v->name, len - 1))
				return saprintf("-%s", v->name);
		} else {
			if (!xstrncasecmp(text, v->name, len))
				return xstrdup(v->name);
		}
	}

	return NULL;
}

GENERATOR(ignored_uin) {
#warning "GENERATOR: ignored uin NOT COPIED"
	static list_t l;
	static int len;

	if (!session_current) 
		return NULL;

	if (!state) {
		l = userlist;
		len = xstrlen(text);
	}

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
	static list_t el;
	static int len;
	session_t *s = session_current;

	if (!s) 
		return NULL;

	if (!state) {
		el = s->userlist;
		len = xstrlen(text);
	}

	while (el) {
		userlist_t *u = el->data;

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
	static list_t el;
	static int len;

	if (!state) {
		el = windows;
		len = xstrlen(text);
	}

	while (el) {
		window_t *w = el->data;
		el = el->next;

		if (!xstrncmp(text, w->target, len))
			return xstrdup(w->target);
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
	static list_t l;
	static int len;

	if (!state) {
		l = sessions;
		len = xstrlen(text);
	}

	while (l) {
		session_t *s = l->data;
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
	}
	return NULL;
}

char *empty_generator(char *text, int state) {
	return NULL;
}

char **my_completion(char *text, int start, int end) {
	char **params = NULL;
	int word = 0, i, abbrs = 0;
	CPFunction *func = known_uin_generator;
	list_t l;
	static int my_send_nicks_count = 0;

	if (start) {
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
	}

	if (start) {
		int in_quote = 0;

		for (i = 1; i <= start; i++) {
			if (rl_line_buffer[i] == '"')
				in_quote = !in_quote;

			if (xisspace(rl_line_buffer[i]) && !xisspace(rl_line_buffer[i - 1]) && !in_quote)
				word++;
		}
		word--;

		for (l = commands; l; l = l->next) {
			command_t *c = l->data;
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
				switch (params[word][0]) {
					case 'u': func = known_uin_generator;		break;
					case 'C': func = conference_generator;		break;
					case 'U': func = unknown_uin_generator;		break;
					case 'c': func = command_generator;		break;
					case 'i': func = ignored_uin_generator;		break;
					case 'b': func = blocked_uin_generator;		break;
					case 'v': func = variable_generator;		break;
					case 'p': func = possibilities_generator;	break;
					case 'P': func = plugin_generator;		break;
					case 'w': func = window_generator;		break;
					case 'f': func = rl_filename_completion_function; break;
					case 'e': func = events_generator;		break;
					case 's': func = session_generator;		break;
					case 'S': func = sessions_var_generator;	break;
					case 'I': func = ignorelevels_generator;	break;
					case 'r': func = reason_generator;		break;
					case 't': func = theme_generator;		break;
					case 'o': func = dir_generator;			break;
					case 'm': func = metacontacts_generator;	break;
					case 'x':
					 default:  func = empty_generator;		break;
				}
			}
		}
	}

	if (start == 0)
		func = command_generator;

	return completion_matches(text, func);
}

