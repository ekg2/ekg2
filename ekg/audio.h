/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
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

#ifndef __EKG_AUDIO_H
#define __EKG_AUDIO_H

typedef char *codec_cap_t[2];

typedef struct {
	char *name;		/* nazwa codeca */
	codec_cap_t *caps; 	/* tablica dostêpnych formatów */
	void *(*init)(const char *from, const char *to);
				/* inicjalizacja codeca */
	int (*process)(void *codec, char *in, int inlen, char **out, int *outlen);
				/* przetwarza blok danych */
	void (*destroy)(void *codec);
				/* zwalnia zasoby codeca */
} codec_t;

int codec_register(codec_t *codec);
void codec_unregister(codec_t *codec);

#endif /* __EKG_AUDIO_H */

