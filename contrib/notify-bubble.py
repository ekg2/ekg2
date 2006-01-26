#!/usr/bin/python
# 
# (c) 2006 Tomasz Torcz <zdzichu@irc.pl>
#
# Displays notify bubble when someone writes to us or becomes available.
# Ingredients needed: 
#- dbus
#- libnotify & notification-daemon from http://galago-project.org/ 
#
# This script is very basic - could use some more intelligence
#

import dbus
import ekg

bus = dbus.SessionBus()
obj = bus.get_object ("org.freedesktop.Notifications", "/org/freedesktop/Notifications")
notif = dbus.Interface (obj, "org.freedesktop.Notifications")

def status_handler(session, uid, status, desc):
	if status == ekg.STATUS_AVAIL:
		current_encoding = ekg.config["jabber:console_charset"] or "iso-8859-2"
		
		user = ekg.session_get(session).user_get(uid)
		uname = unicode(user.nickname or uid, current_encoding)

		htxt  = uname + u" jest dostepn"
		htxt += (uname[-1] == "a" or uname[-1] == "A") and "a" or "y"

		if not desc: desc = ""
		else: desc = unicode(desc, current_encoding)
		timeout = 2000 + len(desc)*50

# (s appname, u id, s icon, s header, s body, as actions a{sv} hints, i timeout_ms)
		notif.Notify (dbus.String("ekg2"), dbus.UInt32(0), "",
			dbus.String(htxt), dbus.String(desc),
			dbus.String(""), {}, dbus.Int32(timeout) )

		return 1

def message_handler(session, uid, type, text, sent_time, ignore_level):
	current_encoding = ekg.config["jabber:console_charset"] or "iso-8859-2"
		
	user = ekg.session_get(session).user_get(uid)
	uname = unicode(user.nickname or uid, current_encoding)
	
	htxt = uname + ":"
	msg = unicode(text[0:127], current_encoding)
		
	timeout = 2000 + len(msg)*50

	notif.Notify (dbus.String("ekg2"), dbus.UInt32(0), "",
		dbus.String(htxt), dbus.String(msg),
		dbus.String(""), {}, dbus.Int32(timeout) )

	return 1

ekg.handler_bind('protocol-status', status_handler)
ekg.handler_bind('protocol-message-received', message_handler)
