/* $Id$ */

/*
 *  (C) Copyright 2003-2005 Tomasz Torcz <zdzichu@irc.pl>
 *                          Leszek Krupiñski <leafnode@wafel.com>
 *                          Adam Kuczyñski <dredzik@ekg2.org>
 *                          Adam Mikuta <adamm@ekg2.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * ideas:
 * - zapamietywanie ostaniej sciezki zwiazanej z danym oknem
 *   w ten sposob bedzie mozna logowac nawet w przypadku niekompletnych
 *   danych uniemo¿liwiaj±cych stworzenie ¶cie¿ki wg. log_path, np
 *   przy zmianach statusu
 */


#include <stdint.h>
#include <stdlib.h>

#include "ekg2-config.h"

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/log.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/themes.h> //print()
#include <ekg/vars.h>
#include <ekg/windows.h>
#include <ekg/userlist.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <errno.h>
#include <string.h>

#include "main.h"



PLUGIN_DEFINE(logs, PLUGIN_LOG, NULL);

QUERY(logs_setvar_default)
{
	xfree(config_logs_path);
	xfree(config_logs_timestamp);
	config_logs_path = xstrdup("~/.ekg2/logs/%S/%u");
	config_logs_timestamp = NULL;
	return 0;
}

int logs_plugin_init(int prio)
{
	plugin_register(&logs_plugin, prio);

        logs_setvar_default(NULL, NULL);

        query_connect(&logs_plugin, "set-vars-default", logs_setvar_default, NULL);
	query_connect(&logs_plugin, "protocol-message-post", logs_handler, NULL);
	query_connect(&logs_plugin, "ui-window-new", logs_handler_newwin, NULL);
	query_connect(&logs_plugin, "protocol-status", logs_status_handler, NULL);

	variable_add(&logs_plugin, "remind_number", VAR_INT, 1, &config_logs_remind_number, NULL, NULL, NULL);
	variable_add(&logs_plugin, "log", VAR_MAP, 1, &config_logs_log, NULL, variable_map(3, 0, 0, "none", 1, 2, "simple", 2, 1, "xml"), NULL);
	variable_add(&logs_plugin, "log_ignored", VAR_INT, 1, &config_logs_log_ignored, NULL, NULL, NULL);
	variable_add(&logs_plugin, "log_status", VAR_BOOL, 1, &config_logs_log_status, NULL, NULL, NULL);
	variable_add(&logs_plugin, "path", VAR_DIR, 1, &config_logs_path, NULL, NULL, NULL);
	variable_add(&logs_plugin, "timestamp", VAR_STR, 1, &config_logs_timestamp, NULL, NULL, NULL);

	debug("[logs] plugin registered\n");

	return 0;
}

static int logs_plugin_destroy()
{
	// zapominamy wszystko
	if (logs_reminded)
		list_destroy(logs_reminded, 1);

	plugin_unregister(&logs_plugin);

	debug("[logs] plugin unregistered\n");

	return 0;
}


/*
 * przygotowanie nazwy pliku bez rozszerzenia
 * %S - sesja nasza
 * %u - u¿ytkownik (uid), z którym piszemy
 * %U - u¿ytkownik (nick)   -||-
 * %Y, %M, %D - rok, miesi±c, dzieñ
 * zwraca ¶cie¿kê, która nale¿y rêcznie zwolniæ przez xfree()
 */

char *logs_prepare_path(session_t *session, char *uid, char **rcpts, char *text, time_t sent, int class)
{
	char *path, *tmp, *uidtmp, datetime[5];
	struct tm *tm = localtime(&sent);
	string_t buf = string_init(NULL);

	if (!(tmp = config_logs_path))
		return NULL;

	while (*tmp) {
		if ((char)*tmp == '%' && (tmp+1) != NULL) {
			switch (*(tmp+1)) {
				case 'S':	string_append_n(buf, session->uid, -1);
						break;
				case 'u':	if (class == EKG_MSGCLASS_SENT ||
						    class == EKG_MSGCLASS_SENT_CHAT)
							uidtmp = xstrdup(get_uid(session, rcpts[0]));
						else
							uidtmp = xstrdup(get_uid(session, uid));
						goto attach; /* avoid code duplication */
				case 'U':	if (class == EKG_MSGCLASS_SENT ||
						    class == EKG_MSGCLASS_SENT_CHAT)
							uidtmp = xstrdup(get_nickname(session, rcpts[0]));
						else
							uidtmp = xstrdup(get_nickname(session, uid));
					attach:
						if (xstrchr(uidtmp, '/'))
							*(xstrchr(uidtmp, '/')) = 0; // strip resource
						string_append_n(buf, uidtmp, -1);
						xfree(uidtmp);
						break;
				case 'Y':	snprintf(datetime, 5, "%4d", tm->tm_year+1900);
						string_append_n(buf, datetime, 4);
						break;
				case 'M':	snprintf(datetime, 3, "%02d", tm->tm_mon+1);
						string_append_n(buf, datetime, 2);
						break;
				case 'D':       snprintf(datetime, 3, "%02d", tm->tm_mday);
						string_append_n(buf, datetime, 2);
						break;
				default:	string_append_c(buf, *(tmp+1));
			};

			tmp++;
		} else if (*tmp == '~' && (*(tmp+1) == '/' || *(tmp+1) == '\0')) {
			const char *home = getenv("HOME");
			string_append_n(buf, (home ? home : "."), -1);
			//string_append_c(buf, '/');
		} else
			string_append_c(buf, *tmp);
		tmp++;
	};

	// sanityzacja sciezki - wywalic "../", zamienic znaki spec. na inne
	// zamieniamy szkodliwe znaki na minusy, spacje na podkreslenia
	// TODO
	xstrtr(buf->str, ' ', '_');

	path = string_free(buf, 0);

	return path;
}


/*
 * otwarcie pliku do zapisu/odczytu
 * tworzy wszystkie katalogi po drodze, je¶li nie istniej± i mkdir =1
 * ext - rozszerzenie jakie nadac
 * zwraca numer deskryptora b±d¼ NULL
 */

FILE* logs_open_file(char *path, char *ext, int makedir)
{
	struct stat statbuf;
	char *dir, *fullname, *slash;
	int slash_pos = 0;
	int pos;
	FILE* fdesc;

        debug("[logs] opening log file\n");

	while (makedir) {
		if (!(slash = xstrchr(path + slash_pos, '/'))) {
			// nie ma juz slashy - zostala tylko nazwa pliku
			makedir = 0; // konczymy petle
			continue;
		};

		slash_pos = slash - path + 1;
		dir = xstrndup(path, slash_pos);

		if (stat(dir, &statbuf) != 0 && mkdir(dir, 0700) == -1) {
			char *bo = saprintf("nie mo¿na %s bo %s", dir, strerror(errno));
			print("generic",bo); // XXX usun±æ !! 
			xfree(bo);
			xfree(dir);
			return NULL;
		}
		xfree(dir);
	} // while mkdir

	if (ext)
		fullname = saprintf("%s.%s", path, ext);
	else
		fullname = xstrdup(path);

	/*
	 * better way - if xml, prepare xml file
	 */
	if (config_logs_log == 2) {
		fdesc = fopen(fullname, "r");

		if (fdesc == NULL) {
			fdesc = fopen(fullname, "a+");
			fputs("<?xml version=\"1.0\"?>\n", fdesc);
			fputs("<!DOCTYPE ekg2log PUBLIC \"-//ekg2log//DTD ekg2log 1.0//EN\" ", fdesc);
			fputs("\"http://www.ekg2.org/DTD/ekg2log.dtd\">\n", fdesc);
			fputs("<ekg2log xmlns=\"http://www.ekg2.org/DTD/\">\n", fdesc);
			fclose(fdesc);
		} else {
			fseek(fdesc, 0, SEEK_END);
			pos = ftell(fdesc);
			fclose(fdesc);
			truncate(fullname, pos - 11); //very ugly - 11 is charcount of </ekg2log>\n
		}
	}

	fdesc = fopen(fullname, "a+");
	xfree(fullname);

	return fdesc;
}

/*
 * przygotowuje timstamp do wstawienia do logów
 */

char * prepare_timestamp(time_t ts)
{
	struct tm *tm = localtime(&ts);
	char * buf;
	buf = xmalloc(101);
	if (config_logs_timestamp) {
		strftime(buf, 100, config_logs_timestamp, tm);
		return buf;
	} else {
		return xstrcpy(buf, itoa(ts));
	}
}

/**
 * glowny handler
 */

QUERY(logs_handler)
{
	char **__session = va_arg(ap, char**), *session = *__session; // session name
	char     **__uid = va_arg(ap, char**), *uid = *__uid;
	char  ***__rcpts = va_arg(ap, char***), **rcpts = *__rcpts;
	char    **__text = va_arg(ap, char**), *text = *__text;
	uint32_t **__format = va_arg(ap, uint32_t**), *format = *__format;
	time_t   *__sent = va_arg(ap, time_t*), sent = *__sent;
	int     *__class = va_arg(ap, int*), class = *__class;
	char     **__seq = va_arg(ap, char**), *seq = *__seq;
	session_t *s = session_find(session); // session pointer
	const char *log_formats;
	char *path;

	if (config_logs_log == 0)
		return 0;

	/* well, 'format' is unused, so silence the warning */
	format = NULL;

	if (class & EKG_NO_THEMEBIT) class -= EKG_NO_THEMEBIT; /** stupid **/

	if (!session)
		return 0;

	if (!(log_formats = session_get(s, "log_formats")))
		return 0;

	if (!(path = logs_prepare_path(s, uid, rcpts, text, sent, class)))
		return 0;

	debug("[logs] logging to file %s\n", path);

	if (config_logs_log == 1 && xstrstr(log_formats, "simple")) {
		debug("[logs] logging simple\n");
		logs_simple(path, session, 
			((class == EKG_MSGCLASS_SENT || class == EKG_MSGCLASS_SENT_CHAT) ? rcpts[0] : uid), 
			text, sent, class, seq, (uint32_t)NULL, (uint16_t)NULL, (char*)NULL, (char*)NULL);
	} else if (config_logs_log == 2 && xstrstr(log_formats, "xml")) {
		debug("[logs] logging xml\n");
		logs_xml(path, session, 
			((class == EKG_MSGCLASS_SENT || class == EKG_MSGCLASS_SENT_CHAT) ? rcpts[0] : uid), 
			text, sent, class, seq);
	}


	// itd. dla innych formatow logow

	xfree(path);
	return 0;
}


/*
 * status handler
 */

QUERY(logs_status_handler)
{
	char **__session = va_arg(ap, char**), *session = *__session; // session name
	char     **__uid = va_arg(ap, char**), *uid = *__uid;
        char **__status = va_arg(ap, char**), *status = *__status;
        char **__descr = va_arg(ap, char**), *descr = *__descr;
	session_t *session_class = session_find(session);
	session_t *s = session_find(session); // session pointer
	userlist_t *userlist = userlist_find(session_class, uid);
	const char *log_formats;
	char *path;
	uint32_t ip=userlist?userlist->ip:0;
	uint16_t port=userlist?userlist->port:0;

	if (!config_logs_log_status)
		return 0;
	
	debug("[logs] logging status\n");

	if (descr == NULL)
		descr = "";

	if (!session)
		return 0;

	if (!(log_formats = session_get(s, "log_formats")))
		return 0;

	if (!(path = logs_prepare_path(s, uid, 0, descr, time(NULL), 6)))
		return 0;
	
	debug("[logs] logging to file %s\n", path);

	if (config_logs_log == 1 && xstrstr(log_formats, "simple")) {
		debug("[logs] logging simple\n");
		logs_simple(path, session, uid, status, time(NULL), 6, 0, ip, port, status, descr);
	}/*TODO else if (config_logs_log == 2 && xstrstr(log_formats, "xml")) {
		debug("[logs] logging xml\n");
		logs_xml(path, session, 
			((class == EKG_MSGCLASS_SENT || class == EKG_MSGCLASS_SENT_CHAT) ? rcpts[0] : uid), 
			text, sent, class, seq);
	}*/


	xfree(path);
	return 0;
}


/*
 * przypomina ostanie logs:remind_number wiadomosci
 * z najmlodszego logu
 */

QUERY(logs_handler_newwin)
{
	//window_t *__result = va_arg(ap, window_t*), result = *__result;

	if (config_logs_remind_number <= 0)
		return 0;

	// otwarcie najm³odszego

	// wczytanie wiadomosci
	// foreach list_add (logs_reminded)
	// query_emit "protocol-message"
	// usunac liste
	return 0;
}

/*
 * zapis w formacie znanym z ekg1
 * typ,uid,nickname,timestamp,{timestamp wyslania dla odleglych}, text
 */

void logs_simple(char *path, char *session, char *uid, char *text, time_t sent, int class, char *seq, uint32_t ip, uint16_t port, char *status, char *descr)
{
	FILE *file;
	char *textcopy = log_escape(text);
	char *descrcopy = log_escape(descr);

	char *timestamp = prepare_timestamp((time_t)time(0));
	char *senttimestamp = prepare_timestamp(sent);
	session_t *s = session_find((const char*)session);
	char *gotten_uid = get_uid(s, uid);
	char *gotten_nickname = get_nickname(s, uid);

	if ( gotten_uid == NULL )
		gotten_uid = uid;

	if ( gotten_nickname == NULL )
		gotten_nickname = uid;

	if (!(file = logs_open_file(path, "txt", 1)) || !s) {
		xfree(senttimestamp);
		xfree(timestamp);
		xfree(textcopy);
		return;
	}
	
	if (class!=6){
		switch ((enum msgclass_t)class) {
			case EKG_MSGCLASS_MESSAGE	: fputs("msgrecv", file);
							  break;
			case EKG_MSGCLASS_CHAT		: fputs("chatrecv", file);
							  break;
			case EKG_MSGCLASS_SENT		: fputs("msgsend", file);
							  break;
			case EKG_MSGCLASS_SENT_CHAT	: fputs("chatsend", file);
							  break;
			case EKG_MSGCLASS_SYSTEM	: fputs("msgsystem", file);
							  break;
			default				: fputs("chatrecv", file);
							  break;

		};
	}else{
		fputs("status",file);
	}
	
	fputc(',', file);

	/*
	 * chatsend,<numer>,<nick>,<czas>,<tre¶æ>
	 * chatrecv,<numer>,<nick>,<czas_otrzymania>,<czas_nadania>,<tre¶æ>
	 * status,<numer>,<nick>,<ip>,<time>,<status>,<descr>
	 */

	fputs(gotten_uid, file);      fputc(',', file);
	fputs(gotten_nickname, file); fputc(',', file);
	if (class==6) {
		fputs(inet_ntoa((struct in_addr){ip}), file);
	       	fputc(':', file);
		fputs(itoa(port), file); 
	       	fputc(',', file);
	}

	fputs(timestamp, file); fputc(',', file);
	
	if (class==6) {
		fputs(status, file); fputc(',', file);
		fputs(descrcopy, file);
	}
	
	if (class == EKG_MSGCLASS_MESSAGE || class == EKG_MSGCLASS_CHAT) {
		fputs(senttimestamp, file);
		fputc(',', file);
	}
	
	if (class!=6)
		fputs(textcopy, file);
	fputs("\n", file);

	xfree(senttimestamp);
	xfree(timestamp);
	xfree(textcopy);
	xfree(descrcopy);
	fclose(file);
}


/*
 * zapis w formacie xml
 */

void logs_xml(char *path, char *session, char *uid, char *text, time_t sent, int class, int seq)
{
	FILE *file;
	char *textcopy = xml_escape(text);
	char *timestamp = prepare_timestamp((time_t)time(0));
	char *senttimestamp = prepare_timestamp(sent);
	session_t *s = session_find((const char*)session);
	char *gotten_uid = xml_escape(get_uid(s, uid));
	char *gotten_nickname = xml_escape(get_nickname(s, uid));

	if (gotten_uid == NULL)
		gotten_uid = uid;

	if (gotten_nickname == NULL)
		gotten_nickname = uid;

	if (!(file = logs_open_file(path, "xml", 1)) || !s) {
		xfree(senttimestamp);
		xfree(timestamp);
		xfree(textcopy);
		return ;
	}

	/*
	 * <message class="chatsend">
	 * <time>
	 *	<sent>...</sent>
	 *	<received>...</received>
	 * </time>
	 * <sender>
	 *	<uid>...</uid>
	 *	<nick>...</nick>
	 * </sender>
	 * <body>
	 *	(#PCDATA)
	 * </body>
	 * </message>
	 */

	fputs("<message class=\"",file);

	switch ((enum msgclass_t)class) {
		case EKG_MSGCLASS_MESSAGE	: fputs("msgrecv", file);
						  break;
		case EKG_MSGCLASS_CHAT		: fputs("chatrecv", file);
						  break;
		case EKG_MSGCLASS_SENT		: fputs("msgsend", file);
						  break;
		case EKG_MSGCLASS_SENT_CHAT	: fputs("chatsend", file);
						  break;
		case EKG_MSGCLASS_SYSTEM	: fputs("msgsystem", file);
						  break;
		default				: fputs("chatrecv", file);
						  break;

	};

	fputs("\">\n", file);

	fputs("\t<time>\n", file);
	fputs("\t\t<received>", file); fputs(timestamp, file); fputs("</received>\n", file);
	if (class == EKG_MSGCLASS_MESSAGE || class == EKG_MSGCLASS_CHAT) {
		fputs("\t\t<sent>", file); fputs(timestamp, file); fputs("</sent>\n", file);
	}
	fputs("\t</time>\n", file);

	fputs("\t<sender>\n", file);
	fputs("\t\t<uid>", file);   fputs(gotten_uid, file);       fputs("</uid>\n", file);
	fputs("\t\t<nick>", file);  fputs(gotten_nickname, file);  fputs("</nick>\n", file);
	fputs("\t</sender>\n", file);

	fputs("\t<body>\n", file);
	fputs(textcopy, file);
	fputs("\t</body>\n", file);

	fputs("</message>\n", file);

	fputs("</ekg2log>\n", file);

	xfree(senttimestamp);
	xfree(timestamp);
	xfree(textcopy);
	fclose(file);
}

/*
 * zapis w formacie gaim'a
 */

void logs_gaim()
{
}


/* interesujace nas wydarzenia */
// query_connect "protocol-message"
//? query_connect "presence??"
// nowe okno


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=8 sw=8 noexpandtab
 */
