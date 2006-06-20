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

#include "audio.h"
#include "commands.h"
#include "dynstuff.h"
#include "plugins.h"
#include "xmalloc.h"

AUDIO_DEFINE(stream);

COMMAND(cmd_streams) {
	list_t l;
	// _debug only (only temporary)
	
	for (l = streams; l; l = l->next) {
		stream_t *s = l->data;
		debug("[STREAM] name: %s IN: 0x%x CODEC: 0x%x OUT: 0x%x\n", s->stream_name, s->input, s->codec, s->output);
		if (s->input)	debug("	 IN, fd: %d\n", s->input->fd);
		if (s->output)	debug("	OUT, fd: %d\n", s->output->fd);
	}

	for (l = audio_inputs; l; l = l->next) {
		audio_t *a = l->data;
		debug("[AUDIO_CODEC] name: %s\n", a->name);
	}

/* /_stream --create "INPUT inputparams"    "CODEC codecparams"			"OUTPUT outputparams" */

/* /_stream --create "STREAM file:dupa.raw" 					"STREAM file:dupa_backup.raw"	*/ /* <--- ekg2 can copy files! :) */
/* /_stream --create "STREAM file:dupa.raw" "CODEC rawcodec fortune-telling:ON" "LAME file:dupa.mp3" 		*/
/* /_stream --create "STREAM file:/dev/tcp host:jakishost port:port http:radio" "STREAM file:radio.raw"		*/ /* and download files too? :) */

/* i think that api if i (we?) write it... will be nice, but code can be obscure... sorry */
	return 0;
}

int codec_register(codec_t *codec)
{

	return -1;
}

void codec_unregister(codec_t *codec)
{

}

int audio_register(audio_t *audio) {
	list_add(&audio_inputs, audio, 0);
	return -1;
}

void audio_unregister(audio_t *audio) {

}

char *stream_buffer_resize(stream_buffer_t *b, char *buf, int len) {
	int oldlen;
	if (!b) return NULL;

	oldlen = b->len;
/* len can be < 0. if it's we remove len bytes from b->buf */
/* else we add len bytes to b->buf */
	b->len += len;

	if (b->len <= 0) { b->len = 0; xfree(b->buf); b->buf = NULL; } 
	else		 b->buf = (char *) xrealloc(b->buf, b->len); 

	if (len > 0) memcpy(b->buf+oldlen, buf, len);
	debug("stream_buffer_resize() oldlen: %d bytes newlen: %d bytes\n", oldlen, b->len);
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
	return len;
}

WATCHER(stream_audio_write) {
	stream_buffer_t *buffer = (stream_buffer_t *) watch;
	int len;

	debug("stream_audio_write() buffer: 0x%x in buffer: %d bytes... writting to fd: %d.... ", buffer, buffer->len, fd);
	len = write(fd, buffer->buf, buffer->len);
	stream_buffer_resize(buffer, NULL, -len);

	debug("written %d bytes left: %d\n", len, buffer->len);
	return 0;
}

WATCHER(stream_handle) {
	stream_t *s = data;
	audio_io_t *audio = NULL;
	watcher_handler_func_t *w = NULL;
	int len;

	debug("stream_handle() name: %s type: %d wtype: %d\n", s->stream_name, type, watch);
	
	if ((int) watch == WATCH_READ)		audio = s->input;
	else if ((int) watch == WATCH_WRITE)	audio = s->output;

	debug("stream_handle() audio_t: 0x%x\n", audio);
	if (!audio) return -1;

	if ((int) watch == WATCH_READ)		w = audio->a->read_handler;
	else if ((int) watch == WATCH_WRITE)	w = audio->a->write_handler;

	debug("stream_handle() watch_t: 0x%x\n", w);
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


	if (len < 0) return -1;

	return 0;
}

int stream_create(char *name, audio_io_t *in, audio_codec_t *c, audio_io_t *out) {
	stream_t *s;

	debug("stream_create() name: %s in: 0x%x codec: 0x%x output: 0x%x\n", name, in, c, out);

	if (!in /* !out */) goto fail;	/* from? where? */

	debug("stream_create() infd: %d\n", in->fd);

	if (in->fd == -1) goto fail; /* reading from fd == -1? i don't think so... */


	s = xmalloc(sizeof(stream_t));
	s->stream_name	= xstrdup(name);
	s->input	= in;
	s->codec	= c;
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
	if (in)		in->a->control_handler(AUDIO_CONTROL_DEINIT, AUDIO_READ, NULL);
/*	if (c)		c->c->control_handler(AUDIO_CONTROL_DEINIT, AUDIO_RDWR, NULL); */
	if (out)	out->a->control_handler(AUDIO_CONTROL_DEINIT, AUDIO_WRITE, NULL);
	return 0;
}

audio_io_t *stream_audio_control(audio_control_t type, audio_way_t way, ...) {
	char *file = NULL;
	audio_io_t *aio = NULL;
	va_list ap;

	if (type == AUDIO_CONTROL_MODIFY) { debug("stream_audio_control() AUDIO_CONTROL_MODIFY called but we not support it right now, sorry\n"); return NULL; }

	va_start(ap, way);

	if (type == AUDIO_CONTROL_MODIFY || type == AUDIO_CONTROL_DEINIT) {
		aio = *(va_arg(ap, audio_io_t **));
	} else if (type == AUDIO_CONTROL_INIT) {
		aio = xmalloc(sizeof(audio_io_t));
	}

	if (type == AUDIO_CONTROL_CHECK || type == AUDIO_CONTROL_INIT || type == AUDIO_CONTROL_MODIFY) {
		char *attr;

		while ((attr = va_arg(ap, char *))) {
			char *val = va_arg(ap, char *);
			debug("[stream_audio_control] attr: %s value: %s\n", attr, val);
			if (!xstrcmp(attr, "file")) file = xstrdup(val);
		}

	} else if (type == AUDIO_CONTROL_DEINIT) {
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

/* if no file specified, continue with strange defaults ;) */
	if (!file && way == AUDIO_READ)		file = xstrdup("/dev/urandom");
	else if (!file && way == AUDIO_WRITE)	file = xstrdup("/dev/null");

	aio->a  = &stream_audio;
	aio->fd = -1;

	if (type == AUDIO_CONTROL_INIT && file) {
		aio->fd = open(file, 
			way == AUDIO_READ 	? O_RDONLY : 
			way == AUDIO_WRITE 	? O_WRONLY : 
						  O_RDWR);
	}

	xfree(file);

	va_end(ap);
	return aio;
}

int audio_initialize() {
	audio_register(&stream_audio);
#if 0
	stream_create("Now playing: /dev/urandom",
			stream_audio_control(AUDIO_CONTROL_INIT, AUDIO_READ, "file", "/dev/urandom"),	/* reading from /dev/urandom */
			NULL, /* no codec */
			stream_audio_control(AUDIO_CONTROL_INIT, AUDIO_WRITE, "file", "/dev/dsp")	/* writing to /dev/dsp */
		     );
#endif			
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
