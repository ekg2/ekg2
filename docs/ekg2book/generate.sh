#!/bin/sh

PREINPUTS="header.xml bookinfo.xml userbook.xml"
POSTINPUTS="develbook.xml faq.xml footer.xml"
OUTPUT="book.xml"
GENPROG="./txt2docbook.py"

cat /dev/null > $OUTPUT

for input in $PREINPUTS
do
   cat $input >> $OUTPUT
done

# pluginy

cat "plugins_header.xml" >> $OUTPUT

for i in ../../plugins/*
do
   if [ -f $i ]
   then
      continue
   fi

   if [ -f $i/doc.xml -o -f $i/commands.txt -o -f $i/vars.txt ]
   then
      cat "plugin_header.xml" | sed -e s/PLUGIN/`basename $i`/ >> $OUTPUT
      
      if [ -f $i/doc.xml ]
      then
         cat $i/doc.xml >> $OUTPUT
      fi

      if [ -f $i/commands.txt ]
      then
         $GENPROG -c $i/commands.txt >> $OUTPUT
      fi
      if [ -f $i/vars.txt ]
      then
         $GENPROG -v $i/vars.txt >> $OUTPUT
      fi
      if [ -f $i/session.txt ]
      then
         $GENPROG -s $i/session.txt >> $OUTPUT
      fi

      cat "plugin_footer.xml" >> $OUTPUT
   fi
   
done

cat "plugins_footer.xml" >> $OUTPUT

for input in $POSTINPUTS
do
   cat $input >> $OUTPUT
done

