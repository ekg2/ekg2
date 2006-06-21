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

#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "audio.h"
#include "commands.h"
#include "dynstuff.h"
#include "plugins.h"
#include "themes.h"
#include "xmalloc.h"

AUDIO_DEFINE(stream);

COMMAND(cmd_streams) {
	PARASC
	const char **create = NULL;
	int display = 0;
	list_t l;

/* /_stream --create "INPUT inputparams"    "CODEC codecparams"			"OUTPUT outputparams" */

/* /_stream --create "STREAM file:dupa.raw" 					"STREAM file:dupa_backup.raw"	*/ /* <--- ekg2 can copy files! :) */
/* /_stream --create "STREAM file:dupa.raw" "CODEC rawcodec fortune-telling:ON" "LAME file:dupa.mp3" 		*/
/* /_stream --create "STREAM file:/dev/tcp host:jakishost port:port http:radio" "STREAM file:radio.raw"		*/ /* and download files too? :) */

/* i think that api if i (we?) write it... will be nice, but code can be obscure... sorry */

	if (match_arg(params[0], 'c', TEXT("create"), 2)) {		/* --create */
		create = &params[1];
	} else if (match_arg(params[0], 'l', TEXT("list"), 2) || !params[0]) {	/* list if --list. default action is --list (if we don't have params)  */
		display = 1;
	} else {							/* default action is --create (if we have params) */
		create = &params[0];
	}

	if (create) {
		const char *input, *codec, *output;

		if (create[0] && create[1] && create[2]) 	{ input = create[0]; codec = create[1]; output = create[2]; } 
		else if (create[0] && create[1])		{ input = create[0]; codec = NULL;	output = create[2]; } 
		else {
			wcs_printq("invali_params", name);
			return -1; 
		}
		/* XXX here, we ought to build arrays with INPUT name && paramas CODEC name && params OUTPUT name && params */
	} else if (!params[0]) {	/* no params, display list */
		display = 1;
	} else {
		wcs_printq("invalid_params", name);
		return -1;
	}

	if (display) { 
		/* XXX, display nice info to __status/__current window */
		/* XXX, add more debug info */
		for (l = streams; l; l = l->next) {
			stream_t *s = l->data;
			debug("[STREAM] name: %s IN: 0x%x CODEC: 0x%x OUT: 0x%x\n", s->stream_name, s->input, s->codec, s->output);
			if (s->input)	debug("	 IN, AUDIO: %s fd: %d bufferlen: %d\n", s->input->a->name, s->input->fd, s->input->buffer->len);
			if (s->output)	debug("	OUT, AUDIO: %s fd: %d bufferlen: %d\n", s->output->a->name, s->output->fd, s->output->buffer->len);
		}

		for (l = audio_inputs; l; l = l->next) {
			audio_t *a = l->data;
			debug("[AUDIO_INPUT] name: %s\n", a->name);
		}

		for (l = audio_codecs; l; l = l->next) {
			codec_t *c = l->data;
			debug("[AUDIO_CODEC] name: %s\n", c->name);
		}
	} 
	return 0;
}

int codec_register(codec_t *codec) {
/* XXX check if we already codec->name on the list */
	list_add(&audio_codecs, codec, 0);
	return 0;
}

void codec_unregister(codec_t *codec)
{

}

audio_t *audio_find(const char *name) {
	list_t l;
	if (!name) 
		return NULL;
	for (l = audio_inputs; l; l = l->next) {
		audio_t *a = l->data;
		if (!xstrcmp(a->name, name)) 
			return a;

	}
	return NULL;
}

int audio_register(audio_t *audio) {
	if (!audio)			return -1;
	if (audio_find(audio->name))	return -2;

	list_add(&audio_inputs, audio, 0);
	return 0;
}

void audio_unregister(audio_t *audio) {

}

char *stream_buffer_resize(stream_buffer_t *b, char *buf, int len) {
	int oldlen;
	if (!b) return NULL;

	if (!buf && len > 0) {
		debug("ERR stream_buffer_resize() len > 0 but buf NULL....\n"); 
		return b->buf;
	}

	oldlen = b->len;
/* len can be < 0. if it's we remove len bytes from b->buf */
/* else we add len bytes to b->buf */
	b->len += len;

	if (b->len <= 0) { b->len = 0; xfree(b->buf); b->buf = NULL; } 
	else {
		if (len < 0) { 
			int move = -len;

			if (b->len - move < 0) move = b->len;
			memmove(b->buf, b->buf+move, b->len-move);	/* iwil! */
		}
		b->buf = (char *) xrealloc(b->buf, b->len);
	}

	if (len > 0) memcpy(b->buf+oldlen, buf, len);

	debug("stream_buffer_resize() b: 0x%x oldlen: %d bytes newlen: %d bytes\n", b, oldlen, b->len);
	return b->buf;
}
		/* READING / WRITING FROM FILEs */
WATCHER(stream_audio_read) {
	int maxlen = 4096, len;
	char *buf;
	/* XXX, w private (data) moze byc ile maks. mozemy przeczytac itd... */
	
	buf = xmalloc(maxlen);
	len = read(fd, buf, maxlen);
	stream_buffer_resize((stream_buffer_t *) watch, buf, len);
	xfree(buf);

	debug("stream_audio_read() read: %d bytes from fd: %d\n", len, fd);
	if (len == 0) return -1;
	return len;
}

WATCHER(stream_audio_write) {
	stream_buffer_t *buffer = (stream_buffer_t *) watch;
	int len;

	debug("stream_audio_write() buffer: 0x%x in buffer: %d bytes... writting to fd: %d....\n", buffer, buffer->len, fd);
	len = write(fd, buffer->buf, buffer->len);

	if (len > 0) 	stream_buffer_resize(buffer, NULL, -len);
	else 		debug("write() failed: %d %s", errno, strerror(errno));

	debug(".... written %d bytes left: %d\n", len, buffer->len);
	return len;
}

WATCHER(stream_handle) {
	stream_t *s = data;
	audio_io_t *audio = NULL;
	watcher_handler_func_t *w = NULL;
	int len;

//	debug("stream_handle() name: %s type: %d wtype: %d\n", s->stream_name, type, watch);
	
	if ((int) watch == WATCH_READ)		audio = s->input;
	else if ((int) watch == WATCH_WRITE)	audio = s->output;

	if (!audio) debug("stream_handle() audio_t: 0x%x\n", audio);
	if (!audio) return -1;


	if ((int) watch == WATCH_WRITE && !audio->buffer->len) {
		/* XXX, we should set w->type to 0... and only to WATCH_WRITE when we have smth to write... */
		if (s->input->fd == -1) return -1;
		return 0;
	}

	if ((int) watch == WATCH_READ)		w = audio->a->read_handler;
	else if ((int) watch == WATCH_WRITE)	w = audio->a->write_handler;

	if (!w) debug("stream_handle() watch_t: 0x%x\n", w);
	if (!w) return -1;

	len = w(type, fd, (char *) audio->buffer, audio->private);

	if ((int) watch == WATCH_READ && s->output) {	/* if watch write do nothing */
		if (!s->codec) {
			/* we just copy data from input->buf */
			stream_buffer_resize(s->output->buffer, s->input->buffer->buf, s->input->buffer->len);
			stream_buffer_resize(s->input->buffer, NULL, -s->input->buffer->len);
		} else {
			/* XXX send data to codec! */
		}

		/* XXX, if this is read handler, and we don't have watch handler for writing stream->out->fd == -1 then we don't need to wait for WATCH_WRITE and we here do it */
	}

	if (len < 0) { 
		close(fd);
		audio->fd = -1;
		return -1;
	}

	return 0;
}

int stream_create(char *name, audio_io_t *in, audio_codec_t *co, audio_io_t *out) {
	stream_t *s;

	debug("stream_create() name: %s in: 0x%x codec: 0x%x output: 0x%x\n", name, in, co, out);

	if (!in || !out) goto fail;	/* from? where? */

	debug("stream_create() infd: %d outfd: %d\n", in->fd, out->fd);

	if (in->fd == -1) goto fail; /* reading from fd == -1? i don't think so... */


	s = xmalloc(sizeof(stream_t));
	s->stream_name	= xstrdup(name);
	s->input	= in;
	s->codec	= co;
	s->output	= out;

	list_add(&streams, s, 0);

	/* allocate buffors */
	if (in)		in->buffer	= xmalloc(sizeof(stream_buffer_t));
	if (out)	out->buffer 	= xmalloc(sizeof(stream_buffer_t));

	watch_add(NULL, in->fd, WATCH_READ, stream_handle, s);
	if (out->fd != -1) watch_add(NULL, out->fd, WATCH_WRITE, stream_handle, s);

	return 1;
fail:
	/* deinit */
	if (in)		in->a->control_handler(AUDIO_CONTROL_DEINIT, AUDIO_READ, &in);
	if (co)		co->c->control_handler(AUDIO_CONTROL_DEINIT, AUDIO_RDWR, &co); 
	if (out)	out->a->control_handler(AUDIO_CONTROL_DEINIT, AUDIO_WRITE, &out);
	return 0;
}

AUDIO_CONTROL(stream_audio_control) {
	audio_io_t *aio = NULL;
	va_list ap;

	if (type == AUDIO_CONTROL_MODIFY) { debug("stream_audio_control() AUDIO_CONTROL_MODIFY called but we not support it right now, sorry\n"); return NULL; }

	va_start(ap, way);

	if (type == AUDIO_CONTROL_INIT) {
		char *attr;
		char *file = NULL;

		int fd = -1;

		while ((attr = va_arg(ap, char *))) {
			char *val = va_arg(ap, char *);
			debug("[stream_audio_control] attr: %s value: %s\n", attr, val);
			if (!xstrcmp(attr, "file")) file = xstrdup(val);
		}
			/* if no file specified, continue with strange defaults ;) */
		if (!file && way == AUDIO_READ)		file = xstrdup("/dev/urandom");
		else if (!file && way == AUDIO_WRITE)	file = xstrdup("/dev/null");


		if (file) {
			fd = open(file, O_CREAT | (
				way == AUDIO_READ 	? O_RDONLY : 
				way == AUDIO_WRITE 	? O_WRONLY : O_RDWR), S_IRUSR | S_IWUSR);
			if (fd == -1) { 
				debug("[stream_audio_control] OPENING FILE FAILED %d %s!\n", errno, strerror(errno));
				goto fail;
			}
		}

		aio = xmalloc(sizeof(audio_io_t));
		aio->a  = &stream_audio;
		aio->fd = fd;
fail:
		xfree(file);
	} else if (type == AUDIO_CONTROL_DEINIT) {
		aio = *(va_arg(ap, audio_io_t **));


		/* closing fd && freeing buffer in API ? */
		xfree(aio);
		aio = NULL;
	} else if (type == AUDIO_CONTROL_HELP) {
		debug("[stream_audio_control] known atts:\n"
			"	--FILE <FILE>\n"
			"	--maxreadbuffer <BUFFER>\n"
			"OUT:	--input nazwa_czegostam\n"
			"IN:	--output nawa_czegostam\n"
			" ....\n"
			"this audio_t can WORK in two-ways READ/WRITE\n");
	}

	if (type == AUDIO_CONTROL_INIT) {
	}

	va_end(ap);
	return aio;
}

int audio_initialize() {
	audio_t *inp, *out;
	audio_codec_t *co = NULL;;
	audio_register(&stream_audio);

	if (0 && (inp = audio_find("stream"))) {
		out = inp;
		stream_create("Now playing: /dev/urandom",
				inp->control_handler(AUDIO_CONTROL_INIT, AUDIO_READ, "file", "/dev/urandom", NULL), 	/* reading from /dev/urandom */
				NULL, /* no codec */
				out->control_handler(AUDIO_CONTROL_INIT, AUDIO_WRITE, "file", "/dev/dsp", NULL)		/* writing to /dev/dsp */
			     );
	}
	if (0 && (inp = audio_find("oss")) && (out = audio_find("stream"))) {
		stream_create("Now recording: /dev/dsp",
				inp->control_handler(AUDIO_CONTROL_INIT, AUDIO_READ, /* "device", "/dev/dsp", */ "birate", "8000", "sample", "16", "channels", "1", NULL),
				co,
				out->control_handler(AUDIO_CONTROL_INIT, AUDIO_WRITE, "file", "plik.raw", NULL)
			      );
	}
	if (0 &&  (out = audio_find("oss")) && (inp = audio_find("stream"))) {
		stream_create("Now playing to: /dev/dsp",
				inp->control_handler(AUDIO_CONTROL_INIT, AUDIO_READ, "file", "plik.raw", NULL),
				co,
				out->control_handler(AUDIO_CONTROL_INIT, AUDIO_WRITE, "birate", "8000", "sample", "16", "channels", "1", NULL)
			      );
	}

	return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
