#!/usr/bin/python
# -*- encoding: iso-8859-2 -*-

import re
import sys
import getopt

msg_session_vars = "Zmienne sesyjne"
msg_vars = "Zmienne"
msg_commands = "Polecenia"
str_type = "typ"
msg_type = "Typ"
str_def_val = "domyślna wartość"
msg_def_val = "Domyślna wartość"
str_params = "parametry"
msg_params = "Parametry"
str_short_desc = "krotki opis"
msg_short_desc = "Krótki opis"

debug = True

def usage():
    """Display usage message"""
    sys.stdout = sys.stderr
    print "Usage: "+sys.argv[0]+" [-c|-v|-s] <filename.txt>"
    print "\t-c\tparse command file (default)"
    print "\t-v\tparse vars file"
    print "\t-s\tparse session vars file"

def debug(str):
    """Write message to strerr, if debugging is enabled."""
    if debug:
    	sys.stderr.write("[debug] {"+str+"}\n")

def warn(str):
    """Write message to stderr."""
    sys.stderr.write("[warn] {"+str+"}\n")

def die(str):
    """Write message to stderr and exit with an error."""
    sys.stderr.write("[FATAL] {"+str+"}\n")
    sys.exit(1)

def strip_indent_amount(line):
    """Return a number after which char to cut, to get a visual 8-char unindent"""
    if len(line) > 0 and line[0] == '\t':
    	return 1
    if len(line) > 7 and line[0:8] == '        ':
    	return 8
    if len(line) > 1 and line[0:1] == ' ' and line[1] == '\t':
    	return 2
    if len(line) > 2 and line[0:2] == '  ' and line[2] == '\t':
    	return 3
    if len(line) > 3 and line[0:3] == '   ' and line[3] == '\t':
    	return 4
    if len(line) > 4 and line[0:4] == '    ' and line[4] == '\t':
    	return 5
    if len(line) > 5 and line[0:5] == '     ' and line[5] == '\t':
    	return 6
    if len(line) > 6 and line[0:6] == '      ' and line[6] == '\t':
    	return 7
    if len(line) > 7 and line[0:7] == '       ' and line[7] == '\t':
    	return 8
    return 0

def is_indented(line):
    """Whether it's properly indented"""
    if strip_indent_amount(line) == 0:
    	return False
    else:
    	return True

def strip_indent(fname, linenum, line):
    """Unindent the line by 8 visual chars, or raise an exception."""
    ret = strip_indent_amount(line)
    if ret == 0:
    	raise Exception('Invalid indent %s:%d' % (fname, linenum))
    elif ret >= 2 and ret < 8:
        warn('Unclean indent %s:%d' % (fname, linenum))
    elif ret == 8 and line[7] == '\t':
        warn('Unclean indent %s:%d' % (fname, linenum))
    return line[ret:]

def parse_header(fname, linenum, vars, session, commands, line):
    """Parse an undindented header, returning an XML snippet"""
    if line.find(':') < 0:
    	raise Exception('Header expected (%s:%d)' % (fname, linenum))
    debug('header on line %d: %s' % (linenum, line))
    data = line.split(':')
    # header name
    if vars or session:
    	if data[0] == str_type:
	    title = msg_type
	elif data[0] == str_def_val:
	    title = msg_def_val
	else:
	    raise Exception("Unknown header [%s] (%s:%d)" % (data[0], fname, linenum))
    elif commands:
	if data[0] == str_params:
	    title = msg_params
	elif data[0] == str_short_desc:
	    title = msg_short_desc
	else:
	    raise Exception("Unknown header [%s] (%s:%d)" % (data[0], fname, linenum))
    para = data[1].replace('&', '&amp;')
    para = para.replace('<', '&lt;')
    para = para.replace('>', '&gt;')
    return "<formalpara><title>%s</title><para>%s</para></formalpara>\n" % (title, para)

def print_entry(record):
    """Print an XML snippet of the supplied record."""
    print """<varlistentry>
<term>
%(term)s
</term>
<listitem>
%(header)s
<screen>
<![CDATA[%(desc)s]]>
</screen>
</listitem>
</varlistentry>
""" % record

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
    debug('Reading file %s' % (fname))

    # begin output
    output = "<sect2><title>";
    if session:
	output += msg_session_vars
    if vars:
	output += msg_vars
    elif commands:
	output += msg_commands
    output += "</title>\n<variablelist>\n"
    print(output)

    record = None
    state_was = 'top_level'
    linenum = 0
    r = re.compile("%T(.+)%n")
    line = file.readline()
    while line:
	linenum += 1
	line = line[:-1]
	line = r.sub("\\1", line)
	
	if state_was == 'top_level':
	    if line[0:2] == "//" or line == "":
	        # still top level
	    	state_was = 'top_level'
	    elif len(line) > 0 and (line[0] == ' ' or line[0] == "\t"):
	    	raise Exception('Unexpected input on top level (%s:%d)' % (fname, linenum))
	    else:
	        debug('entry start on line %d: %s' % (linenum, line))
	        state_was = 'entry_start'
		record = {'term': line, 'desc' : '', 'header': ''}
	
	elif state_was == 'entry_start':
	    # this must be a header	    
	    if is_indented(line):
	    	line = strip_indent(fname, linenum, line)
	    else:
	    	raise Exception('Header expected (%s:%d)' % (fname, linenum))
            record['header'] += parse_header(fname, linenum, vars, session, commands, line)
	    state_was = 'header'
	
	elif state_was == 'header':
	    if line == '':
	    	debug('entry ended on line %d' % (linenum - 1))
	    	state_was = 'top_level'
		if record:
		    print_entry(record)
		record = {'term': line, 'desc' : '', 'header': ''}
		# so it doesn't match later
		line = None
	    elif is_indented(line):
	    	line = strip_indent(fname, linenum, line)
	    else:
	    	raise Exception('Header, separator, or empty line expected (%s:%d)' % (fname, linenum))
	    if line == None:
	    	pass
	    elif line == '':
	    	# separator
		debug('entry headers ended on line %d' % (linenum - 1))
		state_was = 'entry_contents'
	    elif line.find(':') >= 0:
            	record['header'] += parse_header(fname, linenum, vars, session, commands, line)
	    else:
	    	raise Exception('Unparseable header or extra whitespace in separator (%s:%d)' % (fname, linenum))
 
        elif state_was == 'entry_contents':
	    if line == '':
	    	state_was = 'top_level'
		print_entry(record)
		record = {'term': line, 'desc' : '', 'header': ''}
	    elif is_indented(line):
	        debug('entry contents on line %d' % (linenum))
		record['desc'] += strip_indent(fname, linenum, line) + "\n"
	    else:
	    	raise Exception('Expected entry contents, separator, or empty line (%s:%d)' % (fname, linenum))

        else:
	    raise Exception('Unknown state (%s:%d)' % (fname, linenum))
        line = file.readline()
	    
    if record:
        print_entry(record)
    print('</variablelist></sect2>')

if __name__ == "__main__":
    try:
    	main()
    except Exception, e:
    	die(e.args[0])
