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

#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <sys/utsname.h>

#include <ekg/sessions.h>
#include <ekg/themes.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include "input.h"
#include "IRCVERSION.h"

int irc_getircoldcol(char *org)
{
	char *p=org;
	int ibg, ifg, isfg, isbg, isdel, i, ret;

	if (!p || !*p) 
		return 0;

	isfg=isbg=isdel=0;

	if (sscanf(p, "%02d", &ifg)==1) {
		p++; if (isdigit(*p)) p++;
		isfg=1;
	}
	if (*p == ',') {
		p++; isdel=1;
		if (sscanf(p, "%02d", &ibg)==1) {
			p++; if(isdigit(*p)) p++; 
			isbg=1;
		}
	}

	i = ((p-org))&0xff;
	ret=i<<24;

	if (isfg) {
		ret|=(isfg<<17);
		ret|=(ifg<<8);
	} 
	if (isdel && !isbg) { isbg=1; } /* ibg=DEFAULT_COLOR; } */
	if (isbg) {
		ret|=(isbg<<16);
		ret|=ibg;
	}
	return ret;
}

char *irc_ircoldcolstr_to_ekgcolstr(session_t *sess, char *str, int strip)
{
	int  col, oldstrip = strip;
	char mirc_sux_so_much[16]=  "WkbgrypRYGcCBPKw";
	char mirc_sux_even_more[16]="xlehszqszhddeqlx";
	char *p;
	string_t s;

	if(!(str && xstrlen(str))) return xstrdup("");

	s = string_init("");
	if (strip)
		strip = session_int_get(sess, "STRIPMIRCCOL");

	for (;*str;)
	{
		if (*str == 3) /* ^c */
		{
			/* str++; */
			col = irc_getircoldcol(str+1);
		    	if (strip) goto coloring_finito;
			if (!col) {
				string_append(s, "%n");
				goto coloring_finito;
			}
			if (col&0x20000) {
				string_append_c(s, '%');
				string_append_c(s, mirc_sux_so_much
						[(col>>8)&0xf]);
			}
			if (col&0x10000) {
				string_append_c(s, '%');
				string_append_c(s, mirc_sux_even_more
						[col&0xf]);
			}
coloring_finito:
			str+=(col>>24)&0xff;
		} else if (*str == 2) /* ^b */
			string_append(s, "%T");
		else if (*str == 15) /* ^o */
			string_append(s, "%n");
		else if (*str == 18 || *str == 22) /* ^r */
			string_append(s, "%V");
		else if (*str == 31) /* ^_ */
			string_append(s, "%U");
		else if (*str == '%')
			string_append(s, "\\%");
		else if (*str == '\\')
			string_append(s, "\\\\");
		else 
			string_append_c(s, *str);
		str++;
	}
	if (oldstrip)
		string_append(s, "%n");
	p = format_string(s->str);
	string_free(s, 1);
	return p;
}


/*
 *
 * http://www.irchelp.org/irchelp/rfc/ctcpspec.html
 *
 */


int is_ctcp(char *mesg)
{
	int i;
	char *p;
	ctcp_t *ret;

	if ((p = xstrchr(mesg, ' ')))
		*p='\0';

	for (i=0,ret=ctcps; (ret->name); i++,ret++)
		if (!xstrcmp(mesg, ret->name)) {
			if (p) *p=' ';
			return i+1;
		}

	return 0;
}

char *ctcp_parser(session_t *sess, int ispriv, char *sender, char *recp, char *s)
{
	irc_private_t *j = session_private_get(sess);
	char *begin, *end, *winname, *p, *bang, *newsender, *coloured;
	string_t ret;
	int ctcp;

	if (!s || xstrlen(s) < 2)
		return s?xstrdup(s):NULL;

	winname = saprintf("%s%s", IRC4, recp);
	ret = string_init("");
	p = begin = s;

	while (1) {
		if (!(begin = xstrchr(begin, 1))) 
			break;
		if (!(end = xstrchr(begin+1, 1)))
			break;
		*begin = '\0';
		begin++;
		*end = '\0';
		if ((ctcp=is_ctcp(begin))) {

			if ((bang = xstrchr(sender+1, '!'))) 
				*bang = '\0';

			newsender = saprintf("%s%s", IRC4, sender+1);

			coloured = irc_ircoldcolstr_to_ekgcolstr(sess, begin,1);
			if (ispriv) {
				if ((ctcp_main_priv(sess, j, ctcp, coloured, newsender,
								bang?bang+1:"", winname)))
				{
					/* blah blah blah */
				}
			} else {
				ctcp_main_noti(sess, j, ctcp, coloured, newsender, 
						bang?bang+1:"", winname);
			}
			xfree(coloured);

			if (bang) *bang = '!';
			string_append(ret, p);
			p = end+1;
		} else {
			/*
			print_window(winname, sess, 0, "irc_unknown_ctcp",
					session_names(sess), sender, begin,
					spc+1);
			*spc = ' ';
			*/
			irc_write(j, "NOTICE %s :\01ERRMSG %s :unknown ctcp\01\r\n",
					sender+1, begin);
			begin--; *begin=1; *end=1; 
		}
		begin=end+1;
	}

	xfree(winname);
	string_append(ret, p);
	p = string_free(ret, 0);
	if (xstrlen(p)) return p;
	xfree(p);
	return NULL;
}

/* <quote>
 *   This is used by losers on IRC to simulate "role playing" games.
 * </quote> ;-)
 */
CTCP_COMMAND(ctcp_main_priv)
{
	char *ischn = xstrchr(SOP(_005_CHANTYPES), targ[4]);
	char *space = xstrchr(ctcp, ' ');
	int mw = session_int_get(s, "make_window");
	char *ta, *tb, *tc;
	char *purename=sender+4, *win;
	struct utsname un;
	time_t timek;
	window_t *w;

	if (space) while ((*space) && (*space == ' ')) space++;

	win = ischn?targ:sender;
	w = window_find_s(s, win);
	if (!(ischn || w || (mw&4))) win = window_current->target;

switch (number) {
    case CTCP_ACTION:	/* ===== ===== ===== ===== ===== ACTION */
	/* skip spaces... */
	if (space && xstrlen(space)) {
		print_window(win, s, ischn?(mw&1):!!(mw&4),
				ischn?"irc_ctcp_action_pub":"irc_ctcp_action",
				session_name(s), purename, idhost, targ+4, space);
	}
	return 0;

	
    case CTCP_DCC:		/* ===== ===== ===== ===== ===== DCC */
	irc_write(j, "NOTICE %s :\01ERRMSG %s :not handled\01\r\n", 
			purename, ctcp);
	return 0;

	
    case CTCP_SED:		/* ===== ===== ===== ===== ===== SED */
	irc_write(j, "NOTICE %s :\01ERRMSG %s :not handled\01\r\n", 
			purename, ctcp);
	return 0;

	
    case CTCP_FINGER:	/* ===== ===== ===== ===== ===== FINGER */
	ta = xstrdup(ctime(&(s->last_conn)));
	if (ta[xstrlen(ta)-1] == '\n') ta[xstrlen(ta)-1]='\0';

	print_window(win, s, ischn?(mw&1):!!(mw&4),
			ischn?"irc_ctcp_request_pub":"irc_ctcp_request",
			session_name(s), purename, idhost, targ+4, ctcp);

	irc_write(j, "NOTICE %s :\01FINGER :%s connected since %s\01\r\n",
			purename, j->nick, ta);
	xfree(ta);
	return 0;

	
    case CTCP_VERSION:	/* ===== ===== ===== ===== ===== VERSION */
	print_window(win, s, ischn?(mw&1):!!(mw&4), 
			ischn?"irc_ctcp_request_pub":"irc_ctcp_request",
			session_name(s), purename, idhost, targ+4, ctcp);

	ta = (char *)session_get(s, "VERSION_NAME");
	tb = (char *)session_get(s, "VERSION_NO");
	tc = (char *)session_get(s, "VERSION_SYS");
	if (uname(&un) || tc) {
		irc_write(j, "NOTICE %s :\01VERSION %s%s%s\01\r\n",
				purename, ta?ta:"IRC plugin under EKG2:",
				tb?tb:IRCVERSION":",
				tc?tc:"unknown OS");
		return 0;
	}
	irc_write(j, "NOTICE %s :\01VERSION %s%s%s %s %s\01\r\n",
			purename, ta?ta:"IRC plugin under EKG2:",
			tb?tb:IRCVERSION":",
			un.sysname, un.release, un.machine);
	return 0;


    case CTCP_SOURCE:	/* ===== ===== ===== ===== ===== SOURCE */
	print_window(win, s, ischn?(mw&1):!!(mw&4),
			ischn?"irc_ctcp_request_pub":"irc_ctcp_request",
			session_name(s), purename, idhost, targ+4, ctcp);

	irc_write(j, "NOTICE %s :\01SOURCE \02\x1fhttp://ekg2.org/ekg2-current.tar.gz\x1f\02\01\r\n",
			purename);
	return 0;


    case CTCP_USERINFO:	/* ===== ===== ===== ===== ===== USERINFO */
	ta = (char *)session_get(s, "USERINFO");
	print_window(win, s, ischn?(mw&1):!!(mw&4),
			ischn?"irc_ctcp_request_pub":"irc_ctcp_request",
			session_name(s), purename, idhost, targ+4, ctcp);

	irc_write(j, "NOTICE %s :\01USERINFO :%s\01\r\n",
			purename, ta?ta:"no userinfo set");
	return 0;


    case CTCP_CLIENTINFO:	/* ===== ===== ===== ===== ===== CLIENTINFO */
	print_window(win, s, ischn?(mw&1):!!(mw&4),
			ischn?"irc_ctcp_request_pub":"irc_ctcp_request",
			session_name(s), purename, idhost, targ+4, ctcp);

	irc_write(j, "NOTICE %s :\01CLIENTINFO \01\r\n",
			purename);
	return 0;


    case CTCP_PING:		/* ===== ===== ===== ===== ===== PING */
	print_window(win, s, ischn?(mw&1):!!(mw&4),
			ischn?"irc_ctcp_request_pub":"irc_ctcp_request",
			session_name(s), purename, idhost, targ+4, ctcp);

	irc_write(j, "NOTICE %s :\01PING %s\01\r\n",
			purename, space?space:"");
	return 0;

	
    case CTCP_TIME:		/* ===== ===== ===== ===== ===== TIME */
	print_window(win, s, ischn?(mw&1):!!(mw&4),
			ischn?"irc_ctcp_request_pub":"irc_ctcp_request",
			session_name(s), purename, idhost, targ+4, ctcp);

	timek = time(NULL);
	ta = xstrdup(ctime(&timek));
	if (ta[xstrlen(ta)-1] == '\n') ta[xstrlen(ta)-1]='\0';

	irc_write(j, "NOTICE %s :\01TIME %s\01\r\n",
			purename, ta);
	xfree(ta);
	return 0;


    case CTCP_ERRMSG:	/* ===== ===== ===== ===== ===== ERRMSG */
	print_window(win, s, ischn?(mw&1):!!(mw&4),
			ischn?"irc_ctcp_request_pub":"irc_ctcp_request",
			session_name(s), purename, idhost, targ+4, ctcp);

	irc_write(j, "NOTICE %s :\01ERRMSG %s\01\r\n",
			purename, space?space:"");
	return 0;

	
} /* switch() */

	return (0);
}


CTCP_COMMAND(ctcp_main_noti)
{
	char *ischn = xstrchr(SOP(_005_CHANTYPES), targ[4]);
	char *space = xstrchr(ctcp, ' ');
	int mw = session_int_get(s, "make_window");
	char *t, *win;
	window_t *w;

	if (space) while ((*space) && (*space == ' ')) space++;

	win = ischn?targ:sender;
	w = window_find_s(s, win);
	if (!ischn && !w && !(mw&4)) win = window_current->target;

	t = irc_ircoldcolstr_to_ekgcolstr(s, space,1);
	print_window(win, s, ischn?(mw&1):!!(mw&8),
			"irc_ctcp_reply", session_name(s),
			ctcps[number-1].name, sender+4, idhost, t);
	xfree (t);
	
	return (0);
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
