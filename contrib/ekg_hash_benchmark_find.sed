#!/bin/sed -nf
#

/format_add("/{
:MainLoop
s/);/);/;
t EndLoop
N
b MainLoop
:EndLoop
p
}
