#!/bin/sh

# XXX, INPUT_ENCODING nie wiedziec czemu nie dziala.
# 	dlatego tymczasowo OUTPUT_LANGUAGE jest Polish
#	co powoduje zmiane kodowania strony na ISO-8859-2
#
#

#	"EXPAND_AS_DEFINED= QUERY COMMAND WATCHER \n" \

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
		"\"BINDING_FUNCTION(x)=void x(const char *arg)\"" \
		"\"TIMER(x)=int x(int type, void *data)\"" \
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

