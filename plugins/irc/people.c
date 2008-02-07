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

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#define __EKG_STUFF_H

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/sessions.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include <ekg/queries.h>

#include "people.h"
#include "irc.h"

enum { OTHER_NETWORK };

static LIST_FREE_ITEM(list_irc_people_free, people_t *) {
	xfree(data->nick);
	xfree(data->realname);
	xfree(data->host);
	xfree(data->ident);
	xfree(data);
}

static LIST_FREE_ITEM(list_irc_channel_free, channel_t *) {
	xfree(data->name);
	xfree(data->topic);
	xfree(data->topicby);
	xfree(data->mode_str);
	list_destroy(data->banlist, 1);
	xfree(data);
}

/* add others
 */
int irc_xstrcasecmp_default(char *str1, char *str2)
{
	return xstrcasecmp(str1, str2);
}

/* this function searches for a given nickname on a given list
 * nick MUST BE without preceeding 'irc:' string
 * nick can contain mode prefix (one of): '@%+'
 *
 * list should be one of:
 *     private->channels 
 *     private->people->channels->onchan
 */
people_t *irc_find_person(list_t p, char *nick)
{
	int (*comp_func)(char *,char*);
	people_t *person;

	if (!(nick && p)) return NULL;

	/* debug only, delete after proper testing */
	if (!xstrncmp(nick, IRC4, 4))
		debug_error("programmer's mistake in call to irc_find_person!: %s\n", nick);

	if (*nick == '+' || *nick == '%' || *nick == '@') nick++;

	comp_func = irc_xstrcasecmp_default;

	for (; p; p=p->next)
	{
		person = (people_t *)(p->data);
		if (person->nick && !comp_func(nick, person->nick+4))
			return person;
	}
	return NULL;
}

/* p = private->channel || */
channel_t *irc_find_channel(list_t p, char *channame)
{
	channel_t *chan;
	if (!(channame && p)) return NULL;

	for (; p; p=p->next)
	{
		chan = (channel_t *)(p->data);
		if (chan->name && (!xstrcmp(chan->name, channame) ||
					!xstrcmp((chan->name)+4, channame)))
			return chan;
	}
	return NULL;
}

/* p = private->people->channels */
people_chan_t *irc_find_person_chan(list_t p, char *channame)
{
	people_chan_t *ret;
	channel_t *chan;
	if (!(channame && p)) return NULL;

	for (; p; p=p->next)
	{
		ret = (people_chan_t *)(p->data);
		chan = (channel_t *)(ret->chanp);
		if (chan && chan->name && (!xstrcmp(chan->name, channame) ||
					!xstrcmp(chan->name+4, channame)))
			return ret;
	}
	return NULL;
}

/* irc_add_person_int()
 *
 * this is internal function
 *
 * this function adds person given by nick to internal structures of
 * irc plugin
 *
 * @param s - current session structure
 * @param j - irc private structure of current session
 * @param nick - nickname of user without <em>'irc:'</em>
 *   prefix, and possibly with '@%+' prefix
 * @param chan - channel structure, on which nick appeared,
 *   unfortunatelly it can't be NULL in current implementation
 *
 * @return pointer to allocated people_t structure.
 */
static people_t *irc_add_person_int(session_t *s, irc_private_t *j,
		char *nick, channel_t *chan)
{
	people_t *person, *peronchan;
	people_chan_t *pch_tmp;
	userlist_t *ulist;
	window_t *w;
	int i, k, mode = 0, irccol = 0;
	char *ircnick, *modes, *t;

	k = (xstrlen(SOP(_005_PREFIX))>>1);
	modes = xmalloc(k * sizeof(char));
	for (i=0; i<k; i++) modes[i] = SOP(_005_PREFIX)[i+k+1];
	modes[i-1] = '\0';
	if ((t = xstrchr(modes, *nick)))
		mode = 1<<(k-(t-modes)-2);

	/* debug("irc_add_person_int: %s %d %d\n", modes, mode, k); */
	if (mode) nick++;

	ircnick = saprintf("%s%s", IRC4, nick);
	w = window_find_s(s, chan->name);
	/* add user to userlist of window (of a given channel) if not yet there */
	if (w && !(ulist = userlist_find_u(&(w->userlist), ircnick))) {
	/*	debug("+userlisty %d, ", mode); */
		ulist = userlist_add_u(&(w->userlist), ircnick, nick);
		irccol = irc_color_in_contacts(modes, mode, ulist);
	}

	/* add entry in private->people if nick's not yet there */
	/* ok new irc-find-person checked */
	if (!(person = irc_find_person(j->people, nick))) {
	/*	debug("+%s lista ludzi, ", nick); */
		person = xmalloc(sizeof(people_t));
		person->nick = xstrdup(ircnick);
		/* K&Rv2 5.4 */
		list_add(&(j->people), person, 0);
	}
	/* add entry in private->channels->onchan if nick's not yet there */
	if (!(peronchan = irc_find_person(chan->onchan, nick)))  {
	/*	debug("+do kana³u, "); */
		list_add(&(chan->onchan), person, 0);
	}
	xfree(ircnick);

	/* if channel's not yet on given user channels, add it to his channels */
	/* as I haven't looked here for a longer time I'm wondering is this check needed at all */
	if (!(pch_tmp = irc_find_person_chan(person->channels, chan->name)))
	{
	/*	debug("+lista kana³ów usera %08X ", person->channels); */
		pch_tmp = xmalloc(sizeof(people_chan_t));
		pch_tmp->mode = mode;
		pch_tmp->chanp = chan;
		irc_nick_prefix(j, pch_tmp, irccol);
		list_add(&(person->channels), pch_tmp, 0);
	/*	debug(" %08X\n", person->channels); */
	} //else { pch_tmp->mode = mode; }

	xfree(modes);
	return person;
}

people_t *irc_add_person(session_t *s, irc_private_t *j,
		char *nick, char *channame)
{
	channel_t *chan;
	people_t *ret;
	if (!nick)
		return NULL;

	if (!(chan = irc_find_channel(j->channels, channame)))
		/* GiM: if someone typed /quote names *
		 * and he's not on that channel... */
		return NULL;

	ret = irc_add_person_int(s, j, nick, chan);
	query_emit_id(NULL, USERLIST_REFRESH);
	return ret;
}

int irc_add_people(session_t *s, irc_private_t *j, char *names, char *channame)
{
	channel_t *chan;
	char **nick=NULL, **save, *tmp;

	if (!(channame && names))
		return -1;

	if (!(chan = irc_find_channel(j->channels, channame)))
	{
		tmp = saprintf("People on %s: %s", channame, names);
		if (session_int_get(s, "DISPLAY_IN_CURRENT")&1)
			print_window(window_current->target, s, 0, "generic",
					tmp);
		else
			print_window("__status", s, 0, "generic",
					tmp);

		return 0;
	}
	debug("[irc] add_people() %08X\n", j);
	save = nick = array_make(names, " ", 0, 1, 0);
	while (*nick) {
		irc_add_person_int(s, j, *nick, chan);
		nick++;
	}

	query_emit_id(NULL, USERLIST_REFRESH);

	array_free(save);
	return 0;	
}

static int irc_del_person_channel_int(session_t *s, irc_private_t *j, people_t *nick, channel_t *chan) {
	userlist_t *ulist = NULL;
	people_chan_t *tmp;
	window_t *w;

	if (!nick || !chan) {
		debug_error("programmer's mistake in call to irc_del_channel_int: nick: %s chan: %s\n", nick ? "OK" : "NULL", chan ? "OK" : "NULL");
		return -1;
	}
	
	/* GiM: We can't use chan->window->userlist,
	 * cause, window could be already destroyed. ;/ 
	 */
	if ((w = window_find_s(s, chan->name)))
		ulist = userlist_find_u(&(w->userlist), nick->nick);
	if (ulist) {
	/* delete from userlist 
		debug("-userlisty, "); */
		userlist_remove_u(&(w->userlist), ulist);
	}
	
	if ((tmp = irc_find_person_chan(nick->channels, chan->name))) {
	/* delete entry in private->people->channels 
		debug("-lista kana³ów usera, "); */
		list_remove(&(nick->channels), tmp, 1);
	}
	if (!(nick->channels)) {
	/* delete entry in private->people 
		debug("-%s lista ludzi, ", nick->nick); */
		LIST_REMOVE(&(j->people), nick, list_irc_people_free);
		
		list_remove(&(chan->onchan), nick, 0);
		return 1;
	}
	
	/* delete entry in private->channels->onchan
	debug("-z kana³u\n"); */
	list_remove(&(chan->onchan), nick, 0);
	return 0;
}
/* irc_del_person_channel()
 * 
 * deletes data from internal structures, when user has been kicked of or parts from a given channel
 *
 * @param s - current session structure
 * @param j - irc private structure of current session
 * @param nick - nickname of user without <em>'irc:'</em>
 *   prefix, can contain '@%+' prefix
 * @param chan - channel structure, where part/kick occured
 *
 * @return	-1 - no such channel, no such user <br />
 * 		0 - user removed from given channel <br />
 * 		1 - user removed from given channel and that was the last channel shared with that user
 */
int irc_del_person_channel(session_t *s, irc_private_t *j, char *nick, char *channame)
{
	int ret;
	people_t *person;
	channel_t *chan;
	if (!(chan = irc_find_channel(j->channels, channame)))
		return -1;
	if (!(person = irc_find_person(j->people, nick)))
		return -1;

	ret = irc_del_person_channel_int(s, j, person, chan);

	query_emit_id(NULL, USERLIST_REFRESH);
	return ret;
}

/* irc_del_person()
 *
 * delete structures associated with given user, e.g. when he
 * /quits from IRC
 *
 * @param s - current session structure
 * @param j - irc private structure of current session
 * @param nick - nickname of user without <em>'irc:'</em>
 *   prefix, can contain '@%+' prefix
 * @param chan - channel structure, where part/kick occured
 *
 * @return 	-1 - no such nickname
 * 		1 - user entry deleted from internal structures
 */
int irc_del_person(session_t *s, irc_private_t *j, char *nick,
		char *wholenick, char *reason, int doprint)
{
	people_t *person;
	people_chan_t *pech;
	window_t *w;
	list_t tmp;
	char *longnick;

	if (!(person = irc_find_person(j->people, nick))) 
		return -1;

	/* if person doesn't have any channels, we shouldn't get here
	 */
	if (! (tmp = person->channels) ) {
		debug_error("logic error in call to irc_del_person!, %s doesn't have any channels\n", nick);
		/* I'm not adding memory freeing here, since we shouldn't get here by any chance,
		 */
		return -1;
	}
	/* 
	 * GiM: removing from private->people is in
	 * 	irc_del_person_channel_int
	 *
	 * tmp is set, we can run the loop
	 */
	while (1) {
		if (!(tmp && (pech = tmp->data))) break;

		if (doprint)
			print_window(pech->chanp->name,
				s, 0, "irc_quit", session_name(s), 
				nick, wholenick, reason);

		/* if this call returns !0 it means
		 * person has been deleted, so let's break
		 * the loop
		 */
		if (irc_del_person_channel_int(s, j, person, pech->chanp))
			break;

		tmp = person->channels;
	}

	longnick = saprintf("%s%s", IRC4, nick);
	w = window_find_s(s, longnick);
	if (w) {
		if (session_int_get(s, "close_windows") > 0) {
			debug("[irc] del_person() window_kill(w, 1); %s\n", w->target);
			window_kill(w);
		}
		if (doprint)
			print_window(longnick,s, 0, "irc_quit",
					session_name(s), nick,
					wholenick, reason);
	}
	xfree(longnick);

	query_emit_id(NULL, USERLIST_REFRESH);
	return 0;
}

int irc_del_channel(session_t *s, irc_private_t *j, char *name)
{
	list_t p;
	channel_t *chan;
	char *tmp;
	window_t *w;

	if (!(chan = irc_find_channel((j->channels), name)))
		return -1;

	debug("[irc]_del_channel() %s\n", name);
	while ((p = (chan->onchan)))
		if (!(p->data)) break;
		else irc_del_person_channel_int(s, j, (people_t *)p->data, chan);

	tmp = chan->name;	chan->name = NULL;
	xfree(chan->topic);
	xfree(chan->topicby);
	xfree(chan->mode_str);
	list_destroy(chan->banlist, 1);

	/* GiM: because we check j->channels in our kill-window handler
	 * this must be done, before, we'll try to kill_window.... */
	list_remove(&(j->channels), chan, 1);
	
	w = window_find_s(s, tmp);
	if (w && (session_int_get(s, "close_windows") > 0)) {
		debug("[irc]_del_channel() window_kill(w); %s\n", w->target);
		window_kill(w);
	}
	xfree(tmp);

	query_emit_id(NULL, USERLIST_REFRESH);
	return 0;
}


static int irc_sync_channel(session_t *s, irc_private_t *j, channel_t *p) 
{
	p->syncmode = 2;
	/* to ma sie rownac ile ma byc roznych syncow narazie tylko WHO
	 * ale moze bedziemy syncowac /mode +b, +e, +I) */
	gettimeofday(&(p->syncstart), NULL);
	watch_write(j->send_watch, "WHO %s\r\n", p->name+4);
	watch_write(j->send_watch, "MODE %s +b\r\n", p->name+4);
	return 0;
}


channel_t *irc_add_channel(session_t *s, irc_private_t *j, char *name, window_t *win)
{
	channel_t *p;
	p = irc_find_channel(j->channels, name);
	if (!p) {
		p		= xmalloc(sizeof(channel_t));
		p->name		= saprintf("%s%s", IRC4, name);
		p->window	= win;
		debug("[irc] add_channel() WINDOW %08X\n", win);
		if (session_int_get(s, "auto_channel_sync") != 0)
			irc_sync_channel(s, j, p);
		list_add(&(j->channels), p, 0);
		return p;
	}
	return NULL;
}

int irc_color_in_contacts(char *modes, int mode, userlist_t *ul)
{
	int  i, len;
	len = xstrlen(modes);

	/* GiM: this could be done much easier on intel ;/ */
	for (i=0; i<len; i++)
		if (mode & (1<<(len-1-i))) break;
	

	switch (i) {
		case 0:	ul->status = EKG_STATUS_AVAIL;		break;
		case 1:	ul->status = EKG_STATUS_AWAY;		break;
		case 2:	ul->status = EKG_STATUS_XA;		break;
		case 3:	ul->status = EKG_STATUS_INVISIBLE;	break;
		default:ul->status = EKG_STATUS_ERROR;		break;
	}
	return i;
}

int irc_nick_prefix(irc_private_t *j, people_chan_t *ch, int irc_color)
{
	char *t = SOP(_005_PREFIX);
	char *p = xstrchr(t, ')');
	*(ch->sign)=' ';
	(ch->sign)[1] = '\0';
	if (p) {
		p++;
		if (irc_color < xstrlen(p))
			*(ch->sign) = p[irc_color];
	} 
	return 0;
}
		
/* irc_nick_change()
 *
 * this is internal function called when give person changes nick
 *
 * @param s - current session structure
 * @param j - irc private structure of current session
 * @param old - old nickname of user without <em>'irc:'</em>
 *   prefix, and WITHOUT '@%+' prefix
 * @param new - new nickname of user without <em>'irc:'</em>
 *   prefix, and WITHOUT '@%+' prefix
 *
 * @return	0
 */
int irc_nick_change(session_t *s, irc_private_t *j, char *old, char *new)
{
	userlist_t *ulist, *newul;
	list_t i;
	people_t *per;
	people_chan_t *pch;
	window_t *w;
	char *t1, *t2 = saprintf("%s%s", IRC4, new);

	if (!(per = irc_find_person(j->people, old))) {
		debug_error("irc_nick_change() person not found?\n");
		xfree(t2);
		return 0;
	}

	for (i=s->userlist; i; i = i->next) {
		userlist_t *u = i->data;
		list_t m;

		for (m = u->resources; m; m = m->next) {
			ekg_resource_t *r = m->data;

			if (r->private != per) continue;

			xfree(r->name);
			r->name = xstrdup(t2);
			/* XXX, here. readd to list, coz it'll be bad sorted. :( */
			break;
		}
	}

	debug("[irc] nick_change():\n");
	for (i=per->channels; i; i=i->next)
	{
		pch = (people_chan_t *)i->data;
		w = window_find_s(s, pch->chanp->name);
		if (w && (ulist = userlist_find_u(&(w->userlist), old))) {
			newul = userlist_add_u(&(w->userlist), t2, new);
			newul->status = ulist->status;
			userlist_remove_u(&(w->userlist), ulist);
			/* XXX dj, userlist_replace() */
			/* GiM: Yes, I thought about doin' this 'in place'
			 * but we would have to change position in userlist
			 * to still keep it sorted, so I've chosen to do this
			 * this way */
		}
	}

	query_emit_id(NULL, USERLIST_REFRESH);

	t1 = per->nick;
	per->nick = t2;
	xfree(t1);
	return 0;
}

/* GiM: nope, people will never be free ;/ */
int irc_free_people(session_t *s, irc_private_t *j)
{
	list_t t1;
	people_t *per;
	channel_t *chan;
	window_t *w;

	debug("[irc] free_people() %08X %s\n", s, s->uid);
	for (t1=j->people; t1; t1=t1->next) {
		per = (people_t *)t1->data;
		list_destroy(per->channels, 1);
		per->channels=NULL;
	}

	for (t1=j->channels; t1; t1=t1->next) {
		chan = (channel_t *)t1->data;
		list_destroy(chan->onchan, 0);
		chan->onchan = NULL;

		/* GiM: check if window isn't allready destroyed */
		w = window_find_s(s, chan->name);
		if (w && w->userlist)
			userlist_free_u(&(w->userlist));
		/* 
		 * window_kill(chan->window, 1);
		 */
	}

	LIST_DESTROY(j->people, list_irc_people_free);
	j->people = NULL;

	LIST_DESTROY(j->channels, list_irc_channel_free);
	j->channels = NULL;

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
