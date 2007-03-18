#!/bin/sh

# XXX, INPUT_ENCODING nie wiedziec czemu nie dziala.
# 	dlatego tymczasowo OUTPUT_LANGUAGE jest Polish
#	co powoduje zmiane kodowania strony na ISO-8859-2
#
#

# XXX, nie dziala. dlatego jest PREDERFINED ze skopiowanymi deklaracjami z odpowiednich .h 
#	w sumie niby ok, ale musimy pilnowac deklaracji... BTW. nie wiem w sumie czy jest jakis
#	duzy sens aby deklrarowac kazda funkcje... @param quiet - czy byc cicho...
#	Troche wiecej sensu jest przy deklarowaniu juz jakie mozliwe parametry sa w name...
#	"EXPAND_AS_DEFINED= QUERY COMMAND WATCHER \n" \

# XXX, DOT. generowanie grafiki. callgraphow, itd, fajne. tylkoze zasobozerne.. raz dziennie moze mozna
#	to zrobic, ale nie ciagle.
#	"HAVE_DOT=YES\n" \
#	"INCLUDE_GRAPH=NO\n" \
#	"INCLUDED_BY_GRAPH=NO\n" \
#	\

#	"WARN_IF_UNDOCUMENTED=YES\n" \

rm -rf ./doxygen

echo -ne \
	"PROJECT_NAME=ekg2\n" \
	"FILE_PATTERNS=*.c *.h\n" \
	"OPTIMIZE_OUTPUT_FOR_C=YES\n" \
	\
	"ENABLE_PREPROCESSING=YES\n" \
	"MACRO_EXPANSION=YES\n" \
	"EXPAND_ONLY_PREDEF=YES\n" \
	"PREDEFINED="	\
		"\"WATCHER_AUDIO(x)=int x(int type, int fd, string_t buf, void *data)\"" \
		"\"COMMAND(x)=int x(const char *name, const char **params, session_t *session, const char *target, int quiet)\"" \
		"\"QUERY(x)=int x(void *data, va_list ap)\"" \
		"\"WATCHER(x)=int x(int type, int fd, watch_type_t watch, void *data)\"" \
		"\"WATCHER_LINE(x)=int x(int type, int fd, const char *watch, void *data)\"" \
		"\"WATCHER_SESSION(x)=int x(int type, int fd, watch_type_t watch, session_t *s)\"" \
		"\"WATCHER_SESSION_LINE(x)=int x(int type, int fd, const char *watch, session_t *s)\"" \
		"\"BINDING_FUNCTION(x)=void x(const char *arg)\"" \
		"\"TIMER(x)=int x(int type, void *data)\"" \
		"\"TIMER_SESSION(x)=int x(int type, session_t *s)\"" \
		\
		"\"SNIFF_HANDLER(x, type)=static int x(session_t *s, const connection_t *hdr, const type *pkt, int len)\"" \
		"\"GG_PACKED=\""	\
		\
		"\"JABBER_HANDLER(x)=static void x(session_t *s, xmlnode_t *n)\"" \
		"\"JABBER_HANDLER_GET_REPLY(x)=static void x(session_t *s, jabber_private_t *j, xmlnode_t *n, const char *from, const char *id)\"" \
		"\"JABBER_HANDLER_IQ(x)=static void x(session_t *s, xmlnode_t *n, jabber_iq_type_t iqtype, const char *from, const char *id)\"" \
		"\"JABBER_HANDLER_RESULT(x)=static void x(session_t *s, xmlnode_t *n, const char *from, const char *id)\""	\
		"\"JABBER_HAVE_SSL=1\"" 	\
		"\"STRICT_XMLNS=0\""		\
		"\"WITH_JABBER_DCC=1\""		\
		"\"GMAIL_MAIL_NOTIFY=1\""	\
		"\n"\
	\
	"INPUT=../\n" \
	"EXCLUDE=../ekg2-config.h\n" \
	"RECURSIVE=YES\n" \
	"INPUT_ENCODING=ISO-8859-2\n" \
	"OUTPUT_LANGUAGE=Polish\n" \
	"EXTRACT_ALL=YES\n" \
	"EXTRACT_STATIC=YES\n" \
	"FULL_PATH_NAMES=YES\n" \
	"STRIP_FROM_PATH=`dirname ${PWD}`\n" \
	\
	"QUIET=YES\n" \
	"WARNINGS=YES\n" \
	\
	"OUTPUT_DIRECTORY=./doxygen\n" \
	"GENERATE_LATEX=NO\n" \
	"HTML_OUTPUT=./\n" \
	\
	| doxygen -

