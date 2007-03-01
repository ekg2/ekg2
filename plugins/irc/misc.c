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
#include <ekg/win32.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#ifndef NO_POSIX_SYSTEM
#include <arpa/inet.h>
#endif

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include <ekg/queries.h>

#define GDEBUG
#define MARLENE

#include "irc.h"
#include "misc.h"
#include "people.h"
#include "input.h"
#include "autoacts.h"

char *sopt_keys[SERVOPTS] = { NULL, NULL, "PREFIX", "CHANTYPES", "CHANMODES", "MODES", "CHANLIMIT", "NICKLEN" };

#define OMITCOLON(x) ((*x)==':'?(x+1):(x))

/* cos co blabla, zwraca liczbe pochlonietych znakow przez '*' XXX */
static int do_sample_wildcard_match(const char *str, const char *matchstr, const char stopon) {
	int i;

	return xstrlen(matchstr);

	for (i = 0; (str[i] && str[i] != stopon); i++) {
	}
	debug_error("XXX do_sample_wildcard_match() retval: %d\n", i);

	return i;
}

static int irc_access_parse(session_t *s, channel_t *chan, people_t *p, int flags) {
	list_t l;

	if (!s || !chan || !p)
		return -1;

#define dchar(x) debug("%c", x);

	for (l = s->userlist; l; l = l->next) {
		userlist_t *u = l->data;
		ekg_resource_t *r;
		list_t m;

		int i, j;

		if (!p->ident || !p->host) continue;

		for (m = u->resources; m; m = m->next) {
			r = m->data;

			if (r->private == p) {
				string_t s = string_init(r->descr); xfree(r->descr);
				
				string_append_c(s, ',');
				string_append(s, chan->name+4);
				r->descr = string_free(s, 0);
				goto next3;
			}
		}

		if (xstrncmp(u->uid, "irc:", 4)) continue;	/* skip irc: */
/* parse nick! */		
		for (i = 4, j = 0; (u->uid[i] != '\0' && u->uid[i] != '!'); i++, j++) {
			dchar(u->uid[i]);

			if (u->uid[i] != p->ident[j]) {
				if (u->uid[i] == '*') j += do_sample_wildcard_match(&u->uid[i+1], &p->ident[j], '!');
				else if (u->uid[i] == '?') continue;
				else goto next;
			}
		} if (!u->uid[i]) goto next;
		dchar('!');
		i++;

/* parse ident@ */
		for (j = 4 /* skip irc: */; (u->uid[i] != '\0' && u->uid[i] != '@'); i++, j++) {
			dchar(u->uid[i]);

			if (u->uid[i] != p->nick[j]) {
				if (u->uid[i] == '*') j += do_sample_wildcard_match(&u->uid[i+1], &p->ident[j], '@');
				else if (u->uid[i] == '?') continue;
				else goto next;
			}
		} if (!u->uid[i]) goto next;
		dchar('@');
		i++;

/* parse host: */
		for (j = 0; (u->uid[i] != '\0' && u->uid[i] != ':'); i++, j++) {
			dchar(u->uid[i]);

			if (u->uid[i] != p->host[j]) {
				if (u->uid[i] == '*') j += do_sample_wildcard_match(&u->uid[i+1], &p->ident[j], ':');
				else if (u->uid[i] == '?') continue;
				else goto next;
			}
		} if (!u->uid[i]) goto next;
		dchar('\n');
		i++;

		debug_error("irc_access_parse() %s!%s@%s MATCH with %s\n", p->ident, p->nick+4, p->host, u->uid+4);

/* let's rock with channels */
		{
			char **arr = array_make(&u->uid[i], ",", 0, 1, 0);

			int ismatch = 0;


			for (i=0; arr[i]; i++) {
				int k;
				debug_error("CHAN%d: %s: ", i, arr[i]);

				for (j = 0, k = 4 /* skip irc: */; arr[i][j]; j++, k++) {
					if (arr[i][j] != chan->name[k]) {
						if (arr[i][j] == '*') k += do_sample_wildcard_match(&arr[i][j], &chan->name[k], '\0');
						else if (arr[i][j] == '?') continue;
						else goto next2;
					}
				}
				if (chan->name[k] != '\0') 
					goto next2;

				ismatch = 1;
				debug_error("MATCH\n");
				break;
next2:
				debug_error("NOT MATCH\n");
				continue;
					
			}
			array_free(arr);

			if (!ismatch) continue;
		}

		r = userlist_resource_add(u, p->nick, 0);

		r->status	= xstrdup(EKG_STATUS_AVAIL);
		r->descr	= xstrdup(chan->name+4);
		r->private	= p;

		/* tutaj ladnie by wygladalo jakbysmy wywolali protocol-status.. ale jednak to jest b. kiepski pomysl */
		if (xstrcmp(u->status, EKG_STATUS_AVAIL)) {
			xfree(u->status);
			xfree(u->descr);
			u->status 	= xstrdup(EKG_STATUS_AVAIL);
			u->descr	= xstrdup("description... ?");
			query_emit_id(NULL, USERLIST_CHANGED, &s, &(u->uid));
		}

		debug_error("USER: 0x%x PERMISION GRANTED ON CHAN: 0x%x\n", u, chan);
		continue;

next:
		dchar('\n');
		debug_error("irc_access_parse() %s!%s@%s NOT MATCH with %s\n", p->ident, p->nick+4, p->host, u->uid+4);
next3:
		continue;
	}
#undef dchar

	return 0;
}


static int gatoi(char *buf, int *a) {
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

int irc_parse_line(session_t *s, char *buf, int fd)
{
	irc_private_t *j = s->priv;
	int	i, c=0, ecode;
	char	*p, *q[20];

	int	len = xstrlen(buf);

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

	if (xstrlen(q[1]) > 1) {
		if(!gatoi(q[1], &ecode)) {
			/* for scripts */
			char *emitname = saprintf(("irc-protocol-numeric %s"), q[1]);
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

static int irc_parse_identhost(char *identhost, char **ident, char **host)  {
	char	*tmp;

	xfree(*ident);
	xfree(*host);
	if ((tmp = xstrchr(identhost, '@'))) {
		*ident = xstrndup(identhost, tmp-identhost);
		*host  = xstrdup(tmp+1);
	} else {
		*ident = xstrdup(identhost);
		*host = NULL;
	}
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
			query_emit_id(NULL, PROTOCOL_CONNECTED, &__session);
			session_connected_set(s, 1);
			s->last_conn = time(NULL);
			session_unidle(s);
			t = xstrchr(param[3], '!');
			if (t)  j->host_ident=xstrdup(++t); 
			else j->host_ident=NULL;
			debug("\nspoko miejscówka ziom!...[%s:%s]\n", j->nick, j->host_ident);
			j->connecting = 0;
			j->autoreconnecting = 0;

			SOP(_005_PREFIX) = xstrdup("(ov)@+");
			SOP(_005_CHANTYPES) = xstrdup("#!");
			SOP(_005_MODES) = xstrdup("3");
			SOP(_005_NICKLEN) = xstrdup("9");
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
					if (xstrncmp(param[i], sopt_keys[k], xstrlen(sopt_keys[k])))
						continue;
					xfree(SOP(k));
					SOP(k) = xstrdup(xstrchr(param[i],
								'=')+1);
					if (xstrlen(SOP(k))==0) {
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

	if (irccommands[ecode].future == 0 && !xstrcmp("ERROR", irccommands[ecode].comm)) {
		/* here error @ CONNECT 
		 *   21:03:35 [:_empty_][ERROR][:Trying to reconnect too fast.]
		 * no I:line's etc.. everything that disconnects fd
		 */
/*		print_window(NULL, s, 0, "IRC_ERR_FIRSTSECOND", session_name(s), irccommands[ecode].comm, IOK2(2)); */
		if (j->connecting)
			irc_handle_disconnect(s, IOK2(2), EKG_DISCONNECT_FAILURE); 
		else    debug("!j->connecting\n");
		return -1;
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
					watch_write(j->send_watch, "NICK %s\r\n", j->nick);
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
			/* zero, identify with nickserv */
			if (xstrlen(session_get(s, "identify"))) {
				/* temporary */
				watch_write(j->send_watch, "PRIVMSG nickserv :IDENTIFY %s\n", session_get(s, "identify"));
				/* XXX, bedzie:
				 * 	session_get(s, "identify") 
				 * 		<nick_ns> <host_ns *weryfikacja zeby nikt nie spoofowac*> "<NICK1 HASLO>" "<NICK2 HASLO>" "[GLOWNE HASLO]"
				 *
				 * 	array_make() splitowane po spacjach. " " traktuj jako jeden parametr
				 * 		p[2...x] jesli ma spacje sprawdz czy nicki do spacji sie zgadzaja... jesli tak uzyj haslo.
				 * 				jesli nie, uzywaj haslo.
				 *
				 * 	i dopiero robmy IDENTIFY jesli dostaniem request od serwisow !!!
				 */
			}

			/* first we join */
			if (xstrlen(session_get(s, "AUTO_JOIN")))
				watch_write(j->send_watch, "JOIN %s\r\n", session_get(s, "AUTO_JOIN"));
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

					if (chan && chan->syncmode)
						irc_access_parse(s, chan, osoba, 0);
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
	watch_write(j->send_watch, "PONG %s\r\n", param[2]);
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
 * b
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
		dest = xstrdup(param[2]);
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
		if ((pubtous = xstrcasestr(ctcpstripped, j->nick))) {
			tous = pubtous[xstrlen(j->nick)];
			if (!isalnum(tous) && !isalpha_pl(tous))
				if (pubtous == ctcpstripped || (!isalnum(*(pubtous-1)) && !isalpha_pl(*(pubtous-1))))
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
		int isour = 0;
  		if (xosd_is_priv) /* @ wrong place */
			query_emit_id(NULL, MESSAGE_DECRYPT, &(s->uid), &dest, &ctcpstripped, &secure , NULL);
		else
			query_emit_id(NULL, MESSAGE_DECRYPT, &dest, &(s->uid), &ctcpstripped, &secure , NULL);

		/* TODO 'secure' var checking, but still don't know how to react to it (GiM)
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

isour - 0 tutaj czy wiadomosc jest od nas.

irc-protocol-message uid, nick, isour, istous, ispriv, dest.
	*/

		query_emit_id(NULL, IRC_PROTOCOL_MESSAGE,
				&(s->uid), &xosd_nick, &coloured, &isour,
				&xosd_to_us, &xosd_is_priv, &dest);
				/*&sender,&text,&to_us,&is_priv,&channame);*/

		xfree(ctcpstripped);
		xfree(coloured);
		xfree(me);
		me = xstrdup(session_uid_get(s));
		sent = time(NULL);
		class |= EKG_NO_THEMEBIT;

		ignore_nick = saprintf("%s%s", IRC4, OMITCOLON(param[0]));
		if (xosd_is_priv || !(ignored_check(s, ignore_nick) & IGNORE_MSG))
			query_emit_id(NULL, PROTOCOL_MESSAGE, &me, &dest, &rcpts, &head,
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

		if (irc_access_parse(s, irc_find_channel(j->channels, OMITCOLON(param[2])), person, 0));
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

			if (query_emit_id(NULL, MESSAGE_ENCRYPT, &__sid, &__uid_full, &__msg, &__secure) == 0 && __secure) 
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
	query_emit_id(NULL, IRC_KICK, &_session, &_nick, &channel, &uid);
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
		watch_write(j->send_watch, "JOIN %s\r\n", OMITCOLON(param[3]));

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
			if (act) ch->mode |= val; else ch->mode&=~val;
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
 *			watch_write(j->send_watch, "MODE %s +%c\r\n",  param[2], moderpl->str[1]);
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
        xfree(mode_abc);
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
