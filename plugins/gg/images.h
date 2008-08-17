/*
 *  (C) Copyright 2004 Piotr Kupisiewicz <deletek@ekg2.org>
 *		  2006 Adam Mikuta <adamm@ekg2.org>
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

#ifndef __EKG_GG_IMAGES_H
#define __EKG_GG_IMAGES_H

#include <ekg/dynstuff.h>

typedef struct {
	char *filename;
	char *data;
	uint32_t size;
	uint32_t crc32;
} image_t;

extern list_t images;

#define GG_CRC32_INVISIBLE 99

void gg_changed_images(const char *var);
void image_remove_queue(image_t *i);
void image_flush_queue();

#endif /* __EKG_GG_IMAGES_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */

