#!/bin/sh

# XXX, INPUT_ENCODING nie wiedziec czemu nie dziala.
# 	dlatego tymczasowo OUTPUT_LANGUAGE jest Polish
#	co powoduje zmiane kodowania strony na ISO-8859-2
#
#

rm -rf ./doxygen

echo -ne \
	"PROJECT_NAME=ekg2\n" \
	"FILE_PATTERNS=*.c *.h\n" \
	"OPTIMIZE_OUTPUT_FOR_C=YES\n" \
	\
	"INPUT=../\n" \
	"INPUT_ENCODING=ISO-8859-2\n" \
	"OUTPUT_LANGUAGE=Polish\n" \
	"RECURSIVE=YES\n" \
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

