/*
 *  (C) Copyright 2004 Michal 'GiM' Spadlinski <gim at skrzynka dot pl>
 *                     Jakub 'darkjames' Zawadzki <darkjames@darkjames.ath.cx>
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/utsname.h>
#include <pwd.h>

#ifdef sun	/* Solaris, thanks to Beeth */
#include <sys/filio.h>
#endif

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

#define fix(s) ((s) ? (s) : "")

#define DEFPARTMSG "EKG2 bejbi! http://ekg2.org/"
#define DEFQUITMSG "EKG2 - It's better than sex!"

#define SGPARTMSG(x) session_get(x, "PART_MSG")
#define SGQUITMSG(x) session_get(x, "QUIT_MSG")

#define PARTMSG(x) (SGPARTMSG(x)?SGPARTMSG(x):DEFPARTMSG)
#define QUITMSG(x) (SGQUITMSG(x)?SGQUITMSG(x):DEFQUITMSG)

/*                                                                       *
 * ======================================== STARTUP AND STANDARD FUNCS - *
 *                                                                       */

static int irc_theme_init();

PLUGIN_DEFINE(irc, PLUGIN_PROTOCOL, irc_theme_init);

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

	if (irc_private(s))
		return;

	j = xmalloc(sizeof(irc_private_t));
	memset(j, 0, sizeof(irc_private_t));
	j->fd = -1;

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
	irc_private_t *j = irc_private(s);
	const char *uid = session_uid_get(s);
	int i;

	if (xstrncasecmp(uid, IRC4, 4) || !j)
		return;

	irc_free_people(s, j);
	xfree(j->host_ident);
	xfree(j->server);
	xfree(j->nick);

	irc_free_people(s, j);

        for (i=0; i<SERVOPTS; i++)
                xfree(j->sopt[i]);
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
	print("generic", "IRC plugin by Michal 'GiM' Spadlinski, Jakub 'darkjames' Zawadzki v. "IRCVERSION);

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

	if (!xstrncasecmp(*uid, IRC4, 4) && xstrlen(*uid)>4)
		(*valid)++;

	return 0;
}


/*                                                                       *
 * ======================================== CONNECTING ----------------- *
 *                                                                       */

/* we rather won't be acting as server, so doing this
 * function _too_ universal is imho senseless
 */
int irc_common_bind(session_t *s, int fd, const char *ip, int port)
{
	int inetptonres4, inetptonres6, family=0, plen;
	struct sockaddr_in ipv4;
#ifdef HAVE_GETADDRINFO
	struct sockaddr_in6 ipv6;
#endif
	struct sockaddr *p;


	ipv4.sin_family = PF_INET;
	ipv4.sin_port = htons(port);
	p = (struct sockaddr *)&ipv4;
	plen = sizeof(struct sockaddr_in);
#ifdef HAVE_INET_PTON
	inetptonres4 = inet_pton(PF_INET, ip, &(ipv4.sin_addr));
	if (inetptonres4>0) {
		family = PF_INET;
	}
#ifdef HAVE_GETADDRINFO
	inetptonres6 = inet_pton(PF_INET6, ip, &(ipv6.sin6_addr));
	if (inetptonres6>0 && !family) {
		ipv6.sin6_family = family = PF_INET6;
		ipv6.sin6_port = htons(port);
		family = PF_INET6;
		p = (struct sockaddr *)&ipv6;
		plen = sizeof(struct sockaddr_in6);
	}
#endif
	/* darkjames: nie mozemy zak³adaæ, ¿e -1 nie wyst±pi,
	 * -1 sugeruje, ¿e nie ma supportu, a nie ¿e podano jaki¶ z³y parametr
	 * co je¶li trafisz na maszynê gdzie np nie ma getaddrinfo a
	 * wsparcie jest tylko dla PF_INET6 ?
	 * [niemo¿liwe, ale nie mo¿esz zak³adaæ o tym co zwóric dana funkcja]
	 */
	if (inetptonres4<1 && inetptonres6<1) {
		print("invalid_local_ip", session_name(s));
		session_set(s, "local_ip", NULL);
		config_changed = 1;
		ipv4.sin_addr.s_addr = htonl(INADDR_ANY);
	}
#else
	ipv4.sin_addr.s_addr = inet_addr(ip);
#endif
	return bind(fd, p, plen);
}

int irc_common_connect_ext(session_t *s, int fd, int family, const char *lip, int lport)
{
	if (irc_common_bind(s, fd, lip, lport)) {
		debug ("[irc] handle_resolver() bind() failed: %s\n",
				strerror(errno));
		print ("generic_error", strerror(errno));
		/* nie ustawiamy tu local_ip na NULL */
	}
	
	return 0; /*irc_common_connect(s, fd, family, ip, port);*/

}


/*                                                                       *
 * ======================================== HANDLERS ------------------- *
 *                                                                       */

static void irc_handle_disconnect(session_t *s)
{
	irc_private_t *j = irc_private(s);
        char *__session = xstrdup(session_uid_get(s));
        char *__reason = NULL;
        int __type = EKG_DISCONNECT_FAILURE;

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

	query_emit(NULL, "protocol-disconnected", &__session, &__reason, &__type, NULL);

	xfree(__reason);
	xfree(__session);
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
		//xfree(idta);
		irc_handle_disconnect(s);
		return;
	}

	debug("[irc] handle_stream()");

	buf = xmalloc(4096);
	memset(buf, 0, 4096);

	if ((len = read(fd, buf, 4095)) < 1) {
		debug(" readerror\n");
		print("generic_error", strerror(errno));
		xfree(idta);
		irc_handle_disconnect(s);
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
	irc_private_t *j = irc_private(idta->session);
	const char *real, *localhostname;
	char *pass;


	if (type != 0) {
		debug ("[irc] handle_connect(): type %d\n",type);
		return;
	}
	debug ("[irc] handle_connect()\n");

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
		print("generic_error", strerror(res));
		irc_handle_disconnect(idta->session);
		return;
	}

	watch_add(&irc_plugin, fd, WATCH_READ, 1, irc_handle_stream, data);

	idta->session->last_conn = time(NULL);

	real = session_get(idta->session, "realname");
	real = real ? xstrlen(real) ? real : j->nick : j->nick;
	localhostname = session_get(idta->session, "local_ip");
	if (!xstrlen(localhostname))
			localhostname = NULL;
	pass = (char *)session_password_get(idta->session);
	pass = xstrlen(strip_spaces(pass))?saprintf("PASS %s\r\n", strip_spaces(pass)):xstrdup("");
	irc_write(j, "%sUSER %s %s unused_field :%s\r\nNICK %s\r\n",
			pass, j->nick, localhostname?localhostname:"12", real, j->nick);
	xfree(pass);
}

void irc_handle_resolver(int type, int fd, int watch, void *data)
{
	irc_handler_data_t *idta = (irc_handler_data_t *) data;
	session_t *s = idta->session;
	irc_private_t *j = irc_private(s);
#ifdef HAVE_GETADDRINFO
	struct sockaddr_in6 ipv6;
#endif
	struct sockaddr_in ipv4;

	int family, one = 1, res, expectedres, inetptonres;
	const char *port_s = session_get(s, "port");
	const char *local_ip = session_get(s, "local_ip");
	char bufek[100];
	int port = (port_s) ? atoi(port_s) : 6667, connret;

	if (type != 0) 
		return;

	debug("[irc] handle_resolver() %d\n", type);

	if ((res = read(fd, &family, sizeof(long))) != sizeof(long))
	{
		if (res == -1)
			debug("[irc] unable to read data from resolver: %s\n", strerror(errno));
		else
			debug("[irc] read %d bytes from resolver. not good\n", res);
		close(fd);
		print("generic_error", "Ziomu¶ twój resolver co¶ nie tegesuje");
		j->connecting = 0;
	}

	expectedres = sizeof(ipv4);

	if (family == PF_INET)
		res = read(fd, &ipv4, sizeof(ipv4));
#ifdef HAVE_GETADDRINFO
	else if (family == PF_INET6) {
		res = read(fd, &ipv6, sizeof(ipv6));
		expectedres = sizeof(ipv6);
	}
#endif

	if (res != expectedres) {
		if (res == -1)
			debug("[irc] unable to read data from resolver: %s\n", strerror(errno));
		else
			debug("[irc] read %d bytes instead of %d from resolver. not good\n",
					res, expectedres);
		close(fd);
		print("generic_error", "Ziomu¶ twój resolver co¶ nie tegesuje");
		j->connecting = 0;
		return;
	}
#if defined(HAVE_INET_NTOP) && defined(HAVE_GETADDRINFO)
	if (family == PF_INET)
		debug("[irc] handle_resolver4() resolved to %s\n",
				inet_ntop(family, &ipv4.sin_addr, bufek, 100));
	else
		debug("[irc] handle_resolver6() resolved to %s\n",
				inet_ntop(family, &ipv6.sin6_addr, bufek, 100));
#else
	if (family == PF_INET)
		debug("[irc] handle_resolver4() resolved to %s\n",
				inet_ntoa(ipv4.sin_addr));
	else
		debug("[irc] resolved but doesn't have inet_ntop ?\n");
#endif

	close(fd);
	if (family == PF_INET && ipv4.sin_addr.s_addr == INADDR_NONE)
	{
		print("generic_error", "Could not resolve your server");
		j->connecting = 0;
		return;
	}

	if ((fd = socket(family, SOCK_STREAM, 0)) == -1) {
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

	if (xstrlen(local_ip) > 1)
		irc_common_connect_ext(s, fd, family, local_ip, 0);

	ipv4.sin_port = htons(port);
#ifdef HAVE_GETADDRINFO
	ipv6.sin6_port = htons(port);
#endif

	debug("[irc] handle_resolver() connecting to host, port: %d\n", port);

	if (family == PF_INET) {
		connret = connect(fd, (struct sockaddr*) &ipv4, sizeof(ipv4));
	}
#ifdef HAVE_GETADDRINFO
	else if (family == PF_INET6) {
		connret = connect(fd, (struct sockaddr*) &ipv6, sizeof(ipv6));
	}
#endif

	if (connret < 0 && errno != EINPROGRESS) {
		debug("[irc] handle_resolver() connect() failed: %s\n",
				strerror(errno));
		print("generic_error", strerror(errno));
		irc_handle_disconnect(s);
		return;
	}

        watch_add(&irc_plugin, fd, WATCH_WRITE, 0, irc_handle_connect, idta);
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

void resolver_child_handler(child_t *c, int pid, const char *name, int status, void *priv)
{
	debug("(%s) resolver [%d] exited with %d\n", name, pid, status);
}

COMMAND(irc_command_connect)
{
	const char *server, *newnick;
	int res, fd[2];
	irc_private_t *j = irc_private(session);

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
		close(fd[0]);
		close(fd[1]);
		print("generic_error", strerror(errno));
		return -1;
	}

	if (!res) {
		struct sockaddr_in a;
		char buf[100];
		int len;
#ifdef HAVE_GETADDRINFO
		struct addrinfo *ai;

		if (!getaddrinfo(server, NULL, NULL, &ai))
		{
			/* we'll take first from the list
			 * later i'll maybe add searching in the list...
			 */
			len = ai->ai_family;
			memcpy(buf, &len, sizeof(long));
			memcpy(buf+sizeof(long), ai->ai_addr, ai->ai_addrlen);
			len = sizeof(long) + ai->ai_addrlen;
			freeaddrinfo(ai);
		} else {
			a.sin_addr.s_addr = INADDR_NONE;
			len = PF_INET;
		}
#else

		if ((a.sin_addr.s_addr = inet_addr(server)) == INADDR_NONE) {
			struct hostent *he = gethostbyname(server);

			if (!he)
				a.sin_addr.s_addr = INADDR_NONE;
			else
				memcpy(&a.sin_addr, he->h_addr, sizeof(a));
		}

		len = PF_INET;
#endif
		close(fd[0]);
		if (len == PF_INET)
		{
			memcpy(buf, &len, sizeof(long));
			memcpy(buf+sizeof(long), &a, sizeof(a));
			len = sizeof(long) + sizeof(a);
		}
		write(fd[1], buf, len);

		sleep(1);
		exit(0);
	} else {
		irc_handler_data_t *idta = (irc_handler_data_t *) xmalloc (sizeof(irc_handler_data_t));

		close(fd[1]);

		idta->session = session;

		watch_add (&irc_plugin, fd[0], WATCH_READ, 0, irc_handle_resolver, idta);

		/* TEST ONLY */
		child_add(&irc_plugin, res, session->uid, resolver_child_handler, NULL);

	}
	j->connecting = 1;

	printq("connecting", session_name(session));

	if (!xstrcmp(session_status_get(session), EKG_STATUS_NA))
		session_status_set(session, EKG_STATUS_AVAIL);

	return 0;
}

COMMAND(irc_command_disconnect)
{
	irc_private_t *j = irc_private(session);
	
	debug("[irc] comm_disconnect() !!!\n");

	if (!session_check(session, 1, IRC3)) {
		print("invalid_session");
		return -1;
	}

        /* if ,,reconnect'' timer exists we should stop doing */
        if (timer_remove(&irc_plugin, "reconnect") == 0) {
		printq("auto_reconnect_removed", session_name(session));
		return 0;
	}

	if (!j->connecting && !session_connected_get(session)) {
			print("not_connected", session_name(session));
			return -1;
	}
	/* params can be NULL
	 * [if we get ERROR from server]
	 */
	if (params && session_connected_get(session))
		irc_write (j, "QUIT :%s\r\n", params[0]?params[0]:QUITMSG(session));

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
	irc_private_t *j = irc_private(session);

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
	irc_private_t *j = irc_private(session);
	const char *uid=NULL;
        window_t *w;
        int class = EKG_MSGCLASS_SENT, ischn; 
	int ekgbeep = EKG_NO_BEEP;
        char *me, *format=NULL, *seq=NULL, *head, *chantypes, *coloured;
        char **rcpts, prefix[2], *mline[2], mlinechr;
        const time_t sent = time(NULL);					
	people_t *person;
	people_chan_t *perchn = NULL;
	int secure = 0;
	char *sid = NULL, *uid_tmp = NULL;
	unsigned char *__msg = NULL;

	if (!session_check(session, 1, IRC3)) {
		print("invalid_session");
		return -1;
	}

	if (!params[0] || !params[1]) {
		if (!params[0]) print("not_enough_params", name);
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

	sid = xstrdup(session->uid);
	uid_tmp = xstrdup(uid);
	mline[0] = (char *)params[1];
	while ( (mline[1]=xstrchr(mline[0], '\n')) )
	{
		mlinechr = *mline[1];
		*mline[1]='\0';
		__msg = xstrdup(mline[0]);

		query_emit(NULL, "message-encrypt", &sid, &uid_tmp, &__msg, &secure);
		irc_write(j, "PRIVMSG %s :%s\r\n", uid+4, __msg);
		xfree(__msg);
		mline[0] = (char *)(mline[1]+1);
		*mline[1] = mlinechr;
	}
	__msg = xstrdup(mline[0]);
	query_emit(NULL, "message-encrypt", &sid, &uid_tmp, &__msg, &secure);
	irc_write(j, "PRIVMSG %s :%s\r\n", uid+4, __msg);
	xfree(sid);
	xfree(uid_tmp);
	xfree(__msg);

	chantypes = SOP(_005_CHANTYPES);
	ischn = !!xstrchr(chantypes, uid[4]);
	if ((person = irc_find_person(j->people, j->nick)))
		perchn = irc_find_person_chan(person->channels, (char *)uid);

	w = window_find_s(session, uid);
	rcpts = xmalloc(sizeof(char *) * 2);
	me = xstrdup(session_uid_get(session));

	prefix[1] = '\0';
	prefix[0] = perchn?*(perchn->sign):' ';
	if (!session_int_get(session, "SHOW_NICKMODE_EMPTY") && *prefix==' ')
		*prefix='\0';
	head = format_string(format_find(ischn?"irc_msg_sent_chan":
				w?"irc_msg_sent_n":"irc_msg_sent"),
			session_name(session), prefix,
			j->nick, j->nick, uid+4, params[1]);

	coloured = irc_ircoldcolstr_to_ekgcolstr(session, head, 1);

	rcpts[0] = xstrdup(!!w?w->target:uid);
	rcpts[1] = NULL;

	class |= EKG_NO_THEMEBIT;

	query_emit(NULL, "protocol-message", &me, &me, &rcpts, &coloured, &format, &sent, &class, &seq, &ekgbeep, &secure);
	xfree(me);
	xfree(head);
	xfree(rcpts[0]);
	xfree(rcpts);
	xfree(coloured);
	
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
	irc_private_t *j = irc_private(session);

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
	irc_private_t *j = irc_private(session);
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
	irc_private_t *j = irc_private(s);
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
#if 0
		if (!params[0]) {
#endif
			session_descr_set(session, NULL);
			session_status_set(session, EKG_STATUS_AVAIL);
			session_unidle(session);
			goto change;
#if 0
		} else {
			print("invalid_params", "irc:back");
			return -1;
		}
#endif
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
			(j = irc_private(w->session)) &&
			(tmp = SOP(_005_CHANTYPES)) &&
			xstrchr(tmp, (w->target)[4]) &&
			irc_find_channel((j->channels), (w->target)) &&
			session_connected_get(w->session)
			)
	{
		irc_write(j, "PART %s :%s\r\n", (w->target)+4, PARTMSG(w->session));
	}
	return 0;
}

int irc_topic_header(void *data, va_list ap)
{
	char **top   = va_arg(ap, char **);
	char **setby = va_arg(ap, char **);
	char **modes = va_arg(ap, char **);
	char *targ = window_current->target;
	channel_t *chanp = NULL;
	irc_private_t *j = NULL;
	char *tmp = NULL;

	*top = *setby = *modes = NULL;
	if (targ && !xstrncasecmp(targ, IRC4, 4) && window_current->session &&
			session_check(window_current->session, 1, IRC3) &&
			(j = irc_private(window_current->session)) &&
			(tmp = SOP(_005_CHANTYPES)) &&
			xstrchr(tmp, targ[4]) &&
			(chanp = irc_find_channel((j->channels), targ)) &&
			session_connected_get(window_current->session)
			)
	{
		*top   = xstrdup(chanp->topic);
		*setby = xstrdup(chanp->topicby);
		*modes = xstrdup(chanp->mode_str);
	}
	return 0;
}

enum { IRC_GC_CHAN=0, IRC_GC_NOT_CHAN, IRC_GC_ANY };
char *irc_getchan_int(session_t *s, const char *name, int checkchan)
{
	char *ret, *tmp;
	irc_private_t *j = irc_private(s);

	if (!(name && xstrlen(name)))
		return NULL;

	if (!xstrncasecmp(name, IRC4, 4))
		ret = xstrdup(name);
	else
		ret = saprintf("irc:%s", name);

	if (checkchan == 2) 
		return ret;

	tmp = SOP(_005_CHANTYPES);
	if (tmp && ((!!xstrchr(tmp, ret[4]))-checkchan) )
		return ret;
	else
		xfree(ret);
	return NULL;
}

char *irc_getchan(session_t *s, const char **params, const char *name,
		const char ***v, int pr, int checkchan)
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
	debug ("dupa: %s - %s\n", tf, ts);

	if (pr) { tmp=tf; tf=ts; ts=tmp; }

	if (!(chan=irc_getchan_int(s, tf, checkchan))) {
		if (!(chan = irc_getchan_int(s, ts, checkchan)))
		{
			print("invalid_params", name);
			return 0;
		}
		*v = pr?(&(params[1])):(params);
	} else *v = pr?(params):&(params[1]);

	
	return chan;
}
/*****************************************************************************/

COMMAND(irc_command_topic)
{
	irc_private_t *j = irc_private(session);
	char *chan, *newtop;
	const char **mp;

	if (!(chan=irc_getchan(session, params, name, 
					&mp, 0, IRC_GC_CHAN))) 
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
	irc_private_t *j = irc_private(session);
	int modes, i;
	const char **mp;
	char *op, *nicks, *tmp, c, *chan, *p;

	if (!(chan=irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN))) 
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
	c=xstrchr(name, 'p')?'o':xstrchr(name, 'h')?'h':'v';
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
	const char **mp;

	/* GiM: XXX */
	if (!(who=irc_getchan(session, params, name,
					&mp, 0, IRC_GC_ANY))) 
		return -1;

	if (*mp) {
		for (i=0; ctcps[i].name; i++)
			if (!xstrcasecmp(ctcps[i].name, *mp))
				break;
	} else i = CTCP_VERSION-1;

	/*if (!ctcps[i].name) {
		return -1;
	}*/

	irc_write(irc_private(session), "PRIVMSG %s :\01%s %s\01\r\n",
			who+4, ctcps[i].name?ctcps[i].name:(*mp),
			*mp?mp[1]?mp[1]:"":"");

	xfree(who);
	return 0;
}

COMMAND(irc_command_me)
{
	irc_private_t *j = irc_private(session);
	char *chan, *chantypes = SOP(_005_CHANTYPES), *str, *col;
	const char **mp;
	int mw = session_int_get(session, "make_window"), ischn;

	if (!(chan=irc_getchan(session, params, name,
					&mp, 1, IRC_GC_ANY)))
		return -1;

	if (!(*mp))
		return -1;
	
	ischn = chantypes?!!xstrchr(chantypes, chan[4]):0;
	
	if (mp[1])
		str = saprintf("%s %s", *mp, mp[1]);
	else
		str = xstrdup(*mp);

	irc_write(irc_private(session), "PRIVMSG %s :\01ACTION %s\01\r\n",
			chan+4, str);

	col = irc_ircoldcolstr_to_ekgcolstr(session, str, 1);
	print_window(chan, session, ischn?(mw&1):!!(mw&2),
			ischn?"irc_ctcp_action_y_pub":"irc_ctcp_action_y",
			session_name(session), j->nick, chan, col);
	xfree(chan);
	xfree(col);
	xfree(str);
	return 0;
}

COMMAND(irc_command_mode)
{
	char *chan;
	const char **mp;

	if (!(chan=irc_getchan(session, params, name,
					&mp, 1, IRC_GC_CHAN))) 
		return -1;
	
	if (!(*mp)) {
		print("not_enough_params", name);
		xfree(chan);
		return -1;
	}

	irc_write(irc_private(session), "MODE %s %s %s\r\n",
			chan+4, *mp, mp[1]?mp[1]:"");

	xfree(chan);
	return 0;
}

COMMAND(irc_command_umode)
{
	irc_private_t *j = irc_private(session);
	char *umode;
	const char **mp;

	if (!(umode = irc_getchan(session, params, name,
					&mp, 0, IRC_GC_ANY)))
		return -1;

	irc_write(j, "MODE %s %s\r\n", j->nick, umode+4);

	xfree (umode);
	return 0;
}

COMMAND(irc_command_whois)
{
	char *person;
	const char **mp;

	if (!(person = irc_getchan(session, params, name,
					&mp, 0, IRC_GC_NOT_CHAN)))
		return -1;

	irc_write(irc_private(session),	"WHOIS %s\r\n", person+4);
	xfree (person);
	return 0;
}

COMMAND(irc_command_query)
{
	window_t *w;
	char *tar, **p = xcalloc(3, sizeof(char*)), *tmp;
	const char **mp;
	int i;

        for (i = 0; i<2 && params[i]; i++)
                p[i] = xstrdup(params[i]);

        p[i] = NULL;

        if (params[0] && (tmp = xstrrchr(params[0], '/'))) {
                tmp++;
                
                xfree(p[0]);
                p[0] = xstrdup(tmp);
        }
	
	if (!(tar = irc_getchan(session, (const char **) p, name,
					&mp, 0, IRC_GC_NOT_CHAN))) {
		return -1;
	}

	tmp = xstrdup(tar);
	tmp = strip_quotes(tmp);
	
	w = window_find_s(session, tmp);

	if (!w)
		w = window_new(tmp, session, 0);

	window_switch(w->id);

	xfree(tmp);
	for (i = 0; i<2 && params[i]; i++)
        	xfree(p[i]);
	xfree(tar);
	xfree(p);
	return 0;
}

COMMAND(irc_command_jopacy)
{
	irc_private_t *j = irc_private(session);
	char *tar = NULL, *str, *tmp;
	const char **mp;
	int uf;

	if (!(tar = irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN)))
		return -1;

	uf = ((*mp) && mp[1]);
	tmp = saprintf("JOIN %s\r\n", tar+4);
	if (!xstrcmp(name, "part") || !xstrcmp(name, "cycle")) {
		str = saprintf("PART %s :%s%s%s\r\n%s", tar+4,
				(*mp)?(*mp):PARTMSG(session),
				uf?" ":"",uf?mp[1]:"",
				!xstrcmp(name, "cycle")?tmp:"");
	} else if (!xstrcmp(name, "join")) {
		str = tmp; tmp=NULL;
	} else
		return 0;

	irc_write(j, str);
	xfree(tar);
	xfree(str);
	xfree(tmp);

	return 0;
}

COMMAND(irc_command_nick)
{
	irc_private_t *j = irc_private(session);

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

int irc_plugin_init(int prio)
{
	struct passwd *pwd_entry = getpwuid(getuid());
	list_t l;

	plugin_register(&irc_plugin, prio);

	query_connect(&irc_plugin, "protocol-validate-uid", irc_validate_uid, NULL);
	query_connect(&irc_plugin, "plugin-print-version", irc_print_version, NULL);
	query_connect(&irc_plugin, "ui-window-kill", irc_window_kill, NULL);
	query_connect(&irc_plugin, "session-added", irc_session, (void*) 1);
	query_connect(&irc_plugin, "session-removed", irc_session, (void*) 0);
	query_connect(&irc_plugin, "irc-topic",		irc_topic_header, (void*) 0);
	
	command_add(&irc_plugin, IRC4, "?",		irc_command_inline_msg, 0, NULL);
	command_add(&irc_plugin, "irc:connect", NULL,	irc_command_connect, 0, NULL);
	command_add(&irc_plugin, "irc:disconnect", "?",irc_command_disconnect, 0, NULL);
	command_add(&irc_plugin, "irc:reconnect", "?",	irc_command_reconnect, 0, NULL);

	command_add(&irc_plugin, "irc:join", "w", 	irc_command_jopacy, 0, NULL);
	command_add(&irc_plugin, "irc:part", "w ?",	irc_command_jopacy, 0, NULL);
	command_add(&irc_plugin, "irc:cycle", "w ?",	irc_command_jopacy, 0, NULL);
	command_add(&irc_plugin, "irc:query", "uUw",	irc_command_query, 0, NULL);
	command_add(&irc_plugin, "irc:nick", "?",	irc_command_nick, 0, NULL);
	command_add(&irc_plugin, "irc:topic", "w ?",	irc_command_topic, 0, NULL);
	command_add(&irc_plugin, "irc:people", NULL,	irc_command_pipl, 0, NULL);
	command_add(&irc_plugin, "irc:add", NULL,	irc_command_add, 0, NULL);
	command_add(&irc_plugin, "irc:msg", "uUw ?",	irc_command_msg, 0, NULL);
	command_add(&irc_plugin, "irc:me", "uUw ?",	irc_command_me, 0, NULL);
	command_add(&irc_plugin, "irc:ctcp", "w? ? ?",	irc_command_ctcp, 0, NULL);
	command_add(&irc_plugin, "irc:mode", "w ?",	irc_command_mode, 0, NULL);
	command_add(&irc_plugin, "irc:umode", "?",	irc_command_umode, 0, NULL);
	command_add(&irc_plugin, "irc:whois", "uU",	irc_command_whois, 0, NULL);
	
	/* TODO 
	command_add(&irc_plugin, "irc:admin", "",       NULL, 0, NULL); /quote admin
	command_add(&irc_plugin, "irc:kick", "",        NULL, 0, NULL); /quote kick nick  :reason
	command_add(&irc_plugin, "irc:ban",  "",        NULL, 0, NULL); /mode +b <%nick>
	command_add(&irc_plugin, "irc:map",  "",        NULL, 0, NULL); /quote map
	command_add(&irc_plugin, "irc:links",  "",      NULL, 0, NULL); /quote links
	*/
	command_add(&irc_plugin, "irc:op", "w ?",	irc_command_devop, 0, NULL);
	command_add(&irc_plugin, "irc:deop", "w ?",	irc_command_devop, 0, NULL);
	command_add(&irc_plugin, "irc:voice", "w ?",	irc_command_devop, 0, NULL);
	command_add(&irc_plugin, "irc:devoice", "w ?",	irc_command_devop, 0, NULL);
	command_add(&irc_plugin, "irc:halfop", "w ?",	irc_command_devop, 0, NULL);
	command_add(&irc_plugin, "irc:dehalfop", "w ?",	irc_command_devop, 0, NULL);
	
	command_add(&irc_plugin, "irc:away", "?",	irc_command_away, 0, NULL);
	command_add(&irc_plugin, "irc:_autoaway", NULL,	irc_command_away, 0, NULL);
	command_add(&irc_plugin, "irc:back", NULL,	irc_command_away, 0, NULL);
	command_add(&irc_plugin, "irc:_autoback", NULL,	irc_command_away, 0, NULL);
	command_add(&irc_plugin, "irc:quote", "?",	irc_command_quote, 0, NULL);

	/* lower case: names of variables that reffer to client itself */
	plugin_var_add(&irc_plugin, "alt_nick", VAR_STR, NULL, 0, NULL);
	plugin_var_add(&irc_plugin, "auto_away", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "auto_back", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "auto_connect", VAR_BOOL, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "dcc_port", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "default", VAR_BOOL, "0", 0, changed_var_default);
        plugin_var_add(&irc_plugin, "display_notify", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "local_ip", VAR_STR, 0, 0, NULL);
	plugin_var_add(&irc_plugin, "log_formats", VAR_STR, "xml,simple", 0, NULL);
	plugin_var_add(&irc_plugin, "make_window", VAR_INT, "2", 0, NULL);
	if (pwd_entry != NULL)
		plugin_var_add(&irc_plugin, "nickname", VAR_STR, pwd_entry->pw_name, 0, NULL);
	else
		plugin_var_add(&irc_plugin, "nickname", VAR_STR, NULL, 0, NULL);
	plugin_var_add(&irc_plugin, "password", VAR_STR, 0, 1, NULL);
	plugin_var_add(&irc_plugin, "port", VAR_INT, "6667", 0, NULL);
	if (pwd_entry != NULL)
		plugin_var_add(&irc_plugin, "realname", VAR_STR, pwd_entry->pw_gecos, 0, NULL);
	else
		plugin_var_add(&irc_plugin, "realname", VAR_STR, NULL, 0, NULL);
	plugin_var_add(&irc_plugin, "server", VAR_STR, 0, 0, NULL);

	/* upper case: names of variables, that reffer to protocol stuff */
	plugin_var_add(&irc_plugin, "AUTO_JOIN", VAR_STR, 0, 0, NULL);
	plugin_var_add(&irc_plugin, "DISPLAY_PONG", VAR_INT, "1", 0, NULL);
	plugin_var_add(&irc_plugin, "DISPLAY_AWAY_NOTIFICATION", VAR_INT, "1", 0, NULL);
	plugin_var_add(&irc_plugin, "DISPLAY_IN_CURRENT", VAR_INT, "2", 0, NULL);
	plugin_var_add(&irc_plugin, "DISPLAY_NICKCHANGE", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "DISPLAY_QUIT", VAR_INT, "0", 0, NULL);
	/* plugin_var_add(&irc_plugin, "HIGHLIGHTS", VAR_STR, 0, 0, NULL); */
	plugin_var_add(&irc_plugin, "PART_MSG", VAR_STR, DEFPARTMSG, 0, NULL);
	plugin_var_add(&irc_plugin, "QUIT_MSG", VAR_STR, DEFQUITMSG, 0, NULL);
	plugin_var_add(&irc_plugin, "REJOIN", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "REJOIN_TIME", VAR_INT, "2", 0, NULL);
	
	plugin_var_add(&irc_plugin, "SHOW_NICKMODE_EMPTY", VAR_INT, "1", 0, NULL);
	plugin_var_add(&irc_plugin, "SHOW_MOTD", VAR_INT, "1", 0, NULL);
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
	debug("I love you honey bunny\n");

	/* %1 should be _always_ session name, if it's not so,
	 * you should report this to me (GiM)
	 */
	
	/* %2 - prefix, %3 - nick, %4 - nick+ident+host, %5 - chan, %6 - msg*/
	format_add("irc_msg_sent",	"%P<%n%3/%5%P>%n %6", 1);
	format_add("irc_msg_sent_n",	"%P<%n%3%P>%n %6", 1);
	format_add("irc_msg_sent_chan",	"%P<%w%{2@%+gcp}X%2%3%P>%n %6", 1);
	format_add("irc_msg_sent_chanh","%P<%W%{2@%+GCP}X%2%3%P>%n %6", 1);
	
	format_add("irc_msg_f_chan",	"%B<%w%{2@%+gcp}X%2%3/%5%B>%n %6", 1); /* NOT USED */
	format_add("irc_msg_f_chanh",	"%B<%W%{2@%+GCP}X%2%3/%5%B>%n %6", 1); /* NOT USED */
	format_add("irc_msg_f_chan_n",	"%B<%w%{2@%+gcp}X%2%3%B>%n %6", 1);
	format_add("irc_msg_f_chan_nh",	"%B<%W%{2@%+GCP}X%2%3%B>%n %6", 1);
	format_add("irc_msg_f_some",	"%b<%n%3%b>%n %6", 1);

	format_add("irc_not_f_chan",	"%B(%w%{2@%+gcp}X%2%3/%5%B)%n %6", 1); /* NOT USED */
	format_add("irc_not_f_chanh",	"%B(%W%{2@%+GCP}X%2%3/%5%B)%n %6", 1); /* NOT USED */
	format_add("irc_not_f_chan_n",	"%B(%w%{2@%+gcp}X%2%3%B)%n %6", 1);
	format_add("irc_not_f_chan_nh",	"%B(%W%{2@%+GCP}X%2%3%B)%n %6", 1);
	format_add("irc_not_f_some",	"%b(%n%3%b)%n %6", 1);

	format_add("irc_joined", _("%> %Y%2%n has joined %4\n"), 1);
	format_add("irc_joined_you", _("%> %RYou%n has joined %4\n"), 1);
	format_add("irc_left", _("%> %g%2%n has left %4 (%5)\n"), 1);
	format_add("irc_kicked", _("%> %Y%2%n has been kicked out by %R%3%n from %5 (%6)\n"), 1);
	format_add("irc_kicked_you", _("%> You have been kicked out by %R%3%n from %5 (%6)\n"), 1);
	format_add("irc_quit", _("%> %Y%2%n has quit irc (%4)\n"), 1);
	format_add("irc_unknown_ctcp", _("%> %Y%2%n sent unknown CTCP %3: (%4)\n"), 1);
	format_add("irc_ctcp_action_y_pub", _("%> %y%e* %2%n %4\n"), 1);
	format_add("irc_ctcp_action_y", _("%> %Y%e* %2%n %4\n"), 1);
	format_add("irc_ctcp_action_pub", _("%> %y%h* %2%n %5\n"), 1);
	format_add("irc_ctcp_action", _("%> %Y%h* %2%n %5\n"), 1);
	format_add("irc_ctcp_request_pub", _("%> %Y%2%n requested ctcp %5 from %4\n"), 1);
	format_add("irc_ctcp_request", _("%> %Y%2%n requested ctcp %5\n"), 1);
	format_add("irc_ctcp_reply", _("%> %Y%2%n CTCP reply from %3: %5\n"), 1);


	format_add("IRC_ERR_CANNOTSENDTOCHAN", "%! %2: %1\n", 1);
	
	format_add("IRC_RPL_FIRSTSECOND", "%> (%1) %2 %3\n", 1);
	format_add("IRC_RPL_SECONDFIRST", "%> (%1) %3 %2\n", 1);
	format_add("IRC_RPL_JUSTONE", "%> (%1) %2\n", 1);
	format_add("IRC_RPL_NEWONE", "%> (%1,%2) 1:%3 2:%4 3:%5 4:%6\n", 1);

	format_add("IRC_ERR_FIRSTSECOND", "%! (%1) %2 %3\n", 1);
	format_add("IRC_ERR_SECONDFIRST", "%! (%1) %3 %2\n", 1);
	format_add("IRC_ERR_JUSTONE", "%! (%1) %2\n", 1);
	format_add("IRC_ERR_NEWONE", "%! (%1,%2) 1:%3 2:%4 3:%5 4:%6\n", 1);
	
	format_add("IRC_RPL_CANTSEND", _("%> Cannot send to channel %T%2%n\n"), 1);
	format_add("RPL_MOTDSTART", "%g,+=%G-----\n", 1);
	format_add("RPL_MOTD",      "%g|| %n%2\n", 1);
	format_add("RPL_ENDOFMOTD", "%g`+=%G-----\n", 1);
	
	/*Wykorzystane w : /mode +b|e|I %2 - kanal %3 - to co dostalismy od serwera */
	/* TO JEST TYMCZASOWE BÊDZIE INACZEJ, NIE U¯YWAÆ TYCH FORMATEK */
	format_add("RPL_LISTSTART",  "%g,+=%G-----\n", 1);
	format_add("RPL_EXCEPTLIST", "%g|| %n %5 - %W%2%n: except %c%3\n", 1);
	format_add("RPL_BANLIST",    "%g|| %n %5 - %W%2%n: ban %c%3\n", 1);
	format_add("RPL_INVITELIST", "%g|| %n %5 - %W%2%n: invite %c%3\n", 1);;
	format_add("RPL_EMPTYLIST" , "%g|| %n Empty list \n", 1);
	format_add("RPL_LINKS",      "%g|| %n %5 - %2  %3  %4\n", 1);
	format_add("RPL_ENDOFLIST",  "%g`+=%G----- %2%n\n", 1);
 
	format_add("RPL_AWAY", _("%G||%n away     : %2 - %3\n"), 1);
	/* in whois %2 is always nick */
	format_add("RPL_WHOISUSER", _("%G.+===%g-----\n%G||%n (%T%2%n) (%3@%4)\n"
			"%G||%n realname : %6\n"), 1);
	format_add("RPL_WHOISCHANNELS", _("%G||%n %|channels : %3\n"), 1);
	format_add("RPL_WHOISSERVER", _("%G||%n %|server   : %3 (%4)\n"), 1);
	format_add("RPL_WHOISOPERATOR", _("%G||%n %|ircOp    : %3\n"), 1);
	format_add("RPL_WHOISIDLE", _("%G||%n %|idle     : %3 (signon: %4)\n"), 1);
	format_add("RPL_ENDOFWHOIS", _("%G`+===%g-----\n"), 1);
	
	format_add("RPL_TOPIC", _("%> Topic %2: %3\n"), 1);
	/* \n not needed if you're including date [%3] */
	format_add("IRC_RPL_TOPICBY", _("%> set by %2 on %4"), 1);
	format_add("IRC_TOPIC_CHANGE", _("%> %T%2%n changed topic on %T%4%n: %5\n"), 1);
	format_add("IRC_TOPIC_UNSET", _("%> %T%2%n unset topic on %T%4%n\n"), 1);
	format_add("IRC_MODE_CHAN", _("%> %2/%4 sets mode%5\n"), 1);
	format_add("IRC_MODE", _("%> (%1) %2 set mode %3 on You\n"), 1);

	format_add("IRC_PINGPONG", _("%) (%1) ping/pong %c%2%n\n"), 1);
	format_add("IRC_YOUNEWNICK", _("%> You are now known as %G%3%n\n"), 1);
	format_add("IRC_NEWNICK", _("%> %g%2%n is now known as %G%4%n\n"), 1);
	format_add("IRC_TRYNICK", _("%> Will try to use %G%2%n instead\n"), 1);
	
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
