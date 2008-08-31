/* $Id$ */

#ifndef __EKG_DEBUG_H
#define __EKG_DEBUG_H

typedef enum {
	DEBUG_IO = 1,
	DEBUG_IORECV,
	DEBUG_FUNCTION,
	DEBUG_ERROR,
	DEBUG_GGMISC,		/* cause of a lot GG_DEBUG_MISC in libgadu we've got special formats for them... */
	DEBUG_WHITE,
	DEBUG_WARN,
	DEBUG_OK,
	DEBUG_WTF			/* serious fail */
} debug_level_t;

#ifndef DISABLE_DEBUG
void debug(const char *format, ...);
void debug_ext(debug_level_t level, const char *format, ...);
#else
#define debug(...)
#define debug_ext(...)
#endif

#define debug_io(args...)	debug_ext(DEBUG_IO, args)
#define debug_iorecv(args...)	debug_ext(DEBUG_IORECV, args)
#define debug_function(args...) debug_ext(DEBUG_FUNCTION, args)
#define debug_error(args...)	debug_ext(DEBUG_ERROR, args)
#define debug_white(args...)	debug_ext(DEBUG_WHITE, args)
#define debug_warn(args...)	debug_ext(DEBUG_WARN, args)
#define debug_ok(args...)	debug_ext(DEBUG_OK, args)
#define debug_wtf(args...)	debug_ext(DEBUG_WTF, args)

#endif


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
