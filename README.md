EKG2
====

Multiplatform, multiprotocol, plugin-based instant messanger with (GTK2 GUI || console UI)!

EKG2 is OpenSource IM application for Unix systems (Linux, *BSD, SunOS, Solaris, MacOS, X, BeOS) available under the terms of GPL.

EKG2 is plugin-based, because of this it can support many different protocols. It can also support different GUIs! Below is a list of available plugins.

Protocol plugins:

* Jabber - (XMPP, gtalk and Tlen support),
* GG (gadu-gadu) - (using libgadu, the most popular polish IM protocol),
* IRC - (mostly IRCnet oriented),

GUI plugins:
* GTK2 - under development,
* ncurses - primary ekg2 ui, console
* readline

Logging facility:
* logs - multi plugin, that allows logging in irssi like format, xml, simple and raw,
* logsqlite - logging in sqlite db file,
* logsoracle - logging in oracle db

Script languages bindings: Python, Perl, Ruby (under development)

Other superb plugins:
* dbus - basic dbus support (allows you creating superb now-playing ;) !),
* sim (using openssl) & gpg & rot ;) - these plugins allow encrypting supported protocols,
* xosd - on screen display,
* rc & httprc_xajax - rc plugins allows remote control over ekg2,
* xmsg - simple messaging through filesystem (e.g. notifications from other apps),
* jogger - allows to manipulate JoggerBot via ekg2,
* mail - checking local maildir/mailbox for new messages,
* sms - sms sending,
* sniff - built-in sniffer for analyzing protocols

There are also few others not mentioned here!