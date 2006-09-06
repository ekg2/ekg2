/* $Id$ */

#ifndef __EKG_DEBUG_H
#define __EKG_DEBUG_H

#define DEBUG_IO	1
#define DEBUG_IORECV	2
#define DEBUG_FUNCTION	3
#define DEBUG_ERROR	4
#define DEBUG_GGMISC	5		/* cause of a lot GG_DEBUG_MISC in libgadu we've got special formats for them... */

void debug(const char *format, ...);
void debug_ext(int level, const char *format, ...);

#define debug_io(args...)	debug_ext(DEBUG_IO, args)
#define debug_iorecv(args...)	debug_ext(DEBUG_IORECV, args)
#define debug_function(args...) debug_ext(DEBUG_FUNCTION, args)
#define debug_error(args...)	debug_ext(DEBUG_ERROR, args)

#endif


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
