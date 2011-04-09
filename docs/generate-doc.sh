#!/bin/sh

rm -rf ./doxygen

sed \
  -e "s,@OUTPUT_LANGUAGE@,Polish,g;" \
  -e "s,@EXTRA_FILE_PATTERNS@,doxygenpl.txt,g;" \
  -e "s,@STRIP_FROM_PATH@,$(dirname ${PWD}),g" \
 doxygen.settings | doxygen -

