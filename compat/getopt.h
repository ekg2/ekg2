/* Declarations for getopt.
   Copyright (C) 1989-1994, 1996-1999, 2001 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/* Excessive comments removed by Wojtek Kaniewski <wojtekka@irc.pl> */
  
#ifndef _GETOPT_H

#ifndef __need_getopt
# define _GETOPT_H 1
#endif

#if !defined __GNU_LIBRARY__
# include <ctype.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

extern char *optarg;

extern int optind;

extern int opterr;

extern int optopt;

#ifndef __need_getopt
struct option
{
# if (defined __STDC__ && __STDC__) || defined __cplusplus
  const char *name;
# else
  char *name;
# endif
  int has_arg;
  int *flag;
  int val;
};

# define no_argument		0
# define required_argument	1
# define optional_argument	2
#endif	/* need getopt */

#if (defined __STDC__ && __STDC__) || defined __cplusplus
# ifdef __GNU_LIBRARY__
extern int getopt (int __argc, char *const *__argv, const char *__shortopts);
# else /* not __GNU_LIBRARY__ */
extern int getopt ();
# endif /* __GNU_LIBRARY__ */

# ifndef __need_getopt
extern int getopt_long (int __argc, char *const *__argv, const char *__shortopts,
		        const struct option *__longopts, int *__longind);
extern int getopt_long_only (int __argc, char *const *__argv,
			     const char *__shortopts,
		             const struct option *__longopts, int *__longind);

/* Internal only.  Users should not call this directly.  */
extern int _getopt_internal (int __argc, char *const *__argv,
			     const char *__shortopts,
		             const struct option *__longopts, int *__longind,
			     int __long_only);
# endif
#else /* not __STDC__ */
extern int getopt ();
# ifndef __need_getopt
extern int getopt_long ();
extern int getopt_long_only ();

extern int _getopt_internal ();
# endif
#endif /* __STDC__ */

#ifdef	__cplusplus
}
#endif

#undef __need_getopt

#endif /* getopt.h */
