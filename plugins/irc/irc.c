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
#include <ekg/win32.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef NO_POSIX_SYSTEM
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#endif

#include <sys/types.h>

#ifndef NO_POSIX_SYSTEM
#include <sys/socket.h>
#endif

#include <sys/stat.h>
#define __USE_POSIX
#ifndef NO_POSIX_SYSTEM
#include <netdb.h>
#endif

#include <sys/time.h>

#ifndef NO_POSIX_SYSTEM
#include <sys/utsname.h>
#include <pwd.h>
#endif

#ifdef __sun
/* Solaris, thanks to Beeth */
#include <sys/filio.h>
#endif

#include <ekg/commands.h>
#include <ekg/debug.h>
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

#include <ekg/queries.h>

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

/* ************************ KNOWN BUGS ***********************
 *  OTHER LESS IMPORTANT BUGS
 *    -> somewhere with altnick sending
 *      G->dj: still not as I would like it to be
 *    -> 09/12/05 01:41:27 <Greyer> czemu nie dzia³a jednoczesne opowanie kilku osób ? (wczesniej zgloszone kiedystam, czekamy na rewrite irc_getchan() :)
 *  !BUGS (?) TODO->check
 *    -> buggy auto_find. if smb type smth on the channel.
 *        *  10:58:27 ::: Nieprawidowe parametry. Sprobuj help find *
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

/* 
 * IDEAS: 
 *  -> maybe some params in /connect command ?
 *     for example if we have in /session server krakow.irc.pl,poznan.irc.pl
 *     and we want connect to poznan.irc.pl (150.254.64.64) nor krakow... we type
 *     /connect poznan.irc.pl
 *     /connect #2
 *     /connect 150.254.64.64
 *     next, if we want to connect instead of default 6667 port to i.e 6665 we type
 *     /connect :6665
 *     or we can use both.
 *     /connect poznan.irc.pl:6665 || /connect 150.254.64.64 6665.
 * ---
 *  G->dj: you have to, cause del will kill me for another comment in PL ;)
 */
/*                                                                       *
 * ======================================== STARTUP AND STANDARD FUNCS - *
 *                                                                       */

static int irc_theme_init();
static WATCHER_LINE(irc_handle_resolver);
static COMMAND(irc_command_disconnect);
static int irc_really_connect(session_t *session);
static char *irc_getchan_int(session_t *s, const char *name, int checkchan);
static char *irc_getchan(session_t *s, const char **params, const char *name,
      char ***v, int pr, int checkchan);

static char *irc_config_default_access_groups;

PLUGIN_DEFINE(irc, PLUGIN_PROTOCOL, irc_theme_init);

#ifdef EKG2_WIN32_SHARED_LIB
	EKG2_WIN32_SHARED_LIB_HELPER
#endif

/*
 * irc_private_init()
 *
 * inialize irc_private_t for a given session.
 */
static void irc_private_init(session_t *s) {
	irc_private_t	*j;

	if (!s || s->priv || (s->plugin != &irc_plugin))
		return;

	userlist_free(s);
	userlist_read(s);

	j = xmalloc(sizeof(irc_private_t));
	j->fd = -1;

	/* G->dj: I've told you why this is here, not on every system NULL is 0x00000000
	 * that's why I'm just commentig this out not removing...
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
static void irc_private_destroy(session_t *s) {
	irc_private_t	*j;
	int		i;

	list_t 		tmplist;

	if (!s || !(j = s->priv) || (s->plugin != &irc_plugin))
		return;

	userlist_write(s);

	/*irc_free_people(s, j); wtf? */
	xfree(j->host_ident);
	xfree(j->nick);

	for (tmplist=j->bindlist; tmplist; tmplist=tmplist->next) {
		xfree( ((connector_t *)tmplist->data)->address);
		xfree( ((connector_t *)tmplist->data)->hostname);
	}
	list_destroy(j->bindlist, 1);

	for (tmplist=j->connlist; tmplist; tmplist=tmplist->next) {
		xfree( ((connector_t *)tmplist->data)->address);
		xfree( ((connector_t *)tmplist->data)->hostname);
	}
	list_destroy(j->connlist, 1);

	irc_free_people(s, j);

        for (i = 0; i<SERVOPTS; i++)
                xfree(j->sopt[i]);
	xfree(j);
	session_private_set(s, NULL);
}

static char *irc_make_banmask(session_t *session, const char *nick, const char *ident, const char *hostname) {
/* 
 *        1 (Nick)   - nick!*@*
 *        2 (User)   - *!*ident@*
 *        4 (Host)   - *!*@host.*
 *	  4 (IP)     - *!*@*.168.11.11 - buggy, it bans @*.11 
 *        8 (Domain) - *!*@*.domain.net
 *        8 (IP)     - *!*@192.168.11.*
 */
	char		*host = xstrdup(hostname);
	const char	*tmp[4];
	char		*temp = NULL;

	int		family = 0; 
	char		ind = '.';
	int		bantype = session_int_get(session, "ban_type");
	
#ifdef HAVE_INET_PTON
	char		buf[33];
	
	if (xstrchr(host, ':')) {
		/* to protect againt iwil var in ircd.conf (ircd-hybrid)
		 *  dot_in_ip6_addr = yes;
		 */ 
		if (host[xstrlen(host)-1] == '.') 
			host[xstrlen(host)-1] = 0;
			
		if (inet_pton(AF_INET6, host, &buf) > 0) {
			family = AF_INET6;
			ind = ':';
		}
	}
	else if (inet_pton(AF_INET, host, &buf) > 0)
		family = AF_INET;
#else
/* TODO */
	print("generic_error", "It seem you don't have inet_pton() current version of irc_make_banmask won't work without this function. If you want to get work it faster contact with developers ;>");
#endif

	if (host && !family && (temp=xstrchr(host, ind)))
		*temp = '\0';
	if (host && family && (temp=xstrrchr(host, ind)))
		*temp = '\0';

	if (bantype > 15) bantype = 10;

	memset(tmp, 0, sizeof(tmp));
#define getit(x) tmp[x]?tmp[x]:"*"
	if (bantype & 1) tmp[0] = nick;
	if (bantype & 2 && (ident[0] != '~' || session_int_get(session, "dont_ban_user_on_noident") == 0 )) tmp[1] = ident;
	if (family) {
		if (bantype & 8) tmp[2] = host;
		if (bantype & 4) tmp[3] = hostname ? temp?temp+1:NULL : NULL;
	} else {
		if (bantype & 4) tmp[2] = host;
		if (bantype & 8) tmp[3] = hostname ? temp?temp+1:NULL : NULL;
	}


/*	temp = saprintf("%s!*%s@%s%c%s", getit(0), getit(1), getit(2), ind, getit(3)); */
	temp = saprintf("%s!%s@%s%c%s", getit(0), getit(1), getit(2), ind, getit(3));
 	xfree(host);
	return temp;
#undef getit
}

/*
 * irc_session()
 *
 * adding and deleting a session
 */
static QUERY(irc_session) {
	char		*session = *(va_arg(ap, char**));
	session_t	*s = session_find(session);

	if (!s)
		return 0;

	if (data)
		irc_private_init(s);
	else
		irc_private_destroy(s);

	return 0;
}

/**
 * irc_print_version()
 *
 * handler for <i>PLUGIN_PRINT_VERSION</i> query requests<br>
 * print info about this plugin [Copyright note+version]
 *
 * @note what the heck this can be ? ;) (c GiM)
 *
 * @return 0
 */

static QUERY(irc_print_version) {
	print("generic", "IRC plugin by Michal 'GiM' Spadlinski, Jakub 'darkjames' Zawadzki v. "IRCVERSION);
	return 0;
}

static QUERY(irc_setvar_default) {
	xfree(irc_config_default_access_groups);
	irc_config_default_access_groups = xstrdup("__ison");
	return 0;
}

static int irc_resolver_sort(void *s1, void *s2) {
	connector_t *sort1 = s1; /*, *sort2 = s2;*/
	int prefer_family = AF_INET;
/*	
	if (sort1->session != sort2->session)
		return 0;
*/	
	if (session_int_get(sort1->session, "prefer_family") == AF_INET6) 
		prefer_family = AF_INET6;

/*	debug("%d && %d -> %d\n", sort1->family, sort2->family, prefer_family); */

	if (prefer_family == sort1->family)
		return 1;
	return 0;
}

static int irc_resolver2(session_t *session, char ***arr, char *hostname, int port, int dobind) {
#ifdef HAVE_GETADDRINFO
	struct  addrinfo *ai, *aitmp, hint;
	void *tm = NULL;
#else
#warning "irc: You don't have getaddrinfo() resolver may not work! (ipv6 for sure)"
	struct hostent *he4;
#endif	

/*	debug("[IRC] %s fd = %d\n", hostname, fd); */
#ifdef HAVE_GETADDRINFO
	memset(&hint, 0, sizeof(struct addrinfo));
	hint.ai_socktype=SOCK_STREAM;
	if (!getaddrinfo(hostname, NULL, &hint, &ai)) {
		for (aitmp = ai; aitmp; aitmp = aitmp->ai_next) {
			char *ip = NULL, *buf;

			if (aitmp->ai_family == AF_INET6)
				tm = &(((struct sockaddr_in6 *) aitmp->ai_addr)->sin6_addr);
			if (aitmp->ai_family == AF_INET) 
				tm = &(((struct sockaddr_in *) aitmp->ai_addr)->sin_addr);
#ifdef HAVE_INET_NTOP
			ip = xmalloc(100);
			inet_ntop(aitmp->ai_family, tm, ip, 100);
#else
			if (aitmp->ai_family == AF_INET6) {
				/* G: this doesn't have a sense since we're in child */
				/* print("generic_error", "You don't have inet_ntop() and family == AF_INET6. Please contact with developers if it happens."); */
				ip =  xstrdup("::");
			} else
				ip = xstrdup(inet_ntoa(*(struct in_addr *)tm));
#endif 
			buf = saprintf("%s %s %d %d\n", hostname, ip, aitmp->ai_family, (!dobind) ? port : 0);
			//write(fd, buf, xstrlen(buf));
			array_add(arr, buf);
//			xfree(buf);
			xfree(ip);
		}
		freeaddrinfo(ai);
	}
#else 
	if ((he4 = gethostbyname(hostname))) {
		/* copied from http://webcvs.ekg2.org/ekg2/plugins/irc/irc.c.diff?r1=1.79&r2=1.80 OLD RESOLVER VERSION...
		 * .. huh, it was 8 months ago..*/
		char *ip = xstrdup(inet_ntoa(*(struct in_addr *) he4->h_addr));
		array_add(arr, saprintf("%s %s %d %d\n", hostname, ip, AF_INET, (!dobind) ? port : 0));
	} else array_add(arr, saprintf("%s : no_host_get_addrinfo()\n", hostname));
#endif

/* G->dj: getaddrinfo was returninig 3 times, cause you haven't given hints...
 * dj->G: thx ;>
 */
	return 0;
}

/**
 * irc_validate_uid()
 *
 * handler for <i>PROTOCOL_VALIDATE_UID</i><br>
 * checks, if @a uid is <i>proper for irc plugin</i>.
 *
 * @note <i>Proper for irc plugin</i> means if @a uid starts with "irc:" and uid len > 4
 *
 * @param ap 1st param: <i>(char *) </i><b>uid</b>  - of user/session/command/whatever
 * @param ap 2nd param: <i>(int) </i><b>valid</b> - place to put 1 if uid is valid for irc plugin.
 * @param data NULL
 *
 * @return 	-1 if it's valid uid for irc plugin<br>
 * 		 0 if not
 */

static QUERY(irc_validate_uid) {
	char	*uid 	= *(va_arg(ap, char **));
	int	*valid 	= va_arg(ap, int *);

	if (!uid)
		return 0;

	if (!xstrncasecmp(uid, IRC4, 4) && uid[4]) {
		(*valid)++;
		return -1; /* if it's correct uid for irc we don't need to send to others... */
	}

	return 0;
}

/**
 * irc_protocols()
 *
 * handler for <i>GET_PLUGIN_PROTOCOLS</i><br>
 * It just add "irc:" to @a arr
 *
 * @note I know it's nowhere used. It'll be used by d-bus plugin.
 *
 * @param ap 1st param: <i>(char **) </i><b>arr</b> - array with available protocols
 * @param data NULL
 *
 * @return 0
 */

static QUERY(irc_protocols) {
	char ***arr	= va_arg(ap, char ***);

	array_add(arr, "irc:");
	return 0;
}

#ifdef NO_POSIX_SYSTEM
static void irc_changed_resolve_child(session_t *s, const char *var, HANDLE fd) {
#else
static void irc_changed_resolve_child(session_t *s, const char *var, int fd) {
#endif
	int isbind	= !xstrcmp((char *) var, "hostname");
	char *tmp	= xstrdup(session_get(s, var));

	if (tmp) {
		char *tmp1 = tmp, *tmp2;
		char **arr = NULL;

		/* G->dj: I'm changing order, because
		 * we should connect first to first specified host from list...
		 * Yeah I know code look worse ;)
		 */
		do {
			if ((tmp2 = xstrchr(tmp1, ','))) *tmp2 = '\0';
			irc_resolver2(s, &arr, tmp1, -1, isbind);
			tmp1 = tmp2+1;
		} while (tmp2);

		tmp2 = array_join(arr, NULL);
		array_free(arr);
#ifdef NO_POSIX_SYSTEM
		DWORD written;
		WriteFile(fd, tmp2, xstrlen(tmp2), &written, NULL);
		WriteFile(fd, "EOR\n", 4, &written, NULL);
#else
		write(fd, tmp2, xstrlen(tmp2));
		write(fd, "EOR\n", 4);
#endif
		sleep(3);
#ifdef NO_POSIX_SYSTEM
		CloseHandle(fd);
#else
		close(fd);
#endif
		xfree(tmp2);
	}
	xfree(tmp);
}

#ifdef NO_POSIX_SYSTEM
struct win32_tmp {	char session[100]; 
			char var[11]; 
			HANDLE fd; 
			HANDLE fd2; 
		};

static THREAD(irc_changed_resolve_child_win32) {
	struct win32_tmp *helper = data;

	CloseHandle(helper->fd);

	irc_changed_resolve_child(session_find(helper->session), helper->var, helper->fd2);
	xfree(helper);

/*	TerminateThread(GetCurrentThread(), 1); */
	return 0;
}
#endif

static void irc_changed_resolve(session_t *s, const char *var) {
	irc_private_t	*j = irc_private(s);
	int		isbind;
	int		res, fd[2];
	list_t		*rlist = NULL;
	if (!j)
		return;

	if (pipe(fd) == -1) {
		print("generic_error", strerror(errno));
		return;
	}

	isbind = !xstrcmp((char *) var, "hostname");
	/* G->dj: THIS WAS VERY, VERY NASTY BUG... 
	 *        SUCH A LITTLE ONE, AND SO TIME-CONSUMING ;/
	 */
	if (isbind) { rlist = &(j->bindlist); j->bindtmplist = NULL; }
	else { rlist = &(j->connlist); j->conntmplist = NULL; }

	if (*rlist) {
		list_t tmplist;
		for (tmplist=*rlist; tmplist; tmplist=tmplist->next) {
			xfree( ((connector_t *)tmplist->data)->address);
			xfree( ((connector_t *)tmplist->data)->hostname);
		}
		list_destroy(*rlist, 1);
		*rlist = NULL;
	}
#ifdef NO_POSIX_SYSTEM
	struct win32_tmp *helper = xmalloc(sizeof(struct win32_tmp));

	strncpy((char *) &helper->session, s->uid, sizeof(helper->session)-1);
	strncpy((char *) &helper->var, var, sizeof(helper->var)-1);

	DuplicateHandle(GetCurrentProcess(), (HANDLE) fd[1], GetCurrentProcess(), &(helper->fd2), DUPLICATE_SAME_ACCESS, TRUE, DUPLICATE_SAME_ACCESS);
	DuplicateHandle(GetCurrentProcess(), (HANDLE) fd[0], GetCurrentProcess(), &(helper->fd), DUPLICATE_SAME_ACCESS, TRUE, DUPLICATE_SAME_ACCESS);
	debug("[fds] after dupliaction: [0, %d] [1, %d]\n", helper->fd, helper->fd2);

	if ((res = (int) win32_fork(irc_changed_resolve_child_win32, helper)) == 0)
#else
	if ((res = fork()) < 0)
#endif
	{
		print("generic_error", strerror(errno));
		close(fd[0]);
		close(fd[1]);
		return;
	}
	j->resolving++;
	if (res) {
		irc_resolver_t *irdata = xmalloc(sizeof(irc_resolver_t));
		irdata->session = xstrdup(s->uid);
		irdata->plist   = rlist;

#ifndef NO_POSIX_SYSTEM
		close(fd[1]);
#else
		CloseHandle(fd[1]);
#endif
		watch_add_line(&irc_plugin, fd[0], WATCH_READ_LINE, irc_handle_resolver, irdata);

		return;
	} 
	/* Child */
#ifndef NO_POSIX_SYSTEM
	close(fd[0]);
	irc_changed_resolve_child(s, var, fd[1]);
	exit(0);
#endif
	return;
 }

/*                                                                       *
 * ======================================== HANDLERS ------------------- *
 *                                                                       */

void irc_handle_disconnect(session_t *s, const char *reason, int type)
{
/* 
 * EKG_DISCONNECT_NETWORK @ irc_handle_stream type == 1
 * EKG_DISCONNECT_FAILURE @ irc_handle_connect when we got timeouted or connection refused or other..
 * EKG_DISCONNECT_FAILURE @ irc_c_error(misc.c) when we recv ERROR message @ connecting
 * EKG_DISCONNECT_FAILURE @ irc_command_connect when smth goes wrong.
 * EKG_DISCONNECT_STOPPED @ irc_command_disconnect when we do /disconnect when we are not connected and we try to connect.
 * EKG_DISCONNECT_USER    @ irc_command_disconnect when we do /disconnect when we are connected.
 */
	irc_private_t	*j = irc_private(s);
        char		*__session, *__reason;
        int		__type = type;

	if (!j) {
		debug("[irc_ierror] @irc_handle_disconnect j == NULL");
		return;
	}

	session_connected_set(s, 0);
	j->connecting = 0;

	// !!! 
	if (j->send_watch) {
		j->send_watch->type = WATCH_NONE;
		watch_free(j->send_watch);	
		j->send_watch = NULL;
	}
	if (j->recv_watch) {
		watch_free(j->recv_watch);
		j->recv_watch = NULL;
	}
	watch_remove(&irc_plugin, j->fd, WATCH_WRITE);

	close(j->fd);
	j->fd = -1;
	irc_free_people(s, j);

	debug ("%d [%d:%d:%d,%d]\n", type, EKG_DISCONNECT_FAILURE, EKG_DISCONNECT_STOPPED, EKG_DISCONNECT_NETWORK, EKG_DISCONNECT_USER);
	switch (type) {
		case EKG_DISCONNECT_FAILURE:
			break;
		case EKG_DISCONNECT_STOPPED:
			/* this is just in case somebody would add some servers that have given
			 * port disabled so answer comes very fast and plug would think
			 * he is 'disconnected' while in fact he would try many times to
			 * reconnect to given host
			 */
			j->autoreconnecting = 0;
			/* if ,,reconnect'' timer exists we should stop doing */
			if (timer_remove(&irc_plugin, "reconnect") == 0)
				print("auto_reconnect_removed", session_name(s)); 
			break;
			/*
			 * default:
			 * 	debug("[irc_handle_disconnect] unknow || !handled type = %d %s", type, reason);
			 */
	}
	__reason  = xstrdup(format_find(reason));
	__session = xstrdup(session_uid_get(s));
	
	if (!xstrcmp(__reason, "")) {
		xfree(__reason);
		__reason = xstrdup(reason);
	}
			
	query_emit_id(NULL, PROTOCOL_DISCONNECTED, &__session, &__reason, &__type, NULL);
	xfree(__reason);
	xfree(__session);
}

static WATCHER_LINE(irc_handle_resolver) {
	irc_resolver_t *resolv = (irc_resolver_t *) data;
	session_t *s = session_find(resolv->session);
	irc_private_t *j;
	char **p;

	if (!s || !(j = irc_private(s)) ) return -1;

	if (type) {
		debug("[irc] handle_resolver for session %s type = 1 !! 0x%x resolving = %d connecting = %d\n", resolv->session, resolv->plist, j->resolving, j->connecting);
		xfree(resolv->session);
		xfree(resolv);
		if (j->resolving > 0)
			j->resolving--;
		if (j->resolving == 0 && j->connecting == 2) {
			debug("[irc] hadnle_resolver calling really_connect\n");
			irc_really_connect(s);
		}
		return -1;
	}
/* 
 * %s %s %d %d hostname ip family port\n
 */
	if (!xstrcmp(watch, "EOR")) return -1;	/* koniec resolvowania */
	if ((p = array_make(watch, " ", 4, 1, 0)) && p[0] && p[1] && p[2] && p[3]) {
		connector_t *listelem = xmalloc(sizeof(connector_t));
    		listelem->session = s;
		listelem->hostname = xstrdup(p[0]);
		listelem->address  = xstrdup(p[1]);
		listelem->port     = atoi(p[3]);
		listelem->family   = atoi(p[2]);
		list_add_sorted((resolv->plist), listelem, 0, &irc_resolver_sort);
		debug("%s (%s %s) %x %x\n", p[0], p[1], p[3], resolv->plist, listelem); 
		array_free(p);
	} else { 
		debug("[irc] received some kind of junk from resolver thread: %s\n", watch);
		array_free(p);
		return -1;
	}
	return 0;
}

static WATCHER_LINE(irc_handle_stream) {
	session_t *s = session_find(data);
	irc_private_t *j = irc_private(s);

	/* ups, we get disconnected */
	if (type == 1) {
		if (s) j->recv_watch = NULL;
		/* this will cause  'Removed more than one watch...' */
		debug ("[irc] handle_stream(): ROZ£¡CZY£O %d %d\n", session_connected_get(s), j->connecting);
		
		/* avoid reconnecting when we do /disconnect */
		if (s && (session_connected_get(s) || j->connecting))
			irc_handle_disconnect(s, NULL, EKG_DISCONNECT_NETWORK);
		xfree(data);
		return 0;
	}

	if (!s) { 
		debug("The worst happen you've deleted Our Session (%s) ;(\n", data); 
		return -1; /* watch_remove(&irc_plugin, fd, WATCH_READ); * /plugin -irc makes it but when we delete only that specific session ? irc:test * */ 
	}


	/* this shouldn't be like that, it would be better to change
	 * query_connect, so the handler should get char not
	 * const char, so the queries could modify this param,
	 * I'm not sure if this is good idea, just thinking...
	 */
	irc_parse_line(s, (char *)watch, fd);
	return 0;
}

static WATCHER(irc_handle_connect) { /* tymczasowy */
	session_t		*s = session_find(data);
	irc_private_t		*j = irc_private(s);
	const char		*real = NULL, *localhostname = NULL;
	char			*pass = NULL;
	int			res = 0; 
	socklen_t		res_size = sizeof(res);

	if (type == 1) {
		debug ("[irc] handle_connect(): type %d\n", type);
		xfree(data);
		return 0;
	}

	if (!s) { 
		debug("[irc] handle_connect(): session %s deleted. :(\n", data);  
		return -1; /* watch_remove(&irc_plugin, fd, WATCH_WRITE); */
	}

	debug ("[irc] handle_connect()\n");

	if (type || getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
		if (type) debug("[irc] handle_connect(): SO_ERROR %s\n", strerror(res));

		/* try next server. */
		/* 'if' because someone can make: /session server blah and /reconnect
		 * during already began process of connecting
		 */
		if (j->conntmplist) {
			if (!type) DOT("IRC_TEST_FAIL", "Connect", ((connector_t *) j->conntmplist->data), s, res); 
			j->conntmplist = j->conntmplist->next;
		}
		irc_handle_disconnect(s, (type == 2) ? "Connection timed out" : strerror(res), EKG_DISCONNECT_FAILURE);
		return -1; /* ? */
	}

	timer_remove(&irc_plugin, "reconnect");
	DOT("IRC_CONN_ESTAB", NULL, ((connector_t *) j->conntmplist->data), s, 0);

	j->recv_watch = watch_add_line(&irc_plugin, fd, WATCH_READ_LINE, irc_handle_stream, xstrdup((char *) data));
	j->send_watch = watch_add_line(&irc_plugin, fd, WATCH_WRITE_LINE, NULL, NULL);

	real = session_get(s, "realname");
	real = real ? xstrlen(real) ? real : j->nick : j->nick;
	if (j->bindtmplist)	
		localhostname = ((connector_t *) j->bindtmplist->data)->hostname;
	if (!xstrlen(localhostname))
		localhostname = NULL;
	pass = (char *)session_password_get(s);
	pass = xstrlen(strip_spaces(pass))?
		saprintf("PASS %s\r\n", strip_spaces(pass)) : xstrdup("");
	watch_write(j->send_watch, "%sUSER %s %s unused_field :%s\r\nNICK %s\r\n",
			pass, j->nick, localhostname?localhostname:"12", real, j->nick);
	xfree(pass);
	return -1;
}

/*                                                                       *
 * ======================================== COMMANDS ------------------- *
 *                                                                       */
#if 0
static void resolver_child_handler(child_t *c, int pid, const char *name, int status, void *priv) {
	debug("(%s) resolver [%d] exited with %d\n", name, pid, status);
}
#endif

static int irc_build_sin(session_t *s, connector_t *co, struct sockaddr **address) {
	struct sockaddr_in  *ipv4;
	struct sockaddr_in6 *ipv6;
	int len = 0;
	int defport = session_int_get(s, "port");
	int port;

	*address = NULL;

	if (!co) 
		return 0;
	port = co->port < 0 ? defport : co->port;
	if (port < 0) port = DEFPORT;

	if (co->family == AF_INET) {
		len = sizeof(struct sockaddr_in);

		ipv4 = xmalloc(len);

		ipv4->sin_family = AF_INET;
		ipv4->sin_port   = htons(port);
#ifdef HAVE_INET_PTON
		inet_pton(AF_INET, co->address, &(ipv4->sin_addr));
#else
#warning "irc: You don't have inet_pton() connecting to ipv4 hosts may not work"
#ifdef HAVE_INET_ATON /* XXX */
		if (!inet_aton(co->address, &(ipv4->sin_addr))) {
			debug("inet_aton() failed on addr: %s.\n", co->address);
		}
#else
#warning "irc: You don't have inet_aton() connecting to ipv4 hosts may not work"
#endif
#warning "irc: Yeah, You have inet_addr() connecting to ipv4 hosts may work :)"
		if ((ipv4->sin_addr.s_addr = inet_addr(co->address)) == -1) {
			debug("inet_addr() failed or returns 255.255.255.255? on %s\n", co->address);
		}
#endif

		*address = (struct sockaddr *) ipv4;
	} else if (co->family == AF_INET6) {
		len = sizeof(struct sockaddr_in6);

		ipv6 = xmalloc(len);
		ipv6->sin6_family  = AF_INET6;
		ipv6->sin6_port    = htons(port);
#ifdef HAVE_INET_PTON
		inet_pton(AF_INET6, co->address, &(ipv6->sin6_addr));
#else
#warning "irc: You don't have inet_pton() connecting to ipv6 hosts may not work "
#endif

		*address = (struct sockaddr *) ipv6;
	}
	return len;
}

static int irc_really_connect(session_t *session) {
	irc_private_t		*j = irc_private(session);
	connector_t		*connco, *connvh = NULL;
	struct sockaddr		*sinco,  *sinvh  = NULL;
	int			one = 1, connret = -1, bindret = -1, err;
	int			sinlen, fd;
	int 			timeout;
	watch_t			*w;

	if (!j->conntmplist) j->conntmplist = j->connlist;
	if (!j->bindtmplist) j->bindtmplist = j->bindlist;

	if (!j->conntmplist) {
		print("generic_error", "Ziomu¶ twój resolver co¶ nie tegesuje (!j->conntmplist)");
 		return -1;
 	}

	j->autoreconnecting = 1;
	connco = (connector_t *)(j->conntmplist->data);
	sinlen = irc_build_sin(session, connco, &sinco);
	if (!sinco) {
		print("generic_error", "Ziomu¶ twój resolver co¶ nie tegesuje (!sinco)"); 
		return -1;
	}

	if ((fd = socket(connco->family, SOCK_STREAM, 0)) == -1) {
		err = errno;
		debug("[irc] handle_resolver() socket() failed: %s\n",
				strerror(err));
		irc_handle_disconnect(session, strerror(err), EKG_DISCONNECT_FAILURE);
		goto irc_conn_error;
	}
	j->fd = fd;
	debug("[irc] socket() = %d\n", fd);

	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
	if (ioctl(fd, FIONBIO, &one) == -1) {
		err = errno;
		debug("[irc] handle_resolver() ioctl() failed: %s\n",
				strerror(err));
		irc_handle_disconnect(session, strerror(err), EKG_DISCONNECT_FAILURE);
		goto irc_conn_error;
	}

	/* loop, optimize... */
	if (j->bindtmplist) 
		connvh = j->bindtmplist->data;	
	irc_build_sin(session, connvh, &sinvh);
	while (bindret && connvh) {
		DOT("IRC_TEST", "Bind", connvh, session, 0);
		if (connvh->family == connco->family)  {
			bindret = bind(fd, sinvh, sinlen);
			if (bindret == -1)
				DOT("IRC_TEST_FAIL", "Bind", connvh, session, errno);
 		}

		if (bindret && j->bindtmplist->next) {
			xfree(sinvh);
			j->bindtmplist = j->bindtmplist->next;
			connvh = j->bindtmplist->data;
			irc_build_sin(session, connvh, &sinvh);
			if (!sinvh)
				continue;
		} else
			break;
	}
	j->connecting = 1;
	DOT("IRC_TEST", "Connecting", connco, session, 0);
	connret = connect(fd, sinco, sinlen);
	debug("connecting: %s %s\n", connco->hostname, connco->address);

	xfree(sinco);
	xfree(sinvh);

	if (connret &&
#ifdef NO_POSIX_SYSTEM
		(WSAGetLastError() != WSAEWOULDBLOCK)
#else
		(errno != EINPROGRESS)
#endif
		) {
		debug("[irc] really_connect control point 1\n");
		err = errno;
		DOT("IRC_TEST_FAIL", "Connect", connco, session, err);
		j->conntmplist = j->connlist->next;
		irc_handle_disconnect(session, strerror(err), EKG_DISCONNECT_FAILURE);
		return -1;

	}
	if (session_status_get(session) == EKG_STATUS_NA)
		session_status_set(session, EKG_STATUS_AVAIL);

	w = watch_add(&irc_plugin, fd, WATCH_WRITE, irc_handle_connect, xstrdup(session->uid) );
	if ((timeout = session_int_get(session, "connect_timeout") > 0)) 
		watch_timeout_set(w, timeout);
	
 	return 0;
irc_conn_error: 
	xfree(sinco);
	xfree(sinvh);
	return -1;

}

static COMMAND(irc_command_connect) {
	irc_private_t		*j = irc_private(session);
	const char		*newnick;

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
	if (j->resolving) {
		printq("generic", "resolving in progress... you will be connected as soon as possible");
		j->connecting = 2;
		return -1;
	}
	return irc_really_connect(session);
}

static COMMAND(irc_command_disconnect) {
	irc_private_t	*j = irc_private(session);
	const char	*reason = params[0]?params[0]:QUITMSG(session);
	debug("[irc] comm_disconnect() !!!\n");

	if (!j->connecting && !session_connected_get(session) && !j->autoreconnecting) {
		printq("not_connected", session_name(session));
		return -1;
	}

	if (reason && session_connected_get(session))
		watch_write(j->send_watch, "QUIT :%s\r\n", reason);

	if (j->connecting || j->autoreconnecting)
		irc_handle_disconnect(session, reason, EKG_DISCONNECT_STOPPED);
	else
		irc_handle_disconnect(session, reason, EKG_DISCONNECT_USER);

	return 0;
}

static COMMAND(irc_command_reconnect) {
	irc_private_t	*j = irc_private(session);

	if (j->connecting || session_connected_get(session)) 
		irc_command_disconnect(name, params, session, target, quiet);
	return irc_command_connect(name, params, session, target, quiet);
}

/*****************************************************************************/

static COMMAND(irc_command_msg) {
	irc_private_t	*j = irc_private(session);
	people_chan_t	*perchn = NULL;
	people_t	*person;
        window_t	*w;

	int		prv = 0; /* NOTICE | PRIVMSG */
	
	char		*mline[2];
	char		*tmpbuf;
	
	int		ischn;  /* IS CHANNEL ? */
	char            prefix[2] = {' ', '\0'};
	char		*__msg = NULL;
	char		*head, *coloured;
	const char	*frname; /* formatname */

        int		class = EKG_MSGCLASS_SENT | EKG_NO_THEMEBIT;
	int		ekgbeep = EKG_NO_BEEP;
	const time_t	sent = time(NULL);
	char		*format=NULL, *seq=NULL;
	int		secure = 0;
	
	char		**rcpts;
	const char	*uid=NULL;
	char		*sid = NULL, *uid_full = NULL;

	uid = target;
	w = window_find_s(session, uid);

	prv = xstrcmp(name, ("notice"));
	ischn = !!xstrchr(SOP(_005_CHANTYPES), uid[4]);
/* PREFIX */
	if ((ischn && (person = irc_find_person(j->people, j->nick)) && (perchn = irc_find_person_chan(person->channels, (char *)uid))))
		prefix[0] = *(perchn->sign);

	if (!ischn || (!session_int_get(session, "SHOW_NICKMODE_EMPTY") && *prefix==' '))
		*prefix='\0';

	frname = format_find(prv?
				ischn?"irc_msg_sent_chan":w?"irc_msg_sent_n":"irc_msg_sent":
				ischn?"irc_not_sent_chan":w?"irc_not_sent_n":"irc_not_sent");

	sid 	 = xstrdup(session_uid_get(session));
	if (!xstrncasecmp(uid, IRC4, 4)) {
		uid_full = xstrdup(uid);
	} else {
		uid_full = saprintf("%s:%s", IRC3, uid);
	}
	rcpts    = xmalloc(sizeof(char *) * 2);
	rcpts[0] = xstrdup(!!w?w->target:uid);
	rcpts[1] = NULL;

	debug ("%s - %s\n", uid_full, rcpts[0]);

	tmpbuf   = (mline[0] = xstrdup(params[1]));
	while ((mline[1] = split_line(&(mline[0])))) {
		char *__mtmp;
		int isour = 1;
		int xosd_to_us = 0;
		int xosd_is_priv = !ischn;
		
		__msg = xstrdup((const char *)mline[1]);

		head = format_string(frname, session_name(session), prefix,
				j->nick, j->nick, uid_full+4, __msg);

		coloured = irc_ircoldcolstr_to_ekgcolstr(session, head, 1);

		query_emit_id(NULL, IRC_PROTOCOL_MESSAGE, &(sid), &(j->nick), &__msg, &isour, &xosd_to_us, &xosd_is_priv, &uid_full);

		query_emit_id(NULL, MESSAGE_ENCRYPT, &sid, &uid_full, &__msg, &secure);
				
		query_emit_id(NULL, PROTOCOL_MESSAGE, &sid, &sid, &rcpts, &coloured, &format, &sent, &class, &seq, &ekgbeep, &secure);

		/* "Thus, there are 510 characters maximum allowed for the command and its parameters." [rfc2812]
		 * yes, I know it's a nasty variable reusing ;)
		 */
		__mtmp = __msg;
		debug ("%s ! %s\n", j->nick, j->host_ident);
		xosd_is_priv = xstrlen(__msg);
		isour = 510 - (prv?7:6) - 6 - xstrlen(uid_full+4) - xstrlen(j->host_ident) - xstrlen(j->nick);
		/* 6 = 3xspace + '!' + 2xsemicolon; -> [:nick!ident@hostident PRIVMSG dest :mesg] */
		while (xstrlen(__mtmp) > isour && __mtmp < __msg + xosd_is_priv)
		{
			xosd_to_us = __mtmp[isour];
			__mtmp[isour] = '\0';
			watch_write(j->send_watch, "%s %s :%s\r\n", (prv) ? "PRIVMSG" : "NOTICE", uid_full+4, __mtmp);
			__mtmp[isour] = xosd_to_us;
			__mtmp += isour;
		}
		watch_write(j->send_watch, "%s %s :%s\r\n", (prv) ? "PRIVMSG" : "NOTICE", uid_full+4, __mtmp);

		xfree(__msg);
		xfree(coloured);
		xfree(head);
	}

	xfree(rcpts[0]);
	xfree(rcpts);
	
	xfree(sid);
	xfree(uid_full);
	xfree(tmpbuf);

	session_unidle(session);
	return 0;
}

static COMMAND(irc_command_inline_msg) {
	const char	*p[2] = { NULL, params[0] };
	if (!target || !params[0])
		return -1;
	return irc_command_msg(("msg"), p, session, target, quiet);
}

static COMMAND(irc_command_parse) {
	string_t temp = string_init(NULL);
	const char *t = params[0];

	if (!irc_private(session)->recv_watch) {
		debug_error("NO RECV WATCH!\n");
		return -1;
	}

	while (*t) {
		if (*t == '\\' && t[1]) {
			t++;
			if (*t == 'n') string_append_c(temp, '\n');
			else if (*t == 'r') string_append_c(temp, '\r');
			else {
				string_append_c(temp, '\\');
				string_append_c(temp, *t);
			}

		} else string_append_c(temp, *t);
		t++;
	}

	debug_error("ff() %s\n", temp->str);

	string_insert(irc_private(session)->recv_watch->buf, 0, temp->str);
	watch_handle_line(irc_private(session)->recv_watch);

	string_free(temp, 1);

	return 0;
}

static COMMAND(irc_command_quote) {
	watch_write(irc_private(session)->send_watch, "%s\r\n", params[0]);
	return 0;
}

static COMMAND(irc_command_pipl) {
	irc_private_t	*j = irc_private(session);
	list_t		t1, t2;
	people_t	*per;
	people_chan_t	*chan;

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

static COMMAND(irc_command_alist) {
	irc_private_t	*j = irc_private(session);
	int isshow = 0;

#if 0
	for (l = j->people; l; l = l->next) {
		people_t	*per = l->data;

		printq("irc_access_known", session_name(session), per->nick+4, per->ident, per->host);
	}
#endif

	if (!params[0] || match_arg(params[0], 'l', "list", 2) || (isshow = match_arg(params[0], 's', "show", 2))) {	/* list, list, show */
		list_t l;

		for (l = session->userlist; l; l = l->next) {
			userlist_t *u = l->data;
			/* u->resources; */
		}

		return 0;
	}

	if (match_arg(params[0], 'a', "add", 2)) {
	/* params[1] - nick!ident@hostname or nickname (in form irc:nick) 
	 * params[2] - options + channels like: +ison +autoop:#linux,#linux.pl +autounban:* +autovoice:* 
	 * ZZZ params[2] - channels: #chan1,#chan2 or '*' 
	 * ZZZ params[3] - options.
	 */

/* /irc:access -a irc:nickname #chan1,#chan2 +autoop +autounban +revenge +ison */

		char *mask = NULL;
		const char *channel;
		char *groupstr;

		userlist_t *u;

		if (!params[1] || !(channel = params[2])) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!xstrncmp(params[1], "irc:", 4)) {	/* nickname */
			list_t l;
			for (l = j->people; l; l = l->next) {
				people_t *per = l->data;

				if (!xstrcmp(per->nick+4, params[1]+4)) {
					/* XXX, here generate mask */
					mask = saprintf("%s!%s@%s", per->nick+4, per->ident, per->host);
					break;
				}
			}
			
			if (!mask) {
				printq("user_not_found", params[1]);
				return -1;
			}
		} else {
			/* XXX verify if mask in params[1] is ok */
			mask = xstrdup(params[1]);
		}



		{
			char *tmp = saprintf("irc:%s:%s", mask, channel);
			u = userlist_add(session, tmp, params[1]);
			if (params[3]) {
				char **arr = array_make(params[3], " ", 0, 1, 1);
				int i;

				for (i=0; arr[i]; i++) {
					const char *value = arr[i];

					if (arr[i][0] == '+')
						value++;

					if (!xstrcmp(value, "autoop")) 		ekg_group_add(u, "__autoop");		/* +o */
					else if (!xstrcmp(value, "autovoice"))	ekg_group_add(u, "__autovoice");	/* +v */
					else if (!xstrcmp(value, "autounban"))	ekg_group_add(u, "__autounban");	/* -b */

					else if (!xstrcmp(value, "autoban"))	ekg_group_add(u, "__autoban");		/* +b */
					else if (!xstrcmp(value, "autodevop"))	ekg_group_add(u, "__autodevop");	/* -o, -h, -v */
					else if (!xstrcmp(value, "revenge"))	ekg_group_add(u, "__revenge");		/* + */

					else if (!xstrcmp(value, "ison"))	ekg_group_add(u, "__ison");		/* + */
					else printq("irc_access_invalid_flag", value);
				}
				array_free(arr);
			} else u->groups = group_init(irc_config_default_access_groups);
			xfree(tmp);
		}

		groupstr = group_to_string(u->groups, 1, 1);
		printq("irc_access_added", session_name(session), "0" /* XXX # */, mask, channel, groupstr);
		xfree(groupstr);

		xfree(mask);
	
		/* XXX sync if wanted */
		return 0;
	}

	if (match_arg(params[0], 'd', "delete", 2)) {
		printq("generic_error", "stub function use /del");
		return -1;
	}

	if (match_arg(params[0], 'e', "edit", 2)) {
		printq("generic_error", "stub function");
		return -1;
	}

	if (match_arg(params[0], 'S', "sync", 2)) {
		printq("generic_error", "stub function");
		return -1;
	}

	printq("invalid_params", name);
	return -1;
}

static COMMAND(irc_command_add) {
	printq("generic_error", "/irc:add do nothing. if you want friendlists use /irc:access");
/* XXX, wrapper do /irc:access --add $UID +ison ? */
	return -1;
}

static COMMAND(irc_command_away) {
	irc_private_t	*j = irc_private(session);
	int 		isaway = 0;

	if (!xstrcmp(name, ("back"))) {
		session_descr_set(session, NULL);
		session_status_set(session, EKG_STATUS_AVAIL);
		session_unidle(session);
	} else if (!xstrcmp(name, ("away"))) {
		session_descr_set(session, params[0]);
		session_status_set(session, EKG_STATUS_AWAY);
		session_unidle(session);
		isaway = 1;
	} else if (!xstrcmp(name, ("_autoaway"))) {
		session_status_set(session, EKG_STATUS_AUTOAWAY);
		isaway = 1;
	} else if (!xstrcmp(name, ("_autoback"))) {
		session_status_set(session, EKG_STATUS_AUTOBACK);
		session_unidle(session);
	} else {
		printq("generic_error", "Ale o so chozi?");
		return -1;
	}
	if (isaway) {
		const char *status = ekg_status_string(session_status_get(session), 0);
		const char *descr  = session_descr_get(session);
		if (descr)
			watch_write(j->send_watch, "AWAY :%s\r\n", descr);
		else
			watch_write(j->send_watch, "AWAY :%s\r\n", status);
	} else {
		watch_write(j->send_watch, "AWAY :\r\n");
	}
	return 0;
}

/*****************************************************************************/

/**
 * irc_window_kill()
 *
 * handler for <i>UI_WINDOW_KILL</i> query requests
 * It checks if window which will be destroyed is valid irc channel window, and we
 * are on it now. if yes (and of course if we are connected) it send PART message to irc
 * server, with reason got using @a PARTMSG macro
 *
 * @param ap 1st param: <i>(window_t *) </i><b>w</b> - window which will be destroyed 
 * @param data NULL
 *
 * @return 0
 */

static QUERY(irc_window_kill) {
	window_t	*w = *va_arg(ap, window_t **);
	irc_private_t	*j;
	char		*tmp;

	if (w && w->id && w->target && w->session && w->session->plugin == &irc_plugin &&
			(j = irc_private(w->session)) &&
			(tmp = SOP(_005_CHANTYPES)) &&
			xstrchr(tmp, (w->target)[4]) &&
			irc_find_channel((j->channels), (w->target)) &&
			session_connected_get(w->session)
			)
	{
		watch_write(j->send_watch, "PART %s :%s\r\n", (w->target)+4, PARTMSG(w->session, NULL));
	}
	return 0;
}

/** 
 * irc_topic_header() 
 *
 * handler for <i>IRC_TOPIC</i> query requests
 * @param ap 1st param: <i>(char *) </i><b>top</b> - place to put:<br>
 * 	-> <i>topic of current irc channel</i> (if current window is valid irc channel)<br>
 * 	-> <i>ident\@host of current irc user</i> (if current window is known user)
 * @param ap 2nd param: <i>(char *) </i><b>setby</b> - place to put:<br>
 * 	-> <i>topic owner of current irc channel</i><br>
 * 	-> <i>realname of current irc user</i>
 * @param ap 3rd param: <i>(char *) </i><b>modes</b> - place to put:<br>
 * 	<i>modes of current irc channel</i> or <i>undefined if not channel</i>
 * @param data 0 
 *
 * @return 	 1 if it's known irc channel.<br>
 * 		 2 if it's known irc user.<br>
 * 		 0 if it's neither known user, nor channel.<br>
 *		-3 if it's not valid irc window, or session is not connected
 */

static QUERY(irc_topic_header) {
	char		**top   = va_arg(ap, char **);
	char		**setby = va_arg(ap, char **);
	char		**modes = va_arg(ap, char **);

	char		*targ	= window_current->target;
	channel_t	*chanp	= NULL;
	people_t 	*per  	= NULL;

	irc_private_t	*j	= irc_private(window_current->session);
	char		*tmp	= NULL;

	*top = *setby = *modes = NULL;
	if (targ && window_current->session && window_current->session->plugin == &irc_plugin && session_connected_get(window_current->session) )
	{ 
		/* channel */
		if ((tmp = SOP(_005_CHANTYPES)) && 
		     xstrchr(tmp, targ[4]) && 
		     (chanp = irc_find_channel((j->channels), targ))) {
			*top   = xstrdup(chanp->topic);
			*setby = xstrdup(chanp->topicby);
			*modes = xstrdup(chanp->mode_str);
			return 1;
		/* person */
		} else if ((per = irc_find_person((j->people), targ))) { 
			*top   = saprintf("%s@%s", per->ident, per->host);
			*setby = xstrdup(per->realname);
			return 2;
		} else return 0;
	}
	return -3;
}

static char *irc_getchan_int(session_t *s, const char *name, int checkchan)
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
static char *irc_getchan(session_t *s, const char **params, const char *name,
		char ***v, int pr, int checkchan)
{
	char		*chan, *tmpname;
	const char	*tf, *ts, *tp; /* first, second */
	int		i = 0, parnum = 0, argnum = 0, hasq = 0;
	list_t		l;

	if (params) tf = params[0];
	else tf = NULL;
	ts = window_current->target;

	if (pr) { tp=tf; tf=ts; ts=tp; }

	if (!(chan = irc_getchan_int(s, tf, checkchan))) {
		if (!(chan = irc_getchan_int(s, ts, checkchan)))
		{
			print("invalid_params", name);
			return 0;
		}
		pr = !!pr;
	} else {
		pr = !!!pr;
	}

	tmpname = saprintf("irc:%s", name);
	for (l = commands; l; l = l->next) {
		command_t *c = l->data;

		if (&irc_plugin == c->plugin && !xstrcmp(tmpname, c->name))
		{
			while (c->params[parnum])
			{
				if (!hasq && !xstrcmp(c->params[parnum], ("?")))
					hasq = 1;
				parnum++;
			}
			break;
		}
	}
	xfree(tmpname);

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
/*****************************************************************************/

static COMMAND(irc_command_names) {
	irc_private_t	*j = irc_private(session);
	channel_t       *chan;
	userlist_t      *ulist;
        list_t          l;
	string_t	buf;
	int		sort_status[5] = {EKG_STATUS_AVAIL, EKG_STATUS_AWAY, EKG_STATUS_XA, EKG_STATUS_INVISIBLE, 0};
	int             lvl_total[5]    = {0, 0, 0, 0, 0};
	int             lvl, count = 0;
	char            *sort_modes     = xstrchr(SOP(_005_PREFIX), ')')+1;

	int		smlen = xstrlen(sort_modes)+1, nplen = (SOP(_005_NICKLEN)?atoi(SOP(_005_NICKLEN)):0) + 1;
	char            mode[2], **mp, *channame, nickpad[nplen];

	if (!(channame = irc_getchan(session, params, name, &mp, 0, IRC_GC_CHAN))) 
		return -1;

	if (!(chan = irc_find_channel(j->channels, channame))) {
		printq("generic", "irc_command_names: wtf?");
		return -1;
	}
/* jaki jest cel tego ze to bylo robione 2 razy ? */
	mode[1] = '\0';

	for (lvl =0; lvl<nplen; lvl++)
		nickpad[lvl] = 160;
	nickpad[lvl] = '\0';

	print_window(channame, session, 0, "IRC_NAMES_NAME", session_name(session), channame+4);
	buf = string_init(NULL);

	for (lvl = 0; lvl<smlen; ++lvl, ++sort_modes)
	{
		mode[0] = (*sort_modes)?(*sort_modes):160; /* ivil hack*/
		for (l = chan->window->userlist; l; l = l->next)
		{
			char *tmp;
			ulist = (userlist_t *)l->data;
			if (!ulist || (ulist->status != sort_status[lvl]))
				continue;
			++lvl_total[lvl];

			nickpad[nplen -1 -xstrlen((ulist->uid + 4))] = '\0';
			string_append(buf, (tmp = format_string(format_find("IRC_NAMES"), mode, (ulist->uid + 4), nickpad))); xfree(tmp);
			nickpad[nplen -1 -xstrlen((ulist->uid + 4))] = 160;
			++count;
		}
		debug("---separator---\n");
	}

	if (count)
		printq("none", buf->str);

	printq("none2", "");
#define plvl(x) lvl_total[x] ? itoa(lvl_total[x]) : "0"
	if (smlen > 3) /* has halfops */
		print_window(channame, session, 0, "IRC_NAMES_TOTAL_H", session_name(session), channame+4, itoa(count), plvl(0), plvl(1), plvl(2), plvl(3), plvl(4));
	else
		print_window(channame, session, 0, "IRC_NAMES_TOTAL", session_name(session), channame+4, itoa(count), plvl(0), plvl(1), plvl(2));
	debug("[IRC_NAMES] levelcounts = %d %d %d %d %d\n",
			lvl_total[0], lvl_total[1], lvl_total[2], lvl_total[3], lvl_total[4], lvl_total[5]);
#undef plvl

	array_free(mp);
	string_free (buf, 1);
	xfree (channame);
	return 0;
}

static COMMAND(irc_command_topic) {
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

	watch_write(j->send_watch, newtop);
	array_free(mp);
	xfree (newtop);
	xfree (chan);
	return 0;
}

static COMMAND(irc_command_who) {
	irc_private_t	*j = irc_private(session);
	char		**mp, *chan;

	if (!(chan=irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN)))
		return -1;

	watch_write(j->send_watch, "WHO %s\r\n", chan+4);

	array_free(mp);
	xfree(chan);
	return 0;
}

static COMMAND(irc_command_invite) {
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
	watch_write(j->send_watch, "INVITE %s %s\r\n", *mp, chan+4);

	array_free(mp);
	xfree(chan);
	return 0;
}

static COMMAND(irc_command_kick) {
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
	watch_write(j->send_watch, "KICK %s %s :%s\r\n", chan+4, *mp, KICKMSG(session, mp[1]));

	array_free(mp);
	xfree(chan);
	return 0;
}

static COMMAND(irc_command_unban) {
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
	} else {
		if ( (banid = atoi(*mp)) ) {
			chan = irc_find_channel(j->channels, channame+4);
			if (chan && (banlist = (chan->banlist)) ) {
				for (i=1; banlist && i<banid; banlist = banlist->next, ++i);
				if (banlist) /* fit or add  i<=banid) ? */
					watch_write(j->send_watch, "MODE %s -b %s\r\n", channame+4, banlist->data);
				else 
					debug("%d %d out of range or no such ban %08x\n", i, banid, banlist);
			}
			else
				debug("Chanell || chan->banlist not found -> channel not synced ?!Try /mode +b \n");
		}
		else { 
			watch_write(j->send_watch, "MODE %s -b %s\r\n", channame+4, *mp);
		}
	}
	array_free(mp);
	xfree(channame);
	return 0;

}

static COMMAND(irc_command_ban) {
	irc_private_t	*j = irc_private(session);
	char		*chan, **mp, *temp = NULL;
	people_t	*person;

	if (!(chan=irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN))) 
		return -1;

	debug("[irc]_command_ban(): chan: %s mp[0]:%s mp[1]:%s\n",
			chan, mp[0], mp[1]);

	if (!(*mp))
		watch_write(j->send_watch, "MODE %s +b \r\n", chan+4);
	else {
		person = irc_find_person(j->people, (char *) *mp);
		if (person) 
			temp = irc_make_banmask(session, person->nick+4, person->ident, person->host);
		if (temp) {
			watch_write(j->send_watch, "MODE %s +b %s\r\n", chan+4, temp);
			xfree(temp);
		} else
			watch_write(j->send_watch, "MODE %s +b %s\r\n", chan+4, *mp);
	}
	array_free(mp);
	xfree(chan);
	return 0;
}

static COMMAND(irc_command_kickban) {
	const char	*p[4] = { params[0], params[1], params[2], NULL };

	if (!xstrcmp(name, ("kickban")))
	{
		irc_command_kick(("kick"), params, session, target, quiet);
		irc_command_ban(("ban"), params, session, target, quiet);
	} else {
		irc_command_ban(("ban"), params, session, target, quiet);
		irc_command_kick(("kick"), params, session, target, quiet);
	}
	if (p) ;
	return 0;
}


static COMMAND(irc_command_devop) {
	irc_private_t	*j = irc_private(session);
	int		modes, i;
	char		**mp, *op, *nicks, *tmp, c, *chan, *p;
	string_t 	zzz;

	if (!(chan = irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN))) 
		return -1;

	if (!(*mp)) {
		printq("not_enough_params", name);
		xfree(chan);
		return -1;
	}

	modes = atoi(j->sopt[_005_MODES]);
	op = xmalloc((modes+2) * sizeof(char));
		/* H alfop */	/* o P */	/* voice */
	c=xstrchr(name, 'h')?'h':xstrchr(name, 'p')?'o':'v';
	/* Yes, I know there is such a function as memset() ;> */
	for (i=0, tmp=op+1; i<modes; i++, tmp++) *tmp=c;
	op[0]=*name=='d'?'-':'+';

	zzz = string_init(*mp);
	for (i=1; mp[i]; i++)
		string_append_c(zzz, ' '), string_append(zzz, mp[i]);
	
	nicks = string_free(zzz, 0);
	p = nicks;

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
		watch_write(j->send_watch, "MODE %s %s %s\r\n", chan, op, p);
		if (!tmp) break;
		*tmp = ' ';
		tmp++;
		p = tmp;
	}
	chan-=4;
	xfree(chan);
	xfree(nicks);
	xfree(op);
	array_free(mp);
	return 0;
}

static COMMAND(irc_command_ctcp) {
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

	watch_write(irc_private(session)->send_watch, "PRIVMSG %s :\01%s\01\r\n",
			who+4, ctcps[i].name?ctcps[i].name:(*mp));

	array_free(mp);
	xfree(who);
	return 0;
}

static COMMAND(irc_command_ping) {
	char		**mp, *who;
	struct timeval	tv;

	if (!(who=irc_getchan(session, params, name, &mp, 0, IRC_GC_ANY))) 
		return -1;

	gettimeofday(&tv, NULL);
	watch_write(irc_private(session)->send_watch, "PRIVMSG %s :\01PING %d %d\01\r\n",
			who+4 ,tv.tv_sec, tv.tv_usec);

	array_free(mp);
	xfree(who);
	return 0;
}

static COMMAND(irc_command_me) {
	irc_private_t	*j = irc_private(session);
	char		**mp, *chan, *chantypes = SOP(_005_CHANTYPES), *str, *col;
	int		mw = session_int_get(session, "make_window"), ischn;

	if (!(chan=irc_getchan(session, params, name,
					&mp, 1, IRC_GC_ANY)))
		return -1;

	ischn = chantypes?!!xstrchr(chantypes, chan[4]):0;
	
	str = xstrdup(*mp);

	watch_write(irc_private(session)->send_watch, "PRIVMSG %s :\01ACTION %s\01\r\n",
			chan+4, str?str:"");

	col = irc_ircoldcolstr_to_ekgcolstr(session, str, 1);
	print_window(chan, session, ischn?(mw&1):!!(mw&2),
			ischn?"irc_ctcp_action_y_pub":"irc_ctcp_action_y",
			session_name(session), j->nick, chan, col);

	array_free(mp);
	xfree(chan);
	xfree(col);
	xfree(str);
	return 0;
}

static COMMAND(irc_command_mode) {
	char	**mp, *chan;

	if (!(chan=irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN))) 
		return -1;
/* G->dj: I'm still leaving this 
	if (!(*mp)) {
		print("not_enough_params", name);
		array_free(mp);
		xfree(chan);
		return -1;
	}
*/
	debug("%s %s \n", chan, mp[0]);
	if (!(*mp))
		watch_write(irc_private(session)->send_watch, "MODE %s\r\n",
				chan+4);
	else
		watch_write(irc_private(session)->send_watch, "MODE %s %s\r\n",
				chan+4, *mp);

	array_free(mp);
	xfree(chan);
	return 0;
}

static COMMAND(irc_command_umode) {
	irc_private_t	*j = irc_private(session);

	if (!(*params)) {
		print("not_enough_params", name);
		return -1;
	}

	watch_write(j->send_watch, "MODE %s %s\r\n", j->nick, *params);

	return 0;
}

static COMMAND(irc_command_whois) {
	char	**mp, *person;

	if (!(person = irc_getchan(session, params, name,
					&mp, 0, IRC_GC_NOT_CHAN)))
		return -1;

	debug("irc_command_whois(): %s\n", name);
	if (!xstrcmp(name, ("whowas")))
		watch_write(irc_private(session)->send_watch, "WHOWAS %s\r\n", person+4);
        else if (!xstrcmp(name, ("wii")))
		watch_write(irc_private(session)->send_watch, "WHOIS %s %s\r\n", person+4, person+4);
	else	watch_write(irc_private(session)->send_watch, "WHOIS %s\r\n",  person+4);

	array_free(mp);
	xfree (person);
	return 0;
}

static QUERY(irc_status_show_handle) {
	char		**uid = va_arg(ap, char**);
	session_t	*s = session_find(*uid);
	const char	*p[1];

	if (!s)
		return -1;

	p[0] = irc_private(s)->nick;
	p[1] = 0;

	return irc_command_whois(("wii"), p, s, NULL, 0);
}

static COMMAND(irc_command_query) {
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
		array_free(p);
		return -1;
	}

	tmp = xstrdup(tar);
	tmp = strip_quotes(tmp);

	w = window_find_s(session, tmp);

	if (!w) {
		w = window_new(tmp, session, 0);
		if (session_int_get(session, "auto_lusers_sync") > 0)
			watch_write(j->send_watch, "USERHOST %s\r\n", tmp+4);
	}

	window_switch(w->id);

	xfree(tmp);
	array_free(mp);
        array_free(p);
	xfree(tar);
	return 0;
}

static COMMAND(irc_command_jopacy) {
	irc_private_t	*j = irc_private(session);
	char		**mp, *tar = NULL, *pass = NULL, *str, *tmp;
	channel_t	*chan;

	if (!(tar = irc_getchan(session, params, name,
					&mp, 0, IRC_GC_CHAN)))
		return -1;

	if (!xstrcmp(name, ("cycle"))) {
		chan = irc_find_channel(j->channels, tar);
		if (chan && (pass = xstrchr(chan->mode_str, 'k')))
			pass+=2;
		debug("[IRC_CYCLE] %s\n", pass);
	}

	tmp = saprintf("JOIN %s%s\r\n", tar+4, pass ? pass : "");
	if (!xstrcmp(name, ("part")) || !xstrcmp(name, ("cycle"))) {
		str = saprintf("PART %s :%s\r\n%s", tar+4,
				PARTMSG(session,(*mp)),
				!xstrcmp(name, ("cycle"))?tmp:"");
	} else if (!xstrcmp(name, ("join"))) {
		str = tmp; tmp=NULL;
	} else
		return 0;

	watch_write(j->send_watch, str);

	array_free(mp);
	xfree(tar);
	xfree(str);
	xfree(tmp);

	return 0;
}

static COMMAND(irc_command_nick) {
	irc_private_t	*j = irc_private(session);

	/* GiM: XXX FIXME TODO think more about j->connecting... */
	if (j->connecting || session_connected_get(session)) {
		watch_write(j->send_watch, "NICK %s\r\n", params[0]);
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

static COMMAND(irc_command_test) {
	irc_private_t	*j = irc_private(session);
	list_t		tlist = j->connlist;

//#define DOT(a,x,y,z,error) print_window("__status", z, 0, a, session_name(z), x, y->hostname, y->address, itoa(y->port), itoa(y->family), error ? strerror(error) : "")
	
	for (tlist = j->connlist; tlist; tlist = tlist->next)
		DOT("IRC_TEST", "Connect to:", ((connector_t *) tlist->data), session, 0);
	for (tlist = j->bindlist; tlist; tlist = tlist->next)
		DOT("IRC_TEST", "Bind to:", ((connector_t *) tlist->data), session, 0);

	if (j->conntmplist && j->conntmplist->data) {
		if (j->connecting)
			DOT("IRC_TEST", "Connecting:", ((connector_t *) j->conntmplist->data), session, 0);
		else if (session_connected_get(session))
			DOT("IRC_TEST", "Connected:", ((connector_t *) j->conntmplist->data), session, 0);
		else    DOT("IRC_TEST", "Will Connect to:", ((connector_t *) j->conntmplist->data), session, 0);
	}
	if (j->bindtmplist && j->bindtmplist->data)
		DOT("IRC_TEST", "((Will Bind to) || (Binded)) :", ((connector_t *) j->bindtmplist->data), session, 0);
	return 0;
}

static COMMAND(irc_command_genkey) {
#ifndef NO_POSIX_SYSTEM
	extern int sim_key_generate(const char *uid); /* sim plugin */
	char *uid = NULL;

	if (params[0]) 
		uid = saprintf("%s:%s", IRC3, params[0]);
	else    uid = xstrdup(target);
	
	if (!uid) 
		return -1;

	if ((plugin_find(("sim")))) {
#if 0
		struct stat st;
		char *temp1, *temp2;
		int  ret = 0;

                temp1 = saprintf("%s/%s.pem", prepare_path("keys", 0), uid);
                temp2 = saprintf("%s/private-%s.pem", prepare_path("keys", 0), uid);

                if (!stat(temp1, &st) && !stat(temp2, &st)) {
                        printq("key_private_exist");
			ret = -1;
                }
		xfree(temp1); xfree(temp2);

		if (!ret) {
			printq("key_generating");
			ret = sim_key_generate(uid);
			printq("key_generating_success");
		}
#endif
	}
	// etc.. 	
	xfree(uid);
	return 0;
#else
	return -1;
#endif
}

/*                                                                       *
 * ======================================== INIT/DESTROY --------------- *
 *                                                                       */

#define params(x) x

static plugins_params_t irc_plugin_vars[] = {
	/* lower case: names of variables that reffer to client itself */
	PLUGIN_VAR_ADD("alt_nick", 		0, VAR_STR, NULL, 0, NULL),
	PLUGIN_VAR_ADD("alias",			SESSION_VAR_ALIAS, VAR_STR, NULL, 0, NULL),
	PLUGIN_VAR_ADD("auto_away",		SESSION_VAR_AUTO_AWAY, VAR_INT, "0", 0, NULL),
	PLUGIN_VAR_ADD("auto_back",		SESSION_VAR_AUTO_BACK, VAR_INT, "0", 0, NULL),
	PLUGIN_VAR_ADD("auto_connect",		SESSION_VAR_AUTO_CONNECT, VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("auto_find",		SESSION_VAR_AUTO_FIND, VAR_BOOL, "0", 0, NULL),		/* it's really auto_whois */
	PLUGIN_VAR_ADD("auto_reconnect",	SESSION_VAR_AUTO_RECONNECT, VAR_INT, "10", 0, NULL), 
	PLUGIN_VAR_ADD("auto_channel_sync",	0, VAR_BOOL, "1", 0, NULL),				/* like channel_sync in irssi; better DO NOT turn it off! */
	PLUGIN_VAR_ADD("auto_lusers_sync", 	0, VAR_BOOL, "0", 0, NULL),				/* sync lusers, stupid ;(,  G->dj: well why ? */
	PLUGIN_VAR_ADD("ban_type", 		0, VAR_INT, "10", 0, NULL),
	PLUGIN_VAR_ADD("connect_timeout",	SESSION_VAR_CONNECT_TIMEOUT, VAR_INT, "0", 0, NULL),
	PLUGIN_VAR_ADD("close_windows", 	0, VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("dcc_port",		SESSION_VAR_DCC_PORT, VAR_INT, "0", 0, NULL),
	PLUGIN_VAR_ADD("display_notify", 	SESSION_VAR_DISPLAY_NOTIFY, VAR_INT, "0", 0, NULL),
	PLUGIN_VAR_ADD("hostname",		0, VAR_STR, 0, 0, irc_changed_resolve),
	PLUGIN_VAR_ADD("identify",		0, VAR_STR, 0, 0, NULL),
	PLUGIN_VAR_ADD("log_formats",		SESSION_VAR_LOG_FORMATS, VAR_STR, "irssi", 0, NULL),
	PLUGIN_VAR_ADD("make_window",		0, VAR_INT, "2", 0, NULL),
	PLUGIN_VAR_ADD("prefer_family", 	0, VAR_INT, "0", 0, NULL),
	PLUGIN_VAR_ADD("nickname",		0, VAR_STR, NULL, 0, NULL), 				/* value will be inited @ irc_plugin_init() [pwd_entry->pw_name] */
	PLUGIN_VAR_ADD("password",		SESSION_VAR_PASSWORD, VAR_STR, 0, 1, NULL),
	PLUGIN_VAR_ADD("port",			SESSION_VAR_PORT, VAR_INT, "6667", 0, NULL),
	PLUGIN_VAR_ADD("realname",		0, VAR_STR, NULL, 0, NULL),				/* value will be inited @ irc_plugin_init() [pwd_entry->pw_gecos] */
	PLUGIN_VAR_ADD("server",		SESSION_VAR_SERVER, VAR_STR, 0, 0, irc_changed_resolve),

	/* upper case: names of variables, that reffer to protocol stuff */
	PLUGIN_VAR_ADD("AUTO_JOIN",			0, VAR_STR, 0, 0, NULL),
	PLUGIN_VAR_ADD("AUTO_JOIN_CHANS_ON_INVITE", 	0, VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("DEFAULT_COLOR", 		0, VAR_INT, "0", 0, NULL),			/* TODO :> */
	PLUGIN_VAR_ADD("DISPLAY_PONG", 			0, VAR_BOOL, "1", 0, NULL),			/* GiM, do we really want/need '1' here as default? */
	PLUGIN_VAR_ADD("DISPLAY_AWAY_NOTIFICATION", 	0, VAR_INT, "1", 0, NULL),
	PLUGIN_VAR_ADD("DISPLAY_IN_CURRENT", 		0, VAR_INT, "2", 0, NULL),
	PLUGIN_VAR_ADD("DISPLAY_NICKCHANGE", 		0, VAR_INT, "0", 0, NULL),
	PLUGIN_VAR_ADD("DISPLAY_QUIT", 			0, VAR_INT, "0", 0, NULL),
	/* plugin_var_add(&irc_plugin, "HIGHLIGHTS", VAR_STR, 0, 0, NULL); */
	PLUGIN_VAR_ADD("KICK_MSG", 			0, VAR_STR, DEFKICKMSG, 0, NULL),
	PLUGIN_VAR_ADD("PART_MSG", 			0, VAR_STR, DEFPARTMSG, 0, NULL),
	PLUGIN_VAR_ADD("QUIT_MSG", 			0, VAR_STR, DEFQUITMSG, 0, NULL),
	PLUGIN_VAR_ADD("REJOIN", 			0, VAR_INT, "0", 0, NULL),
	PLUGIN_VAR_ADD("REJOIN_TIME", 			0, VAR_INT, "2", 0, NULL),
	PLUGIN_VAR_ADD("SHOW_NICKMODE_EMPTY", 		0, VAR_INT, "1", 0, NULL),
	PLUGIN_VAR_ADD("SHOW_MOTD", 			0, VAR_BOOL, "1", 0, NULL),
	PLUGIN_VAR_ADD("STRIPMIRCCOL", 			0, VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("VERSION_NAME", 			0, VAR_STR, 0, 0, NULL),
	PLUGIN_VAR_ADD("VERSION_NO", 			0, VAR_STR, 0, 0, NULL),
	PLUGIN_VAR_ADD("VERSION_SYS", 			0, VAR_STR, 0, 0, NULL),

	PLUGIN_VAR_END()
};

int irc_plugin_init(int prio)
{
#ifndef NO_POSIX_SYSTEM
	struct passwd	*pwd_entry = getpwuid(getuid());

/* yeah, i know it's static. */
	static char pwd_name[2000] 	= { '\0'};
	static char pwd_realname[2000]	= { '\0'};

	if (pwd_entry) {
		xstrncpy(pwd_name, pwd_entry->pw_name, sizeof(pwd_name));
		xstrncpy(pwd_realname, pwd_entry->pw_gecos, sizeof(pwd_realname));

		pwd_name[sizeof(pwd_name)-1] = '\0';
		pwd_realname[sizeof(pwd_realname)-1] = '\0';
		/* XXX, we need to free buffer allocated by getpwuid()? */
	}

#else
	const char *pwd_name = NULL;
	const char *pwd_realname = NULL;
#endif

/* magic stuff */
	irc_plugin_vars[19].value = pwd_name;
	irc_plugin_vars[22].value = pwd_realname;

	irc_plugin.params = irc_plugin_vars;

	plugin_register(&irc_plugin, prio);

	query_connect_id(&irc_plugin, PROTOCOL_VALIDATE_UID,	irc_validate_uid, NULL);
	query_connect_id(&irc_plugin, GET_PLUGIN_PROTOCOLS,	irc_protocols, NULL);
	query_connect_id(&irc_plugin, PLUGIN_PRINT_VERSION,	irc_print_version, NULL);
	query_connect_id(&irc_plugin, UI_WINDOW_KILL,		irc_window_kill, NULL);
	query_connect_id(&irc_plugin, SESSION_ADDED,		irc_session, (void*) 1);
	query_connect_id(&irc_plugin, SESSION_REMOVED,		irc_session, (void*) 0);
	query_connect_id(&irc_plugin, IRC_TOPIC,		irc_topic_header, (void*) 0);
	query_connect_id(&irc_plugin, STATUS_SHOW,		irc_status_show_handle, NULL);
	query_connect_id(&irc_plugin, IRC_KICK,			irc_onkick_handler, 0);
	query_connect_id(&irc_plugin, SET_VARS_DEFAULT, 	irc_setvar_default, NULL);

#define IRC_ONLY 		SESSION_MUSTBELONG | SESSION_MUSTHASPRIVATE
#define IRC_FLAGS 		IRC_ONLY | SESSION_MUSTBECONNECTED
#define IRC_FLAGS_TARGET	IRC_FLAGS | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET
	commands_lock = &commands;	/* keep it sorted or die */
	
	command_add(&irc_plugin, ("irc:"), "?",		irc_command_inline_msg, IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:_autoaway"), NULL,	irc_command_away, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:_autoback"), NULL,	irc_command_away, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:_conntest"), "?",	irc_command_test, 	IRC_ONLY, NULL);
	command_add(&irc_plugin, ("irc:_genkeys"),  "?",  irc_command_genkey, 0, NULL);
	command_add(&irc_plugin, ("irc:access"), "p uUw ? ?",	irc_command_alist, 0, "-a --add -d --delete -e --edit -s --show -l --list -S --sync");
	command_add(&irc_plugin, ("irc:add"), NULL,	irc_command_add, 	IRC_ONLY | COMMAND_PARAMASTARGET, NULL);
	command_add(&irc_plugin, ("irc:away"), "?",	irc_command_away,	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:back"), NULL,	irc_command_away, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:ban"),  "uUw uU",	irc_command_ban, 	IRC_FLAGS, NULL); 
	command_add(&irc_plugin, ("irc:bankick"), "uUw uU ?", irc_command_kickban,IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:connect"), NULL,	irc_command_connect, 	IRC_ONLY, NULL);
	command_add(&irc_plugin, ("irc:ctcp"), "uUw ?",	irc_command_ctcp, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:cycle"), "w ?",	irc_command_jopacy, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:dehalfop"), "uUw uU uU uU uU uU uU ?",irc_command_devop, IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:deop"), "uUw uU uU uU uU uU uU ?",	irc_command_devop, IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:devoice"), "uUw uU uU uU uU uU uU ?",irc_command_devop, IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:disconnect"), "r ?",irc_command_disconnect,IRC_ONLY, NULL);
	command_add(&irc_plugin, ("irc:find"), "uU",	irc_command_whois, 	IRC_FLAGS, NULL); /* for auto_find */
	command_add(&irc_plugin, ("irc:halfop"), "uUw uU uU uU uU uU uU ?",irc_command_devop, IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:invite"), "uUw uUw",irc_command_invite, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:join"), "w", 	irc_command_jopacy, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:kick"), "uUw uU ?",irc_command_kick, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:kickban"), "uUw uU ?", irc_command_kickban,IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:me"), "uUw ?",	irc_command_me, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:mode"), "w ?",	irc_command_mode, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:msg"), "!uUw !",	irc_command_msg, 	IRC_FLAGS_TARGET, NULL);
	command_add(&irc_plugin, ("irc:names"), "w?",	irc_command_names, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:nick"), "!",	irc_command_nick, 	IRC_ONLY | COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&irc_plugin, ("irc:notice"), "!uUw !",irc_command_msg, 	IRC_FLAGS_TARGET, NULL);
	command_add(&irc_plugin, ("irc:op"), "uUw uU uU uU uU uU uU ?",	irc_command_devop, IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:part"), "w ?",	irc_command_jopacy, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:people"), NULL,	irc_command_pipl, 	IRC_ONLY, NULL);
	command_add(&irc_plugin, ("irc:ping"), "uUw ?",	irc_command_ping, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:query"), "uUw",	irc_command_query,	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:quote"), "!",	irc_command_quote,	IRC_FLAGS | COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&irc_plugin, ("irc:_parse"), "!",	irc_command_parse,	IRC_FLAGS | COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&irc_plugin, ("irc:reconnect"), "r ?",irc_command_reconnect,	IRC_ONLY, NULL);
	command_add(&irc_plugin, ("irc:topic"), "w ?",	irc_command_topic, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:umode"), "?",	irc_command_umode, 	IRC_ONLY /* _FLAGS ? */, NULL);
	command_add(&irc_plugin, ("irc:unban"),  "uUw uU",irc_command_unban, 	IRC_FLAGS, NULL); 
	command_add(&irc_plugin, ("irc:voice"), "uUw uU uU uU uU uU uU ?",irc_command_devop, IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:who"), "uUw",	irc_command_who, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:whois"), "uU",	irc_command_whois, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:whowas"), "uU",	irc_command_whois, 	IRC_FLAGS, NULL);
	command_add(&irc_plugin, ("irc:wii"), "uU",	irc_command_whois, 	IRC_FLAGS, NULL);
/*
	command_add(&irc_plugin, ("irc:admin"), "",       NULL, 0, NULL);   q admin
	command_add(&irc_plugin, ("irc:map"),  "",        NULL, 0, NULL);   q map
	command_add(&irc_plugin, ("irc:links"),  "",      NULL, 0, NULL); V q links
	command_add(&irc_plugin, ("irc:oper"), "",	NULL, 0, NULL);   q oper %nick %pass
	command_add(&irc_plugin, ("irc:trace"), "",	NULL, 0, NULL);   q trace %...
	command_add(&irc_plugin, ("irc:stats"), "\"STATS\" ?",irc_command_quote, 0, NULL); V q stats
	command:add(&irc_plugin, ("irc:list"), .....)			V q list 
*/
	commands_lock = NULL;

	variable_add(&irc_plugin, "access_groups", VAR_STR, 1, &irc_config_default_access_groups, NULL, NULL, NULL);

	irc_setvar_default(NULL, NULL);

/* irc_session by queries do it. */
	return 0;
}

static int irc_plugin_destroy() {
	plugin_unregister(&irc_plugin);

	return 0;
}

static int irc_theme_init()
{
	debug("I love you honey bunny\n");

	/* %1 should be _always_ session name, if it's not so,
	 * you should report this to me (GiM)
	 */
#ifndef NO_DEFAULT_THEME
	/* %2 - prefix, %3 - nick, %4 - nick+ident+host, %5 - chan, %6 - msg*/
	format_add("irc_msg_sent",	"%P<%n%3/%5%P>%n %6", 1);
	format_add("irc_msg_sent_n",	"%P<%n%3%P>%n %6", 1);
	format_add("irc_msg_sent_chan",	"%P<%w%{2@%+gcp}X%2%3%P>%n %6", 1);
	format_add("irc_msg_sent_chanh","%P<%W%{2@%+GCP}X%2%3%P>%n %6", 1);
	
	format_add("irc_not_sent",	"%P(%n%3/%5%P)%n %6", 1);
	format_add("irc_not_sent_n",	"%P(%n%3%P)%n %6", 1);
	format_add("irc_not_sent_chan",	"%P(%w%{2@%+gcp}X%2%3%P)%n %6", 1);
	format_add("irc_not_sent_chanh","%P(%W%{2@%+GCP}X%2%3%P)%n %6", 1);

//	format_add("irc_msg_f_chan",	"%B<%w%{2@%+gcp}X%2%3/%5%B>%n %6", 1); /* NOT USED */
//	format_add("irc_msg_f_chanh",	"%B<%W%{2@%+GCP}X%2%3/%5%B>%n %6", 1); /* NOT USED */
	format_add("irc_msg_f_chan_n",	"%B<%w%{2@%+gcp}X%2%3%B>%n %6", 1);
	format_add("irc_msg_f_chan_nh",	"%B<%W%{2@%+GCP}X%2%3%B>%n %6", 1);
	format_add("irc_msg_f_some",	"%b<%n%3%b>%n %6", 1);

//	format_add("irc_not_f_chan",	"%B(%w%{2@%+gcp}X%2%3/%5%B)%n %6", 1); /* NOT USED */
//	format_add("irc_not_f_chanh",	"%B(%W%{2@%+GCP}X%2%3/%5%B)%n %6", 1); /* NOT USED */
	format_add("irc_not_f_chan_n",	"%B(%w%{2@%+gcp}X%2%3%B)%n %6", 1);
	format_add("irc_not_f_chan_nh",	"%B(%W%{2@%+GCP}X%2%3%B)%n %6", 1);
	format_add("irc_not_f_some",	"%b(%n%3%b)%n %6", 1);
	format_add("irc_not_f_server",	"%g!%3%n %6", 1);

	format_add("IRC_NAMES_NAME",	_("[%gUsers %G%2%n]"), 1);
	format_add("IRC_NAMES",		"%K[%W%1%w%2%3%K]%n ", 1);
	format_add("IRC_NAMES_TOTAL_H",	_("%> %WEKG2: %2%n: Total of %W%3%n nicks [%W%4%n ops, %W%5%n halfops, %W%6%n voices, %W%7%n normal]\n"), 1);
	format_add("IRC_NAMES_TOTAL",	"%> %WEKG2: %2%n: Total of %W%3%n nicks [%W%4%n ops, %W%5%n voices, %W%6%n normal]\n", 1);

	format_add("irc_joined",	_("%> %Y%2%n has joined %4\n"), 1);
	format_add("irc_joined_you",	_("%> %RYou%n have joined %4\n"), 1);
	format_add("irc_left",		_("%> %g%2%n has left %4 (%5)\n"), 1);
	format_add("irc_left_you",	_("%> %RYou%n have left %4 (%5)\n"), 1);
	format_add("irc_kicked",	_("%> %Y%2%n has been kicked out by %R%3%n from %5 (%6)\n"), 1);
	format_add("irc_kicked_you",	_("%> You have been kicked out by %R%3%n from %5 (%6)\n"), 1);
	format_add("irc_quit",		_("%> %Y%2%n has quit irc (%4)\n"), 1);
	format_add("irc_split",		"%> ", 1);
	format_add("irc_unknown_ctcp",	_("%> %Y%2%n sent unknown CTCP %3: (%4)\n"), 1);
	format_add("irc_ctcp_action_y_pub",	"%> %y%e* %2%n %4\n", 1);
	format_add("irc_ctcp_action_y",		"%> %Y%e* %2%n %4\n", 1);
	format_add("irc_ctcp_action_pub",	"%> %y%h* %2%n %5\n", 1);
	format_add("irc_ctcp_action",		"%> %Y%h* %2%n %5\n", 1);
	format_add("irc_ctcp_request_pub",	_("%> %Y%2%n requested ctcp %5 from %4\n"), 1);
	format_add("irc_ctcp_request",		_("%> %Y%2%n requested ctcp %5\n"), 1);
	format_add("irc_ctcp_reply",		_("%> %Y%2%n CTCP reply from %3: %5\n"), 1);


	format_add("IRC_ERR_CANNOTSENDTOCHAN",	"%! %2: %1\n", 1);
	
	format_add("IRC_RPL_FIRSTSECOND",	"%> (%1) %2 %3\n", 1);
	format_add("IRC_RPL_SECONDFIRST",	"%> (%1) %3 %2\n", 1);
	format_add("IRC_RPL_JUSTONE",		"%> (%1) %2\n", 1);
	format_add("IRC_RPL_NEWONE",		"%> (%1,%2) 1:%3 2:%4 3:%5 4:%6\n", 1);

	format_add("IRC_ERR_FIRSTSECOND",	"%! (%1) %2 %3\n", 1);
	format_add("IRC_ERR_SECONDFIRST",	"%! (%1) %3 %2\n", 1);
	format_add("IRC_ERR_JUSTONE",		"%! (%1) %2\n", 1);
	format_add("IRC_ERR_NEWONE",		"%! (%1,%2) 1:%3 2:%4 3:%5 4:%6\n", 1);
	
	format_add("IRC_RPL_CANTSEND",	_("%> Cannot send to channel %T%2%n\n"), 1);
	format_add("RPL_MOTDSTART",	"%g,+=%G-----\n", 1);
	format_add("RPL_MOTD",		"%g|| %n%2\n", 1);
	format_add("RPL_ENDOFMOTD",	"%g`+=%G-----\n", 1);

	
	format_add("RPL_INVITE",	_("%> Inviting %W%2%n to %W%3%n\n"), 1);
 	/* Used in: /mode +b|e|I %2 - chan %3 - data from server */
	/* THIS IS TEMPORARY AND WILL BE DONE OTHER WAY, DO NOT USE THIS STYLES
	 */
	format_add("RPL_LISTSTART",	"%g,+=%G-----\n", 1);
	format_add("RPL_EXCEPTLIST",	_("%g|| %n %5 - %W%2%n: except %c%3\n"), 1);
	format_add("RPL_BANLIST",	_("%g|| %n %5 - %W%2%n: ban %c%3\n"), 1);
	format_add("RPL_INVITELIST",	_("%g|| %n %5 - %W%2%n: invite %c%3\n"), 1);;
	format_add("RPL_EMPTYLIST" ,	_("%g|| %n Empty list \n"), 1);
	format_add("RPL_LINKS",		"%g|| %n %5 - %2  %3  %4\n", 1);
	format_add("RPL_ENDOFLIST", 	"%g`+=%G----- %2%n\n", 1);

	/* %2 - number; 3 - type of stats (I, O, K, etc..) ....*/
	format_add("RPL_STATS",		"%g|| %3 %n %4 %5 %6 %7 %8\n", 1);
	format_add("RPL_STATS_EXT",	"%g|| %3 %n %2 %4 %5 %6 %7 %8\n", 1);
	format_add("RPL_STATSEND",	"%g`+=%G--%3--- %2\n", 1);
	/*
	format_add("RPL_CHLISTSTART",  "%g,+=%G lp %2\t%3\t%4\n", 1);
	format_add("RPL_CHLIST",       "%g|| %n %5 %2\t%3\t%4\n", 1);
	*/
	format_add("RPL_CHLISTSTART",	"%g,+=%G lp %2\t%3\t%4\n", 1);
	format_add("RPL_LIST",		"%g|| %n %5 %2\t%3\t%4\n", 1);

	/* 2 - number; 3 - chan; 4 - ident; 5 - host; 6 - server ; 7 - nick; 8 - mode; 9 -> realname
	 * format_add("RPL_WHOREPLY",   "%g|| %c%3 %W%7 %n%8 %6 %4@%5 %W%9\n", 1);
	 */
	format_add("RPL_WHOREPLY",	"%g|| %c%3 %W%7 %n%8 %6 %4@%5 %W%9\n", 1);
	/* delete those irssi-like styles */

	format_add("RPL_AWAY",		_("%G||%n away     : %2 - %3\n"), 1);
	/* in whois %2 is always nick */
	format_add("RPL_WHOISUSER",	_("%G.+===%g-----\n%G||%n (%T%2%n) (%3@%4)\n"
				"%G||%n realname : %6\n"), 1);

	format_add("RPL_WHOWASUSER",	_("%G.+===%g-----\n%G||%n (%T%2%n) (%3@%4)\n"
				"%G||%n realname : %6\n"), 1);

/* %2 - nick %3 - there is/ was no such nickname / channel, and so on... */
	/*
	format_add("IRC_WHOERROR", _("%G.+===%g-----\n%G||%n %3 (%2)\n"), 1);
	format_add("IRC_ERR_NOSUCHNICK", _("%n %3 (%2)\n"), 1);
	*/

	format_add("RPL_WHOISCHANNELS",	_("%G||%n %|channels : %3\n"), 1);
	format_add("RPL_WHOISSERVER",	_("%G||%n %|server   : %3 (%4)\n"), 1);
	format_add("RPL_WHOISOPERATOR",	_("%G||%n %|ircOp    : %3\n"), 1);
	format_add("RPL_WHOISIDLE",	_("%G||%n %|idle     : %3 (signon: %4)\n"), 1);
	format_add("RPL_ENDOFWHOIS",	_("%G`+===%g-----\n"), 1);
	format_add("RPL_ENDOFWHOWAS",	_("%G`+===%g-----\n"), 1);

	format_add("RPL_TOPIC",		_("%> Topic %2: %3\n"), 1);
	/* \n not needed if you're including date [%4] */
	format_add("IRC_RPL_TOPICBY",	_("%> set by %2 on %4"), 1);
	format_add("IRC_TOPIC_CHANGE",	_("%> %T%2%n changed topic on %T%4%n: %5\n"), 1);
	format_add("IRC_TOPIC_UNSET",	_("%> %T%2%n unset topic on %T%4%n\n"), 1);
	format_add("IRC_MODE_CHAN_NEW",	_("%> %2/%4 sets mode [%5]\n"), 1);
	format_add("IRC_MODE_CHAN",	_("%> %2 mode is [%3]\n"), 1);
	format_add("IRC_MODE",		_("%> (%1) %2 set mode %3 on You\n"), 1);
	
	format_add("IRC_INVITE",	_("%> %W%2%n invites you to %W%5%n\n"), 1);
	format_add("IRC_PINGPONG",	_("%) (%1) ping/pong %c%2%n\n"), 1);
	format_add("IRC_YOUNEWNICK",	_("%> You are now known as %G%3%n\n"), 1);
	format_add("IRC_NEWNICK",	_("%> %g%2%n is now known as %G%4%n\n"), 1);
	format_add("IRC_TRYNICK",	_("%> Will try to use %G%2%n instead\n"), 1);
	format_add("IRC_CHANNEL_SYNCED", "%> Join to %W%2%n was synced in %W%3.%4%n secs", 1);
	/* %1 - sesja ; %2 - Connect, BIND, whatever, %3 - hostname %4 - adres %5 - port 
	 * %6 - family (debug pursuit only) */
	format_add("IRC_TEST",		"%> (%1) %2 to %W%3%n [%4] port %W%5%n (%6)", 1);
	format_add("IRC_CONN_ESTAB",	"%> (%1) Connection to %W%3%n estabilished", 1);
	/* j/w %7 - error */
	format_add("IRC_TEST_FAIL",	"%! (%1) Error: %2 to %W%3%n [%4] port %W%5%n (%7)", 1);
	
	format_add("irc_channel_secure",	"%) (%1) Echelon can kiss our ass on %2 *g*", 1); 
	format_add("irc_channel_unsecure",	"%! (%1) warning no plugin protect us on %2 :( install sim plugin now or at least rot13..", 1); 

	format_add("irc_access_added",	_("%> (%1) %3 [#%2] was added to accesslist chan: %4 (flags: %5)"), 1);
	format_add("irc_access_known", "a-> %2!%3@%4", 1);	/* %2 is nickname, not uid ! */


#endif	/* !NO_DEFAULT_THEME */
	return 0;
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=8 sw=8
 */
