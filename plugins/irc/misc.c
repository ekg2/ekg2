/*
 *  (C) Copyright 2004 Michal 'GiM' Spadlinski <gim at skrzynka dot pl>
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
void irc_handle_write(int type, int fd, int watch, void *data)
{
	irc_private_t *j = data;
	int res;
	
	res = write(j->fd, j->obuf, j->obuf_len);

	if (res == -1) {
		debug("[irc] handle_write() failed: %s\n", strerror(errno));
		xfree(j->obuf);
		j->obuf = NULL;
		j->obuf_len = 0;
		return;
	}

	if (res == j->obuf_len) {
		debug("[irc] handle_write() output buffer empty\n");
		xfree(j->obuf);
		j->obuf = NULL;
		j->obuf_len = 0;
		return;
	}

	memmove(j->obuf, j->obuf + res, j->obuf_len - res);
	j->obuf_len -= res;

	watch_add(&irc_plugin, j->fd, WATCH_WRITE, 0, irc_handle_write, j);
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
	const char *buf;
	char *text;
	int len;
	va_list ap;

	if (!j || !format)
		return -1;

	va_start(ap, format);
	text = vsaprintf(format, ap);
	va_end(ap);

	debug("[irc]_send:  %s\n", text);

	if (!j->obuf) {
		int res;

		len = strlen(text);
		res = write(j->fd, text, len);

		if (res == len) {
			xfree(text);
			return 0;
		}

		if (res == -1) {
			xfree(text);
			return -1;
		}

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
        char *x[1];
        long t;
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
	int i, c=0, ecode;
	char *p, *q[20];
	
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
					if ((*(irccommands[c].handler))(s, j, fd, ecode, q) == -1 ) {
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

/* yes, bloody globals... ;/ */
char irc_lastline[4096] = "";
int irc_lastline_start=0;
int irc_input_parser(session_t *s, char *buf, int len)
{
	irc_private_t *j = session_private_get(s);
	char *p;
	int i, l, fd;

	if (!j) {
		debug ("[irc] input_parser() : no private!");
		return -1;
	}
	fd=j->fd;
	
	debug("[irc] input_parser() %d\n", irc_lastline_start);
	for (i=0,l=irc_lastline_start,p=buf; i<len; i++,l++)
	{
		irc_lastline[l]=buf[i];
		if ('\n' == buf[i]) {
			buf[i]='\0';
			irc_lastline[l]='\0';
			irc_parse_line(s, irc_lastline, l, fd, j);
			if (i+1<len) p=&(buf[i+1]);
			l=-1;
		} 
	}
	/* debug("irc_input_parser() %s || %s\n", p, irc_lastline); */
	/* GiM: nasty hack to concatate splited messages... */
	if (l!=-1) irc_lastline_start=l;
	return 0;
}

IRC_COMMAND(irc_c_init)
{
	int  i, k;
	char *t;
	char *__session = xstrdup(session_uid_get(s));
	switch (irccommands[ecode].num)
	{
		case 1:
			query_emit(NULL, "protocol-connected", &__session);
			xfree(__session);
			session_connected_set(s, 1);
			session_unidle(s);
			t = xstrchr(param[3], '!');
			if (t)  j->host_ident=xstrdup(++t); 
			else j->host_ident=NULL;
			debug("\nspoko miejscówka ziom!...[%s:%s]\n", j->nick, j->host_ident);
			j->connecting = 0;
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
			if (!SOP(_005_PREFIX)) 
				SOP(_005_PREFIX) = xstrdup("(ov)@+");
			if (!SOP(_005_CHANTYPES))
				SOP(_005_CHANTYPES) = xstrdup("#!");
			if (!SOP(_005_MODES))
				SOP(_005_MODES) = xstrdup("3");
			
			irc_autorejoin(s, IRC_REJOIN_CONNECT, NULL);

			break;
		default:
			break;
	}
	
	return 0;
}

IRC_COMMAND(irc_c_error)
{
	int  i;
	char *t, *dest = NULL, *coloured = NULL;
	time_t try;
	window_t *w;

#define IOK2(x) param[x]?OMITCOLON(param[x]):""
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
					"IRC_ERR_NEWONE", session_name(s), 
					IOK(3), IOK(4), IOK(5), IOK(6));
			return (0);
		case IRC_ERR_IGNO:
			return(0);
		default:
			break;
	}
	i = irccommands[ecode].num;
	t = NULL;
	if (param[3]) {
		t = saprintf("%s%s", IRC4, param[3]);
		w = window_find_s(s, t);
		dest = w?t:NULL;
	}
	switch (i) {
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
			coloured = irc_ircoldcolstr_to_ekgcolstr(s, 
					OMITCOLON(param[4]));
			print_window(dest, s, 0, irccommands[ecode].name,
					session_name(s), param[3], coloured);
			xfree(coloured);
			break;
		case 333:
			try = param[5]?atol(OMITCOLON(param[5])):0; 
			print_window(dest, s, 0, "IRC_RPL_TOPICBY",
					session_name(s), param[4], param[5]?
					ctime(&try):"unknown\n");
			break;
		case 372:
		case 375:
		case 376:
			if (!session_int_get(s, "SKIP_MOTD")) {
				coloured = irc_ircoldcolstr_to_ekgcolstr(s,
						IOK2(3));
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
	char *dest = saprintf("%s%s", IRC4, param[3]);
	char *str, *tmp, *col[5];
	int secs, mins, hours, days, which, i;
	time_t timek;
	int timek_int = (int) timek;

	if (irccommands[ecode].num != 317) { /* idle */
		for (i=0; i<5; i++)
			col[i] = irc_ircoldcolstr_to_ekgcolstr(s,
					param[3+i]?OMITCOLON(param[3+i]):NULL);
			
		print_window(dest, s, 0, irccommands[ecode].name, 
				session_name(s), col[0], col[1],
				col[2], col[3], col[4]);

		for (i=0; i<5; i++)
			xfree(col[i]);

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
	xfree(str); str=tmp;
	tmp = mins?saprintf("%s %d %s ", IOK3(str), mins,
			mins==1?"minute":"minutes"):str;
	xfree(str); str=tmp;
	tmp = secs?saprintf("%s %d %s ", IOK3(str), secs,
			secs==1?"second":"seconds"):str;
	xfree(str); str=tmp;

	tmp = xstrdup(ctime(&timek));
	if (tmp[xstrlen(tmp)-1] == '\n') tmp[xstrlen(tmp)-1]='\0';

	print_window(dest, s, 0, irccommands[ecode].name, 
			session_name(s), IOK(3), IOK3(str), 
			which?"N/A":tmp);
	xfree(str);
	xfree(tmp);
	return 0;
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
	char *t;

	if ((t = xstrchr(param[0], '!'))) *t ='\0';
	/* debug("irc_nick> %s %s\n", j->nick, param[0]+1); */
	irc_nick_change(s, j, param[0]+1, OMITCOLON(param[2]));
	if (!xstrcmp(j->nick, param[0]+1)) {
		*t='!';
		print_window(window_current->target, s, 0, "IRC_YOUNEWNICK", 
				session_name(s), OMITCOLON(param[2]));
		
		xfree(j->nick);
		j->nick = xstrdup(OMITCOLON(param[2]));	
	} else {
		print_window(window_current->target, s, 0, "IRC_NEWNICK",
				session_name(s), param[0]+1, OMITCOLON(param[2]));
		if (t) *t='!';
	}
	return 0;
}

/* p[0] - :nick!ident@host
 * p[1] - PRIVMSG | NOTICE
 * p[2] - destination (channel|nick)
 * p[3] - :message
 */
IRC_COMMAND(irc_c_msg)
{
	char *t, *dest, *me, *form=NULL, *seq=NULL, *format, **rcpts = NULL;
	char *head;
	char *ctcpstripped, *coloured;
	int class;
	int ekgbeep = EKG_NO_BEEP;
	int mw = 666,prv=0;
	window_t *w = NULL;
	people_t *person;
	people_chan_t *perchn = NULL;
	time_t sent;

	prv = !xstrcasecmp(param[1], "privmsg");
	if (!prv && xstrcasecmp(param[1], "notice"))
			return 0;

	mw = session_int_get(s, "make_window");
	if ((t = xstrchr(param[0], '!'))) *t='\0';
	/* mesg do nas */
	if (!xstrcmp(j->nick, param[2])) {
		class = (mw&2)?EKG_MSGCLASS_CHAT:EKG_MSGCLASS_MESSAGE; 
		dest = saprintf("irc:%s", param[0]+1);
		format = xstrdup(prv?"irc_msg_f_some":"irc_not_f_some");
	/* kana³ */
	} else {
		class = EKG_MSGCLASS_CHAT;
		// class = (mw&1)?EKG_MSGCLASS_CHAT:EKG_MSGCLASS_MESSAGE;
		dest = saprintf("%s%s", IRC4, param[2]);
		format = xstrdup(prv?"irc_msg_f_chan_n":"irc_not_f_chan_n");
		w = window_find_s(s, dest);
		if (!w) {
			xfree(format);
			format = xstrdup(prv?"irc_msg_f_chan":"irc_not_f_chan");
		}
		if ((person = irc_find_person(j->people, param[0]+1)))
			perchn = irc_find_person_chan(person->channels, dest);
	}

	if (t) *t='!'; 
	me = xstrdup(param[0]+1);
	if (t) *t='\0';	

	if((ctcpstripped = ctcp_parser(s, prv, me, param[2], OMITCOLON(param[3])))) {
		coloured = irc_ircoldcolstr_to_ekgcolstr(s, ctcpstripped);
		debug("<%c%s/%s> %s\n", perchn?*(perchn->sign):' ', me, param[2], coloured);
		xfree(ctcpstripped);
		head = format_string(format_find(format), session_name(s),
				perchn?perchn->sign:" ",
				me, param[0]+1, param[2], coloured);
		xfree(coloured);
		xfree(me);
		me = xstrdup(session_uid_get(s));
		sent = time(NULL);
		class |= EKG_NO_THEMEBIT;

		query_emit(NULL, "protocol-message", &me, &dest, &rcpts, &head, &form, &sent, &class, &seq, &ekgbeep, NULL);

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
	char *channel, *tmp, *text;
	channel_t *ischan;
	window_t *newwin;
	//int  __class = EKG_MSGCLASS_CHAT;
	//time_t __sent = time(NULL);
	channel = saprintf("irc:%s", OMITCOLON(param[2]));
	
	/* We join ? */
	if ((tmp = xstrchr(param[0], '!'))) *tmp='\0';
	/* istnieje jaka¶tam szansa ¿e kto¶ zrobi nick i part i bêdzie
	 * z tego jaka¶ kupa, ale na razie nie chce mi siê nad tym my¶leæ */
	if (!xstrcmp(j->nick, param[0]+1)) {
		*tmp='!';
		newwin = window_new(channel, s, 0);
		window_switch(newwin->id);
		debug("[irc] c_join() %08X\n", newwin);
		ischan = irc_add_channel(s, j , OMITCOLON(param[2]), newwin);
	/* someone joined */
	} else {
		text = xstrdup(param[0]+1);
		irc_add_person(s, j, param[0]+1, OMITCOLON(param[2])); 
		
		if(tmp) *tmp='!';
		print_window(channel, s, 0, "irc_joined", session_name(s),
				text, param[0]+1, OMITCOLON(param[2]));
		xfree(text);
		
	}
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
	char *channel, *tmp, *uid, *coloured;

	if ((tmp = xstrchr(param[0], '!'))) *tmp = '\0';
	/* we part */
	if (!xstrcmp(j->nick, param[0]+1)) {
		irc_del_channel(s, j, OMITCOLON(param[2]));
	} else {
		/* Servers MUST be able to parse arguments in the form of
		 * a list of target, but SHOULD NOT use lists when sending
		 * PART messages to clients.
		 * 
		 * damn it I think rfc should rather say MUSTN'T instead of
		 * SHOULD NOT ;/
		 */
		irc_del_person_channel(s, j, param[0]+1, OMITCOLON(param[2]));
		uid = xstrdup(param[0]+1);
		if (tmp) *tmp='!';

		channel = saprintf("irc:%s", param[2]);
		
		coloured = param[3]?xstrlen(OMITCOLON(param[3]))?
			irc_ircoldcolstr_to_ekgcolstr(s, OMITCOLON(param[3])):
					xstrdup("no reason"):xstrdup("no reason");
		print_window(channel, s, 0, "irc_left", session_name(s),
				uid, param[0]+1, OMITCOLON(param[2]), coloured);
		xfree(coloured);
		xfree(channel);
		xfree(uid);
	}
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
	char *channel, *tmp, *uid, *stajl, *coloured;
	irc_onkick_handler_t *onkick;

	if ((tmp = xstrchr(param[0], '!'))) *tmp = '\0';
	/* we were kicked out */
	if (!xstrcmp(j->nick, param[3])) {
		irc_del_channel(s, j, param[2]);
		stajl = xstrdup("irc_kicked_you");
	} else {
		irc_del_person_channel(s, j, OMITCOLON(param[3]), param[2]);
		stajl = xstrdup("irc_kicked");
	}

	uid = saprintf("%s%s", IRC4, param[0]+1);

	if (tmp) *tmp='!';

	channel = saprintf("irc:%s", param[2]);
	
	coloured = param[4]?xstrlen(OMITCOLON(param[4]))?
		irc_ircoldcolstr_to_ekgcolstr(s, OMITCOLON(param[4])):
				xstrdup("no reason"):xstrdup("no reason");
	print_window(channel, s, 0, stajl, session_name(s), 
			OMITCOLON(param[3]), uid+4, param[0]+1,
			param[2], coloured);
	xfree(coloured);
	xfree(stajl);

	onkick = xmalloc(sizeof(irc_onkick_handler_t));
	onkick->s = s;
	onkick->nick = saprintf("%s%s", IRC4, OMITCOLON(param[3]));
	onkick->chan = channel;
	onkick->kickedby = uid;

	irc_onkick_handler(s, onkick);
	return 0;
}

/* p[0] - :nick!ident@ihost
 * p[1] - QUIT
 * (p[2]) - reason
 */
IRC_COMMAND(irc_c_quit)
{
	char *tmp, *uid, *reason;
	int dq;

	if ((tmp = xstrchr(param[0], '!'))) *tmp = '\0';
	uid = xstrdup(param[0]+1);
	if (tmp) *tmp='!';
	reason = param[2]?xstrlen(OMITCOLON(param[2]))?
		irc_ircoldcolstr_to_ekgcolstr(s, OMITCOLON(param[2])):
		xstrdup("no reason"):xstrdup("no reason");
	
	dq = session_int_get(s, "DISPLAY_QUIT");
	
	irc_del_person(s, j, uid, param[0]+1, reason, !dq);
	
	if (dq)
		print_window(dq==2?window_current->target:"__status",
				s, 0, "irc_quit", session_name(s), 
				uid, param[0]+1, reason);
	
	xfree(reason);
	xfree(uid);
	
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
	window_t *w;
	char *t, *dest=NULL;
	char *coloured;

	t = saprintf("%s%s", IRC4, param[2]);
	w = window_find_s(s, t);
	dest = w?w->target:NULL;
	xfree(t);
	if ((t = xstrchr(param[0], '!')))
		*t = '\0';
	if (xstrlen(OMITCOLON(param[3]))) {
		coloured = irc_ircoldcolstr_to_ekgcolstr(s, OMITCOLON(param[3]));
		print_window(dest, s, 0, "IRC_TOPIC_CHANGE", session_name(s),
				param[2], coloured, param[0]+1);
		xfree(coloured);
	} else
		print_window(dest, s, 0, "IRC_TOPIC_UNSET", session_name(s),
				param[2], param[0]+1);
	if (t) *t='!';
	return 0;
}

#ifdef GDEBUG 
#ifndef MARLENE
#error She's all I really care about. You shouldn't play with my GDEBUG!
#endif
#endif

IRC_COMMAND(irc_c_mode)
{
	int i, k, len, val=0, act=1;
	char *t, *bang, *add, **pars, *channame;
	people_t *per;
	people_chan_t *ch;
	userlist_t *ul;
	window_t *w;
	string_t moderpl;
	
	len = (xstrlen(SOP(_005_PREFIX))>>1);
	add = xmalloc(len * sizeof(char));
	for (i=0; i<len; i++) add[i] = SOP(_005_PREFIX)[i+1];
	add[--len] = '\0';

	/* GiM: FIXME TODO [this shouldn't be xstrcasecmp! user mode */
	if (!xstrcasecmp(param[2], j->nick))
	{
		print_window(window_current->target, s, 0, 
				"IRC_MODE", session_name(s), 
				param[0]+1, OMITCOLON(param[3]));
		return 0;
	/* channel mode */
	}
	t=param[3];
	for (i=0,k=4; i<xstrlen(param[3]); i++, t++) {
		if (*t=='+' || *t=='-')
			act=*t-'-';
		if ((bang=xstrchr(add, *t))) {
			per = irc_find_person(j->people, param[k]);
			if (!per) goto notreallyok;
			ch = irc_find_person_chan(per->channels, param[2]); 
			if (!ch) goto notreallyok;
			/* GiM: ivil hack ;) */
			val = 1<<(len-(bang-add)-1);
			if (act) ch->mode |= val; else ch->mode-=val;
			ul = userlist_find_u(&(ch->chanp->window->userlist), param[k]);
			if(!ul) goto notreallyok;
			
			
			irc_nick_prefix(j, ch, irc_color_in_contacts(add, 
						ch->mode, ul));
		}
notreallyok:
		if (xstrchr(add, *t)) k++;
		if (!param[k]) break;
	}
	
	channame = saprintf("%s%s", IRC4, param[2]);
	w = window_find_s(s, channame);
	bang = xstrchr(param[0], '!');
	if (bang) *bang='\0';
	moderpl =  string_init("");
	pars=&(param[3]);
	while (*pars) {
		string_append_c(moderpl, ' ');
		string_append(moderpl, *pars++);
	}
	print_window(w?w->target:NULL, s, 0, "IRC_MODE_CHAN", session_name(s),
			param[0]+1, param[2], moderpl->str);
	if (bang) *bang='!';
	string_free(moderpl, 1);

	xfree(add);
	xfree(channame);
	return 0;
}


