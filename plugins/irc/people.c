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

#include <stdio.h>

#include <ekg/xmalloc.h>

//#include <ekg/commands.h>
//#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
//#include <ekg/stuff.h>
//#include <ekg/themes.h>
#include <ekg/userlist.h>
//#include <ekg/windows.h>


#include "people.h"
#include "irc.h"


/* tmp = private->channels || private->people->channels->onchan */
people_t *irc_find_person(list_t p, char *nick)
{
	people_t *person = NULL;
	if (!(nick && p)) return NULL;
	if (*nick=='+' || *nick=='@') nick++;

	for (; p; p=p->next)
	{
		person = (people_t *)(p->data);
		if (person->nick && (!xstrcmp(person->nick, nick) ||
					!xstrcmp((person->nick)+4, nick)))
			return person;
	}
	return NULL;
}

/* p = private->channel || */
channel_t *irc_find_channel(list_t p, char *channame)
{
	channel_t *chan = NULL;
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
	people_chan_t *ret = NULL;
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

int irc_add_person_int(irc_private_t *j, char *nick, channel_t *chan)
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
	
	if (mode) nick++;
	ircnick = saprintf("%s%s", IRC4, nick);
	w = window_find(chan->name);
	if (w && !(ulist = userlist_find_u(&(w->userlist), ircnick))) {
	/* add entry in window's userlist
		debug("+userlisty, "); */
		ulist = userlist_add_u(&(w->userlist), ircnick, nick);
		irccol = irc_color_in_contacts(modes, mode, ulist);
	}

	if (!(person = irc_find_person(j->people, nick))) {
	/* add entry in private->people 
		debug("+%s lista ludzi, ", nick); */
		person = xmalloc(sizeof(people_t));
		person->nick = xstrdup(ircnick);
		person->channels=NULL;
		list_add(&(j->people), person, 0);
	}
	if (!(peronchan = irc_find_person(chan->onchan, nick)))  {
	/* add entry in private->channels->onchan
		debug("+do kana³u, "); */
		list_add(&(chan->onchan), person, 0);
	}
	xfree(ircnick);

	if (!(pch_tmp = irc_find_person_chan(person->channels, chan->name)))
	{
	/* add entry in private->people->channels
		debug("+lista kana³ów usera\n"); */
		pch_tmp = xmalloc(sizeof(people_chan_t));
		pch_tmp->mode = mode;
		pch_tmp->chanp = chan;
		irc_nick_prefix(j, pch_tmp, irccol);
		list_add(&(person->channels), pch_tmp, 0);
	} //else { pch_tmp->mode = mode; }

	xfree(modes);
	return 0;
}

int irc_add_person(session_t *s, irc_private_t *j, char *nick, char *channame)
{
	channel_t *chan;
	if (!nick) return -1;
	
	if (!(chan = irc_find_channel(j->channels, channame)))
		/* GiM: if someone typed /names *
		 * and he's not on that channel... */
		return -1;

	//query_emit(NULL, "userlist-changed", __session, __uid);
	return irc_add_person_int(j, nick, chan);
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
	debug("[irc] add_people()\n");
	save = nick = array_make(names, " ", 0, 1, 0);
	while (*nick) {
		irc_add_person_int(j, *nick, chan);
		nick++;
	}
	array_free(save);
	return 0;	
}

int irc_del_person_channel_int(irc_private_t *j, people_t *nick, channel_t *chan)
{
	userlist_t *ulist = NULL;
	people_chan_t *tmp;
	window_t *w;
	char *chtmp;
	if (!(nick && chan))
		return -1;
	
	/* GiM: We can't use chan->window->userlist,
	 * cause, window could be already destroyed. ;/ 
	 */
	if ((w = window_find(chan->name)))
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
		chtmp = nick->nick; nick->nick=NULL;
		list_remove(&(j->people), nick, 1);
		xfree(chtmp);
		
		list_remove(&(chan->onchan), nick, 0);
		return 1;
	}
	
	/* delete entry in private->channels->onchan
	debug("-z kana³u\n"); */
	list_remove(&(chan->onchan), nick, 0);
	return 0;
}

int irc_del_person_channel(session_t *s, irc_private_t *j, char *nick, char *channame)
{
	people_t *person;
	channel_t *chan;
	if (!(chan = irc_find_channel(j->channels, channame)))
		return -1;
	if (!(person = irc_find_person(j->people, nick)))
		return -1;

	return irc_del_person_channel_int(j, person, chan);
}

int irc_del_person(session_t *s, irc_private_t *j, char *nick,
		char *wholenick, char *reason, int doprint)
{
	people_t *person;
	people_chan_t *pech;
	list_t tmp;

	if (!(person = irc_find_person(j->people, nick))) 
		return -1;

	while ((tmp = (person->channels))) {
		if (!(tmp && (pech = tmp->data))) break;

		if (doprint)
			print_window(pech->chanp->name,
				s, 0, "irc_quit", session_name(s), 
				nick, wholenick, reason);

		if (irc_del_person_channel_int(j, person, pech->chanp))
			break;
	}
	/* GiM: removing from private->people is in
	 * irc_del_person_channel_int
	 */
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

	debug("[irc] del_channel() %s\n", name);
	while ((p = (chan->onchan)))
		if (!(p->data)) break;
		else irc_del_person_channel_int(j, (people_t *)p->data, chan);

	xfree(chan->topic);
	/* GiM: because we check j->channels in our kill-window handler
	 * this must be done, before, we'll try to kill_window.... */
	list_remove(&(j->channels), chan, 1);
	
	tmp = saprintf("%s%s", IRC4, name);
	w = window_find(tmp);
	if (w) {
		debug("[irc] del_channel() window_kill(w, 1); %s\n", w->target);
		window_kill(w, 0);
		window_switch(window_current->id);
	}
	xfree(tmp);

	return 0;
}

channel_t *irc_add_channel(session_t *s, irc_private_t *j, char *name, window_t *win)
{
	channel_t *p;
	p = irc_find_channel(j->channels, name);
	if (!p) {
		p = xmalloc(sizeof(channel_t));
		p->name = saprintf("%s%s", IRC4, name);
		p->mode = 0;
		p->window = win;
		p->topic = NULL;
		debug("[irc] add_channel() WINDOW %08X\n", win);
		p->onchan = NULL;
		list_add(&(j->channels), p, 0);
		return p;
	}
	return NULL;
}

int irc_color_in_contacts(char *modes, int mode, userlist_t *ul)
{
	int  i, len;
	char *tmp;
	len = xstrlen(modes);

	/* GiM: this could be done much easier on intel ;/ */
	for (i=0; i<len; i++)
		if (mode & (1<<(len-1-i))) break;
	tmp=ul->status;
	switch (i)
	{
		case 0:
			ul->status = xstrdup(EKG_STATUS_AVAIL);
			break;
		case 1:
			ul->status = xstrdup(EKG_STATUS_AWAY);
			break;
		case 2:
			ul->status = xstrdup(EKG_STATUS_XA);
			break;
		case 3:
			ul->status = xstrdup(EKG_STATUS_INVISIBLE);
			break;
		default:
			ul->status = xstrdup(EKG_STATUS_ERROR);
			break;
	}
	xfree(tmp);
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
		
int irc_nick_change(session_t *s, irc_private_t *j, char *old, char *new)
{
	userlist_t *ulist, *newul;
	list_t i;
	people_t *per;
	people_chan_t *pch;
	window_t *w;
	char *t1, *t2 = saprintf("%s%s", IRC4, new);

	if (!(per = irc_find_person(j->people, old))) {
		xfree(t2);
		return 0;
	}
	debug("[irc] nick_change():\n");
	for (i=per->channels; i; i=i->next)
	{
		pch = (people_chan_t *)i->data;
		w = window_find_s(s, pch->chanp->name);
		if (w && (ulist = userlist_find_u(&(w->userlist), old))) {
			newul = userlist_add_u(&(w->userlist), t2, new);
			newul->status = xstrdup(ulist->status);
			userlist_remove_u(&(w->userlist), ulist);
			/* GiM: Yes, I thought about doin' this 'in place'
			 * but we would have to change position in userlist
			 * to still keep it sorted, so I've chosen to do this
			 * this way */
		}
	}
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
	userlist_t *ulist;
	window_t *w;

	debug("[irc] free_people()\n");
	for (t1=j->people; t1; t1=t1->next)
	{
		per = (people_t *)t1->data;
		list_destroy(per->channels, 1);
		per->channels=NULL;
	}
	for (t1=j->channels; t1; t1=t1->next)
	{
		chan = (channel_t *)t1->data;
		list_destroy(chan->onchan, 0);
		chan->onchan = NULL;
		
		/* GiM: check if window isn't allready destroyed */
		w = window_find_s(s, chan->name);
		if (w && w->userlist)
			userlist_free_u(&(chan->window->userlist));
		/* 
		 * window_kill(chan->window, 1);
		 */
	}
	for (t1=j->people; t1; t1=t1->next)
		xfree(((people_t *)(t1->data))->nick);
	for (t1=j->channels; t1; t1=t1->next) {
		xfree(((channel_t *)(t1->data))->name);
	}

	list_destroy(j->people, 1);
	j->people = NULL;
	list_destroy(j->channels, 1);
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
