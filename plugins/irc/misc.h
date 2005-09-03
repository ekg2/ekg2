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

#ifndef __EKG_PLUGINS_IRC_MISC_H
#define __EKG_PLUGINS_IRC_MISC_H

#include <stdio.h>

#include <ekg/plugins.h>

#include "irc.h"

#define IRC_COMMAND(x) int x(session_t *s, irc_private_t *j, int fd, int ecode, char **param)
typedef int (*Irc_Cmd) 	    (session_t * , irc_private_t * , int   , int      , char **);

#define IRC_LISTBAN		0x001
#define IRC_LISTEXC		0x002
#define IRC_LISTINV		0x004
#define IRC_LISTLIN		0x008
#define IRC_LISTSTA		0x010
#define IRC_LISTWHO		0x020
#define IRC_LISTCHA		0x040
#define IRC_LISTEND		0x080

#define IRC_WHOIS		0x001
#define IRC_WHOWAS		0x002
#define IRC_WHOERR		0x004

enum { IRC_ERR_12=0, IRC_ERR_21, IRC_ERR_ONLY1, IRC_ERR_NEW, IRC_ERR_IGNO,
	IRC_ERR_OTHER,

	IRC_RPL_12=256, IRC_RPL_21, IRC_RPL_ONLY1, IRC_RPL_NEW, IRC_RPL_IGNO,
	IRC_RPL_OTHER };
typedef struct {
	int 		type;
	int 		num;
	const char 	*comm;
	const char	*name;
	Irc_Cmd		handler;
	int 		future;
} IrcCommand;

int irc_input_parser(session_t *s, char *buf, int len);
char *irc_make_banmask(session_t *session, const char *nick, 
		const char *ident, const char *hostname);

IRC_COMMAND(irc_c_init);
IRC_COMMAND(irc_c_invite);
IRC_COMMAND(irc_c_ping);
IRC_COMMAND(irc_c_nick);
IRC_COMMAND(irc_c_msg);
IRC_COMMAND(irc_c_join);
IRC_COMMAND(irc_c_part);
IRC_COMMAND(irc_c_kick);
IRC_COMMAND(irc_c_quit);
IRC_COMMAND(irc_c_error);
IRC_COMMAND(irc_c_list);
IRC_COMMAND(irc_c_namerpl);
IRC_COMMAND(irc_c_mode);
IRC_COMMAND(irc_c_topic);
IRC_COMMAND(irc_c_whois);

/* 1st - 1 if reply in numeric form, 0 if as a string
 * 1st== 1                           | 0
 * 2nd - code                        | 0
 * 3rd - NULL			     | command
 * 4th - name from rfc and stylename | command name
 * 5th - function handler	     | function handler
 * 6th - 0 or one of enum's above...
 * 	 IRC_ERR if error, IRC_RPL if reply [used to determine style
 * 	 of display]
 *
 * for simple numeric replies [and for ERROR] exsist one function
 * irc_c_error
 *
 * first it checks style and if =NOT=  *_OTHER or *_IGNO [ignore]
 * it displays some information
 * if *_OTHER is used reply is treated according to its code
 * starting with line switch(i), where i is code
 *
 * if you add something that is not currently on the list
 * give somewhere [*] in comment
 */
static IrcCommand irccommands[] =
{
	{ 1,	-1,	NULL,	NULL,				&irc_c_error,	IRC_ERR_NEW },
	{ 1,	1,	NULL,	"RPL_WELCOME",		&irc_c_init,	0 },
	{ 1,	2,	NULL,	"RPL_YOURHOST",		&irc_c_init,	0 },
	{ 1,	3,	NULL, 	"RPL_CREATED",		&irc_c_init,	0 },
	{ 1,	4,	NULL,	"RPL_MYINFO",		&irc_c_init,	0 },
	{ 1,	5,	NULL,	"RPL_BOUNCE",		&irc_c_init,	0 },

/*	{ 1,	200,	NULL,	"RPL_TRACELINK",	&irc_c_error,	
	{ 1,	201,	NULL,	"RPL_TRACECONNECTING",	&irc_c_error,	
	{ 1,	202,	NULL,	"RPL_TRACEHANDSHAKE",	&irc_c_error,		
	{ 1,	203,	NULL,	"RPL_TRACEUNKNOWN",	&irc_c_error,	
	{ 1,	204,	NULL,	"RPL_TRACEOPERATOR",	&irc_c_error,	
	{ 1,	205,	NULL,	"RPL_TRACEUSER",	&irc_c_error,	
	{ 1,	206,	NULL,	"RPL_TRACESERVER",	&irc_c_error,	
	{ 1,	207,	NULL,	"RPL_TRACESERVICE",	&irc_c_error,	
	{ 1,	208,	NULL,	"RPL_TRACENEWTYPE",	&irc_c_error,	
	{ 1,	209,	NULL,	"RPL_TRACECLASS",	&irc_c_error,	
	{ 1,	210,	NULL,	"RPL_TRACERECONNECT",	&irc_c_error,	
	{ 1,	261,	NULL,	"RPL_TRACELOG",		&irc_c_error,	
	{ 1,	262,	NULL,	"RPL_TRACEEND",		&irc_c_error,	*/

/*	{ 1,	211,	NULL,	"RPL_STATSLINKINFO",	&irc_c_error,	
	{ 1,	212,	NULL,	"RPL_STATSCOMMANDS",	&irc_c_error,	
	{ 1,	219,	NULL,	"RPL_ENDOFSTATS",	&irc_c_error,	
	{ 1,	242,	NULL,	"RPL_STATSUPTIME",	&irc_c_error,	
	{ 1,	243,	NULL,	"RPL_STATSOLINE",	&irc_c_error,	*/
	/*[ ]  /stats M -> modules */
	{ 1,	212,	NULL, 	"RPL_STATS",		&irc_c_list, IRC_LISTSTA},
	/*[*] /stats C -> connect() */
	{ 1,	213,	NULL,	"RPL_STATS_EXT",	&irc_c_list, IRC_LISTSTA},
	/*[*] /stats I -> I:lines */
	{ 1,	215,	NULL,	"RPL_STATS_EXT",	&irc_c_list, IRC_LISTSTA},
	/*[*] /stats K -> K:lines */
	{ 1,	216,	NULL,	"RPL_STATS_EXT",	&irc_c_list, IRC_LISTSTA},
	/*[*] /stats Y -> classes */
	{ 1,	218,	NULL,	"RPL_STATS",		&irc_c_list, IRC_LISTSTA},
	{ 1,	219,	NULL,	"RPL_STATSEND",		&irc_c_list, IRC_LISTSTA|IRC_LISTEND },
	/*[*] /stats P -> ports */
	{ 1,	220,	NULL,	"RPL_STATS_EXT",	&irc_c_list, IRC_LISTSTA},
	/*[*] /stats A */
	{ 1,	226,	NULL, 	"RPL_STATS",		&irc_c_list, IRC_LISTSTA}, 
	/*[ ] /stats u -> uptime */
	{ 1,	242,	NULL,	"RPL_STATS",		&irc_c_list, IRC_LISTSTA},
	/*[ ] /stats O -> O:lines ; P -> aktywni */
	{ 1,	243,	NULL,	"RPL_STATS_EXT",	&irc_c_list, IRC_LISTSTA},
	/*[*] /stats H -> */
	{ 1,	244,	NULL,	"RPL_STATS",		&irc_c_list, IRC_LISTSTA},
	/*[*] /stats F, R, T, Z, ? */
	{ 1,	249,	NULL,	"RPL_STATS",		&irc_c_list, IRC_LISTSTA},

/*	{ 1,	221,	NULL,	"RPL_UMODEIS",		&irc_c_error,	
	{ 1,	234,	NULL,	"RPL_SERVLIST",		&irc_c_error,	
	{ 1,	235,	NULL,	"RPL_SERVLISTEND",	&irc_c_error,
	
	{ 1,250,NULL,"RPL_STATS",&irc_c_list,IRC_LISTSTA }, [*]
	{ 1,	251,	NULL,	"RPL_LUSERCLIENT",	&irc_c_error,	
	{ 1,	252,	NULL,	"RPL_LUSEROP",		&irc_c_error,	
	{ 1,	253,	NULL,	"RPL_LUSERUNKNOWN",	&irc_c_error,	
	{ 1,	254,	NULL,	"RPL_LUSERCHANNELS",	&irc_c_error,	
	{ 1,	255,	NULL,	"RPL_LUSERME",		&irc_c_error,	
*/

/*
	{ 1,	256,	NULL,	"RPL_ADMINME",		&irc_c_error,	
	{ 1,	257,	NULL,	"RPL_ADMINLOC1",	&irc_c_error,	
	{ 1,	258,	NULL,	"RPL_ADMINLOC2",	&irc_c_error,	
	{ 1,	259,	NULL,	"RPL_ADMINEMAIL",	&irc_c_error,	*/
	{ 1,	263,	NULL,	"RPL_TRYAGAIN",		&irc_c_error,	IRC_ERR_ONLY1},
	
/*	{ 1,	302,	NULL,	"RPL_USERHOST",		&irc_c_error,	
	{ 1,	303,	NULL,	"RPL_ISON",		&irc_c_error,	*/
	
	{ 1,	301,	NULL,	"RPL_AWAY",		&irc_c_error,	IRC_RPL_OTHER},
	{ 1,	305,	NULL,	"RPL_UNAWAY",		&irc_c_error,	IRC_RPL_ONLY1},
	{ 1,	306,	NULL,	"RPL_NOWAWAY",		&irc_c_error,	IRC_RPL_ONLY1},

	{ 1,	311,	NULL,	"RPL_WHOISUSER",	&irc_c_whois,IRC_WHOIS},
	{ 1,	312,	NULL,	"RPL_WHOISSERVER",	&irc_c_whois,IRC_WHOIS},
	{ 1,	313,	NULL,	"RPL_WHOISOPERATOR",	&irc_c_whois,IRC_WHOIS},
	{ 1,	317,	NULL,	"RPL_WHOISIDLE",	&irc_c_whois,IRC_WHOIS},
	{ 1,	318,	NULL,	"RPL_ENDOFWHOIS",	&irc_c_whois,IRC_WHOIS},
	{ 1,	319,	NULL,	"RPL_WHOISCHANNELS",	&irc_c_whois,IRC_WHOIS},
	{ 1,	314,	NULL,	"RPL_WHOWASUSER",	&irc_c_whois,IRC_WHOWAS},
	{ 1,	369,	NULL,	"RPL_ENDOFWHOWAS",	&irc_c_whois,IRC_WHOWAS},

	/* G->dj I want to keep the names from rfc2812 */
	{ 1,	315,	NULL,	"RPL_ENDOFWHO",		&irc_c_list, IRC_LISTWHO|IRC_LISTEND },
	{ 1,	352,	NULL,	"RPL_WHOREPLY",		&irc_c_list, IRC_LISTWHO },	

	/* G->dj: with what it colides ?? */
/*	{ 1,	321,	NULL,	"RPL_LISTSTART",	&irc_c_error,	*/
	{ 1,	321,	NULL,	"RPL_CHLISTSTART",	&irc_c_list, IRC_LISTCHA },
	{ 1,	322,	NULL,	"RPL_LIST",		&irc_c_list, IRC_LISTCHA },
	{ 1,	323,	NULL,	"RPL_LISTEND",		&irc_c_list, IRC_LISTCHA|IRC_LISTEND },

/*	{ 1,	325,	NULL,	"RPL_UNIQOPIS",		&irc_c_error,	*/
	{ 1,	324,	NULL,	"RPL_CHANNELMODEIS",	&irc_c_mode,	IRC_RPL_OTHER},
	/* 331 is really RPL_NOTOPIC, but I don't want another format... */
	{ 1,	331,	NULL,	"RPL_TOPIC",		&irc_c_error,	IRC_RPL_OTHER},
	{ 1,	332,	NULL,	"RPL_TOPIC",		&irc_c_error,	IRC_RPL_OTHER},
	/* [*] 333 not in rfc 2812 */
	{ 1,	333,	NULL,	"RPL_TOPICBY",		&irc_c_error,	IRC_RPL_OTHER},
	{ 1,	341,	NULL,	"RPL_INVITE",		&irc_c_error,	IRC_RPL_OTHER},
/*	{ 1,	443, G->dj: ??? */
/*	{ 1,	351,	NULL,	"RPL_VERSION",		&irc_c_error,	*/
	{ 1,	353,	NULL,	"RPL_NAMREPLY",		&irc_c_namerpl,	0 },
	{ 1,	364,	NULL,	"RPL_LINKS",		&irc_c_list,	IRC_LISTLIN },
	{ 1,	365,	NULL,	"RPL_ENDOFLINKS",	&irc_c_list,	IRC_LISTLIN|IRC_LISTEND }, 

	{ 1,	346,	NULL,	"RPL_INVITELIST",	&irc_c_list,	IRC_LISTINV }, 
	{ 1,	347,	NULL,	"RPL_ENDOFLIST",	&irc_c_list,	IRC_LISTINV|IRC_LISTEND }, 
	{ 1,	348,	NULL,	"RPL_EXCEPTLIST", 	&irc_c_list,	IRC_LISTEXC },
	{ 1,	349,	NULL,	"RPL_ENDOFLIST",	&irc_c_list,	IRC_LISTEXC|IRC_LISTEND },

	{ 1,	366,	NULL,	"RPL_ENDOFNAMES",	&irc_c_error,	IRC_RPL_IGNO},
	{ 1,	367,	NULL,	"RPL_BANLIST",		&irc_c_list, 	IRC_LISTBAN },
	{ 1,	368,	NULL,	"RPL_ENDOFBANLIST",	&irc_c_list,	IRC_LISTBAN|IRC_LISTEND },
/*	{ 1,	371,	NULL,	"RPL_INFO",		&irc_c_error,	*/
	{ 1,	372,	NULL,	"RPL_MOTD",		&irc_c_error,	IRC_RPL_OTHER},
/*	{ 1,	374,	NULL,	"RPL_ENDOFINFO",	&irc_c_error,	*/
	{ 1,	375,	NULL,	"RPL_MOTDSTART",	&irc_c_error,	IRC_RPL_OTHER},
	{ 1,	376,	NULL,	"RPL_ENDOFMOTD",	&irc_c_error,	IRC_RPL_OTHER},
/*	{ 1,	381,	NULL,	"RPL_YOUREOPER",	&irc_c_error,	
	{ 1,	382,	NULL,	"RPL_REHASHING",	&irc_c_error,	
	{ 1,	383,	NULL,	"RPL_YOURESERVICE",	&irc_c_error,	
	{ 1,	391,	NULL,	"RPL_TIME",		&irc_c_error,	
	{ 1,	392,	NULL,	"RPL_USERSSTART",	&irc_c_error,	
	{ 1,	393,	NULL,	"RPL_USERS",		&irc_c_error,	
	{ 1,	394,	NULL,	"RPL_ENDOFUSERS",	&irc_c_error,	
	{ 1,	395,	NULL,	"RPL_NOUSERS",		&irc_c_error,	*/

	{ 1,	401,	NULL,	"ERR_NOSUCHNICK",	&irc_c_error,	IRC_ERR_21 },
	{ 1,	402,	NULL,	"ERR_NOSUCHSERVER",	&irc_c_error,	IRC_ERR_21 },
	{ 1,	403,	NULL,	"ERR_NOSUCHCHANNEL",	&irc_c_error,	IRC_ERR_21 },
/*
 * G->dj: what ? why dya want to put this stuff there ?
+	{ 1,	401,	NULL,	"ERR_NOSUCHNICK",	&irc_c_whois,	IRC_WHOIS  | IRC_WHOERR },
+	{ 1,	402,	NULL,	"ERR_NOSUCHSERVER",	&irc_c_whois,	IRC_WHOIS  | IRC_WHOERR },
+	{ 1,	403,	NULL,	"ERR_NOSUCHCHANNEL",	&irc_c_whois,	IRC_WHOIS  | IRC_WHOERR },
+	{ 1,	406,	NULL,	"ERR_WASNOSUCHNICK",	&irc_c_whois,	IRC_WHOWAS | IRC_WHOERR },
+	
*/
 	{ 1,	404,	NULL,	"ERR_CANNOTSENDTOCHAN",	&irc_c_error,	IRC_ERR_OTHER },
	{ 1,	404,	NULL,	"ERR_CANNOTSENDTOCHAN",	&irc_c_error,	IRC_ERR_OTHER },
	{ 1,	405,	NULL,	"ERR_TOOMANYCHANNELS",	&irc_c_error,	IRC_ERR_12 },
	{ 1,	406,	NULL,	"ERR_WASNOSUCHNICK",	&irc_c_error,	IRC_ERR_21 },
	{ 1,	407,	NULL,	"ERR_TOOMANYTARGETS",	&irc_c_error,	IRC_ERR_12 },
	{ 1,	408,	NULL,	"ERR_NOSUCHSERVICE",	&irc_c_error,	IRC_ERR_21 },
	{ 1,	409,	NULL,	"ERR_NOORIGIN",		&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	411,	NULL,	"ERR_NORECIPIENT",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	412,	NULL,	"ERR_NOTEXTTOSEND",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	413,	NULL,	"ERR_NOTOPLEVEL",	&irc_c_error,	IRC_ERR_21 },
	{ 1,	414,	NULL,	"ERR_WILDTOPLEVEL",	&irc_c_error,	IRC_ERR_21 },
	{ 1,	415,	NULL,	"ERR_BADMASK",		&irc_c_error,	IRC_ERR_21 },
	{ 1,	421,	NULL,	"ERR_UNKNOWNCOMMAND",	&irc_c_error,	IRC_ERR_21 },
	{ 1,	422,	NULL,	"ERR_NOMOTD",		&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	423,	NULL,	"ERR_NOADMININFO",	&irc_c_error,	IRC_ERR_12 },
	{ 1,	424,	NULL,	"ERR_FILEERROR",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	431,	NULL,	"ERR_NONICKNAMEGIVEN",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	432,	NULL,	"ERR_ERRONEUSNICKNAME",	&irc_c_error,	IRC_ERR_21 },
	{ 1,	433,	NULL,	"ERR_NICKNAMEINUSE",	&irc_c_error,	IRC_ERR_OTHER },
	{ 1,	436,	NULL,	"ERR_NICKCOLLISION",	&irc_c_error,	IRC_ERR_12 },
	{ 1,	437,	NULL,	"ERR_UNAVAILRESOURCE",	&irc_c_error,	IRC_ERR_12 },
	{ 1,	441,	NULL,	"ERR_USERNOTINCHANNEL",	&irc_c_error,	IRC_ERR_NEW },
	{ 1,	442,	NULL,	"ERR_NOTONCHANNEL",	&irc_c_error,	IRC_ERR_21 },
	{ 1,	443,	NULL,	"ERR_USERONCHANNEL",	&irc_c_error,	IRC_ERR_NEW },
	{ 1,	444,	NULL,	"ERR_NOLOGIN",		&irc_c_error,	IRC_ERR_12 },
	{ 1,	445,	NULL,	"ERR_SUMMONDISABLED",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	446,	NULL,	"ERR_USERSDISABLED",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	451,	NULL,	"ERR_NOTREGISTERED",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	461,	NULL,	"ERR_NEEDMOREPARAMS",	&irc_c_error,	IRC_ERR_12 },
	{ 1,	462,	NULL,	"ERR_ALREADYREGISTRED",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	463,	NULL,	"ERR_NOPERMFORHOST",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	464,	NULL,	"ERR_PASSWDMISMATCH",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	465,	NULL,	"ERR_YOUREBANNEDCREEP",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	466,	NULL,	"ERR_YOUWILLBEBANNED",	&irc_c_error,	IRC_ERR_NEW },
	{ 1,	467,	NULL,	"ERR_KEYSET",		&irc_c_error,	IRC_ERR_12 },
	{ 1,	471,	NULL,	"ERR_CHANNELISFULL",	&irc_c_error,	IRC_ERR_12 },
	{ 1,	472,	NULL,	"ERR_UNKNOWNMODE",	&irc_c_error,	IRC_ERR_12 },
	{ 1,	473,	NULL,	"ERR_INVITEONLYCHAN",	&irc_c_error,	IRC_ERR_12 },
	{ 1,	474,	NULL,	"ERR_BANNEDFROMCHAN",	&irc_c_error,	IRC_ERR_12 },
	{ 1,	475,	NULL,	"ERR_BADCHANNELKEY",	&irc_c_error,	IRC_ERR_12 },
	{ 1,	476,	NULL,	"ERR_BADCHANMASK",	&irc_c_error,	IRC_ERR_12 },
	{ 1,	477,	NULL,	"ERR_NOCHANMODES",	&irc_c_error,	IRC_ERR_12 },
	{ 1,	478,	NULL,	"ERR_BANLISTFULL",	&irc_c_error,	IRC_ERR_NEW },
	{ 1,	481,	NULL,	"ERR_NOPRIVILEGES",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	482,	NULL,	"ERR_CHANOPRIVSNEEDED",	&irc_c_error,	IRC_ERR_12 },
	{ 1,	483,	NULL,	"ERR_CANTKILLSERVER",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	484,	NULL,	"ERR_RESTRICTED",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	485,	NULL,	"ERR_UNIQOPPRIVSNEEDED",&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	491,	NULL,	"ERR_NOOPERHOST",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	501,	NULL,	"ERR_UMODEUNKNOWNFLAG",	&irc_c_error,	IRC_ERR_ONLY1 },
	{ 1,	502,	NULL,	"ERR_USERSDONTMATCH",	&irc_c_error,	IRC_ERR_ONLY1 },
       
	{ 0,	0,	"PING",	"PING",			&irc_c_ping,	0 },
	{ 0,	0,	"INVITE", "INVITE",		&irc_c_invite,	0 },
	{ 0,	0,	"NICK", "NICK",			&irc_c_nick,	0 },
	{ 0,	0,	"PRIVMSG", "PRIVMSG",		&irc_c_msg,	0 },
	{ 0,	0,	"NOTICE", "NOTICE",		&irc_c_msg,	0 },
	{ 0,	0,	"JOIN", "JOIN",			&irc_c_join,	0 },
	{ 0,	0,	"PART", "PART",			&irc_c_part,	0 },
	{ 0,	0,	"KICK", "KICK",			&irc_c_kick,	0 },
	{ 0,	0,	"QUIT", "QUIT",			&irc_c_quit,	0 },
	{ 0,	0,	"MODE", "MODE",			&irc_c_mode,	0 },
	{ 0,	0,	"TOPIC", "TOPIC",		&irc_c_topic,	0 },
	{ 0,	0,	"ERROR", "ERROR",		&irc_c_error,	0 },
	{ -1,	-1,	NULL,	NULL,			NULL,		0 }
};	
/*
	{ 1,	372, "RPL_MOTD",		irc_c_motd, 1, NULL },
	{ 1,	376, "RPL_ENDOFMOTD",		irc_c_motd, 1, NULL }
*/


#endif /* __EKG_PLUGINS_IRC_MISC_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
