/*
 *  (C) Copyright 2004-2005 Michal 'GiM' Spadlinski <gim at skrzynka dot pl>
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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include <ekg/commands.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include "irc.h"
#include "misc.h"
#include "people.h"
#include "input.h"
#include "autoacts.h"

char *sopt_keys[SERVOPTS] = { NULL, NULL, "PREFIX", "CHANTYPES", "CHANMODES", "MODES" };

#define OMITCOLON(x) ((*x)==':'?(x+1):(x))
/*
 * irc_handle_write()
 *
 * handles writing to socket, spit out from buffer 
 * as much as it can;
 */
WATCHER(irc_handle_write)
{
	irc_private_t	*j = data;
	int		res;

	res = write(j->fd, j->obuf, j->obuf_len);

	if (res == -1)
		debug("[irc] handle_write() failed: %s\n", strerror(errno));

	else if (res == j->obuf_len)
		debug("[irc] handle_write() output buffer empty\n");

	 else { /* OK PROCEED */
		memmove(j->obuf, j->obuf + res, j->obuf_len - res);
		j->obuf_len -= res;

		watch_add(&irc_plugin, j->fd, WATCH_WRITE, 0, irc_handle_write, j);
		return;
	}

	xfree(j->obuf);
	j->obuf = NULL;
	j->obuf_len = 0;
}

/*
 * irc_write()
 *
 * sends text to server, and if it's unable to send everything,
 * it saves the rest to the buffer and it'll send as soon as
 * it'll be able to.
 *
 *  - j - session's private data
 *  - text - how do you think ?
 */
int irc_write(irc_private_t *j, const char *format, ...)
{
	const char	*buf;
	char		*text;
	int		len;
	va_list		ap;

	if (!j || !format)
		return -1;

	va_start(ap, format);
	text = vsaprintf(format, ap);
	va_end(ap);

	debug("[irc]_send: %s\n", text?xstrlen(text)?text:"[0LENGTH]":"[FAILED]");
	if (!text) return -1;

	if (!j->obuf) {
		int	res;

		len = strlen(text);
		res = write(j->fd, text, len);

		if (res == len) {
			xfree(text);
			return 0;

		} else 	if (res == -1) {
			xfree(text);
			return -1;

		} else
			buf = text + res;
	} else
		buf = text;

	len = strlen(buf);

	if (!j->obuf)
		watch_add(&irc_plugin, j->fd, WATCH_WRITE, 0, irc_handle_write, j);

	j->obuf = xrealloc(j->obuf, j->obuf_len + len);
	memcpy(j->obuf + j->obuf_len, buf, len);
	j->obuf_len += len;

	xfree(text);

	return 0;
}

int gatoi(char *buf, int *a)
{
        char	*x[1];
        long	t;
	if(!buf) return (1);
        t=strtol(buf, x, 10);
        if (*x == buf) return (1);
        *a=t;
        return (0);
}

/*****************************************************************************/
/*
 */

int irc_parse_line(session_t *s, char *buf, int len, int fd, irc_private_t *j)
{
	int	i, c=0, ecode;
	char	*p, *q[20];

	char	*emitname;

	p=buf;
	if(!p)
		return -1;
	for (i=0; i<20; i++) q[i]=NULL;
/*
Each IRC message may consist of up to three main parts: the prefix
(optional), the command, and the command parameters (of which there
may be up to 15).  The prefix, command, and all parameters are
separated by one (or more) ASCII space character(s) (0x20).

The presence of a prefix is indicated with a single leading ASCII
colon character (':', 0x3b), which must be the first character of the
message itself.  There must be no gap (whitespace) between the colon
and the prefix.   
*/
	/* GiM: nasty hack, because, prefix is optional... */
	if (':' != *p) { q[0]=":_empty_"; c++; }
	/* dj: maybe instead of :_empty_ we give here sesesion_get(.., server) ?
	 * i don't remember rfc now ;( 
	 * G->dj: truly we shouldn't change this to anything...
	 *        this _empty_ is for our convenience
	 */

	q[c]=p;
	i=0; c++;
        /* GiM: split message into q table */
	while (i<len) {
		/* GiM: find first space */
		while (' ' != *p && i<len) { p++; i++; }
		/* GiM: omit spaces 'by one (or more) ASCII...' */
		while (' ' == *p && i<len) { p++; i++; }
		if (c<20 && i<len) { 
			q[c]=p; c++;
			p--; *p++='\0';
		}
		if ((i<len && ':' == *p) || c==100) break;
	}
	/* GiM: In fact this is probably not needed, but just in case...  */
	for (i=0; i<len; i++) if (buf[i]=='\n' || buf[i]=='\r') buf[i]='\0';

	/* debug only nasty hack ;> */
#ifdef GDEBUG
	i=0;
	while (q[i] != NULL) debug("[%s]",q[i++]);
	debug("\n");
#endif

	if (q[1] != NULL && strlen(q[1])>1) {
		if(!gatoi(q[1], &ecode)) {
			
			/* for perl */
			emitname = saprintf("irc-protocol-numeric %s", q[1]);
			if (query_emit(NULL, emitname, &s->uid, &(q[2])) == -1) { xfree(emitname); return -1; }
			xfree(emitname);
			
			c=0;
			while(irccommands[c].type != -1) {
				if (irccommands[c].type == 1 && irccommands[c].num == ecode) {
					/* I'm sending c not ecode!!!! */
					if ((*(irccommands[c].handler))(s, j, fd, c, q) == -1 ) {
						debug("[irc] parse_line() error while executing handler!\n");
					}
					/* GiM: XXX I don't expect more,
					 * then one handler on list... */
					break;
				}
				c++;
			}
#ifdef GDEBUG
			if (irccommands[c].type == -1) {
				debug("trying default handler\n");
				if ((*(irccommands[0].handler))(s, j, fd, 0, q) == -1 ) {
					debug("[irc] parse_line() error while executing handler!\n");
				}

			}
#endif
		} else { 
			c=0;
			while(irccommands[c].type != -1) {
				if (irccommands[c].type == 0 && 
						!xstrcmp(irccommands[c].comm, q[1])) {
					/* dj: instead of  ecode,    c; */
					if ((*(irccommands[c].handler))(s, j, fd, c, q) == -1 ) {
						debug("[irc] parse_line() error while executing handler!\n");
					}
					break;
				}
				c++;
			}
		}
	}

	return 0;
}

char *irc_make_banmask(session_t *session, const char *nick, const char *ident, const char *hostname) 
{
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
			host[strlen(host)-1] = 0;
			
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
	/* dj: better ban possibilities! */
 	xfree(host);
	return temp;
#undef getit
}

int irc_parse_identhost(char *identhost, char **ident, char **host) 
{
	char	*tmp = strchr(identhost, '@');

	xfree(*ident);
	xfree(*host);

	if (tmp) *tmp = '\0';
	*ident = xstrdup(identhost);
	*host  = tmp?xstrdup(tmp+1):NULL;
	if (tmp) *tmp = '@';

	/* debug("$ %s@%s \n", *ident, *host); */
	return 0;
}

IRC_COMMAND(irc_c_init)
{
	int		i, k;
	char		*t;
	char		*__session = xstrdup(session_uid_get(s));
	connector_t	*temp;
	switch (irccommands[ecode].num)
	{
		case 1:
			temp = j->conntmplist->data;
			query_emit(NULL, "protocol-connected", &__session);
			session_connected_set(s, 1);
			session_unidle(s);
			t = xstrchr(param[3], '!');
			if (t)  j->host_ident=xstrdup(++t); 
			else j->host_ident=NULL;
			debug("\nspoko miejscówka ziom!...[%s:%s]\n", j->nick, j->host_ident);
			j->connecting = 0;

			SOP(_005_PREFIX) = xstrdup("(ov)@+");
			SOP(_005_CHANTYPES) = xstrdup("#!");
			SOP(_005_MODES) = xstrdup("3");
			/* ~~ kinda optimal: */
			SOP(_005_CHANMODES) = xstrdup("b,k,l,imnpsta");
			/* http://www.irc.org/tech_docs/005.html
			CHANMODES= b,  k,l, imnpstr (ircu)
			CHANMODES= b,  k,l, iLmMnOprRst (Bahamut)
			CHANMODES= beI,k,l, imnpstaqr (IRCNet)
			CHANMODES= beI,k,l, imnpsta (Hybrid)
			*/
			break;
		case 2:
		case 3:
			break;
		case 4:
			SOP(USERMODES) = xstrdup(param[5]);
			SOP(CHANMODES) = xstrdup(param[6]);
			break;
		case 5:
			/* rfc says there can be 15 params */
			/* yes I know it should be i<15 */
			for (i=3; i<16; i++) {
				if (!param[i]) break;
				for (k=0; k<SERVOPTS; k++)
				{
					if (sopt_keys[k] == NULL)
						continue;
					if (xstrncmp(param[i], sopt_keys[k],
							xstrlen(sopt_keys[k])))
						continue;
					xfree(SOP(k));
					SOP(k) = xstrdup(xstrchr(param[i],
								'=')+1);
					if (strlen(SOP(k))==0) {
						xfree(SOP(k));
						SOP(k) = NULL;
					}
				}
			}

			irc_autorejoin(s, IRC_REJOIN_CONNECT, NULL);

			break;
		default:
			break;
	}

	xfree(__session);
	return 0;
}

IRC_COMMAND(irc_c_error)
{
	int		i;
	char		*t = NULL, *dest = NULL, *coloured = NULL, *bang;
	time_t		try;
	window_t	*w;
	char		*altnick;
	channel_t	*chanp;

#define IOK2(x) param[x]?OMITCOLON(param[x]):""

	if (!xstrcmp("ERROR", irccommands[ecode].comm)) {
		/* here error @ CONNECT 
		 *   21:03:35 [:_empty_][ERROR][:Trying to reconnect too fast.]
		 * no I:line's etc.. everything that disconnects fd
		 */
		print_window(NULL, s, 0,
				"IRC_ERR_FIRSTSECOND",
				session_name(s), irccommands[ecode].comm, IOK2(2));
		if (j->connecting)
			irc_handle_disconnect(s, param[0], EKG_DISCONNECT_NETWORK);
		else    debug("!j->connecting\n");
		return 0;
	}
	i = irccommands[ecode].future&0x100;
	switch (irccommands[ecode].future&0xff)
	{
		case IRC_ERR_21:
			print_window(NULL, s, 0,
					i?"IRC_RPL_SECONDFIRST":"IRC_ERR_SECONDFIRST", 
					session_name(s), param[3], IOK2(4));
			return (0);
		case IRC_ERR_12:
			print_window(NULL, s, 0,
					i?"IRC_RPL_FIRSTSECOND":"IRC_ERR_FIRSTSECOND", 
					session_name(s), param[3], IOK2(4));
			return (0);
		case IRC_ERR_ONLY1:
			print_window(NULL, s, 0,
					i?"IRC_RPL_JUSTONE":"IRC_ERR_JUSTONE", 
					session_name(s), IOK2(3));
			return (0);
#define IOK(x) param[x]?param[x]:""
		case IRC_ERR_NEW:
			print_window(NULL, s, 0, i?"IRC_RPL_NEWONE":
					"IRC_ERR_NEWONE", session_name(s), param[1],
					IOK(3), IOK(4), IOK(5), IOK(6));
			return (0);
		case IRC_ERR_IGNO:
			return(0);
		default:
			break;
	}
	i = irccommands[ecode].num;
	if (param[3]) {
		t = saprintf("%s%s", IRC4, param[3]);
		w = window_find_s(s, t);
		dest = w?t:NULL;
	}
	switch (i) {
		case 433:
			print_window(NULL, s, 0, "IRC_ERR_SECONDFIRST", 
					session_name(s), param[3], IOK2(4));
			if (j->connecting) {
				altnick = (char *) session_get(s, "alt_nick");
				/* G->dj: why this !xstrcmp ? */
									
				if (altnick && !xstrcmp(param[3], session_get(s, "nickname")) && xstrcmp(param[3], altnick)) {
									
					print_window(NULL, s, 0, "IRC_TRYNICK",
							session_name(s), altnick);
					xfree(j->nick);
					j->nick = xstrdup(altnick);
					irc_write(j, "NICK %s\r\n", j->nick);
				}
			}
			break;
		case 404:
			print_window(dest, s, 0, "IRC_RPL_CANTSEND", session_name(s), param[3]);
			break;
		case 301:
			if (!session_int_get(s, "DISPLAY_AWAY_NOTIFICATION")) 
				break;
			dest = t;
			// NO BREAK!;
		/* topic */
		case 331:
		case 332:
			if ((chanp = irc_find_channel(j->channels, param[3])))
			{
				xfree(chanp->topic);
				chanp->topic = xstrdup(OMITCOLON(param[4]));
				coloured = irc_ircoldcolstr_to_ekgcolstr(s, 
						OMITCOLON(param[4]), 1);
				print_window(dest, s, 0, irccommands[ecode].name,
						session_name(s), param[3], coloured);
				xfree(coloured);
			}
			break;
		case 333:
			if ((chanp = irc_find_channel(j->channels, param[3])))
			{
				xfree(chanp->topicby);
				try = param[5]?atol(OMITCOLON(param[5])):0; 
				if ((bang = xstrchr(param[4], '!'))) *bang = '\0';
				chanp->topicby = xstrdup(param[4]);
				print_window(dest, s, 0, "IRC_RPL_TOPICBY",
						session_name(s), param[4], bang?bang+1:"",
						param[5]?ctime(&try):"unknown\n");
				if (bang) *bang ='!';
			}
			break;

		case 341:
			print_window(dest, s, 0, irccommands[ecode].name, session_name(s), param[3], param[4]);
			break;
		case 376:
			/* first we join */
			if (session_get(s, "AUTO_JOIN") && strlen(session_get(s, "AUTO_JOIN")))
				irc_write(j, "JOIN %s\r\n", session_get(s, "AUTO_JOIN"));
			/* G->dj: someday, someday ;-) 
			 */
		case 372:
		case 375:
			if (session_int_get(s, "SHOW_MOTD") != 0) {
				coloured = irc_ircoldcolstr_to_ekgcolstr(s,
						IOK2(3), 1);
				print_window("__status", s, 0,
						irccommands[ecode].name,
						session_name(s), coloured);
				xfree(coloured);
			}
			break;
		default:
			return(-1);
	}

	xfree(t);
	return 0;
}

IRC_COMMAND(irc_c_whois)
{
	char		*t = saprintf("%s%s", IRC4, param[3]), *dest = NULL;
	char		*str, *tmp, *col[5];
	int		secs, mins, hours, days, which, i;
	time_t		timek;
	int		timek_int = (int) timek;
        window_t	*w = window_find_s(s, t);

	if (session_int_get(s, "DISPLAY_IN_CURRENT")&2)
        	dest = w?t:NULL;

	if (irccommands[ecode].num != 317) { /* idle */
		for (i=0; i<5; i++)
			col[i] = irc_ircoldcolstr_to_ekgcolstr(s,
					param[3+i]?OMITCOLON(param[3+i]):NULL,1);

		/*
		if (irccommands[ecode].future & IRC_WHOERR)
			print_window(dest, s, 0, "IRC_WHOERROR", session_name(s), col[0],  col[1]);
		else
		*/
			print_window(dest, s, 0, irccommands[ecode].name, 
					session_name(s), col[0], col[1],
					col[2], col[3], col[4]);

		for (i=0; i<5; i++)
			xfree(col[i]);

		xfree(t);
		return (0);
	}
	gatoi(IOK2(4), &secs);
	which = gatoi(IOK2(5), &timek_int);
	timek = (time_t)timek_int;

	/* GiM: Yes, I know what is modulo ;> */
	mins = secs/60;
	secs -= (mins * 60);
	hours = mins/60;
	mins -= (hours * 60);
	days = hours/24;
	hours -= (days * 24);

#define IOK3(x) (x)?(x):""
	/* GiM: No, I'm not going to do the same in polish
	 * it'd have to be more cases ;> */
	str = days?saprintf("%d %s ", days, days==1?"day":"days"):NULL;
	tmp = hours?saprintf("%s %d %s ", IOK3(str), hours, 
			hours==1?"hour":"hours"):str;
	if (hours) xfree(str); str=tmp;
	tmp = mins?saprintf("%s %d %s ", IOK3(str), mins,
			mins==1?"minute":"minutes"):str;
	if (mins) xfree(str); str=tmp;
	tmp = secs?saprintf("%s %d %s ", IOK3(str), secs,
			secs==1?"second":"seconds"):str;
	if (secs) xfree(str); str=tmp;

	if (!str) str = xstrdup("Incredible, no idle!");

	tmp = xstrdup(ctime(&timek));
	if (tmp && tmp[xstrlen(tmp)-1] == '\n') tmp[xstrlen(tmp)-1]='\0';

	print_window(dest, s, 0, irccommands[ecode].name, 
			session_name(s), IOK(3), str, 
			which?"N/A":tmp);
	xfree(t);
	xfree(str);
	xfree(tmp);
	return 0;
}

int mode_act = 0;
IRC_COMMAND(irc_c_list)
{
#define PRINT_WINDOW if (!chan || !chan->syncmode) print_window
	char		*dest, *t = NULL;
	int		ltype = irccommands[ecode].future;

	int		endlist = ltype & IRC_LISTEND;
	char		*realname;
	char		*coloured = NULL;

	window_t	*w        = NULL;
	people_t	*osoba    = NULL;
	channel_t	*chan     = NULL;
	list_t		*tlist    = NULL;

	if (endlist) ltype -= IRC_LISTEND;

	if (ltype == IRC_LISTWHO || ltype == IRC_LISTCHA || ltype == IRC_LISTSTA)
		t = NULL;
	else
		t = saprintf("%s%s", IRC4, IOK(3));

	w    = window_find_s(s, t);
	dest = w?t:NULL;

	if (ltype == IRC_LISTWHO || ltype == IRC_LISTBAN) {
 		chan = irc_find_channel(j->channels, IOK(3));
		/* debug("!!!> %s %08X %d\n", IOK(3), chan, ltype); */
	}

	if (!mode_act && ltype != IRC_LISTCHA) 
		PRINT_WINDOW(dest, s, 0, "RPL_LISTSTART", session_name(s));

	if (endlist) {
			if (!mode_act)
				PRINT_WINDOW(dest, s, 0, "RPL_EMPTYLIST", session_name(s), IOK(3)); 

			if (ltype == IRC_LISTSTA) {
				print_window(dest, s, 0, "RPL_STATSEND", session_name(s), IOK2(4), IOK2(3)); 
			} else if (ltype == IRC_LISTCHA) {
				print_window(dest, s, 0, "RPL_ENDOFLIST", session_name(s), IOK2(3));
			} else {
				PRINT_WINDOW(dest, s, 0, "RPL_ENDOFLIST", session_name(s), IOK2(4));
			}
			if (chan) {
				if (chan->syncmode > 0)  {
					chan->syncmode--;
					if (chan->syncmode == 0) {
						struct timeval tv;
						gettimeofday(&tv, NULL);
						tv.tv_usec+=(1000000-chan->syncstart.tv_usec);
						if (tv.tv_usec>1000000)
							tv.tv_sec++, tv.tv_usec-=1000000;
						tv.tv_sec-=chan->syncstart.tv_sec;

						print_window(dest, s, 0, "IRC_CHANNEL_SYNCED", session_name(s), chan->name+4, itoa(tv.tv_sec), itoa(tv.tv_usec));
					}
				}
			}
			mode_act = 0; 
	} else {
		if (irccommands[ecode].num != 321)
			mode_act++;
		switch (ltype) {
			/* TODO: poprawic te 2 pierwsze... */
			case (IRC_LISTSTA):
				print_window(dest, s, 0, irccommands[ecode].name, session_name(s), itoa(mode_act), IOK2(3), IOK2(4), IOK(5), IOK(6), IOK(7), IOK(8));
				break;
			case (IRC_LISTWHO): 
				osoba    = irc_find_person(j->people, IOK(7));
				realname = xstrchr(IOK2(9), ' ');
				PRINT_WINDOW(dest, s, 0, irccommands[ecode].name, session_name(s), itoa(mode_act), IOK2(3), IOK2(4), IOK(5), IOK(6), IOK(7), IOK(8), realname);
				if (osoba) {
					xfree(osoba->host);
					osoba->host = xstrdup(IOK(5));
					xfree(osoba->ident);
					osoba->ident= xstrdup(IOK(4));
					xfree(osoba->realname);
					osoba->realname = xstrdup(realname);
				}
				break;
			/*
			case (IRC_LISTCHA):
				// TODO: /join #number (?)
 				tlist = ...
			case (IRC_LISTINV):
				tlist = ...
			case (IRC_LISTEXC):
				tlist = ...
*/
			case (IRC_LISTBAN):
				if (!tlist) 
					tlist = &(chan->banlist);
				if (chan) {
					if (mode_act == 1 && *tlist) {
						debug("[IRC_LIST] Delete list 0x%x\n", *tlist);
						list_destroy(*tlist, 0);
						*tlist = NULL;
					}
					/*debug("[IRC_LIST] Add to list (id=%d; co=%s) 0x%x\n", mode_act, IOK2(4), tlist);*/
					list_add(tlist, xstrdup(IOK2(4)) , 0);
				}
			default:
				if (param[5] && *param[5] == ':') {
					coloured = irc_ircoldcolstr_to_ekgcolstr(s, param[5]+1, 1);
					PRINT_WINDOW(dest, s, 0, irccommands[ecode].name, session_name(s), IOK(3), IOK2(4), coloured, itoa(mode_act));
				} else {
					PRINT_WINDOW(dest, s, 0, irccommands[ecode].name, session_name(s), IOK2(3), IOK2(4), IOK2(5), itoa(mode_act));
				}
				xfree(coloured);
				break;
		}
	}

	xfree(t);
	return 0;
#undef PRINT_WINDOW
}

#undef IOK
#undef IOK2

/* p[0] - PING
 * p[1] - (:server|:something)		;>
 */
IRC_COMMAND(irc_c_ping)
{
	irc_write(j, "PONG %s\r\n", param[2]);
	if (session_int_get(s, "DISPLAY_PONG"))
		print_window("__status", s, 0, "IRC_PINGPONG", session_name(s), OMITCOLON(param[2]));
	return 0;
}

/* p[0] - :nick!ident@host
 * p[1] - NICK
 * p[2] - :newnick
 */
IRC_COMMAND(irc_c_nick)
{
	char		*t, *temp;
	int		nickdisp = session_int_get(s, "DISPLAY_NICKCHANGE");
	people_t	*per;
	people_chan_t	*ch;
	list_t		l;
	window_t	*w;

	if ((t = xstrchr(param[0], '!'))) *t ='\0';
	/* debug("irc_nick> %s %s\n", j->nick, param[0]+1); */
	irc_nick_change(s, j, param[0]+1, OMITCOLON(param[2]));
	if (!xstrcmp(j->nick, param[0]+1)) {
		print_window(window_current->target, s, 0, "IRC_YOUNEWNICK", 
				session_name(s), t?t+1:"", OMITCOLON(param[2]));
		
		xfree(j->nick);
		j->nick = xstrdup(OMITCOLON(param[2]));	
	} else {
		(per = irc_find_person(j->people, OMITCOLON(param[2])));
		debug("[irc]_c_nick %08X %s\n", per, param[0]+1);
		if (nickdisp || !per)
			print_window(nickdisp==2?window_current->target:"__status",
					s, 0, "IRC_NEWNICK", session_name(s),
					param[0]+1, t?t+1:"", OMITCOLON(param[2]));
		else if (per) {
			for (l = per->channels; l; l=l->next)
			{
				ch = (people_chan_t *)l->data;
				print_window(ch->chanp->name,
						s, 0, "IRC_NEWNICK", session_name(s),
						param[0]+1, t?t+1:"", OMITCOLON(param[2]));
			}
		}

		temp = saprintf("%s%s",IRC4, param[0]+1);
		if ((w = window_find_s(s, temp))) {
			xfree(w->target);
			w->target = saprintf("%s%s", IRC4, OMITCOLON(param[2]));
			print_window(w->target,
					s, 0, "IRC_NEWNICK", session_name(s),
					param[0]+1, t?t+1:"", OMITCOLON(param[2]));
		}
		xfree(temp);
	}
	if (t) *t='!';
	return 0;
}

/* p[0] - :nick!ident@host
 * p[1] - PRIVMSG | NOTICE
 * p[2] - destination (channel|nick)
 * p[3] - :message
 */
IRC_COMMAND(irc_c_msg)
{
	char		*t, *dest, *me, *form=NULL, *seq=NULL, *format;
	char		*head, *xosd_nick, *xosd_chan, **rcpts = NULL;
	char		*ctcpstripped, *coloured, *pubtous, tous, prefix[2];
	int		class, ekgbeep= EKG_NO_BEEP;
	int		mw = 666, prv=0;
	window_t	*w = NULL;
	people_t	*person;
	people_chan_t	*perchn = NULL;
	time_t		sent;
	int		secure = 0, xosd_to_us = 0, xosd_is_priv = 0;
	char		*ignore_nick = NULL;

	prv = !xstrcasecmp(param[1], "privmsg");
	if (!prv && xstrcasecmp(param[1], "notice"))
			return 0;

	mw = session_int_get(s, "make_window");
	
	ctcpstripped = ctcp_parser(s, prv, param[0], param[2], OMITCOLON(param[3]));

	if ((t = xstrchr(param[0], '!'))) *t='\0';
	me = xstrdup(t?t+1:"");
	xosd_nick = OMITCOLON(param[0]);
	xosd_chan = param[2];

	/* probably message from server ... */
	if (j->connecting && !prv) {
		/* (!xstrcmp(":_empty_", param[0]) || !xstrcmp("AUTH", param[2])) */
		class = (mw&16)?EKG_MSGCLASS_CHAT:EKG_MSGCLASS_MESSAGE; 
		dest = saprintf(param[2]);
		format = xstrdup("irc_not_f_server");
		/* WTF ? WHY this -1 ? insane ?
		 * dj->G: because of it: (param[0]+1) 
		 * G->dj: this is really shitty hack/workaround
		 *        never do things like that, better change those
		 *        param[0]+1 to OMITCOLON, and btw: I think in 
		 *        IRC_COMMANDs we shouldn't change param-s table
		 *
		param[0] = (char *) session_get(s, "server")-1; */
		xosd_to_us = 1;
		/*param[0] = saprintf(":%s",session_get(s, "server"));*/
	/* private message ... */
	} else if (!xstrcmp(j->nick, param[2])) {
		/* dj: if he's not on the list we should add him */
		/* G->dj: okey, but must be done in other way imho
		 * this 'param[0]' as a channel doesn't like nice to me...
		person = irc_find_person(j->people, param[0]+1);
		if (!person) 
			person = irc_add_person(s, j, param[0]+1, NULL);
		if (person && t) 
			irc_parse_identhost(t+1, &(person->ident), &(person->host));
		*/
		class = (mw&2)?EKG_MSGCLASS_CHAT:EKG_MSGCLASS_MESSAGE; 
		dest = saprintf("%s%s", IRC4, OMITCOLON(param[0]));
		format = xstrdup(prv?"irc_msg_f_some":"irc_not_f_some");
		ekgbeep = EKG_TRY_BEEP;
		xosd_to_us = xosd_is_priv = 1;
	/* message on channel ... */
	} else {
		class = EKG_MSGCLASS_CHAT;
		// class = (mw&1)?EKG_MSGCLASS_CHAT:EKG_MSGCLASS_MESSAGE;
		dest = saprintf("%s%s", IRC4, param[2]);
		if (ctcpstripped && (pubtous = strcasestr(ctcpstripped, j->nick))) {
			tous = pubtous[xstrlen(j->nick)];
			if (!isalnum(tous) && !isalpha_pl(tous))
			{
				ekgbeep = EKG_TRY_BEEP;
				xosd_to_us = 1;
			} 
		}
		w = window_find_s(s, dest);

		format = saprintf("irc_%s_f_chan%s%s", prv?"msg":"not",
					(!w)?"":"_n", ekgbeep?"h":"");

		if ((person = irc_find_person(j->people, param[0]+1)))
		{
			/* G->dj: I'm not sure if this what I've added
			 *        will still do the same you wanted */
			if (t && !(person->ident) && !(person->host))
				irc_parse_identhost(t+1, &(person->ident), &(person->host));

			perchn = irc_find_person_chan(person->channels, dest);
			debug("<person->channels: %08X %s %08X>\n", person->channels, dest, perchn);
		}

	}

	if (ctcpstripped) {
		if (xosd_is_priv)
			query_emit(NULL, "message-decrypt", &(s->uid), &dest, &ctcpstripped, &secure , NULL);
		else
			query_emit(NULL, "message-decrypt", &dest, &(s->uid), &ctcpstripped, &secure , NULL);
	
		/* TODO 'secure' var checking, but still don't know how to react to it 
		 * CHANNEL DONE.
		 */
		coloured = irc_ircoldcolstr_to_ekgcolstr(s, ctcpstripped,1);
		debug("<%c%s/%s> %s\n", perchn?*(perchn->sign):' ', param[0]+1, param[2], OMITCOLON(param[3]));
		prefix[1] = '\0';
		prefix[0] = perchn?*(perchn->sign):' ';
		if (!session_int_get(s, "SHOW_NICKMODE_EMPTY") && *prefix==' ')
			*prefix='\0';
		head = format_string(format_find(format), session_name(s),
				prefix, param[0]+1, me, param[2], coloured, "Y ");
		xfree(coloured);
		coloured = irc_ircoldcolstr_to_ekgcolstr(s, ctcpstripped,0);
	/*
234707 <@dredzik> GiM, string nadawca, string wiadomo¶æ, bool
234707 wiadomo¶æ_do_ciebie, bool kana³_czy_priv, string
234707 je¿eli_kana³_to_nazwa_kana³u
010539 <@dredzik> GiM, hm... jeszcze by siê przyda³a jedna rzecz - tak ¿eby
010539 pierwszym argumentem by³a sesja
	*/

		query_emit(NULL, "irc-protocol-message",
				&(s->uid), &xosd_nick, &coloured, 
				&xosd_to_us, &xosd_is_priv, &xosd_chan);
				/*&sender,&text,&to_us,&is_priv,&channame);*/

		xfree(ctcpstripped);
		xfree(coloured);
		xfree(me);
		me = xstrdup(session_uid_get(s));
		sent = time(NULL);
		class |= EKG_NO_THEMEBIT;

		ignore_nick = saprintf("%s%s", IRC4, OMITCOLON(param[0]));
		if (xosd_is_priv || !(ignored_check(s, ignore_nick) & IGNORE_MSG))
			query_emit(NULL, "protocol-message", &me, &dest, &rcpts, &head,
					&form, &sent, &class, &seq, &ekgbeep, &secure);
		xfree(ignore_nick);

		xfree(head);
	}	

	if (t) *t='!';
	xfree(dest);
	xfree(me);
	xfree(format);

	return 0;
}
/* p[0] - :nick!ident@host
 * p[1] - JOIN
 * p[2] - :channel
 *
 */
IRC_COMMAND(irc_c_join)
{
	char		*channel, *tmp;
	channel_t	*ischan;
	window_t	*newwin;
	people_t	*person;
	//int  __class = EKG_MSGCLASS_CHAT;
	//time_t __sent = time(NULL);
	int		me = 0;
	char		*ignore_nick;

	channel = saprintf("%s:%s", IRC3, OMITCOLON(param[2]));
	
	if ((tmp = xstrchr(param[0], '!'))) *tmp='\0';
	/* istnieje jaka¶tam szansa ¿e kto¶ zrobi nick i part i bêdzie
	 * but I have no head to this now... */
	me = !xstrcmp(j->nick, param[0]+1); /* We join ? */
	if (me) {
		newwin = window_new(channel, s, 0);
		window_switch(newwin->id);
		debug("[irc] c_join() %08X\n", newwin);
		ischan = irc_add_channel(s, j , OMITCOLON(param[2]), newwin);
	/* someone joined */
	} else {
		person = irc_add_person(s, j, param[0]+1, OMITCOLON(param[2])); 
		if (person && tmp && !(person->ident) && !(person->host))
			irc_parse_identhost(tmp+1, &(person->ident), &(person->host));
		
	}

	ignore_nick = saprintf("%s%s", IRC4, param[0]+1);
	if (!(ignored_check(s, ignore_nick) & IGNORE_NOTIFY)) {
		print_window(channel, s, 0, me ? "irc_joined_you" : "irc_joined",
				session_name(s), param[0]+1, tmp?tmp+1:"", OMITCOLON(param[2]));
		if (me)	{
			int __secure = 0;
    			char *__sid      = xstrdup(session_uid_get(s));
    			char *__uid_full = xstrdup(channel);
			char *__msg	 = xstrdup("test");

			if (query_emit(NULL, "message-encrypt", &__sid, &__uid_full, &__msg, &__secure) == 0 && __secure) 
				print_window(channel, s, 0, "irc_channel_secure", session_name(s), OMITCOLON(param[2]));
			else 	print_window(channel, s, 0, "irc_channel_unsecure", session_name(s), OMITCOLON(param[2]));
			xfree(__msg);
			xfree(__uid_full);
			xfree(__sid);
		}
	}
	if (tmp) *tmp='!';

	xfree(ignore_nick);
	xfree(channel);
	return 0;
}

/* p[0] - :nick!ident@host
 * p[1] - PART
 * p[2] - channel
 * (p[3] - :reason) - optional
 */
IRC_COMMAND(irc_c_part)
{
	char	*channel, *tmp, *coloured, *ignore_nick;
	int	me = 0;
	
	if ((tmp = xstrchr(param[0], '!'))) *tmp = '\0';
	me = !xstrcmp(j->nick, param[0]+1); /* we part ? */
	
	debug("[irc]_c_part: %s %s\n", j->nick, param[0]+1);
	/* Servers MUST be able to parse arguments in the form of
	 * a list of target, but SHOULD NOT use lists when sending
	 * PART messages to clients.
	 * 
	 * damn it I think rfc should rather say MUSTN'T instead of
	 * SHOULD NOT ;/
	 */
	if (me) 
		irc_del_channel(s, j, OMITCOLON(param[2]));
	else 
		irc_del_person_channel(s, j, param[0]+1, OMITCOLON(param[2]));
		
	channel = saprintf("%s%s", IRC4, param[2]);
		
	coloured = param[3]?xstrlen(OMITCOLON(param[3]))?
		irc_ircoldcolstr_to_ekgcolstr(s, OMITCOLON(param[3]), 1)
		:xstrdup("no reason"):xstrdup("no reason");
	/* TODO: if channel window exists do print_window, else do nothing (?)
	 * now after alt+k if user was on that channel-window, we recved info
	 * about parting on __status window, is it right ?
	 * G->dj: yep, but we can make this behaviour dependent on something
	 * e.g: on my fave: DISPLAY_IN_CURRENT :)
	 */
	ignore_nick = saprintf("%s%s", IRC4, param[0]+1);
	if (!(ignored_check(s, ignore_nick) & IGNORE_NOTIFY)) {
		print_window(channel, s, 0, (me)?"irc_left_you":"irc_left", session_name(s),
				param[0]+1, tmp?tmp+1:"", OMITCOLON(param[2]), coloured);
	}
	xfree(ignore_nick);
	
	if (tmp) *tmp='!';

	xfree(coloured);
	xfree(channel);
	
	return 0;
}

/* this is quite similiar to PART
 * p[0] - :nick!ident@host
 * p[1] - KICK
 * p[2] - channel
 * p[3] - nick
 * (p[4] - :reason) - optional
 */
IRC_COMMAND(irc_c_kick)
{
	char			*channel, *tmp, *uid, *coloured;
	char			*_session, *_nick;
	int			me = !xstrcmp(j->nick, param[3]);

	if ((tmp = xstrchr(param[0], '!'))) *tmp = '\0';
	/* we were kicked out */
	if (me)
		irc_del_channel(s, j, param[2]);
	else
		irc_del_person_channel(s, j, OMITCOLON(param[3]), param[2]);

	uid = saprintf("%s%s", IRC4, param[0]+1);

	if (tmp) *tmp='!';

	channel = saprintf("%s:%s", IRC3, param[2]);
	
	coloured = param[4]?xstrlen(OMITCOLON(param[4]))?
		irc_ircoldcolstr_to_ekgcolstr(s, OMITCOLON(param[4]), 1):
				xstrdup("no reason"):xstrdup("no reason");

	/* session, kicked_nick, kicker_nick, kicker_ident+host, chan, reason */
	print_window(channel, s, 0, me ? "irc_kicked_you" : "irc_kicked",  session_name(s), 
			OMITCOLON(param[3]), uid+4, tmp?tmp+1:"",
			param[2], coloured);
	xfree(coloured);

/*sending irc-kick event*/
	_session = xstrdup(session_uid_get(s));
	_nick = saprintf("%s%s", IRC4, OMITCOLON(param[3]));
	query_emit(NULL, "irc-kick", &_session, &_nick, &channel, &uid);
	xfree(_nick);
	xfree(_session);

	xfree(channel);
	xfree(uid);
	return 0;
}

/* p[0] - :nick!ident@ihost
 * p[1] - QUIT
 * (p[2]) - reason
 */
IRC_COMMAND(irc_c_quit)
{
	char	*tmp, *reason;
	int	dq;
	/* TODO: SPLIT MODE! */
	int	split = 0;

	if ((tmp = xstrchr(param[0], '!'))) *tmp = '\0';
	reason = param[2]?xstrlen(OMITCOLON(param[2]))?
		irc_ircoldcolstr_to_ekgcolstr(s, OMITCOLON(param[2]), 1):
		xstrdup("no reason"):xstrdup("no reason");
	
	if (split) 
		dq = 0; /* (?) */
	else
		dq = session_int_get(s, "DISPLAY_QUIT");
	
	irc_del_person(s, j, param[0]+1, tmp?tmp+1:"", reason, !dq);
	
	if (dq)
		print_window(dq==2?window_current->target:"__status",
				s, 0, (split)?"irc_split":"irc_quit",
				session_name(s), param[0]+1, tmp?tmp+1:"",
				reason);
	
	xfree(reason);
	if (tmp) *tmp='!';
	
	return 0;
}


IRC_COMMAND(irc_c_namerpl)
{
	if (!param[3]) return -1;
	/* rfc2812 */
	if (*param[3]!='*' && *param[3]!='=' && *param[3]!='@')	{
		debug("[irc] c_namerpl() kindda shitty ;/\n");
		return -1;
	}
	if (!param[5]) {
		debug("[irc] c_namerpl() even more shitty!\n");
		return -1;
	}
	irc_add_people (s, j, OMITCOLON(param[5]), param[4]);
	return 0;
}

IRC_COMMAND(irc_c_topic)
{
	window_t	*w;
	char		*t, *dest=NULL;
	char		*coloured;
	channel_t	*chanp = NULL;

	t = saprintf("%s%s", IRC4, param[2]);
	w = window_find_s(s, t);
	chanp = irc_find_channel(j->channels, param[2]);
	dest = w?w->target:NULL;
	xfree(t);
	if ((t = xstrchr(param[0], '!')))
		*t = '\0';
	xfree(chanp->topic);
	xfree(chanp->topicby);
	if (xstrlen(OMITCOLON(param[3]))) {
		chanp->topic   = xstrdup(OMITCOLON(param[3]));
		chanp->topicby = xstrdup(OMITCOLON(param[0]));
		coloured = irc_ircoldcolstr_to_ekgcolstr(s, OMITCOLON(param[3]), 1);
		print_window(dest, s, 0, "IRC_TOPIC_CHANGE", session_name(s),
				param[0]+1, t?t+1:"", param[2], coloured);
		xfree(coloured);
	} else {
		chanp->topic   = xstrdup("No topic set!\n");
		chanp->topicby = xstrdup(OMITCOLON(param[0]));
		print_window(dest, s, 0, "IRC_TOPIC_UNSET", session_name(s),
				param[0]+1, t?t+1:"", param[2]);
	}
	if (t) *t='!';
	return 0;
}

#ifdef GDEBUG 
#ifndef MARLENE
#error She's all I really care about. You shouldn't play with my GDEBUG!
#endif
#endif

/* p[0] - :nick!ident@ihost
 * p[1] - INVITE
 * p[2] - our nickname
 * p[3] - :channel
 */
/* TODO: add the person, that invites us, to list ? */
IRC_COMMAND(irc_c_invite)
{
	char	*tmp;
	/*
	char *nick = NULL;
	char *dest = NULL; 
	*/

	if ((tmp = xstrchr(param[0], '!')))
		*tmp = '\0';

	print_window(window_current->target, s, 0, "IRC_INVITE",
			session_name(s), param[0]+1, tmp?tmp+1:"",
			param[2], OMITCOLON(param[3]));

	if (session_int_get(s, "AUTO_JOIN_CHANS_ON_INVITE") == 1)
		irc_write(j, "JOIN %s\r\n", OMITCOLON(param[3]));

	if (tmp) *tmp = '!';

	return 0;
}

IRC_COMMAND(irc_c_mode)
{
	int		i, k, len, val=0, act=1, is324=irccommands[ecode].num==324;
	char		*t, *bang, *add, **pars, *channame, *mode_abc, *mode_c;
	people_t	*per;
	people_chan_t	*ch;
	channel_t	*chan;
	userlist_t	*ul;
	window_t	*w;
	string_t	moderpl;

	/* MODE <channel|nick> <mode> <modeparams>
	 * <nick> <chan> <mode> <modeparams>
	 */
	/* GiM: FIXME TODO [this shouldn't be xstrcasecmp! user mode */
	if (is324) {
		param = &(param[1]);
	} else if (!xstrcasecmp(param[2], j->nick)) {
		print_window(window_current->target, s, 0, 
				"IRC_MODE", session_name(s), 
				param[0]+1, OMITCOLON(param[3]));
		return 0;
	/* channel mode */
	}

	len = (xstrlen(SOP(_005_PREFIX))>>1);
	add = xmalloc(len * sizeof(char));
	for (i=0; i<len; i++) add[i] = SOP(_005_PREFIX)[i+1];
	if (len) add[--len] = '\0';

	mode_abc = xstrdup(SOP(_005_CHANMODES));
	if ( (mode_c = xstrchr(mode_abc, ',')) && ++mode_c) 
		if ( (mode_c = xstrchr(mode_c, ',')) && ++mode_c)
			if ((xstrchr(mode_c, ',')))
				*xstrchr(mode_c, ',')='\0';

	t=param[3];
	for (i=0,k=4; i<xstrlen(param[3]) && xstrlen(param[k]); i++, t++) {
		if (*t=='+' || *t=='-') {
			act=*t-'-';
			continue;
		}

		/* Modes in PREFIX are not listed but could be considered type B. */
		if ((bang=xstrchr(add, *t))) {
			/* 23:26:o2 CET 2oo5-22-o1 yet another ivil hack */
			if (xstrchr(param[k], ' '))
				*xstrchr(param[k], ' ') = '\0';

			per = irc_find_person(j->people, param[k]);
			if (!per) goto notreallyok;
			ch = irc_find_person_chan(per->channels, param[2]); 
			if (!ch) goto notreallyok;
			/* GiM: ivil hack ;) */
			val = 1<<(len-(bang-add)-1);
			if (act) ch->mode |= val; else ch->mode-=val;
			ul = userlist_find_u(&(ch->chanp->window->userlist), param[k]);
			if (!ul) goto notreallyok;

			irc_nick_prefix(j, ch, irc_color_in_contacts(add, 
						ch->mode, ul));
		} 
notreallyok:
		if (xstrchr(add, *t)) k++;
		else if (xstrchr(mode_abc, *t) && (act || !xstrchr(mode_c, *t))) k++;
				
		if (!param[k]) break;
	}

	channame = saprintf("%s%s", IRC4, param[2]);
	w = window_find_s(s, channame);
	bang = xstrchr(param[0], '!');
	if (bang) *bang='\0';
	moderpl =  string_init("");
	pars=&(param[3]);

	while (*pars) {
		string_append(moderpl, *pars++);
		if (*pars)
			string_append_c(moderpl, ' ');
	}
	if (!is324) {
		print_window(w?w->target:NULL, s, 0, "IRC_MODE_CHAN_NEW", session_name(s),
				param[0]+1, bang?bang+1:"", param[2], moderpl->str);
/*		if (moderpl->str[1] == 'b')
 *			irc_write(j, "MODE %s +%c\r\n",  param[2], moderpl->str[1]);
 */
	} else {
		print_window(w?w->target:NULL, s, 0, "IRC_MODE_CHAN", session_name(s),
				param[2], moderpl->str);

		if ((chan = irc_find_channel(j->channels, param[2]))) {
			xfree(chan->mode_str);
			chan->mode_str = xstrdup(moderpl->str);
		}
	}
	if (bang) *bang='!';
	string_free(moderpl, 1);

	xfree(add);
	xfree(channame);
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
