/*
 *  (C) Copyright 2005 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#ifndef __PERL_EKG_H_
#define __PERL_EKG_H_

#include <ekg/dynstuff.h>
#include <ekg/scripts.h>
#undef _

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
extern scriptlang_t perl_lang;
extern plugin_t     perl_plugin;
#define perl_private(s) (perl_private_t *) script_private_get(s)

typedef struct {
	void *tmp;

} perl_private_t;

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=8 sw=8
 */
#endif
