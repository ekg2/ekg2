#!/usr/bin/python
# -*- encoding: iso-8859-2 -*-

import re
import sys
import getopt

def usage():
    sys.stdout = sys.stderr
    print "Usage: "+sys.argv[0]+" [-c|-v|-s] <filename.txt>"
    print "\t-c\tparse command file (default)"
    print "\t-v\tparse vars file"
    print "\t-s\tparse session vars file"

def debug(str):
    sys.stderr.write("[debug] {"+str+"}\n")

def main():
    
    if len(sys.argv) < 2:
	usage()
	sys.exit(1)

    try:
	opts, args = getopt.getopt(sys.argv[1:], "cvs")
    except getopt.GetoptError:
	usage()
	sys.exit(1)

    vars     = None
    commands = True
    session  = True
    for o, a in opts:
	if o == "-v":
	    vars = True
	    commands = None
	    session = None
	if o == "-c":
	    vars = None
	    commands = True
	    session = None
	if o == "-s":
	    vars = None
	    commands = None
	    session = True
    
    fname = args[0]
    try:
	file = open(fname, "r")
    except Exception, e:
	sys.stdout = sys.stderr
	print "Error: can't open file for reading"
	print str(e)
	sys.exit(1)

    output = "<sect2><title>";
    if session:
	output += "Zmienne sesyjne"
    if vars:
	output += "Zmienne"
    elif commands:
	output += "Polecenia"
    output += "</title>\n<variablelist>\n"
    record = None
    r = re.compile("%T(.+)%n")
    line = file.readline()
    while line[0:2] == "//":
	line = file.readline()
    
    while line == "":
	line = file.readline()

    while line[0] == "\t" or line[0] == "\n": 
	line = file.readline()
	
    while line:
	line = line[:-1]
#	line = line.replace('&', '&amp;')
#	line = line.replace('<', '&lt;')
#	line = line.replace('>', '&gt;')
	line = r.sub("\\1", line)
	if line == "" and not record:
	    pass
	elif line == "":
	    record['desc'] += "\n"
	elif line[0] != "\t" and line[0] != " ":
	    if record:
		output += """<varlistentry>
<term>
%(term)s
</term>
<listitem>
%(desc)s]]>
</screen>
</listitem>
</varlistentry>
""" % record
	    record = {'term' : '', 'desc' : ''}
	    record['term'] = line
	else:
	    found = False
	    if (vars or commands or session) and line.find(':') >= 0:
		data = line.split(':')
		if vars or session:
		    if data[0] == "\ttyp":
			found = True
			title = "Typ"
		    elif data[0] == "\tdomyślna wartość":
			found = True
			title = "Domyślna wartość"
		elif commands:
		    if data[0] == "\tparametry":
			found = True
			title = "Parametry"
		    elif data[0] == "\tkrotki opis":
			found = True
			title = "Krótki opis"
		if found:
		    para = data[1]
	    if not found:
		if record['desc'] == "":
		    record['desc'] = "<screen>\n<![CDATA[\n"
		record['desc'] += line[1:]+"\n"
	    else:
		if record['desc'][-9:] == '<![CDATA[':
		    record['desc'] = record['desc'][:-18]
		para = para.replace('&', '&amp;')
		para = para.replace('<', '&lt;')
		para = para.replace('>', '&gt;')
		record['desc'] += """<formalpara>
<title>
%s
</title>
<para>
%s
</para>
</formalpara>
<screen>
<![CDATA[""" % (title, para)
	line = file.readline()
    if record and record['term'] and record['desc']:
	output += """<varlistentry>
<term>
%(term)s
</term>
<listitem>
%(desc)s]]>
</screen>
</listitem>
</varlistentry>
""" % record
    output += "\n</variablelist></sect2>"
    print output

if __name__ == "__main__":
    main()
