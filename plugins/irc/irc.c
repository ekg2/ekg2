/*
 *  (C) Copyright 2004-2005 Michal 'GiM' Spadlinski <gim at skrzynka dot pl>
 *			Jakub 'darkjames' Zawadzki <darkjames@darkjames.ath.cx>
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
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
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
#include "autoacts.h"

#include "IRCVERSION.h"

#define DEFPORT 6667

#define DEFPARTMSG "EKG2 bejbi! http://ekg2.org/"
#define DEFQUITMSG "EKG2 - It's better than sex!"
#define DEFKICKMSG "EKG2 - Y0U 57iNK2 50 MUCH!"

#define SGPARTMSG(x) session_get(x, "PART_MSG")
#define SGQUITMSG(x) session_get(x, "QUIT_MSG")
#define SGKICKMSG(x) session_get(x, "KICK_MSG")

#define PARTMSG(x,r) (r?r: SGPARTMSG(x)?SGPARTMSG(x):DEFPARTMSG)
#define QUITMSG(x) (SGQUITMSG(x)?SGQUITMSG(x):DEFQUITMSG)
#define KICKMSG(x,r) (r?r: SGKICKMSG(x)?SGKICKMSG(x):DEFKICKMSG)

#define RECTIMERADD(s) if (session_int_get(s, "auto_reconnect")>0)\
		timer_add(&irc_plugin, "reconnect", session_int_get(s, "auto_reconnect"), 0, irc_handle_reconnect, xstrdup(s->uid))
#define RECTIMERDEL timer_remove(&irc_plugin, "reconnect")
#define INVALID_SESSION \
	if (xstrncasecmp(uid, IRC4, 4)) {\
		if (xstrncasecmp(session_current->uid, IRC4, 4)) {\
			printq("invalid_session");\
			return -1;\
		}\
		else {\
			uid = saprintf("%s%s", IRC4, uid);\
		}\
	}


/* ************************ KNOWN BUGS ***********************
 *  OTHER LESS IMPORTANT BUGS
 *    -> somewhere with altnick sending
 *    G->dj: still not as I would like it to be
 *  !BUGS (?) TODO->check
 *    -> auto_reconnect timer handling 
 *    -> buggy auto_find. if smb write smth on the channel.
 *        *  10:58:27 ::: Nieprawidowe parametry. Sprobuj help find * (fix ?, darkjames)
 *************************************************************
 */
/* *************************** TODO **************************
 *
 * -> split mode.
 * -> disconnection detection
 *       why not just sending simple PING to SERVER and deal with it's
 *       pong reply...
 *               PING konstantynopolitanczykiewikowna lublin.irc.pl
 *       :prefix PONG lublin.irc.pl konstantynopolitanczykiewikowna
 * *************************************************************
 */

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
	const char	*uid = session_uid_get(s);
	irc_private_t	*j;

	if (xstrncasecmp(uid, IRC4, 4) || xstrlen(uid)<5)
		return;

	if (irc_private(s))
		return;

	j = xmalloc(sizeof(irc_private_t));
	j->fd = -1;

	/* G->dj: I've told you why this is here, not on every system NULL is 0x00000000
	 * that's why I'm just commentig this out not removing...
	memset(j, 0, sizeof(irc_private_t));
	
	j->nick = NULL;
	j->host_ident = NULL;
	j->obuf = NULL;
	j->people = NULL;
	j->channels = NULL;
	for (i = 0; i<SERVOPTS; i++) 
		j->sopt[i] = NULL;
	*/
	session_connected_set(s, 0);

	session_private_set(s, j);
}

/*
 * irc_private_destroy()
 *
 * cleanup stuff: free irc_private_t for a given session and some other things
 */
static void irc_private_destroy(session_t *s)
{
	irc_private_t	*j = irc_private(s);
	const char	*uid = session_uid_get(s);
	int		i;

	if (xstrncasecmp(uid, IRC4, 4) || !j)
		return;

	/*irc_free_people(s, j); wtf? */
	xfree(j->host_ident);
	xfree(j->nick);

	irc_free_people(s, j);

        for (i = 0; i<SERVOPTS; i++)
                xfree(j->sopt[i]);
	xfree(j);
	session_private_set(s, NULL);
}

int irc_postinit(void *data, va_list ap)
{
	list_t l;
	for (l = sessions; l; l = l->next) {
		if (!xstrncasecmp( session_uid_get( (session_t *) l->data), IRC4, 4)) {
			debug("TODO: load alist session %s alist = %s\n",
					session_uid_get( (session_t *) l->data),
					session_get( (session_t *) l->data, "alist"));
		}
	}
        return 0;
}

/*
 * irc_session()
 *
 * adding and deleting a session
 */
int irc_session(void *data, va_list ap)
{
	char		**session = va_arg(ap, char**);
	session_t	*s = session_find(*session);

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

//int irc_resolver_sort(void *s1, void *s2)
int irc_resolver_sort(void *s1, void *s2)
{
	connector_t *sort1 = s1, *sort2 = s2;
	int prefer_family = AF_INET;
/*	
	if (sort1->session != sort2->session)
		return 0;
*/	
	if (session_int_get(sort1->session, "prefer_family") == AF_INET6) 
		prefer_family = AF_INET6;

	debug("%d && %d -> %d\n", sort1->family, sort2->family, prefer_family);
	
	if (prefer_family == sort1->family)
		return 0;
	else
		return 1;
}

list_t irc_resolver2(session_t *session, char *hostname, list_t *lista, int dobind) 
{
	connector_t *listelem;
	int port = session_int_get(session, "port"); 
	char *bufek = NULL;
	void *tm = NULL;
#ifdef HAVE_GETADDRINFO
	struct  addrinfo *ai, *aitmp, hint;
#endif	

	/* G->dj: we're not allowing port part in hostname,
	 * because of IPv6 format... [delimited with colon]
	 */

	if (port < 1 ) 	port = DEFPORT;
	if (dobind)	port = 0;

	debug("[IRC] %s 0x%x\n", hostname, lista);
#ifdef HAVE_GETADDRINFO
	memset(&hint, 0, sizeof(struct addrinfo));
	hint.ai_socktype=SOCK_STREAM;
	if (!getaddrinfo(hostname, NULL, &hint, &ai)) {
		for (aitmp = ai; aitmp; aitmp = aitmp->ai_next) {
			listelem = xmalloc(sizeof(connector_t));
		
			listelem->session = session;
			listelem->family  = aitmp->ai_family;
			listelem->hostname= xstrdup(hostname);

			if (aitmp->ai_family == AF_INET6)
				tm = &(((struct sockaddr_in6 *) aitmp->ai_addr)->sin6_addr);
			if (aitmp->ai_family == AF_INET) 
				tm = &(((struct sockaddr_in *) aitmp->ai_addr)->sin_addr);

#ifdef HAVE_INET_NTOP
			bufek = xmalloc(100);
			inet_ntop(aitmp->ai_family, tm, bufek, 100);
			listelem->address   = bufek;
#else
			if (aitmp->ai_family == AF_INET6) {
				print("generic_error", "Nie masz inet_ntop() a family == AF_INET6. resolver nie bedzie dzialac"); /* jak nie mozliwe to wywalic */
				listelem->address =  xstrdup("::");
			}
			else
				listelem->address   = xstrdup(inet_ntoa(*(struct in_addr *)tm));
#endif 
			listelem->port    = port;
			debug("+ %s\n", listelem->address);

			list_add_sorted(lista, listelem, 0, &irc_resolver_sort);
			debug("%x %x\n", aitmp, aitmp->ai_next);
		}
		freeaddrinfo(ai);
	}
#else 
	    print("generic_error", "Nie masz getaddrinfo() na razie resolver nie bedzie dzialac");
#endif

/* G->dj: getaddrinfo was returninig 3 times, cause you haven't given hints...
 */
	return (*lista);
}


/*
 * irc_validate_uid()
 *
 * checks, if uid is proper, and if this is a plugin that
 * should deal with such a uid
 */
int irc_validate_uid(void *data, va_list ap)
{
	char	**uid = va_arg(ap, char **);
	int	*valid = va_arg(ap, int *);

	if (!*uid)
		return 0;

	if (!xstrncasecmp(*uid, IRC4, 4) && xstrlen(*uid)>4)
		(*valid)++;

	return 0;
}

void irc_changed_resolve(session_t *s, const char *var) {
	irc_private_t	*j = irc_private(s);
	list_t          *rlist = NULL, tmplist;
	char            *tmp   = xstrdup(session_get(s, var));
 	
	if (!xstrcmp((char *) var, "server")) rlist = &(j->connlist);
	else if (!xstrcmp((char *) var, "hostname")) rlist = &(j->bindlist);
	/* G->dj: what this is for, cause I don't get it ?
	 * dj->G: ? */
	if (*rlist) {
		for (tmplist=*rlist; tmplist; tmplist=tmplist->next) {
			xfree( ((connector_t *)tmplist->data)->address);
			xfree( ((connector_t *)tmplist->data)->hostname);
		}
		list_destroy(*rlist, 1);
		*rlist = NULL;
	}
 
        if (rlist && tmp) {
                char *tmp2;

                while(tmp2 = xstrrchr(tmp, ',')) {
                        irc_resolver2(s, tmp2+1, rlist, !xstrcmp(var, "hostname"));
                        *tmp2 = 0;
                }
                irc_resolver2(s, tmp, rlist, !xstrcmp(var, "hostname"));
        }
        xfree(tmp);
        return;

	xfree(tmp);
	return;
 }

/*                                                                       *
 * ======================================== HANDLERS ------------------- *
 *                                                                       */

void irc_handle_disconnect(session_t *s, char *reason, int type)
{
	irc_private_t	*j = irc_private(s);
        char		*__session = xstrdup(session_uid_get(s));
        char		*__reason = xstrdup(reason);
        int		__type = type;

	if (!j || session_connected_get(s) == 0)
		return;

	debug("[irc]_handle_disconnect %d\n", type);
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

	/* G->dj: I'm commenting out whole stuff 
	 * with rectimeradd, because, as you've probably already noticed
	 * timer handling is buggy   [iirc handler is called twice]
	 * [I know about this since quite a long time]
	 */
	/*RECTIMERADD(s);*/
	xfree(__reason);
	xfree(__session);
}

void irc_handle_stream(int type, int fd, int watch, void *data)
{
	irc_handler_data_t *idta = (irc_handler_data_t *) data;
	session_t *s = idta->session;
	char *buf = NULL;
	int len;

	/* ups, we get disconnected */
	if (type == 1) {
		debug ("[irc] handle_stream(): ROZ£¡CZY£O\n");
		irc_handle_disconnect(s, NULL, EKG_DISCONNECT_NETWORK);
		xfree(idta);
		return;
	}

	debug("[irc] handle_stream()");

	buf = xmalloc(4096);

	if ((len = read(fd, buf, 4095)) < 1) {
		debug(" readerror %s\n", strerror(errno));
		print("generic_error", strerror(errno));
		/* GiM->dj: type 1 is just disconnection,
		 * this is read data error,
		 * besides this is needed since you put 
		 * REC_TIMERDEL in irc_handle_disconnect
		 */
		irc_handle_disconnect(s, NULL, EKG_DISCONNECT_NETWORK); 
		watch_remove(&irc_plugin, fd, WATCH_READ);
	} else {
		buf[len] = '\0';
		debug(" recv %d\n", len);
		irc_input_parser(s, buf, len);
	}

	xfree(buf);

	return;
}

void irc_handle_connect(int type, int fd, int watch, void *data)
{
	irc_handler_data_t	*idta = (irc_handler_data_t *) data;
	int			res = 0, res_size = sizeof(res);
	irc_private_t		*j = irc_private(idta->session);
	const char		*real = NULL, *localhostname = NULL;
	char			*pass = NULL;
	connector_t		*tmpcon = NULL;

	/* buggy timer-handling (?), check it! (darkjames) */	
	if (type == 1) {
		debug ("[irc] handle_connect(): type %d\n",type);
		/* debug("%d\n", session_int_get(idta->session, "auto_reconnect")); */
		/*RECTIMERADD(idta->session);*/
		/* this is called when we're connected so watch out! */
		return;
	}
	/*RECTIMERDEL;*/
	debug ("[irc] handle_connect()\n");

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
		debug("[irc] handle_connect(): SO_ERROR\n");
		tmpcon = j->conntmplist->data;
		
		DOT_FAIL_EXT("Connect", tmpcon, idta->session,res);
//		print("generic_error", strerror(res));
		session_connected_set(idta->session, 0);
		j->connecting = 0;
// try next server.
		j->conntmplist = j->conntmplist->next;

		irc_handle_disconnect(idta->session, "conn_failed_connecting", 
				EKG_DISCONNECT_FAILURE);
		return;
	}

	watch_add(&irc_plugin, fd, WATCH_READ, 1, irc_handle_stream, data);

	idta->session->last_conn = time(NULL);

	real = session_get(idta->session, "realname");
	real = real ? xstrlen(real) ? real : j->nick : j->nick;
/*	localhostname = session_get(idta->session, "hostname");*/
	if (j->bindtmplist)	
		localhostname = ((connector_t *) j->bindtmplist->data)->hostname;
	if (!xstrlen(localhostname))
		localhostname = NULL;
	pass = (char *)session_password_get(idta->session);
	pass = xstrlen(strip_spaces(pass))?
		saprintf("PASS %s\r\n", strip_spaces(pass)) : xstrdup("");
	irc_write(j, "%sUSER %s %s unused_field :%s\r\nNICK %s\r\n",
			pass, j->nick, localhostname?localhostname:"12", real, j->nick);
	xfree(pass);
}

void irc_handle_reconnect(int type, void *data)
{
	session_t		*s = session_find((char*) data);
	irc_private_t		*j = irc_private(s);
	char *tmp;

	if (type == 1 || !s || session_connected_get(s) == 1 || j->connecting)
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

int irc_build_sin(connector_t *co, struct sockaddr **address)
{
	struct sockaddr_in  *ipv4;
	struct sockaddr_in6 *ipv6;
	int len = 0;
	
	*address = NULL;
	
	if (!co) 
		return 0;
	
	if (co->family == AF_INET) {
		len = sizeof(struct sockaddr_in);
	
		ipv4 = xmalloc(len);
		
		ipv4->sin_family = AF_INET;
		ipv4->sin_port   = htons(co->port);
		inet_pton(AF_INET, co->address, &(ipv4->sin_addr));

		*address = (struct sockaddr *) ipv4;

	} else if (co->family == AF_INET6) {
		len = sizeof(struct sockaddr_in6);
	
		ipv6 = xmalloc(len);
		ipv6->sin6_family  = AF_INET6;
		ipv6->sin6_port    = htons(co->port);
		inet_pton(AF_INET6, co->address, &(ipv6->sin6_addr));
		
		*address = (struct sockaddr *) ipv6;
	}
	return len;
}
 
COMMAND(irc_command_connect)
{
	irc_private_t		*j = irc_private(session);
	irc_handler_data_t	*idta;
	const char		*newnick;
	int			one = 1, connret = -1, bindret = -1, err;
	
	connector_t		*connco, *connvh = NULL;
	struct sockaddr		*sinco,  *sinvh  = NULL;
	int			sinlen, fd;

/*
	if (prefer_family == 2) prefer_family = PF_INET6;
	else prefer_family = PF_INET;*/

	if (!session_check(session, 1, IRC3)) {
		printq("invalid_session");
		return -1;
	}
	if (!session_get(session, "server")) {
		printq("generic_error", "gdzie lecimy ziom ?! [/session server]");
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

	if (!j->conntmplist) j->conntmplist = j->connlist;
	if (!j->bindtmplist) j->bindtmplist = j->bindlist;

	if (!j->conntmplist) {
		print("generic_error", "Ziomu¶ twój resolver co¶ nie tegesuje (!j->conntmplist)");
		/* G->dj: plz r8 ynglysch */
 		return -1;
 	}

	connco = j->conntmplist->data;
	sinlen = irc_build_sin(connco, &sinco);
	if (!sinco) {
		print("generic_error", "Ziomu¶ twój resolver co¶ nie tegesuje (!sinco)"); 
		return -1;
	}

	j->connecting = 1;

	if ((fd = socket(connco->family, SOCK_STREAM, 0)) == -1) {
		err = errno;
		debug("[irc] handle_resolver() socket() failed: %s\n",
				strerror(err));
		print("generic_error", strerror(err));
		irc_handle_disconnect(session, "conn_failed_connecting",
				EKG_DISCONNECT_FAILURE);
		goto irc_conn_error;
	}
	j->fd = fd;
	debug("[irc] socket() = %d\n", fd);

	if (ioctl(fd, FIONBIO, &one) == -1) {
		err = errno;
		debug("[irc] handle_resolver() ioctl() failed: %s\n",
				strerror(err));
		print("generic_error", strerror(err));
		irc_handle_disconnect(session, "conn_failed_connecting", 
				EKG_DISCONNECT_FAILURE);
		goto irc_conn_error;
	}
	
	/* loop, optimaize... */
	if (j->bindtmplist) 
		connvh = j->bindtmplist->data;	
	irc_build_sin(connvh, &sinvh);
	while (bindret && connvh) {
		DOT("Bind", connvh, session);
		if (connvh->family == connco->family)  {
			bindret = bind(fd, sinvh, sinlen);
			if (bindret == -1)
				DOT_FAIL("Bind", connvh, session);
 		}

		if (bindret && j->bindtmplist->next) {
			xfree(sinvh);
			j->bindtmplist = j->bindtmplist->next;
			connvh = j->bindtmplist->data;
			irc_build_sin(connvh, &sinvh);
			if (!sinvh)
				continue;
		} else
			break;
	}

	/* connect */

//	printq("connecting", session_name(session));	
	DOT("Connecting", connco, session);
	connret = connect(fd, sinco, sinlen);
 
	xfree(sinco);
	xfree(sinvh);
 
	if (connret && errno != EINPROGRESS) {
		err = errno;
		debug("[irc] handle_resolver() connect() failed: %s\n",
				strerror(err));
		print("generic_error", strerror(err));
		irc_handle_disconnect(session, "conn_failed_connecting", 
				EKG_DISCONNECT_FAILURE);
		return -1;

	}
	if (!xstrcmp(session_status_get(session), EKG_STATUS_NA))
		session_status_set(session, EKG_STATUS_AVAIL);

	idta = (irc_handler_data_t *) xmalloc (sizeof(irc_handler_data_t));
	idta->session = session;
	watch_add(&irc_plugin, fd, WATCH_WRITE, 0, irc_handle_connect, idta);
 	return 0;
irc_conn_error: 
	xfree(sinco);
	xfree(sinvh);
	return -1;
}

COMMAND(irc_command_disconnect)
{
	irc_private_t	*j = irc_private(session);
	char		*reason = (char *)(params? params[0]?params[0]:QUITMSG(session): NULL);
	int		fd = j->fd;
	/* dj: we copy fd, because irc_handle_disconnect would change it
	 * 
	 * params can be NULL
	 * [if we get ERROR from server, misc.c irc_c_error]
	 */
	
	debug("[irc] comm_disconnect() !!!\n");

	if (!session_check(session, 1, IRC3)) {
		printq("invalid_session");
		return -1;
	}

        /* if ,,reconnect'' timer exists we should stop doing */
        if (timer_remove(&irc_plugin, "reconnect") == 0) {
		printq("auto_reconnect_removed", session_name(session));
		return 0;
	}

	if (!j->connecting && !session_connected_get(session)) {
			printq("not_connected", session_name(session));
			return -1;
	}
	
	if (reason && session_connected_get(session))
		irc_write (j, "QUIT :%s\r\n", reason);

	if (j->connecting) {
		j->connecting = 0;
		irc_handle_disconnect(session, reason, EKG_DISCONNECT_STOPPED);
	} else
		irc_handle_disconnect(session, reason, EKG_DISCONNECT_USER);

	watch_remove(&irc_plugin, fd, WATCH_READ);

	return 0;
}

COMMAND(irc_command_reconnect)
{
	irc_private_t	*j = irc_private(session);

	if (!session_check(session, 1, IRC3)) {
		printq("invalid_session");
		return -1;
	}
	
	if (j->connecting || session_connected_get(session))
		irc_command_disconnect(name, params, session, target, quiet);

	return irc_command_connect(name, params, session, target, quiet);
}

/*****************************************************************************/

COMMAND(irc_command_msg)
{
	irc_private_t	*j = irc_private(session);
	const char	*uid=NULL;
        window_t	*w;
        int		class = EKG_MSGCLASS_SENT, ischn; 
	int		ekgbeep = EKG_NO_BEEP;
        char		*me, *format=NULL, *seq=NULL, *head, *chantypes, *coloured;
        char		**rcpts, prefix[2], *mline[2], mlinechr;
        const time_t	sent = time(NULL);					
	people_t	*person;
	people_chan_t	*perchn = NULL;
	int		secure = 0;
	char		*sid = NULL, *uid_full = NULL;
	unsigned char	*__msg = NULL;
	int		prv = 0;

	if (!session_check(session, 1, IRC3)) {
		printq("invalid_session");
		return -1;
	}

	if (!params[0] || !params[1]) {
		if (!params[0]) printq("not_enough_params", name);
		return -1;
	}

	uid = params[0];
	if (xstrncasecmp(uid, IRC4, 4)) {
		printq("invalid_session");
		return -1;
	}

	if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	prv = xstrcmp(name, "notice");
	sid = xstrdup(session->uid);
	uid_full = xstrdup(uid);
	mline[0] = (char *)params[1];
	while ( (mline[1]=xstrchr(mline[0], '\n')) )
	{
		mlinechr = *mline[1];
		*mline[1]='\0';
		__msg = xstrdup(mline[0]);

		query_emit(NULL, "message-encrypt", &sid, &uid_full, &__msg, &secure);
		irc_write(j, "%s %s :%s\r\n", prv?"PRIVMSG" : "NOTICE", uid+4, __msg);
		xfree(__msg);

		mline[0] = (char *)(mline[1]+1);
		*mline[1] = mlinechr;
	}
	__msg = xstrdup(mline[0]);
	query_emit(NULL, "message-encrypt", &sid, &uid_full, &__msg, &secure);
	irc_write(j, "%s %s :%s\r\n", prv?"PRIVMSG" : "NOTICE", uid+4, __msg);
	xfree(sid);
	xfree(uid_full);
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
	head = format_string(format_find(prv?
				ischn?"irc_msg_sent_chan":w?"irc_msg_sent_n":"irc_msg_sent":
				ischn?"irc_not_sent_chan":w?"irc_not_sent_n":"irc_not_sent"),
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
	const char	*p[2] = { target, params[0] };

	return irc_command_msg("msg", p, session, target, quiet);
}

COMMAND(irc_command_quote)
{
	irc_private_t	*j = irc_private(session);

	if (!session_check(session, 1, IRC3)) {
		printq("invalid_session");
		return -1;
	}

	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	irc_write(j, "%s\r\n", params[0]);

	return 0;
}


COMMAND(irc_command_pipl)
{
	irc_private_t	*j = irc_private(session);
	list_t		t1, t2;
	people_t	*per;
	people_chan_t	*chan;

	if (!session_check(session, 1, IRC3)) {
		printq("invalid_session");
		return -1;
	}

	debug("[irc] this is a secret command ;-)\n");

	for (t1 = j->people; t1; t1=t1->next) {
		per = (people_t *)t1->data;
		debug("(%s)![%s]@{%s} ", per->nick, per->ident, per->host);
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
	char *cmd = NULL;
	int ret = 0;
	
	if (!session_check(session, 1, IRC3)) {
		printq("invalid_session");
		return -1;
	}
	cmd = saprintf("/access --add %s -", params[0] ? params[0] : target);
	ret = command_exec(NULL, session, cmd, 0);

	xfree(cmd);

	return ret;
}

int irc_write_status(session_t *s, int quiet)
{
	irc_private_t	*j = irc_private(s);
	const char	*status;
	char		*descr;

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
	
	/*xfree(descr); dj->G: bugfix
	 *  G->dj: ?! what this causes, adn do we free this anywhere later ?
	 * 
	 */

	return 0;
}

COMMAND(irc_command_away)
{
	if (!session_check(session, 1, IRC3)) {
		printq("invalid_session");
		return -1;
	}

	if (!xstrcmp(name, "back")) {
#if 0
		if (!params[0]) {
#endif
			session_descr_set(session, NULL);
			session_status_set(session, EKG_STATUS_AVAIL);
			session_unidle(session);
#if 0
		} else {
			printq("invalid_params", "irc:back");
			return -1;
		}
#endif
	} else if (!xstrcmp(name, "away")) {
		if (params[0]) 
			session_descr_set(session, params[0]);
		else 
			session_descr_set(session, NULL);
		session_status_set(session, EKG_STATUS_AWAY);
		session_unidle(session);
	} else if (!xstrcasecmp(name, "_autoaway")) {
		session_status_set(session, EKG_STATUS_AUTOAWAY);
	} else if (!xstrcasecmp(name, "_autoback")) {
		session_status_set(session, EKG_STATUS_AVAIL);
		session_unidle(session);
	} else {
		printq("generic_error", "Ale o so chozi?");
		return -1;
	}

	irc_write_status(session, quiet);
	return 0;
}

/*****************************************************************************/

int irc_window_kill(void *data, va_list ap)
{
	window_t	**xw = va_arg(ap, window_t **);
	window_t	*w = *xw;
	irc_private_t	*j = NULL;
	char		*tmp = NULL;

	if (w && w->id && w->target && !xstrncasecmp(w->target, IRC4, 4) &&
			w->session && session_check(w->session, 1, IRC3) &&
			(j = irc_private(w->session)) &&
			(tmp = SOP(_005_CHANTYPES)) &&
			xstrchr(tmp, (w->target)[4]) &&
			irc_find_channel((j->channels), (w->target)) &&
			session_connected_get(w->session)
			)
	{
		irc_write(j, "PART %s :%s\r\n", (w->target)+4, PARTMSG(w->session, NULL));
	}
	return 0;
}

int irc_topic_header(void *data, va_list ap)
{
	char		**top   = va_arg(ap, char **);
	char		**setby = va_arg(ap, char **);
	char		**modes = va_arg(ap, char **);

	char		*targ = window_current->target;
	channel_t	*chanp = NULL;
	people_t 	*per   = NULL;

	irc_private_t	*j = NULL;
	char		*tmp = NULL;

	*top = *setby = *modes = NULL;
	if (targ && !xstrncasecmp(targ, IRC4, 4) && window_current->session &&
			session_check(window_current->session, 1, IRC3) &&
			(j = irc_private(window_current->session)) &&
			session_connected_get(window_current->session)
			)
	{ 
		/* channel */
		if ((tmp = SOP(_005_CHANTYPES)) && 
		     xstrchr(tmp, targ[4]) && 
		     (chanp = irc_find_channel((j->channels), targ))) {
			*top   = xstrdup(chanp->topic);
			*setby = xstrdup(chanp->topicby);
			*modes = xstrdup(chanp->mode_str);
		/* person */
		} else if ((per = irc_find_person((j->people), targ))) { 
			*top   = saprintf("%s@%s", per->ident, per->host);
			*setby = xstrdup(per->realname);
		}
	}
	return 0;
}

char *irc_getchan_int(session_t *s, const char *name, int checkchan)
{
	char		*ret, *tmp;
	irc_private_t	*j = irc_private(s);

	if (!xstrlen(name))
		return NULL;

	if (!xstrncasecmp(name, IRC4, 4))
		ret = xstrdup(name);
	else
		ret = saprintf("%s%s", IRC4, name);

	if (checkchan == 2) 
		return ret;

	tmp = SOP(_005_CHANTYPES);
	if (tmp && ((!!xstrchr(tmp, ret[4]))-checkchan) )
		return ret;
	else
		xfree(ret);
	return NULL;
}

/* pr - specifies priority of check
 *   0 - first param, than current window
 *   1 - reverse
 * as side effect it adjust table passed as v argument
 */
char *irc_getchan(session_t *s, const char **params, const char *name,
		char ***v, int pr, int checkchan)
{
	char		*chan;
	const char	*tf, *ts, *tp; /* first, second */
	int		i = 0, parnum = 0, argnum = 0, hasq = 0;
	list_t		l;


	if (!session_check(s, 1, IRC3)) {
		print("invalid_session");
		return 0;
	}
	if (!session_connected_get(s)) {
		print("not_connected", session_name(s));
		return 0;
	}

	if (params) tf = params[0];
	else tf = NULL;
	ts = window_current->target;

	if (pr) { tp=tf; tf=ts; ts=tp; }

	if (!(chan=irc_getchan_int(s, tf, checkchan))) {
		if (!(chan = irc_getchan_int(s, ts, checkchan)))
		{
			print("invalid_params", name);
			return 0;
		}
		pr = !!pr;
	} else {
		pr = !!!pr;
	}

	for (l = commands; l; l = l->next) {
		command_t *c = l->data;
		char *tmpname = saprintf("%s%s", IRC4, name);

		if (!xstrcasecmp(tmpname, c->name) && &irc_plugin == c->plugin)
			while (c->params[parnum])
			{
				if (!strcmp(c->params[parnum], "?"))
					hasq = 1;
				parnum++;
			}
		xfree(tmpname);
	}

	do {
		if (params)
			while (params[argnum])
				argnum++;

		(*v) = (char **)xcalloc(parnum+1, sizeof (char *));

		debug("argnum %d parnum %d pr %d hasq %d\n",
				argnum, parnum, pr, hasq);

		if (!pr) {
			if (!hasq)
			{
				for (i=0; i<parnum && i<argnum; i++) {
					(*v)[i] = xstrdup(params[i]);
					debug("  v[%d] - %s\n", i, (*v)[i]);
				}
			} else {
				for (i=0; i < parnum-2 && i<argnum; i++)
				{
					(*v)[i] = xstrdup(params[i]);
					debug("o v[%d] - %s\n", i, (*v)[i]);
				}
				if (params [i] && params[i+1]) {
					(*v)[i] = saprintf("%s %s",
						   params[i], params[i+1]);
					i++;
				} else if (params[i]) {
					(*v)[i] = xstrdup(params[i]);
					i++;
				}
			}
			(*v)[i] = NULL;
		} else {
			for (i=0; i<argnum; i++)
				(*v)[i] = xstrdup(params[i+1]);
		}
		/*
		i=0;
		while ((*v)[i])
			debug ("zzz> %s\n", (*v)[i++]);
		debug("\n");
		*/
	} while (0);

	return chan;
}

int irc_getchan_free(char **mp)
{
	int i=0;
	while (mp[i]) xfree(mp[i++]);
	xfree(mp);
	return 0;
}
/*****************************************************************************/

COMMAND(irc_command_names)
{
	irc_private_t	*j = irc_private(session);
/*	people_t	*per;
	people_chan_t	*pchan;*/
	channel_t       *chan;
	userlist_t      *ulist;
	char		mode[2], *buf, **mp, *channame;
	list_t 		l;
	const char      *sort_status[5]	= {EKG_STATUS_AVAIL, EKG_STATUS_AWAY,
				EKG_STATUS_XA, EKG_STATUS_INVISIBLE, NULL};
	/* G->dj: what do you need status_error ? I don't know network, where
	 * there would be more than 4 modes...
	 */
	char            *sort_modes	= xstrchr(SOP(_005_PREFIX), ')');
	int             lvl_total[5]     = {0, 0, 0, 0, 0};
	int		lvl   = 0;
	int		count = 1;
	
	if ((!sort_modes) ||  !(channame = irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN))) 
	 		return -1;

	mode [1] = '\0';
	chan = irc_find_channel(j->channels, channame);
	if (!chan) {
		printq("generic", "irc_command_names: wtf?");
		return -1;
	}

	buf = xmalloc(1550);
	print_window(channame, session, 0, "irc_names_beg", session_name(session), channame+4);
	while (*sort_modes || lvl == 5) {
//	while (*sort_modes && lvl <=5 ) {
		if (lvl < 5) {
			sort_modes++;
			mode[0] = (*sort_modes)?(*sort_modes):' ';
		}
		else if (lvl == 5) {
			 mode[0] = '?'; 
			 /* TODO: znalezc ta osobe, i jej mode */
		}
		else break; /* tak cholernie na wszelki wypadek */
		
		for (l = chan->window->userlist; l; l = l->next) {
			ulist = l->data;
			if (!ulist || xstrcmp(ulist->status, sort_status[lvl]) )
				continue;
			lvl_total[lvl]++;
			strcat(buf, format_string(format_find("IRC_NAMES"), mode, (ulist->uid + 4)));

			count++;
			if (count == 7) {
				printq("generic", buf);
				buf[0] = '\0';
				count = 1;
			}
		}
		lvl++;
	}
	/*debug(" person->channels: %08X %s %08X>\n", per->channels, channame, chan);*/
	if (count != 7 && count != 1) {
		printq("generic", buf);
	}
/*
	if (lvl < 4) {
		lvl_total[3] = lvl_total[2];
		lvl_total[2] = lvl_total[1]; 
		lvl_total[1] = 0;
	}
	print_window(channame, session, 0, "irc_names_tot", session_name(session), channame+4, itoa(list_count(chan->window->userlist)), itoa(lvl_total[0]), itoa(lvl_total[1]), itoa(lvl_total[2]), itoa(lvl_total[3]), itoa(lvl_total[4]));
*/
	irc_getchan_free(mp);
	xfree (channame);
	xfree(buf);
	return 0;
}

COMMAND(irc_command_topic)
{
	irc_private_t	*j = irc_private(session);
	char		**mp, *chan, *newtop;

	if (!(chan=irc_getchan(session, params, name, 
					&mp, 0, IRC_GC_CHAN))) 
		return -1;

	if (*mp)
		if (xstrlen(*mp)==1 && **mp==':')
			newtop = saprintf("TOPIC %s :\r\n", chan+4);
		else
			newtop = saprintf("TOPIC %s :%s\r\n", 
						chan+4, *mp);
	else
		newtop = saprintf("TOPIC %s\r\n", chan+4);

	irc_write(j, newtop);
	irc_getchan_free(mp);
	xfree (newtop);
	xfree (chan);
	return 0;
}

COMMAND(irc_command_who)
{
	irc_private_t	*j = irc_private(session);
	char		**mp, *chan;

	if (!(chan=irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN)))
		return -1;

	irc_write(j, "WHO %s\r\n", chan+4);

	xfree(chan);
	return 0;
}

COMMAND(irc_command_invite)
{
	irc_private_t	*j = irc_private(session);
	char		**mp, *chan;

	if (!(chan=irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN)))
		return -1;

	if (!(*mp)) {
		printq("not_enough_params", name);
		xfree(chan);
		return -1;
	}
	irc_write(j, "INVITE %s %s\r\n", *mp, chan+4);

	irc_getchan_free(mp);
	xfree(chan);
	return 0;
}

COMMAND(irc_command_kick)
{
	irc_private_t	*j = irc_private(session);
	char		**mp, *chan;

	if (!(chan=irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN))) 
		return -1;

	if (!(*mp)) {
		printq("not_enough_params", name);
		xfree(chan);
		return -1;
	}
	irc_write(j, "KICK %s %s :%s\r\n", chan+4, *mp, KICKMSG(session, mp[1]));

	irc_getchan_free(mp);
	xfree(chan);
	return 0;
}

COMMAND(irc_command_unban)
{
	irc_private_t	*j = irc_private(session);
	char		*channame, **mp;
	channel_t	*chan = NULL;
	list_t		banlist;
	int		i, banid = 0;

	if (!(channame = irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN))) 
		return -1;

	debug("[irc]_command_unban(): chan: %s mp[0]:%s mp[1]:%s\n",
			channame, mp[0], mp[1]);

	if (!(*mp)) {
		printq("not_enough_params", name);
		xfree(channame);
		return -1;
	}
	else {
		if ( (banid = atoi(*mp)) ) {
			chan = irc_find_channel(j->channels, channame+4);
			if (chan && (banlist = (chan->banlist)) ) {
				for (i=1; banlist && i<banid; banlist = banlist->next, ++i);
				if (banlist) /* fit or add  i<=banid) ? */
					irc_write(j, "MODE %s -b %s\r\n", channame+4, banlist->data);
				else 
					debug("%d %d out of range or no such ban %08x\n", i, banid, banlist);
			}
			else
				debug("Chanell || chan->banlist not found -> channel not synced ?!Try /mode +b \n");
		}
		else { 
			irc_write(j, "MODE %s -b %s\r\n", channame+4, *mp);
		}
	}
	irc_getchan_free(mp);
	xfree(channame);
	return 0;

}

COMMAND(irc_command_alist)
{
/*
 *	if (params[1] == NULL && target) 
 *		params[1] = target;
 */
	debug("[irc_alist] ALIST: %s (%s, %s)\n", target, params[0], params[1]);
	return 0;
}

COMMAND(irc_command_ban)
{
	irc_private_t	*j = irc_private(session);
	char		*chan, **mp, *temp = NULL;
	people_t	*person;

	if (!(chan=irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN))) 
		return -1;

	debug("[irc]_command_ban(): chan: %s mp[0]:%s mp[1]:%s\n",
			chan, mp[0], mp[1]);

	if (!(*mp))
		irc_write(j, "MODE %s +b \r\n", chan+4);
	else {
		person = irc_find_person(j->people, (char *) *mp);
		if (person) 
			temp = irc_make_banmask(session, person->nick+4, person->ident, person->host);
		if (temp) {
			irc_write(j, "MODE %s +b %s\r\n", chan+4, temp);
			xfree(temp);
		} else
			irc_write(j, "MODE %s +b %s\r\n", chan+4, *mp);
	}
	irc_getchan_free(mp);
	xfree(chan);
	return 0;
}

COMMAND(irc_command_kickban) {
	const char	*p[4] = { params[0], params[1], params[2], NULL };

	if (!xstrcmp(name, "kickban"))
	{
		irc_command_kick("kick", params, session, target, quiet);
		irc_command_ban("ban", params, session, target, quiet);
	} else {
		irc_command_ban("ban", params, session, target, quiet);
		irc_command_kick("kick", params, session, target, quiet);
	}
	if (p) ;
	return 0;
}


COMMAND(irc_command_devop)
{
	irc_private_t	*j = irc_private(session);
	int		modes, i;
	char		**mp, *op, *nicks, *tmp, c, *chan, *p;

	if (!(chan=irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN))) 
		return -1;

	if (!(*mp)) {
		printq("not_enough_params", name);
		xfree(chan);
		return -1;
	}

	nicks = xstrdup(*mp);
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
	irc_getchan_free(mp);
	return 0;
}

COMMAND(irc_command_ctcp)
{
	int		i;
	char		*who;
	char		**mp;

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

	irc_write(irc_private(session), "PRIVMSG %s :\01%s\01\r\n",
			who+4, ctcps[i].name?ctcps[i].name:(*mp));

	irc_getchan_free(mp);
	xfree(who);
	return 0;
}

COMMAND(irc_command_ping)
{
	char		**mp, *who;
	struct timeval	tv;

	if (!(who=irc_getchan(session, params, name, &mp, 0, IRC_GC_ANY))) 
		return -1;

	gettimeofday(&tv, NULL);
	irc_write(irc_private(session), "PRIVMSG %s :\01PING %d %d\01\r\n",
			who+4 ,tv.tv_sec, tv.tv_usec);

	irc_getchan_free(mp);
	xfree(who);
	return 0;
}

COMMAND(irc_command_me)
{
	irc_private_t	*j = irc_private(session);
	char		**mp, *chan, *chantypes = SOP(_005_CHANTYPES), *str, *col;
	int		mw = session_int_get(session, "make_window"), ischn;

	if (!(chan=irc_getchan(session, params, name,
					&mp, 1, IRC_GC_ANY)))
		return -1;

	ischn = chantypes?!!xstrchr(chantypes, chan[4]):0;
	
	str = xstrdup(*mp);

	irc_write(irc_private(session), "PRIVMSG %s :\01ACTION %s\01\r\n",
			chan+4, str?str:"");

	col = irc_ircoldcolstr_to_ekgcolstr(session, str, 1);
	print_window(chan, session, ischn?(mw&1):!!(mw&2),
			ischn?"irc_ctcp_action_y_pub":"irc_ctcp_action_y",
			session_name(session), j->nick, chan, col);

	irc_getchan_free(mp);
	xfree(chan);
	xfree(col);
	xfree(str);
	return 0;
}

COMMAND(irc_command_mode)
{
	char	**mp, *chan;

	if (!(chan=irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN))) 
		return -1;
/* G->dj: I'm still leaving this 
	if (!(*mp)) {
		print("not_enough_params", name);
		irc_getchan_free(mp);
		xfree(chan);
		return -1;
	}
*/
	debug("%s %s \n", chan, mp[0]);
	if (!(*mp))
		irc_write(irc_private(session), "MODE %s\r\n",
				chan+4);
	else
		irc_write(irc_private(session), "MODE %s %s\r\n",
				chan+4, *mp);

	irc_getchan_free(mp);
	xfree(chan);
	return 0;
}

COMMAND(irc_command_umode)
{
	irc_private_t	*j = irc_private(session);

	if (!(*params)) {
		print("not_enough_params", name);
		return -1;
	}

	irc_write(j, "MODE %s %s\r\n", j->nick, *params);

	return 0;
}

COMMAND(irc_command_whois)
{
	char	**mp, *person;

	if (!(person = irc_getchan(session, params, name,
					&mp, 0, IRC_GC_NOT_CHAN)))
		return -1;

	debug("irc_command_whois(): %s\n", name);
	if (!xstrcmp(name, "whowas"))
		irc_write(irc_private(session),	"WHOWAS %s\r\n", person+4);
        else if(!xstrcmp(name, "wii"))
		irc_write(irc_private(session),	"WHOIS %s %s\r\n", person+4, person+4);
	else
		irc_write(irc_private(session),	"WHOIS %s\r\n",  person+4);

	irc_getchan_free(mp);
	xfree (person);
	return 0;
}

int irc_status_show_handle(void *data, va_list ap)
{
	char		**uid = va_arg(ap, char**);
	session_t	*s = session_find(*uid);
//	const char	*p[2];
	const char	*p[1];

	if (!s)
		return -1;

	p[0] = irc_private(s)->nick;
	p[1] = 0;

	return irc_command_whois("wii", p, s, NULL, 0);
}

COMMAND(irc_command_query)
{
	irc_private_t	*j = irc_private(session);
	window_t	*w;
	char		**mp, *tar, **p = xcalloc(3, sizeof(char*)), *tmp;
	int		i;

	for (i = 0; i<2 && params[i]; i++)
                p[i] = xstrdup(params[i]);

	p[i] = NULL;/* G->dj: I'm leaving this, I like things like 
		     * that to be clearly visible
		     */

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

	if (!w) {
		w = window_new(tmp, session, 0);
		if (session_int_get(session, "auto_lusers_sync") > 0)
			irc_write(j, "USERHOST %s\r\n", tmp+4);
	}

	window_switch(w->id);

	xfree(tmp);
	for (i = 0; i<2 && params[i]; i++)
        	xfree(p[i]);

	irc_getchan_free(mp);
	xfree(tar);
	xfree(p);
	return 0;
}

COMMAND(irc_command_jopacy)
{
	irc_private_t	*j = irc_private(session);
	char		**mp, *tar = NULL, *pass = NULL, *str, *tmp;
	channel_t	*chan;

	if (!(tar = irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN)))
		return -1;

	if (!xstrcmp(name, "cycle")) {
		chan = irc_find_channel(j->channels, tar);
		if (chan && (pass = xstrchr(chan->mode_str, 'k')))
			pass+=2;
		debug("[IRC_CYCLE] %s\n", pass);
	}

	tmp = saprintf("JOIN %s%s\r\n", tar+4, pass ? pass : "");
	if (!xstrcmp(name, "part") || !xstrcmp(name, "cycle")) {
		str = saprintf("PART %s :%s\r\n%s", tar+4,
				PARTMSG(session,(*mp)),
				!xstrcmp(name, "cycle")?tmp:"");
	} else if (!xstrcmp(name, "join")) {
		str = tmp; tmp=NULL;
	} else
		return 0;

	irc_write(j, str);

	irc_getchan_free(mp);
	xfree(tar);
	xfree(str);
	xfree(tmp);

	return 0;
}

COMMAND(irc_command_nick)
{
	irc_private_t	*j = irc_private(session);

	if (!session_check(session, 1, IRC3)) {
		printq("invalid_session");
		return -1;
	}
	
	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}
	if (!j) {
		printq("sesion_doesnt_exist", session->uid);
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

COMMAND(irc_command_test) {
	irc_private_t	*j = irc_private(session);
	list_t       tlist = j->connlist;
	connector_t  *test = 0;
	
	for (tlist = j->connlist; tlist; tlist = tlist->next) {
		test = tlist->data;
		DOT("Connect to:", test, session);
	}
	
	for (tlist = j->bindlist; tlist; tlist = tlist->next) {
		test = tlist->data;
		DOT("Bind to:", test, session);
	}
	
	if (j->conntmplist && j->conntmplist->data)  {
		test = j->conntmplist->data;
		DOT("Connecting:", test, session);
	}
	
	if (j->conntmplist && j->bindtmplist->data) {
		test = j->bindtmplist->data;
		DOT("Binding:", test, session);
	}
	return 0;
}

/*                                                                       *
 * ======================================== INIT/DESTROY --------------- *
 *                                                                       */

#define params(x) x

int irc_plugin_init(int prio)
{
	struct passwd	*pwd_entry = getpwuid(getuid());

	plugin_register(&irc_plugin, prio);

	query_connect(&irc_plugin, "protocol-validate-uid", irc_validate_uid, NULL);
	query_connect(&irc_plugin, "plugin-print-version", irc_print_version, NULL);
	query_connect(&irc_plugin, "ui-window-kill",	irc_window_kill, NULL);
	query_connect(&irc_plugin, "session-added",	irc_session, (void*) 1);
	query_connect(&irc_plugin, "session-removed",	irc_session, (void*) 0);
	query_connect(&irc_plugin, "irc-topic",		irc_topic_header, (void*) 0);
	query_connect(&irc_plugin, "status-show",	irc_status_show_handle, NULL);
	query_connect(&irc_plugin, "irc-kick",		irc_onkick_handler, 0);
	
	query_connect(&irc_plugin, "config-postinit",	irc_postinit, 0);

	command_add(&irc_plugin, IRC4, "?",		irc_command_inline_msg, 0, NULL);
	command_add(&irc_plugin, "irc:connect", NULL,	irc_command_connect, 0, NULL);
	command_add(&irc_plugin, "irc:disconnect", "r ?",irc_command_disconnect, 0, NULL);
	command_add(&irc_plugin, "irc:reconnect", "r ?",	irc_command_reconnect, 0, NULL);

	command_add(&irc_plugin, "irc:join", "w", 	irc_command_jopacy, 0, NULL);
	command_add(&irc_plugin, "irc:part", "w ?",	irc_command_jopacy, 0, NULL);
	command_add(&irc_plugin, "irc:cycle", "w ?",	irc_command_jopacy, 0, NULL);
	command_add(&irc_plugin, "irc:query", "uUw",	irc_command_query, 0, NULL);
	command_add(&irc_plugin, "irc:nick", "?",	irc_command_nick, 0, NULL);
	command_add(&irc_plugin, "irc:topic", "w ?",	irc_command_topic, 0, NULL);
	command_add(&irc_plugin, "irc:people", NULL,	irc_command_pipl, 0, NULL);
	command_add(&irc_plugin, "irc:names", "w?",	irc_command_names, 0, NULL);
	command_add(&irc_plugin, "irc:add", NULL,	irc_command_add, 0, NULL);
	command_add(&irc_plugin, "irc:msg", "uUw ?",	irc_command_msg, 0, NULL);
	command_add(&irc_plugin, "irc:notice", "uUw ?",	irc_command_msg, 0, NULL);
	command_add(&irc_plugin, "irc:me", "uUw ?",	irc_command_me, 0, NULL);
	command_add(&irc_plugin, "irc:ctcp", "uUw ?",	irc_command_ctcp, 0, NULL);
	command_add(&irc_plugin, "irc:ping", "uUw ?",	irc_command_ping, 0, NULL);
	command_add(&irc_plugin, "irc:mode", "w ?",	irc_command_mode, 0, NULL);
	command_add(&irc_plugin, "irc:umode", "?",	irc_command_umode, 0, NULL);
	command_add(&irc_plugin, "irc:wii", "uU",	irc_command_whois, 0, NULL);
	command_add(&irc_plugin, "irc:whois", "uU",	irc_command_whois, 0, NULL);
	command_add(&irc_plugin, "irc:find", "uU",	irc_command_whois, 0, NULL); /* for auto_find */
	command_add(&irc_plugin, "irc:whowas", "uU",	irc_command_whois, 0, NULL);

	/* dj>
	 * what about implementing something like that in command_add:
	 * that whatever is in \" \" would be send to executed command
	 * 
	 * without that we have to make handler for each function
	 * even if there is nothing interesting there ;/
	 * PS: maybe such a thing is already implemented, and I don't know that
	 *
	 * g> I'm guessing you wan't to use it just with _some_ commands
	 *    not all that are below...
	 *
	 * d> Why not ? ;)
	 *    I think it could be aliases to command /quote ....
	 *    I really don't like writing unnecessary code (thx crs) ;)
	 */ 
	/* TODO 
	command_add(&irc_plugin, "irc:admin", "",       NULL, 0, NULL);   q admin
	*/
	command_add(&irc_plugin, "irc:access", "p ?",		irc_command_alist, 0, 
				    "-a --add -d --delete -s --show -l --list");
	command_add(&irc_plugin, "irc:ban",  "uUw uU",		irc_command_ban, 0, NULL); 
	command_add(&irc_plugin, "irc:kick", "uUw uU ?",	irc_command_kick, 0, NULL);
	command_add(&irc_plugin, "irc:kickban", "uUw uU ?",	irc_command_kickban, 0, NULL);
	command_add(&irc_plugin, "irc:bankick", "uUw uU ?",	irc_command_kickban, 0, NULL);
	command_add(&irc_plugin, "irc:unban",  "uUw uU",	irc_command_unban, 0, NULL); 
	command_add(&irc_plugin, "irc:invite", "uUw uUw",	irc_command_invite, 0, NULL);
	command_add(&irc_plugin, "irc:who", "uUw",		irc_command_who, 0, NULL);

/*
	command_add(&irc_plugin, "irc:map",  "",        NULL, 0, NULL);   q map
	command_add(&irc_plugin, "irc:links",  "",      NULL, 0, NULL); V q links
	command_add(&irc_plugin, "irc:oper", "",	NULL, 0, NULL);   q oper %nick %pass
	command_add(&irc_plugin, "irc:trace", "",	NULL, 0, NULL);   q trace %...
	command_add(&irc_plugin, "irc:stats", "\"STATS\" ?",irc_command_quote, 0, NULL); V q stats
	command:add(&irc_plugin, "irc:list", .....)			V q list 
	*/
	/* G: Yeah I know it look shitty as hell
	 */
	command_add(&irc_plugin, "irc:op", "uUw uU uU uU uU uU uU ?",	irc_command_devop, 0, NULL);
	command_add(&irc_plugin, "irc:deop", "uUw uU uU uU uU uU uU ?",	irc_command_devop, 0, NULL);
	command_add(&irc_plugin, "irc:voice", "uUw uU uU uU uU uU uU ?",	irc_command_devop, 0, NULL);
	command_add(&irc_plugin, "irc:devoice", "uUw uU uU uU uU uU uU ?",	irc_command_devop, 0, NULL);
	command_add(&irc_plugin, "irc:halfop", "uUw uU uU uU uU uU uU ?",	irc_command_devop, 0, NULL);
	command_add(&irc_plugin, "irc:dehalfop", "uUw uU uU uU uU uU uU ?",	irc_command_devop, 0, NULL);
	
	command_add(&irc_plugin, "irc:away", "?",	irc_command_away, 0, NULL);
	command_add(&irc_plugin, "irc:_autoaway", NULL,	irc_command_away, 0, NULL);
	command_add(&irc_plugin, "irc:back", NULL,	irc_command_away, 0, NULL);
	command_add(&irc_plugin, "irc:_autoback", NULL,	irc_command_away, 0, NULL);
	command_add(&irc_plugin, "irc:quote", "?",	irc_command_quote, 0, NULL);
	command_add(&irc_plugin, "irc:_conntest", "?",	irc_command_test, 0, NULL);
#ifdef IRC_BUILD_ALIAS
// dj: I get sigsegv, and probably memleak
/*	command_add(&irc_plugin, "irc:n", "w?",		irc_command_names, 0, NULL); */
	/* if somebody wants /n->/names without aliases.... */
	alias_add(xstrdup("irc:n ? irc:names"), 0, 0);

#endif

	/* lower case: names of variables that reffer to client itself */
	plugin_var_add(&irc_plugin, "alt_nick", VAR_STR, NULL, 0, NULL);
	plugin_var_add(&irc_plugin, "alias", VAR_STR, 0, 0, NULL);
/*	plugin_var_add(&irc_plugin, "alist", VAR_STR, 0, 0, irc_load_alist); */
	plugin_var_add(&irc_plugin, "alist", VAR_STR, 0, 0, NULL);

	plugin_var_add(&irc_plugin, "auto_away", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "auto_back", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "auto_connect", VAR_BOOL, "0", 0, NULL);
	/* it's really auto_whois */
	plugin_var_add(&irc_plugin, "auto_find", VAR_BOOL, "0", 0, NULL); 
	plugin_var_add(&irc_plugin, "auto_reconnect", VAR_INT, "0", 0, NULL); 
	/* like channel_sync in irssi; better DO NOT turn it off! */
	plugin_var_add(&irc_plugin, "auto_channel_sync", VAR_BOOL, "1", 0, NULL);
	/* sync lusers, stupid ;(,  G->dj: well why ? */
	plugin_var_add(&irc_plugin, "auto_lusers_sync", VAR_BOOL, "0", 0, NULL);	
	plugin_var_add(&irc_plugin, "ban_type", VAR_INT, "10", 0, NULL);
	plugin_var_add(&irc_plugin, "close_windows", VAR_BOOL, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "dcc_port", VAR_INT, "0", 0, NULL);
        plugin_var_add(&irc_plugin, "display_notify", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "hostname", VAR_STR, 0, 0, irc_changed_resolve);
	plugin_var_add(&irc_plugin, "log_formats", VAR_STR, "xml,simple", 0, NULL);
	plugin_var_add(&irc_plugin, "make_window", VAR_INT, "2", 0, NULL);
	plugin_var_add(&irc_plugin, "prefer_family", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "nickname", VAR_STR, pwd_entry ? pwd_entry->pw_name : NULL, 0, NULL);
	plugin_var_add(&irc_plugin, "password", VAR_STR, 0, 1, NULL);
	plugin_var_add(&irc_plugin, "port", VAR_INT, "6667", 0, NULL);
	plugin_var_add(&irc_plugin, "realname", VAR_STR, pwd_entry ? pwd_entry->pw_gecos : NULL, 0, NULL);
	plugin_var_add(&irc_plugin, "server", VAR_STR, 0, 0, irc_changed_resolve);

	/* upper case: names of variables, that reffer to protocol stuff */
	plugin_var_add(&irc_plugin, "AUTO_JOIN", VAR_STR, 0, 0, NULL);
	plugin_var_add(&irc_plugin, "AUTO_JOIN_CHANS_ON_INVITE", VAR_BOOL, "0", 0, NULL);
	/* TODO ;> */
	plugin_var_add(&irc_plugin, "DEFAULT_COLOR", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "DISPLAY_PONG", VAR_BOOL, "1", 0, NULL);
	plugin_var_add(&irc_plugin, "DISPLAY_AWAY_NOTIFICATION", VAR_INT, "1", 0, NULL);
	plugin_var_add(&irc_plugin, "DISPLAY_IN_CURRENT", VAR_INT, "2", 0, NULL);
	plugin_var_add(&irc_plugin, "DISPLAY_NICKCHANGE", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "DISPLAY_QUIT", VAR_INT, "0", 0, NULL);
	/* plugin_var_add(&irc_plugin, "HIGHLIGHTS", VAR_STR, 0, 0, NULL); */
	plugin_var_add(&irc_plugin, "KICK_MSG", VAR_STR, DEFKICKMSG, 0, NULL);
	plugin_var_add(&irc_plugin, "PART_MSG", VAR_STR, DEFPARTMSG, 0, NULL);
	plugin_var_add(&irc_plugin, "QUIT_MSG", VAR_STR, DEFQUITMSG, 0, NULL);
	plugin_var_add(&irc_plugin, "REJOIN", VAR_INT, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "REJOIN_TIME", VAR_INT, "2", 0, NULL);
	
	plugin_var_add(&irc_plugin, "SHOW_NICKMODE_EMPTY", VAR_INT, "1", 0, NULL);
	plugin_var_add(&irc_plugin, "SHOW_MOTD", VAR_BOOL, "1", 0, NULL);
	plugin_var_add(&irc_plugin, "STRIPMIRCCOL", VAR_BOOL, "0", 0, NULL);
	plugin_var_add(&irc_plugin, "VERSION_NAME", VAR_STR, 0, 0, NULL);
	plugin_var_add(&irc_plugin, "VERSION_NO", VAR_STR, 0, 0, NULL);
	plugin_var_add(&irc_plugin, "VERSION_SYS", VAR_STR, 0, 0, NULL);

#if 0
	for (l = sessions; l; l = l->next)
		irc_private_init((session_t*) l->data);
#endif
	return 0;
}

static int irc_plugin_destroy()
{
	list_t	l;

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
	
	format_add("irc_not_sent",	"%P(%n%3/%5%P)%n %6", 1);
	format_add("irc_not_sent_n",	"%P(%n%3%P)%n %6", 1);
	format_add("irc_not_sent_chan",	"%P(%w%{2@%+gcp}X%2%3%P)%n %6", 1);
	format_add("irc_not_sent_chanh","%P(%W%{2@%+GCP}X%2%3%P)%n %6", 1);

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
	format_add("irc_not_f_server",	"%g!%3%n %6", 1);

	format_add("irc_names_beg",     "[%gUsers %G%2%n]", 1);
	format_add("irc_names_tot",     "%> %WEKG2: %2%n: Total of %W%3%n nicks [%W%4%n ops, %W%5%n halfops, %W%6%n voices, %W%7%n normal]\n", 1);

	format_add("irc_joined", _("%> %Y%2%n has joined %4\n"), 1);
	format_add("irc_joined_you", _("%> %RYou%n have joined %4\n"), 1);
	format_add("irc_left", _("%> %g%2%n has left %4 (%5)\n"), 1);
	format_add("irc_left_you", _("%> %RYou%n have left %4 (%5)\n"), 1);
	format_add("irc_kicked", _("%> %Y%2%n has been kicked out by %R%3%n from %5 (%6)\n"), 1);
	format_add("irc_kicked_you", _("%> You have been kicked out by %R%3%n from %5 (%6)\n"), 1);
	format_add("irc_quit", _("%> %Y%2%n has quit irc (%4)\n"), 1);
	format_add("irc_split", "%> ", 1);
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

	
	format_add("RPL_INVITE",    "%> Inviting %W%2%n to %W%3%n\n", 1);
 	/* Used in: /mode +b|e|I %2 - chan %3 - data from server */
	/* THIS IS TEMPORARY AND WILL BE DONE OTHER WAY, DO NOT USE THIS STYLES
	 */
	format_add("RPL_LISTSTART",  "%g,+=%G-----\n", 1);
	format_add("RPL_EXCEPTLIST", "%g|| %n %5 - %W%2%n: except %c%3\n", 1);
	format_add("RPL_BANLIST",    "%g|| %n %5 - %W%2%n: ban %c%3\n", 1);
	format_add("RPL_INVITELIST", "%g|| %n %5 - %W%2%n: invite %c%3\n", 1);;
	format_add("RPL_EMPTYLIST" , "%g|| %n Empty list \n", 1);
	format_add("RPL_LINKS",      "%g|| %n %5 - %2  %3  %4\n", 1);
	format_add("RPL_ENDOFLIST",  "%g`+=%G----- %2%n\n", 1);

	/* %2 - number; 3 - type of stats (I, O, K, etc..) ....*/
	format_add("RPL_STATS",      "%g|| %3 %n %4 %5 %6 %7 %8\n", 1);
	format_add("RPL_STATS_EXT",  "%g|| %3 %n %2 %4 %5 %6 %7 %8\n", 1);
	format_add("RPL_STATSEND",   "%g`+=%G--%3--- %2\n", 1);
	/*
	format_add("RPL_CHLISTSTART",  "%g,+=%G lp %2\t%3\t%4\n", 1);
	format_add("RPL_CHLIST",       "%g|| %n %5 %2\t%3\t%4\n", 1);
	*/
	format_add("RPL_CHLISTSTART","%g,+=%G lp %2\t%3\t%4\n", 1);
	format_add("RPL_LIST",       "%g|| %n %5 %2\t%3\t%4\n", 1);

	/* 2 - number; 3 - chan; 4 - ident; 5 - host; 6 - server ; 7 - nick; 8 - mode; 9 -> realname
	 * format_add("RPL_WHOREPLY",   "%g|| %c%3 %W%7 %n%8 %6 %4@%5 %W%9\n", 1);
	 */
	format_add("RPL_WHOREPLY",   "%g|| %c%3 %W%7 %n%8 %6 %4@%5 %W%9\n", 1);
	/* delete those irssi-like styles */

	format_add("RPL_AWAY", _("%G||%n away     : %2 - %3\n"), 1);
	/* in whois %2 is always nick */
	format_add("RPL_WHOISUSER", _("%G.+===%g-----\n%G||%n (%T%2%n) (%3@%4)\n"
				"%G||%n realname : %6\n"), 1);

	format_add("RPL_WHOWASUSER", _("%G.+===%g-----\n%G||%n (%T%2%n) (%3@%4)\n"
				"%G||%n realname : %6\n"), 1);

/* %2 - nick %3 - there is/ was no such nickname / channel, and so on... */
	/*
	format_add("IRC_WHOERROR", _("%G.+===%g-----\n%G||%n %3 (%2)\n"), 1);
	format_add("IRC_ERR_NOSUCHNICK", _("%n %3 (%2)\n"), 1);
	*/

	format_add("RPL_WHOISCHANNELS", _("%G||%n %|channels : %3\n"), 1);
	format_add("RPL_WHOISSERVER", _("%G||%n %|server   : %3 (%4)\n"), 1);
	format_add("RPL_WHOISOPERATOR", _("%G||%n %|ircOp    : %3\n"), 1);
	format_add("RPL_WHOISIDLE", _("%G||%n %|idle     : %3 (signon: %4)\n"), 1);
	format_add("RPL_ENDOFWHOIS", _("%G`+===%g-----\n"), 1);
	format_add("RPL_ENDOFWHOWAS", _("%G`+===%g-----\n"), 1);

	format_add("RPL_TOPIC", _("%> Topic %2: %3\n"), 1);
	/* \n not needed if you're including date [%4] */
	format_add("IRC_RPL_TOPICBY", _("%> set by %2 on %4"), 1);
	format_add("IRC_TOPIC_CHANGE", _("%> %T%2%n changed topic on %T%4%n: %5\n"), 1);
	format_add("IRC_TOPIC_UNSET", _("%> %T%2%n unset topic on %T%4%n\n"), 1);
	format_add("IRC_MODE_CHAN_NEW", _("%> %2/%4 sets mode [%5]\n"), 1);
	format_add("IRC_MODE_CHAN", _("%> %2 mode is [%3]\n"), 1);
	format_add("IRC_MODE", _("%> (%1) %2 set mode %3 on You\n"), 1);
	
	format_add("IRC_NAMES", _("%K[%W%1%w%[9]2%K]%n"), 1);

	format_add("IRC_INVITE", _("%> %W%2%n invites you to %W%5%n\n"), 1);
	format_add("IRC_PINGPONG", _("%) (%1) ping/pong %c%2%n\n"), 1);
	format_add("IRC_YOUNEWNICK", _("%> You are now known as %G%3%n\n"), 1);
	format_add("IRC_NEWNICK", _("%> %g%2%n is now known as %G%4%n\n"), 1);
	format_add("IRC_TRYNICK", _("%> Will try to use %G%2%n instead\n"), 1);
	format_add("IRC_CHANNEL_SYNCED", "%> Join to %W%2%n was synced in %W%3.%4%n secs", 1);
	/* %1 - sesja ; %2 - Connect, BIND, whatever, %3 - hostname %4 - adres %5 - port 
	 * %6 - family (debug pursuit only) */
	format_add("IRC_TEST", "%> (%1) %2 to %W%3%n [%4] port %W%5%n (%6)", 1);
	/* j/w %6 - error */
	format_add("IRC_TEST_FAIL", "%> (%1) Error: %2 to %W%3%n [%4] port %W%5%n (%6)", 1);

	return 0;
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=0 noexpandtab sw=8
 */
