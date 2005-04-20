XSLTRANSFORMER="xsltproc"

all:
	rm -rf book
	./generate.sh
	xsltproc -stringparam chunker.output.encoding ISO-8859-2 sheet.xsl book.xml
	mkdir book
	mv *.html book/

clean distclean:
	rm -rf book book.xml
