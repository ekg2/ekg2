XSLTRANSFORMER="xsltproc"

all:
	rm -rf book
	./generate.sh
	xsltproc sheet.xsl book.xml
	mkdir book
	mv *.html book/

clean distclean:
	rm -rf book book.xml
