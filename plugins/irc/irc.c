/*
 *  (C) Copyright 2004 Ziomal SMrocku <michal.spadlinski@gim.org.pl>
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

#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/utsname.h>


#include <ekg/commands.h>
#include <ekg/dynstuff.h>
#include <ekg/log.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include "misc.h"
#include "irc.h"
#include "people.h"
#include "input.h"

#include "IRCVERSION.h"

/*                                                                       *
 * ======================================== STARTUP AND STANDARD FUNCS - *
 *                                                                       */

static int irc_plugin_destroy();
static int irc_theme_init();

plugin_t irc_plugin = {
	name: IRC3,
	pclass: PLUGIN_PROTOCOL,
	destroy: irc_plugin_destroy,
	theme_init: irc_theme_init
};

/*
 * irc_private_init()
 *
 * inialize irc_private_t for a given session.
 */
static void irc_private_init(session_t *s)
{
	const char *uid = session_uid_get(s);
	irc_private_t *j;
	int i;

	if (xstrncasecmp(uid, IRC4, 4) || xstrlen(uid)<5)
		return;

	if (session_private_get(s))
		return;

	j = xmalloc(sizeof(irc_private_t));
	memset(j, 0, sizeof(irc_private_t));
	j->fd = -1;

	j->nick = xstrdup(uid+4);
	j->connecting = 0;
	j->nick = NULL;
	j->host_ident=NULL;
	j->obuf=NULL;
	j->obuf_len=0;
	j->people=NULL;
	j->channels=NULL;
	session_connected_set(s, 0);
	for (i=0; i<SERVOPTS; i++) 
		j->sopt[i]=NULL;

	session_private_set(s, j);
}

/*
 * irc_private_destroy()
 *
 * cleanup stuff: free irc_private_t for a given session and some other things
 */
static void irc_private_destroy(session_t *s)
{
	irc_private_t *j = session_private_get(s);
	const char *uid = session_uid_get(s);

	if (xstrncasecmp(uid, IRC4, 4) || !j)
		return;

	irc_free_people(s, j);
	xfree(j->host_ident);
	xfree(j->server);
	xfree(j->nick);

	irc_free_people(s, j);
	
	xfree(j);
	session_private_set(s, NULL);
}

/*
 * irc_session()
 *
 * adding and deleting a session
 */
int irc_session(void *data, va_list ap)
{
	char **session = va_arg(ap, char**);
	session_t *s = session_find(*session);

	if (!s)
		return 0;

	debug("[irc] session(): %s\n", *session);
	if (data)
		irc_private_init(s);
	else
		irc_private_destroy(s);

	return 0;
}

/*
 * irc_print_version()
 *
 * what the heck this can be ? ;)
 */
int irc_print_version(void *data, va_list ap)
{
	print("generic", "IRC plugin by Michal 'GiM' Spadlinski v. "IRCVERSION);

	return 0;
}

/*
 * irc_validate_uid()
 *
 * checks, if uid is proper, and if this is a plugin that
 * should deal with such a uid
 */
int irc_validate_uid(void *data, va_list ap)
{
	char **uid = va_arg(ap, char **);
	int *valid = va_arg(ap, int *);

	if (!*uid)
		return 0;

	if (!xstrncasecmp(*uid, IRC4, 4))
		(*valid)++;
	
	return 0;
}


/*                                                                       *
 * ======================================== HANDLERS ------------------- *
 *                                                                       */

static void irc_handle_disconnect(session_t *s)
{
	irc_private_t *j = irc_private(s);
	int reconnect_delay;

	if (!j)
		return;

	if (j->obuf || j->connecting)
		watch_remove(&irc_plugin, j->fd, WATCH_WRITE);

	if (j->obuf) {
		xfree(j->obuf);
		j->obuf = NULL;
		j->obuf_len = 0;
	}

	session_connected_set(s, 0);
	j->connecting = 0;
	close(j->fd);
	j->fd = -1;

	irc_free_people(s, j);

	reconnect_delay = session_int_get(s, "auto_reconnect");
	if (reconnect_delay > -1) 
		timer_add(&irc_plugin, "reconnect", reconnect_delay, 0, irc_handle_reconnect, xstrdup(s->uid));
}

void irc_handle_stream(int type, int fd, int watch, void *data)
{
	irc_handler_data_t *idta = (irc_handler_data_t *) data;
	session_t *s = idta->session;
	char *buf;
	int len;

	/* ups, we get disconnected */
	if (type == 1) {
		debug ("[irc] handle_stream(): ROZ£¡CZY£O\n");
		irc_handle_disconnect(s);
		return;
	}

	debug("[irc] handle_stream()");

	buf = xmalloc(4096);
	memset(buf, 0, 4096);

	if ((len = read(fd, buf, 4095)) < 1) {
		debug(" readerror\n");
		print("generic_error", strerror(errno));
		goto fail;
	}

	buf[len] = 0;

	debug(" recv %d\n", len);
	irc_input_parser(s, buf, len);

	xfree(buf);

	return;

fail:
	watch_remove(&irc_plugin, fd, WATCH_READ);
}

void irc_handle_connect(int type, int fd, int watch, void *data)
{
	irc_handler_data_t *idta = (irc_handler_data_t *) data;
	int res = 0, res_size = sizeof(res);
	irc_private_t *j = session_private_get(idta->session);
	const char *real;

	debug ("[irc] handle_connect()\n");

	if (type != 0) {
		debug ("[irc] handle_connect(): type %d\n",type);
		return;
	}

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
		print("generic_error", strerror(res));
		irc_handle_disconnect(idta->session);
		return;
	}

	watch_add(&irc_plugin, fd, WATCH_READ, 1, irc_handle_stream, data);

	idta->session->last_conn = time(NULL);

	real = session_get(idta->session, "realname");
	irc_write(j, "USER %s 5 unused_field :%s\r\nNICK %s\r\n",
			j->nick, real?real:j->nick, j->nick);

}

void irc_handle_resolver(int type, int fd, int watch, void *data)
{
	irc_handler_data_t *idta = (irc_handler_data_t *) data;
	session_t *s = idta->session;
	irc_private_t *j = irc_private(s);
	struct in_addr a;
	int one = 1, res;
	const char *port_s = session_get(s, "port");
	int port = (port_s) ? atoi(port_s) : 6667, connret;
	struct sockaddr_in sin;

	if (type != 0)
		return;

	debug("[irc] handle_resolver()\n", type);

	if ((res = read(fd, &a, sizeof(a))) != sizeof(a)) {
		if (res == -1)
			debug("[irc] unable to read data from resolver: %s\n", strerror(errno));
		else
			debug("[irc] read %d bytes from resolver. not good\n", res);
		close(fd);
		print("generic_error", "Ziomu¶ twój resolver co¶ nie tegesuje");
		j->connecting = 0;
		return;
	}

	debug("[irc] handle_resolver() resolved to %s\n", inet_ntoa(a));

	close(fd);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		debug("[irc] handle_resolver() socket() failed: %s\n",
				strerror(errno));
		print("generic_error", strerror(errno));
		irc_handle_disconnect(s);
		return;
	}

	debug("[irc] socket() = %d\n", fd);

	j->fd = fd;

	if (ioctl(fd, FIONBIO, &one) == -1) {
		debug("[irc] handle_resolver() ioctl() failed: %s\n",
				strerror(errno));
		print("generic_error", strerror(errno));
		irc_handle_disconnect(s);
		return;
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = a.s_addr;

	debug("[irc] handle_resolver() connecting to %s:%d\n", 
			inet_ntoa(sin.sin_addr), port);

	connret = connect(fd, (struct sockaddr*) &sin, sizeof(sin));

	if (connret < 0 && errno != EINPROGRESS) {
		debug("[irc] handle_resolver() connect() failed: %s\n",
				strerror(errno));
		print("generic_error", strerror(errno));
		irc_handle_disconnect(s);
		return;
	}

	watch_add(&irc_plugin, fd, WATCH_WRITE, 0, irc_handle_connect, data);
}

void irc_handle_reconnect(int type, void *data)
{
	session_t *s = session_find((char*) data);
	char *tmp;

	if (!s || session_connected_get(s) == 1)
		return;

	tmp = xstrdup("/connect");
	command_exec(NULL, s, tmp, 0);
	xfree(tmp);
}

/*                                                                       *
 * ======================================== COMMANDS ------------------- *
 *                                                                       */

COMMAND(irc_command_connect)
{
	const char *server, *newnick;
	int res, fd[2];
	irc_private_t *j = session_private_get(session);
	
	if (!session_check(session, 1, IRC3)) {
		print("invalid_session");
		return -1;
	}
	if (!(server=session_get(session, "server"))) {
		print("generic_error", "gdzie lecimy ziom ?! [/session server]");
		return -1;
	}
	if (j->connecting) {
		printq("during_connect", session_name(session));
		return -1;
	}

	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		return -1;
	}
	if ((newnick = session_get(session, "nickname"))) {
		xfree(j->nick);
		j->nick = xstrdup(newnick);
	} else {
		printq("generic_error", "gdzie lecimy ziom ?! [/session nickname]");
		return -1;
	}

	debug("[irc] comm_connect() session->uid = %s resolving %s\n",
			session->uid, server);
	if (pipe(fd) == -1) {
		print("generic_error", strerror(errno));
		return -1;
	}

	debug("[irc] comm_connect() resolver pipes = { %d, %d }\n", fd[0], fd[1]);

	if ((res = fork()) == -1) {
		print("generic_error", strerror(errno));
		return -1;
	}

	if (!res) {
		struct in_addr a;

		if ((a.s_addr = inet_addr(server)) == INADDR_NONE) {
			struct hostent *he = gethostbyname(server);

			if (!he)
				a.s_addr = INADDR_NONE;
			else
				memcpy(&a, he->h_addr, sizeof(a));
		}

		write(fd[1], &a, sizeof(a));

		sleep(1);

		exit(0);
	} else {
		irc_handler_data_t *idta = (irc_handler_data_t *) xmalloc (sizeof(irc_handler_data_t));
				
		close(fd[1]);

		idta->session = session;

		watch_add (&irc_plugin, fd[0], WATCH_READ, 0, irc_handle_resolver, idta);
	}
	j->connecting = 1;

	printq("connecting", session_name(session));

	if (!xstrcmp(session_status_get(session), EKG_STATUS_NA))
		session_status_set(session, EKG_STATUS_AVAIL);
	
	return 0;
}

COMMAND(irc_command_disconnect)
{
	irc_private_t *j = session_private_get(session);
	
	debug("[irc] comm_disconnect() !!!\n");

	if (!session_check(session, 1, IRC3)) {
		print("invalid_session");
		return -1;
	}

	if (!j->connecting && !session_connected_get(session)) {
			print("not_connected", session_name(session));
			return -1;
	}
	
	if (session_connected_get(session))
		irc_write (j, "QUIT :%s\r\n", params[0]?params[0]:"EKG2 - It's better than sex!");

	if (j->connecting) {
		j->connecting = 0;
		print("conn_stopped", session_name(session));
	} else
		print("disconnected", session_name(session));

	watch_remove(&irc_plugin, j->fd, WATCH_READ);

	return 0;
}

COMMAND(irc_command_reconnect)
{
	irc_private_t *j = session_private_get(session);

	if (!session_check(session, 1, IRC3)) {
		print("invalid_session");
		return -1;
	}
	
	if (j->connecting || session_connected_get(session))
		irc_command_disconnect(name, params, session, target, quiet);

	return irc_command_connect(name, params, session, target, quiet);
}

/*****************************************************************************/

COMMAND(irc_command_msg)
{
	irc_private_t *j = session_private_get(session);
	const char *uid=NULL;
        window_t *w;
        char **rcpts; 
        int class = EKG_MSGCLASS_SENT; 
        char *me, *format=NULL, *seq=NULL, *head;
        const time_t sent = time(NULL);					

	if (!session_check(session, 1, IRC3)) {
		print("invalid_session");
		return -1;
	}

	if (!params[0] || !params[1]) {
		print("not_enough_params", name);
		return -1;
	}

	uid = params[0];
	if (xstrncasecmp(uid, IRC4, 4)) {
		printq("invalid_session");
		return -1;
	}
	
	if (!session_connected_get(session)) {
		print("not_connected", session_name(session));
		return -1;
	}

	irc_write(j, "PRIVMSG %s :%s\r\n", uid+4, params[1]);

	/* GiM: XXX */
	w = window_find_s(session, uid);
	rcpts = xmalloc(sizeof(char *) * 2);
	me = xstrdup(session_uid_get(session));
	/* GiM: TODO zmieniæ formaty jeszcze */
	head = format_string(format_find(w?"irc_msg_sent_n":"irc_msg_sent"),
				me, j->nick, uid+4, params[1]);
	rcpts[0] = xstrdup(!!w?w->target:uid);
	rcpts[1] = NULL;

	class |= EKG_NO_THEMEBIT;

	query_emit(NULL, "protocol-message", &me, &me, &rcpts, &head, &format, &sent, &class, &seq, NULL);
	xfree(me);
	xfree(head);
	xfree(rcpts[0]);
	xfree(rcpts);
	
	session_unidle(session);

	return 0;
}

COMMAND(irc_command_inline_msg)
{
	const char *p[2] = { target, params[0] };

	return irc_command_msg("msg", p, session, target, quiet);
}

COMMAND(irc_command_quote)
{
	irc_private_t *j = session_private_get(session);

	if (!session_check(session, 1, IRC3)) {
		print("invalid_session");
		return -1;
	}

	if (!params[0]) {
		print("not_enough_params", name);
		return -1;
	}

	if (!session_connected_get(session)) {
		print("not_connected", session_name(session));
		return -1;
	}

	irc_write(j, "%s\r\n", params[0]);

	return 0;
}

COMMAND(irc_command_pipl)
{
	irc_private_t *j = session_private_get(session);
	list_t t1, t2;
	people_t *per;
	people_chan_t *chan;

	if (!session_check(session, 1, IRC3)) {
		print("invalid_session");
		return -1;
	}
	
	debug("[irc] this is a secret command ;-)\n");
			
	for (t1 = j->people; t1; t1=t1->next) {
		per = (people_t *)t1->data;
		debug("[%s] ", per->nick);
		for (t2 = per->channels; t2; t2=t2->next)
		{
			chan = (people_chan_t *)t2->data;
			debug ("%s:%d, ", chan->chanp->name, chan->mode);
		}
		debug("\n");
	}
	return 0;
}

COMMAND(irc_command_add)
{
	if (!session_check(session, 1, IRC3)) {
		print("invalid_session");
		return -1;
	}
	
	return 0;
}

int irc_write_status(session_t *s, int quiet)
{
	irc_private_t *j = session_private_get(s);
	const char *status;
	char *descr;

	if (!s || !j)
		return -1;

	if (!session_connected_get(s)) {
		printq("not_connected", session_name(s));
		return -1;
	}

	status = session_status_get(s);
	descr = (char *)session_descr_get(s);

	if (!xstrcmp(status, EKG_STATUS_AVAIL)) {
		irc_write(j, "AWAY :\r\n");
	} else {
		if (descr)
			irc_write(j, "AWAY :%s\r\n", descr);
		else
			irc_write(j, "AWAY :%s\r\n", status);
	}
	xfree(descr);

	return 0;
}

COMMAND(irc_command_away)
{
	if (!session_check(session, 1, IRC3)) {
		print("invalid_session");
		return -1;
	}

	if (!xstrcmp(name, "back")) {
		if (!params[0]) {
			session_descr_set(session, NULL);
			session_status_set(session, EKG_STATUS_AVAIL);
			session_unidle(session);
			goto change;
		} else {
			print("invalid_params", "irc:back");
			return -1;
		}
	}
	if (!xstrcmp(name, "away")) {
		if (params[0]) 
			session_descr_set(session, params[0]);
		else 
			session_descr_set(session, NULL);
		session_status_set(session, EKG_STATUS_AWAY);
		session_unidle(session);
		goto change;
	}
	if (!xstrcasecmp(name, "_autoaway")) {
		session_status_set(session, EKG_STATUS_AUTOAWAY);
		goto change;
	}
	if (!xstrcasecmp(name, "_autoback")) {
		session_status_set(session, EKG_STATUS_AVAIL);
		session_unidle(session);
		goto change;
	}

	printq("generic_error", "Ale o so chozi?");
	return -1;

change:
	irc_write_status(session, quiet);
	
	return 0;
}

/*****************************************************************************/

int irc_window_kill(void *data, va_list ap)
{
	window_t **xw = va_arg(ap, window_t **);
	window_t *w = *xw;
	irc_private_t *j = NULL;
	char *tmp = NULL;

	if (w && w->id && w->target && !xstrncasecmp(w->target, IRC4, 4) &&
			w->session && session_check(w->session, 1, IRC3) &&
			(j = session_private_get(w->session)) &&
			(tmp = SOP(_005_CHANTYPES)) &&
			xstrchr(tmp, (w->target)[4]) &&
			irc_find_channel((j->channels), (w->target)) &&
			session_connected_get(w->session)
			)
	{
		irc_write(j, "PART %s :%s\r\n", (w->target)+4, 
				"EKG2 bejbi! http://ekg2.org/");
	}
	return 0;
}

char *irc_getarg(session_t *sess, char *stajl, const char *param0)
{
	char *ret;
	
	if (!session_check(sess, 1, IRC3)) {
		print("invalid_session");
		return NULL;
	}
	
	if (!session_connected_get(sess)) {
		print("not_connected", session_name(sess));
		return NULL;
	}

	if (!param0) {
		ret = xstrdup(window_current->target);
		if (xstrncasecmp(ret, IRC4, 4)) {
			print(stajl, session_name(sess), ret);
			xfree(ret);
			return NULL;
		}
	} else {
		if (!xstrncasecmp(param0, IRC4, 4))
			ret = xstrdup(param0);
		else
			ret = saprintf("irc:%s", param0);
	}
	return ret;
}

char *irc_getchan_int(session_t *s, const char *name)
{
	char *ret, *tmp;
	irc_private_t *j = session_private_get(s);

	if (!name) return NULL;
	if (!xstrncasecmp(name, IRC4, 4))
		ret = xstrdup(name);
	else
		ret = saprintf("irc:%s", name);

	tmp = SOP(_005_CHANTYPES);
	if (tmp && xstrchr(tmp, ret[4]))
		return ret;
	else
		xfree(ret);
	return NULL;
}

char *irc_getchan(session_t *s, const char **params, const char ***v, int pr)
{
	char *chan;
	const char *tf, *ts, *tmp; /* first, second */

	if (!session_check(s, 1, IRC3)) {
		print("invalid_session");
		return 0;
	}
	if (!session_connected_get(s)) {
		print("not_connected", session_name(s));
		return 0;
	}

	tf = params[0]; ts = window_current->target;

	if (pr) { tmp=tf; tf=ts; ts=tmp; }

	if (!(chan=irc_getchan_int(s, tf))) {
		if (!(chan = irc_getchan_int(s, ts)))
		{
			print("not_a_channel", session_name(s),
					ts);
			return 0;
		}
		*v = pr?(&(params[1])):(params);
	} else *v = pr?(params):&(params[1]);

	
	return chan;
}
/*****************************************************************************/

COMMAND(irc_command_topic)
{
	irc_private_t *j = session_private_get(session);
	char *chan, *newtop;
	const char **mp;

	if (!(chan=irc_getchan(session, params, &mp, 0))) 
		return -1;
	
	if (*mp)
		if (!mp[1])
			if (xstrlen(*mp)==1 && **mp==':')
				newtop = saprintf("TOPIC %s :\r\n", chan+4);
			else
				newtop = saprintf("TOPIC %s :%s\r\n", 
						chan+4, *mp);
		else
			newtop = saprintf("TOPIC %s :%s %s\r\n", chan+4,
					*mp, mp[1]);
	else
		newtop = saprintf("TOPIC %s\r\n", chan+4);

	irc_write(j, newtop);
	xfree (newtop);
	xfree (chan);
	return 0;
}

COMMAND(irc_command_devop)
{
	irc_private_t *j = session_private_get(session);
	int modes, i;
	const char **mp;
	char *op, *nicks, *tmp, c, *chan, *p;

	if (!(chan=irc_getchan(session, params, &mp, 0))) 
		return -1;
	
	if (!(*mp)) {
		print("not_enough_params", name);
		xfree(chan);
		return -1;
	}
	
	if (mp[1]) nicks = saprintf("%s %s", *mp, mp[1]);
	else nicks = xstrdup(*mp);
	p = nicks;

	modes = atoi(j->sopt[_005_MODES]);
	op = xmalloc((modes+2) * sizeof(char));
	c=xstrchr(name, 'p')?'o':'v';
	/* Yes, I know there is such a function as memset() ;> */
	for (i=0, tmp=op+1; i<modes; i++, tmp++) *tmp=c;
	op[0]=*name=='d'?'-':'+';

	i=0;
	chan+=4;
	tmp = p;
	while (1)
	{
		for (i=0; i<modes; i++)
			if (!(tmp = xstrchr(tmp, ' ')))
				break;
			else 
				tmp++;

		if (tmp) *(--tmp) = '\0';
		op[i+2]='\0';
		irc_write(j, "MODE %s %s %s\r\n", chan, op, p);
		if (!tmp) break;
		*tmp = ' ';
		tmp++;
		p = tmp;
	}
	chan-=4;
	xfree(chan);
	xfree(nicks);
	return 0;
}

COMMAND(irc_command_ctcp)
{
	int i;
	char *who;
	
	if (!session_check(session, 1, IRC3)) {
		print("invalid_session");
		return -1;
	}
	if (!session_connected_get(session)) {
		print("not_connected", session_name(session));
		return -1;
	}

	if (!(who = irc_getarg(session, "ctcp_wtf", *params))) {
		print("not_enough_params", name);
		return -1;
	}

	if (params[1]) {
		for (i=0; ctcps[i].name; i++)
			if (!xstrcasecmp(ctcps[i].name, params[1]))
				break;
	} else i = CTCP_VERSION-1;
	
	if (!ctcps[i].name) {
		return -1;
	}

	irc_write(session_private_get(session), 
			"PRIVMSG %s :\01%s %s\01\r\n", who+4, ctcps[i].name, 
			params[1]?params[2]?params[2]:"":"");

	return 0;
}

COMMAND(irc_command_mode)
{
	char *chan;
	const char **mp;

	if (!(chan=irc_getchan(session, params, &mp, 1))) 
		return -1;
	
	if (!(*mp)) {
		print("not_enough_params", name);
		xfree(chan);
		return -1;
	}

	irc_write(session_private_get(session),
			"MODE %s %s %s\r\n",
			chan+4, *mp, mp[1]?mp[1]:"");

	xfree(chan);
	return 0;
}

COMMAND(irc_command_umode)
{
	irc_private_t *j = session_private_get(session);
	char *umode;

	if (!(*params)) {
		print("not_enough_params", name);
		return -1;
	}

	if (!(umode = irc_getarg(session, "umode_wtf", *params)))
		return -1;

	irc_write(j, "MODE %s %s\r\n", j->nick, umode+4);

	xfree (umode);
	return 0;
}

COMMAND(irc_command_whois)
{
	char *person;

	if (!(person = irc_getarg(session, "umode_wtf", *params)))
		return -1;

	irc_write(session_private_get(session),
			"WHOIS %s\r\n", person+4);

	xfree (person);
	return 0;
}

COMMAND(irc_command_query)
{
	window_t *w;
	char *tar, *tmp = NULL;
	irc_private_t *j = session_private_get(session);
	
	if (!(tar = irc_getarg(session, "query_wtf", *params)))
		return -1;
	
	tmp = SOP(_005_CHANTYPES);
	if (!(tmp && xstrchr(tmp, tar[4]))) {
		w = window_find_s(session, tar);
		if (!w)
			w = window_new(tar, session, 0);
		window_switch(w->id);
	} else {
		print("not_a_user", session_name(session), tar+4);
	}
	return 0;
}

COMMAND(irc_command_join)
{
	irc_private_t *j = session_private_get(session);
	char *tmp, *tar = NULL;
	
	if (!(tar = irc_getarg(session, "join_wtf", *params)))
		return -1;
	
	tmp = SOP(_005_CHANTYPES);
	if (tmp && xstrchr(tmp, tar[4]))
		irc_write(j, "JOIN %s\r\n", tar+4);
	else
		print("not_a_channel", session_name(session), tar+4);

	xfree(tar);

	return 0;
}

COMMAND(irc_command_part)
{
	irc_private_t *j = session_private_get(session);
	char *tmp, *kan;
	const char **mp;

	if (!(kan=irc_getchan(session, params, &mp, 0)))
		return -1;

	if (*mp && mp[1]) {
		irc_write(j, "PART %s :%s %s\r\n", kan+4,
				*mp, mp[1]?mp[1]:"EKG2 bejbi!");
	} else {
		irc_write(j, "PART %s :%s\r\n", kan+4,
				(*mp)?(*mp):"EKG2 bejbi!");
	}

	xfree(kan);
	return 0;
}

COMMAND(irc_command_nick)
{
	irc_private_t *j = session_private_get(session);

	if (!session_check(session, 1, IRC3)) {
		print("invalid_session");
		return -1;
	}
	
	if (!params[0]) {
		print("not_enough_params", name);
		return -1;
	}
	if (!j) {
		print("sesion_doesnt_exist", session->uid);
		return -1;
	}
	
	/* GiM: XXX FIXME TODO think more about j->connecting... */
	if (j->connecting || session_connected_get(session)) {
		irc_write(j, "NICK %s\r\n", params[0]);
		/* this is needed, couse, when connecting and server will
		 * respond, nickname is already in use, and user
		 * will type /nick somethin', server doesn't send respond
		 * about nickname.... */
		if (j->connecting) {
			xfree(j->nick);
			j->nick = xstrdup(params[0]);
		}
	}
	
	return 0;
}

/*                                                                       *
 * ======================================== INIT/DESTROY --------------- *
 *                                                                       */

#define params(x) x

int irc_plugin_init()
{
	list_t l;

	plugin_register(&irc_plugin);

	query_connect(&irc_plugin, "protocol-validate-uid", irc_validate_uid, NULL);
	query_connect(&irc_plugin, "plugin-print-version", irc_print_version, NULL);
	query_connect(&irc_plugin, "ui-window-kill", irc_window_kill, NULL);
	query_connect(&irc_plugin, "session-added", irc_session, (void*) 1);
	query_connect(&irc_plugin, "session-removed", irc_session, (void*) 0);
	
	command_add(&irc_plugin, IRC4, "?",		irc_command_inline_msg, 0, NULL);
	command_add(&irc_plugin, "irc:connect", NULL,	irc_command_connect, 0, NULL);
	command_add(&irc_plugin, "irc:disconnect", NULL,irc_command_disconnect, 0, NULL);
	command_add(&irc_plugin, "irc:reconnect", NULL,	irc_command_reconnect, 0, NULL);

	command_add(&irc_plugin, "irc:join", "w", 	irc_command_join, 0, NULL);
	command_add(&irc_plugin, "irc:part", "w ?",	irc_command_part, 0, NULL);
	command_add(&irc_plugin, "irc:query", "?",	irc_command_query, 0, NULL);
	command_add(&irc_plugin, "irc:nick", "?",	irc_command_nick, 0, NULL);
	command_add(&irc_plugin, "irc:topic", "w ?",	irc_command_topic, 0, NULL);
	command_add(&irc_plugin, "irc:people", NULL,	irc_command_pipl, 0, NULL);
	command_add(&irc_plugin, "irc:add", NULL,	irc_command_add, 0, NULL);
	command_add(&irc_plugin, "irc:msg", "uUw ?",	irc_command_msg, 0, NULL);
	command_add(&irc_plugin, "irc:ctcp", "w? ? ?",	irc_command_ctcp, 0, NULL);
	command_add(&irc_plugin, "irc:mode", "w ?",	irc_command_mode, 0, NULL);
	command_add(&irc_plugin, "irc:umode", "?",	irc_command_umode, 0, NULL);
	command_add(&irc_plugin, "irc:whois", "uU",	irc_command_whois, 0, NULL);
	
	command_add(&irc_plugin, "irc:op", "w ?",	irc_command_devop, 0, NULL);
	command_add(&irc_plugin, "irc:deop", "w ?",	irc_command_devop, 0, NULL);
	command_add(&irc_plugin, "irc:voice", "w ?",	irc_command_devop, 0, NULL);
	command_add(&irc_plugin, "irc:devoice", "w ?",	irc_command_devop, 0, NULL);
	
	command_add(&irc_plugin, "irc:away", "?",	irc_command_away, 0, NULL);
	command_add(&irc_plugin, "irc:_autoaway", NULL,	irc_command_away, 0, NULL);
	command_add(&irc_plugin, "irc:back", NULL,	irc_command_away, 0, NULL);
	command_add(&irc_plugin, "irc:_autoback", NULL,	irc_command_away, 0, NULL);
	command_add(&irc_plugin, "irc:quote", "?",	irc_command_quote, 0, NULL);

	plugin_var_add(&irc_plugin, "server", VAR_STR, 0, 0, NULL);
	plugin_var_add(&irc_plugin, "port", VAR_INT, "6667", 0, NULL);
	plugin_var_add(&irc_plugin, "default", VAR_BOOL, "0", 0, changed_var_default);
	plugin_var_add(&irc_plugin, "auto_away", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "auto_back", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "auto_connect", VAR_BOOL, "0", 0, NULL);
        plugin_var_add(&irc_plugin, "display_notify", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "nickname", VAR_STR, 0, 0, NULL);
	plugin_var_add(&irc_plugin, "realname", VAR_STR, 0, 0, NULL);
	
	/* bit 0 - chan window, bit 1 - mesg window */
	plugin_var_add(&irc_plugin, "make_window", VAR_INT, "2", 0, NULL);

	plugin_var_add(&irc_plugin, "DISPLAY_PONG", VAR_INT, "1", 0, NULL);
	plugin_var_add(&irc_plugin, "DISPLAY_AWAY_NOTIFICATION", VAR_INT, "1", 0, NULL);
	plugin_var_add(&irc_plugin, "DISPLAY_NAMES_IN_CURRENT", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "SKIP_MOTD", VAR_INT, "1", 0, NULL);
	plugin_var_add(&irc_plugin, "NEWCOLORS", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "STRIPMIRCCOL", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "VERSION_NAME", VAR_STR, 0, 0, NULL);
	plugin_var_add(&irc_plugin, "VERSION_NO", VAR_STR, 0, 0, NULL);
	plugin_var_add(&irc_plugin, "VERSION_SYS", VAR_STR, 0, 0, NULL);

	for (l = sessions; l; l = l->next)
		irc_private_init((session_t*) l->data);

	return 0;
}

static int irc_plugin_destroy()
{
	list_t l;

	for (l = sessions; l; l = l->next)
		irc_private_destroy((session_t*) l->data);

	plugin_unregister(&irc_plugin);

	return 0;
}

static int irc_theme_init()
{
	/* %1 should be _always_ session name, if it's not so,
	 * you should report this to me [GiM]
	 */
	
	/* %2 - nick, %3 - chan, %4 - msg*/
        format_add("irc_msg_sent",	"%P<%n%2/%3%P>%n %4", 1);
        format_add("irc_msg_sent_n",	"%P<%n%2%P>%n %4", 1);
	
	format_add("irc_msg_f_chan",	"%B<%n%2/%3%B>%n %4", 1);
        format_add("irc_msg_f_chan_n",	"%B<%n%2%B>%n %4", 1);
	format_add("irc_msg_f_some",	"%b<%n%2%b>%n %4", 1);
	format_add("irc_not_f_chan",	"%B(%n%2/%3%B)%n %4", 1);
        format_add("irc_not_f_chan_n",	"%B(%n%2%B)%n %4", 1);
	format_add("irc_not_f_some",	"%b(%n%2%b)%n %4", 1);

	format_add("join_wtf", "%! (%1) I cannot join %2, sir!\n", 1);
	format_add("part_wtf", "%! (%1) I cannot part %2, sir!\n", 1);
	format_add("query_wtf", "%! (%1) I cannot query %2, sir!\n", 1);
	format_add("ctcp_wtf", "%! (%1) I cannot ctcp %2, sir!\n", 1);
	format_add("umode_wtf", "%! (%1) I cannot exec umode on %2, sir!\n", 1);
	format_add("not_a_channel", "%! (%1) %2 <- doesn't seem like a channel to me!\n", 1);
	format_add("irc_joined", "%> (%1) %2 has joined %4\n", 1);
	format_add("irc_left", "%> (%1) %2 has left %4 (%5)\n", 1);
	format_add("irc_kicked", "%> (%1) %2 has been kicked out by %3 from %5 (%6)\n", 1);
	format_add("irc_kicked_you", "%> (%1) You have been kicked out by %3 from %5 (%6)\n", 1);
	format_add("irc_quit", "%> (%1) %2 has quit irc (%4)\n", 1);
	format_add("irc_unknown_ctcp", "%> (%1) %2 sent unknown CTCP %3: (%4)\n", 1);
	format_add("irc_ctcp_action_pub", "%> (%1) %Y* %h%2%n %4\n", 1);
	format_add("irc_ctcp_action", "%> (%1) %V* %2%n %4\n", 1);
	format_add("irc_ctcp_request_pub", "%> (%1) %2 requested ctcp %4 from %3\n", 1);
	format_add("irc_ctcp_request", "%> (%1) %2 requested ctcp %4\n", 1);
	format_add("irc_ctcp_reply", "%> (%1) %2 CTCP reply from %3: %4\n", 1);


	format_add("IRC_ERR_CANNOTSENDTOCHAN", "%! %2: %1\n", 1);
	
	format_add("IRC_RPL_FIRSTSECOND", "%> (%1) %2 %3\n", 1);
	format_add("IRC_RPL_SECONDFIRST", "%> (%1) %3 %2\n", 1);
	format_add("IRC_RPL_JUSTONE", "%> (%1) %2\n", 1);
	format_add("IRC_RPL_NEWONE", "%> (%1) 1:%2 2:%3 3:%4 4:%5\n", 1);

	format_add("IRC_ERR_FIRSTSECOND", "%! (%1) %2 %3\n", 1);
	format_add("IRC_ERR_SECONDFIRST", "%! (%1) %3 %2\n", 1);
	format_add("IRC_ERR_JUSTONE", "%! (%1) %2\n", 1);
	format_add("IRC_ERR_NEWONE", "%! (%1) 1:%2 2:%3 3:%4 4:%5\n", 1);
	
	format_add("IRC_RPL_CANTSEND", "%> (%1) Cannot send to channel %2\n", 1);
	format_add("RPL_AWAY", "%G||%n away     : %2 - %3\n", 1);
	format_add("RPL_TOPIC", "%> (%1) Topic %2: %3\n", 1);
	format_add("RPL_MOTDSTART", "%g,+=%G-----\n", 1);
	format_add("RPL_MOTD",      "%g|| %n%2\n", 1);
	format_add("RPL_ENDOFMOTD", "%g`+=%G-----\n", 1);

	/* in whois %2 is always nick */
	format_add("RPL_WHOISUSER", "%G.+===%g-----\n%G||%n (%T%2%n) (%3@%4)\n"
			"%G||%n realname : %6\n", 1);
	format_add("RPL_WHOISCHANNELS", "%G||%n %|channels : %3\n", 1);
	format_add("RPL_WHOISSERVER", "%G||%n %|server   : %3 (%4)\n", 1);
	format_add("RPL_WHOISOPERATOR", "%G||%n %|ircOp    : %3\n", 1);
	format_add("RPL_WHOISIDLE", "%G||%n %|idle     : %3 (signon: %4)\n", 1);
	format_add("RPL_ENDOFWHOIS", "%G`+===%g-----\n", 1);
	
	/* \n not needed if you're including date [%3] */
	format_add("IRC_RPL_TOPICBY", "%> set by %2 on %3", 1);
	format_add("IRC_TOPIC_CHANGE", "%> %T%4%n changed topic on %T%2%n: %3\n", 1);
	format_add("IRC_TOPIC_UNSET", "%> Topic %2 unset by: %3\n", 1);
	format_add("IRC_MODE_CHAN", "%> %2/%3 sets mode%4\n", 1);
	format_add("IRC_MODE", "%> %2 set mode %3 on You\n", 1);

	format_add("IRC_PINGPONG", "%) (%1) ping/pong %2\n", 1);
	format_add("IRC_YOUNEWNICK", "%> (%1) You are now known as %2\n", 1);
	format_add("IRC_NEWNICK", "%> (%1) %2 is now known as %3\n", 1);
	
	return 0;
}

