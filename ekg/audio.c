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
		debug("[STREAM] name: %s\n", s->stream_name);
	}

	for (l = audio_inputs; l; l = l->next) {
		audio_t *a = l->data;
		debug("[AUDIO_CODEC] name: %s\n", a->name);
	}

/* XXX, /_stream --create "INPUT inputparams" "CODEC codecparams" "OUTPUT outputparams" */

/* /_stream --create "FILE file:dupa.raw" "FILE file:dupa_backup.raw" */ /* <--- ekg2 can copy files! :) */
/* /_stream --create "FILE file:dupa.raw" "CODEC rawcodec fortune-telling:ON" "LAME dupa.mp3" */
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

		/* READING / WRITING FROM FILEs */
WATCHER(stream_audio_read) {
	int maxlen = 4096, len;
	char *buf;
	/* XXX, w private moze byc ile maks. mozemy przeczytac itd... */
	
	buf = xmalloc(maxlen);
	len = read(fd, buf, maxlen);
	xfree(buf);

	debug("stream_audio_read() read: %d bytes from fd: %d\n", len, fd);
	return len;
}

WATCHER(stream_audio_write) {
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

	len = w(type, fd, watch, audio->private);

	if (len < 0) return -1;

	return 0;
}

int stream_create(char *name, audio_io_t *in, audio_codec_t *c, audio_io_t *out) {
	stream_t *s;

	debug("stream_create() name: %s in: 0x%x codec: 0x%x output: 0x%x\n", name, in, c, out);
	if (!in /* !out */) return 0;

	debug("stream_create() infd: %d\n", in->fd);

	if (in->fd == -1) return -1; /* fails */


	s = xmalloc(sizeof(stream_t));
	s->stream_name	= xstrdup(name);
	s->input	= in;
	s->codec	= c;
	s->output	= out;

	list_add(&streams, s, 0);

	watch_add(NULL, in->fd, WATCH_READ, stream_handle, s);

	return 1;
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
