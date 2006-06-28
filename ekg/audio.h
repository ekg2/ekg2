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

#include "dynstuff.h"
#include "plugins.h"

typedef enum { AUDIO_CONTROL_INIT = 0, AUDIO_CONTROL_SET, AUDIO_CONTROL_GET, AUDIO_CONTROL_DEINIT, AUDIO_CONTROL_HELP }
			audio_control_t;
typedef enum { AUDIO_READ = 0, AUDIO_WRITE, AUDIO_RDWR, } 
			audio_way_t;
typedef enum { CODEC_CODE = 0, CODEC_DECODE, } 
			codec_way_t;

#define CODEC_RECODE(x) int x(int type, stream_buffer_t *input, stream_buffer_t *output, void *data)
#define AUDIO_CONTROL(x) audio_io_t	*x(audio_control_t type, audio_way_t way, audio_io_t *aio, ...)
#define CODEC_CONTROL(x) audio_codec_t	*x(audio_control_t type, audio_way_t way, audio_codec_t *aco, ...)

#define AUDIO_DEFINE(x)\
	extern AUDIO_CONTROL(x##_audio_control);\
	extern WATCHER(x##_audio_read);		\
	extern WATCHER(x##_audio_write);	\
	audio_t x##_audio = { \
		.name = #x, \
		.control_handler= x##_audio_control, \
		.read_handler	= x##_audio_read, \
		.write_handler  = x##_audio_write, \
	}

#define CODEC_DEFINE(x)\
	extern CODEC_CONTROL(x##_codec_control);\
	extern CODEC_RECODE(x##_codec_code);	\
	extern CODEC_RECODE(x##_codec_decode);	\
	codec_t x##_codec = { \
		.name = #x, \
		.control_handler= x##_codec_control,	\
		.code_handler	= x##_codec_code,	\
		.decode_handler = x##_codec_decode,	\
	}

typedef struct {
	char *buf;
	int len;
} stream_buffer_t;

typedef struct {
	char *name;	/* nazwa urzadzenia */
	
	void *(*control_handler)(audio_control_t, audio_way_t, void *, ...);	/* initing / checking if audio_io_t is correct / deiniting */
	watcher_handler_func_t *read_handler;
	watcher_handler_func_t *write_handler;

	void *private;
} audio_t;

typedef struct {
	audio_t *a;
	int fd;
	stream_buffer_t *buffer;
	void *private;
} audio_io_t;

typedef struct {
	char *name;	/* nazwa codeca */

	void *(*control_handler)(audio_control_t, audio_way_t, void *, ...);    /* initing / checking if audio_codec_t is correct / deiniting */

		/* IN: int type, stream_buffer_t *input, stream_buffer_t *output, void *private 
		 * OUT: how many bytes he code/decode */
	int (*code_handler)(int, stream_buffer_t *, stream_buffer_t *, void *);
	int (*decode_handler)(int, stream_buffer_t *, stream_buffer_t *, void *);
	void *private;
} codec_t;

typedef struct {
	codec_t *c;		/* codec_t * */
	codec_way_t way;		/* CODEC_CODE CODEC_DECODE */
	
	void *private;
} audio_codec_t;

typedef struct {
	char *stream_name;
	audio_io_t	*input;
	audio_codec_t	*codec;
	audio_io_t	*output;

	void *private;
} stream_t;

list_t audio_codecs;
list_t audio_inputs;
list_t streams;

char *stream_buffer_resize(stream_buffer_t *b, char *buf, int len);

int audio_register(audio_t *audio);
void audio_unregister(audio_t *audio);

int codec_register(codec_t *codec);
void codec_unregister(codec_t *codec);

int audio_initialize();

#endif /* __EKG_AUDIO_H */


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
