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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { AUDIO_CONTROL_INIT = 0, AUDIO_CONTROL_SET, AUDIO_CONTROL_GET, AUDIO_CONTROL_DEINIT, AUDIO_CONTROL_HELP }
			audio_control_t;
typedef enum { AUDIO_READ = 0, AUDIO_WRITE, AUDIO_RDWR, } 
			audio_way_t;
typedef enum { CODEC_CODE = 0, CODEC_DECODE, } 
			codec_way_t;

#define WATCHER_AUDIO(x) int x(int type, int fd, string_t buf, void *data)
typedef WATCHER_AUDIO(audio_handler_func_t);

#define __AINIT(a, way, args...) a ? a->control_handler(AUDIO_CONTROL_SET, way, NULL, args, NULL) : NULL
#define __CINIT(c, args...) c ? c->control_handler(AUDIO_CONTROL_SET, AUDIO_RDWR, NULL, args, NULL) : NULL

#define __AINIT_F(name, way, args...) __AINIT((audio_find(name)), way, args)
#define __CINIT_F(name, args...) __CINIT((codec_find(name)), args)


#define CODEC_RECODE(x) int x(int type, string_t input, string_t output, void *data)
#define AUDIO_CONTROL(x) audio_io_t	*x(audio_control_t type, audio_way_t way, audio_io_t *aio, ...)
#define CODEC_CONTROL(x) audio_codec_t	*x(audio_control_t type, audio_way_t way, audio_codec_t *aco, ...)

#define AUDIO_DEFINE(x)\
	extern AUDIO_CONTROL(x##_audio_control);\
	extern WATCHER_AUDIO(x##_audio_read);		\
	extern WATCHER_AUDIO(x##_audio_write);	\
	audio_t x##_audio = { \
		.name = #x, \
		.control_handler= (void*) x##_audio_control, \
		.read_handler	= x##_audio_read, \
		.write_handler	= x##_audio_write, \
	}

#define CODEC_DEFINE(x)\
	extern CODEC_CONTROL(x##_codec_control);\
	extern CODEC_RECODE(x##_codec_code);	\
	extern CODEC_RECODE(x##_codec_decode);	\
	codec_t x##_codec = { \
		.name = #x, \
		.control_handler= (void*) x##_codec_control,	\
		.code_handler	= x##_codec_code,	\
		.decode_handler = x##_codec_decode,	\
	}

typedef struct audio {
	struct audio *next;

	char *name;	/* nazwa urzadzenia */
	
	void *(*control_handler)(audio_control_t, audio_way_t, void *, ...);	/* initing / checking if audio_io_t is correct / deiniting */
	audio_handler_func_t *read_handler;
	audio_handler_func_t *write_handler;

	void *priv_data;
} audio_t;

typedef struct {
	audio_t *a;
	int fd;
	unsigned int outb;		/* how many bytes go through handler */
	string_t buffer;
	void *priv_data;
} audio_io_t;

typedef struct codec {
	struct codec *next;

	char *name;	/* nazwa codeca */

	void *(*control_handler)(audio_control_t, audio_way_t, void *, ...);	/* initing / checking if audio_codec_t is correct / deiniting */

		/* IN: int type, string_t input, string_t output, void *priv_data 
		 * OUT: how many bytes he code/decode */
	int (*code_handler)(int, string_t, string_t, void *);
	int (*decode_handler)(int, string_t, string_t, void *);
	void *priv_data;
} codec_t;

typedef struct {
	codec_t *c;			/* codec_t * */
	codec_way_t way;		/* CODEC_CODE CODEC_DECODE */
	
	void *priv_data;
} audio_codec_t;

typedef struct stream {
	struct stream *next;

	char *stream_name;
	audio_io_t	*input;
	audio_codec_t	*codec;
	audio_io_t	*output;

	void *priv_data;
} stream_t;

int stream_create(char *name, audio_io_t *in, audio_codec_t *co, audio_io_t *out);

int audio_register(audio_t *audio);
audio_t *audio_find(const char *name);
void audio_unregister(audio_t *audio);

int codec_register(codec_t *codec);
codec_t *codec_find(const char *name);
void codec_unregister(codec_t *codec);

int audio_initialize();
int audio_deinitialize();

#ifdef __cplusplus
}
#endif

#endif /* __EKG_AUDIO_H */


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
