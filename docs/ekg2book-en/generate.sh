#!/bin/sh

PREINPUTS="design/header.xml bookinfo.xml userbook.xml"
POSTINPUTS="develbook.xml faq.xml design/footer.xml"
OUTPUT="book.xml"
GENPROG="./txt2docbook.py"
set -e

cat /dev/null > $OUTPUT

for input in $PREINPUTS
do
   cat $input >> $OUTPUT
done

# pluginy

cat "design/plugins_header.xml" >> $OUTPUT

for i in ../../plugins/*
do
   if [ -f $i ]
   then
      continue
   fi

   if [ -f $i/doc.xml -o -f $i/commands-en.txt -o -f $i/vars-en.txt ]
   then
      cat "design/plugin_header.xml" | sed -e s/PLUGIN/`basename $i`/ >> $OUTPUT
      
      if [ -f $i/doc.xml ]
      then
         cat $i/doc.xml >> $OUTPUT
      fi

      if [ -f $i/commands-en.txt ]
      then
         $GENPROG -c $i/commands-en.txt >> $OUTPUT
      fi
      if [ -f $i/vars-en.txt ]
      then
         $GENPROG -v $i/vars-en.txt >> $OUTPUT
      fi
      if [ -f $i/session-en.txt ]
      then
         $GENPROG -s ../session-en.txt >> $OUTPUT
         $GENPROG -s $i/session-en.txt >> $OUTPUT
      fi

      cat "design/plugin_footer.xml" >> $OUTPUT
   fi
   
done

cat "design/plugins_footer.xml" >> $OUTPUT

for input in $POSTINPUTS
do
   cat $input >> $OUTPUT
done

