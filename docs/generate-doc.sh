#!/bin/sh

rm -rf ./doxygen

echo -ne \
	"PROJECT_NAME=ekg2\n" \
	"FILE_PATTERNS=*.c *.h\n" \
	"OPTIMIZE_OUTPUT_FOR_C=YES\n" \
	\
	"INPUT=../\n" \
	"INPUT_ENCODING=ISO-8859-2\n" \
	"EXTRACT_STATIC=YES\n" \
	"RECURSIVE=YES\n" \
	"EXTRACT_ALL=YES\n" \
	\
	"QUIET=YES\n" \
	"WARNINGS=YES\n" \
	\
	"OUTPUT_DIRECTORY=./doxygen\n" \
	"GENERATE_LATEX=NO\n" \
	"HTML_OUTPUT=./\n" \
	\
	| doxygen -

