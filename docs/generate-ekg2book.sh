#!/bin/sh
# Generate the ekg2 book or cleanup in a given directory.

set -e

lang=${1}
case $lang in
pl)
	dir="ekg2book"
	;;
en)
	dir="ekg2book-en"
	;;
*)
	echo "Provide a valid language code first argument, not \"$1\"." >&2
	exit 1
esac

action=${2:-'generate'}

cd "${dir}"

PREINPUTS="design/header.xml bookinfo.xml userbook.xml"
POSTINPUTS="develbook.xml faq.xml design/footer.xml"
OUTPUT="book.xml"
GENPROG="./txt2docbook.py"

case $action in
generate)
	rm -rf book
	cat ${PREINPUTS} "design/plugins_header.xml" > $OUTPUT
	for i in ../../plugins/*
	do
		if [ -f $i ]; then continue; fi
		if [ -f $i/doc.xml -o -f $i/commands-${lang}.txt -o -f $i/vars-${lang}.txt ]; then
			sed -e s/PLUGIN/`basename $i`/ "design/plugin_header.xml" >> $OUTPUT
			if [ -f $i/doc.xml ]; then
				cat $i/doc.xml >> $OUTPUT
			fi
			if [ -f $i/commands-${lang}.txt ]; then
				$GENPROG -c $i/commands-${lang}.txt >> $OUTPUT
			fi
			if [ -f $i/vars-${lang}.txt ]; then
				$GENPROG -v $i/vars-${lang}.txt >> $OUTPUT
			fi
			if [ -f $i/session-${lang}.txt ]; then
				$GENPROG -s ../session-${lang}.txt >> $OUTPUT
				$GENPROG -s $i/session-${lang}.txt >> $OUTPUT
			fi
			cat "design/plugin_footer.xml" >> $OUTPUT
		fi
	done
	cat "design/plugins_footer.xml" $POSTINPUTS >> $OUTPUT
	${XSLTRANSFORMER:-'xslproc'} -stringparam chunker.output.encoding ISO-8859-2 sheet.xsl ${OUTPUT}
	mkdir book
	mv *.html book/
	;;

clean|distclean)
	rm -rf book book.xml
	;;

*)
	echo "Unknown action \"$action\"." >&2
	exit 1
	;;
esac

