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
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "audio.h"
#include "commands.h"
#include "debug.h"
#include "dynstuff.h"
#include "dynstuff_inline.h"
#include "plugins.h"
#include "themes.h"
#include "stuff.h"
#include "xmalloc.h"

#include "audio_wav.h"

codec_t *audio_codecs;
audio_t *audio_inputs;
stream_t *streams;

AUDIO_DEFINE(stream);

DYNSTUFF_LIST_DECLARE_NF(audio_inputs, audio_t, 
	static __DYNSTUFF_LIST_ADD,		/* audio_inputs_add() */
	static __DYNSTUFF_LIST_UNLINK)		/* audio_inputs_unlink() */

DYNSTUFF_LIST_DECLARE_NF(audio_codecs, codec_t, 
	static __DYNSTUFF_LIST_ADD,		/* audio_codecs_add() */
	static __DYNSTUFF_LIST_UNLINK)		/* audio_codecs_unlink() */

static __DYNSTUFF_LIST_ADD(streams, stream_t, NULL);	/* streams_add() */

/*********************************************************************************/

typedef struct {
	audio_io_t *parent;
	char *file;
	char *format;
	WAVEHDR *wave;
} stream_private_t; 

COMMAND(cmd_streams) {

/* /_stream --create "INPUT inputparams"    "CODEC codecparams"			"OUTPUT outputparams" */

/* /_stream --create "STREAM file:dupa.raw" 					"STREAM file:dupa_backup.raw"	*/ /* <--- ekg2 can copy files! :) */
/* /_stream --create "STREAM file:dupa.raw" "CODEC rawcodec fortune-telling:ON" "LAME file:dupa.mp3" 		*/
/* /_stream --create "STREAM file:/dev/tcp host:jakishost port:port http:radio" "STREAM file:radio.raw"		*/ /* and download files too? :) */

/* i think that api if i (we?) write it... will be nice, but code can be obscure... sorry */

	if (!params[0] || match_arg(params[0], 'l', "list", 2) || !params[0]) {	/* list if --list. default action is --list (if we don't have params)  */
		stream_t *sl;
		audio_t *al;
		codec_t *cl;

		/* XXX, add more debug info, get info using  */
		for (sl = streams; sl; sl = sl->next) {
			stream_t *s = sl;

			/* co->c->control_handler(AUDIO_CONTROL_GET, AUDIO_WRITE, co, "freq", &freq, "sample", &sample, "channels", &channels, NULL); */
			
			printq("stream_info", s->stream_name);
			if (s->input) {
				printq("stream_info_in", s->input->a->name, itoa(s->input->fd), itoa(s->input->buffer->len), itoa(s->input->outb));
			}

			if (s->output) {
				printq("stream_info_out",s->output->a->name, itoa(s->output->fd), itoa(s->output->buffer->len), itoa(s->input->outb));
			}

			debug("[STREAM] name: %s IN: 0x%x CODEC: 0x%x OUT: 0x%x\n", s->stream_name, s->input, s->codec, s->output);
			if (s->input)	debug("	 IN, AUDIO: %s fd: %d bufferlen: %d\n", s->input->a->name, s->input->fd, s->input->buffer->len);
			if (s->output)	debug("	OUT, AUDIO: %s fd: %d bufferlen: %d\n", s->output->a->name, s->output->fd, s->output->buffer->len);
		}

		for (al = audio_inputs; al; al = al->next) {
			audio_t *a = al;
			char **arr;

			printq("audio_device", a->name);

			debug("[AUDIO_INPUT] name: %s\n", a->name);

			for (arr = (char **) a->control_handler(AUDIO_CONTROL_HELP, AUDIO_RDWR, NULL); arr && *arr;) {
				char *attr = *(arr++);
				char *val  = *(arr++);
				debug("... AUDIO_CONTROL_HELP: %s %s\n", attr, val);
			}
		}

		for (cl = audio_codecs; cl; cl = cl->next) {
			codec_t *c = cl;
			char **arr;

			printq("audio_codec", c->name);
			debug("[AUDIO_CODEC] name: %s\n", c->name);

			for (arr = (char **) c->control_handler(AUDIO_CONTROL_HELP, AUDIO_RDWR, NULL); arr && *arr;) {
				char *attr = *(arr++);
				char *val  = *(arr++);
				debug("... AUDIO_CONTROL_HELP: %s %s\n", attr, val);
			}
		}
		return 0;
	}

	if (match_arg(params[0], 'c', "create", 2)) {		/* --create */
		const char **p = &params[1];

		const char *input;
		const char *codec;
		const char *output;
		const char *name = NULL;

#if 0
		if (match_arg(p[0], 'n', "name", 2) && p[1]) {		/* --name */
			name = p[1];
			p = &p[2];
		}
#endif

		if (!p[0] || !p[1]) {
			printq("not_enough_params", name);
			return -1; 
		}

		input = p[1];				/* input */
		codec = p[2] ? p[2] : NULL;		/* codec if we have 3 params.. otherwise NULL */
		output = !p[2] ? p[1] : p[2];		/* output */

		{
			audio_t *input_s;
			codec_t *codec_s;
			audio_t *output_s;

			char **input_array = array_make(input, " ", 0, 1, 1);
			char **codec_array = codec ? array_make(codec, " ", 0, 1, 1) : NULL;
			char **output_array = array_make(output, " ", 0, 1, 1);
			
			input_s = audio_find(input_array[0]);
			codec_s = codec ? codec_find(codec_array[0]) : NULL;
			output_s = audio_find(output_array[0]);

			if (!input_s)
				printq("audio_not_found", input_array[0]);
			else if (!codec_s && codec)
				printq("codec_not_found", codec_array[0]);
			else if (!output_s)
				printq("audio_not_found", output_array[0]);
			else {
				/* XXX inicjuj */

			}

			array_free(input_array);
			array_free(codec_array);
			array_free(output_array);
		}

		return 0;
	}

	if (match_arg(params[0], 'C', "close", 2)) {		/* --close */
		return -1;
	}

	printq("invalid_params", name);
	
	return -1;
}

/**
 * codec_find()
 *
 * Find codec_t by @a name
 *
 * @param name - name of codec_t
 *
 * @return if @a name founded, return struct describing it, else NULL
 */

codec_t *codec_find(const char *name) {
	codec_t *cl;

	if (!name) 
		return NULL;
	for (cl = audio_codecs; cl; cl = cl->next) {
		codec_t *c = cl;
		if (!xstrcmp(c->name, name)) 
			return c;

	}
	return NULL;
}

/**
 * codec_register()
 *
 * Register new codec_t (@a codec)
 *
 * @note	This should be done @@ *_plugin_init() and just before
 * 		plugin_register() If codec_register() fails (return not 0)
 * 		than you <b>should NOT</b> call plugin_register() only display some info...
 * 		and return -1
 *
 * @param codec - codec_t to register
 *
 * @sa codec_unregister() - to unregister codec_t
 *
 * @return 	-1 if invalid params (@a codec NULL)<br>
 * 		-2 if codec with such name already exists<br>
 * 		 0 on success
 */

int codec_register(codec_t *codec) {
	if (!codec)			return -1;
	if (codec_find(codec->name))	return -2;

	audio_codecs_add(codec);
	return 0;
}

/**
 * audio_unregister()
 *
 * Unregister codec_t
 *
 * @note 	This should be done @@ *_plugin_destroy() just before
 * 		plugin_unregister()
 *
 * @param codec - codec_t to unregister
 */

void codec_unregister(codec_t *codec) {
	if (!codec) return;

	/* XXX here, we should search for <b>all</b> streams using this codec, and unload them */
	audio_codecs_unlink(codec);
}

/**
 * audio_find()
 *
 * Find audio_t by @a name
 *
 * @param name - name of audio_t
 *
 * @return if @a name founded, return struct describing it. else NULL
 */

audio_t *audio_find(const char *name) {
	audio_t *al;

	if (!name) 
		return NULL;

	for (al = audio_inputs; al; al = al->next) {
		audio_t *a = al;
		if (!xstrcmp(a->name, name)) 
			return a;

	}
	return NULL;
}

/**
 * audio_register()
 *
 * Register new audio I/O (@a audio)
 *
 * @note 	This should be done @@ *_plugin_init() and just before 
 * 		plugin_register() If audio_register() fails (return not 0)
 * 		than you <b>should NOT</b> call plugin_register() only display some info...
 * 		and return -1
 *
 * @param audio - audio_t to register
 *
 * @sa audio_unregister() - to unregister audio_t
 *
 * @return 	-1 if invalid params (@a audio NULL)<br>
 * 		-2 if audio with such name already exists<br>
 * 		 0 on success
 */

int audio_register(audio_t *audio) {
	if (!audio)			return -1;
	if (audio_find(audio->name))	return -2;

	audio_inputs_add(audio);
	return 0;
}

/**
 * audio_unregister()
 *
 * Unregister audio_t
 *
 * @note 	This should be done @@ *_plugin_destroy() just before
 * 		plugin_unregister()
 *
 * @param audio - audio_t to unregister
 */

void audio_unregister(audio_t *audio) {
	if (!audio) return;

	/* XXX here, we should search for <b>all</b> streams using this audio, and unload them */
	audio_inputs_unlink(audio);
}
		/* READING / WRITING FROM FILEs */
WATCHER_AUDIO(stream_audio_read) {
	int maxlen = 4096, len;
	char *sbuf;
	
	sbuf = xmalloc(maxlen);
	len = read(fd, sbuf, maxlen);

	string_append_raw(buf, sbuf, len);

	xfree(sbuf);

	debug("stream_audio_read() read: %d bytes from fd: %d\n", len, fd);
	if (len == 0) return -1;
	return len;
}

WATCHER_AUDIO(stream_audio_write) {
	stream_private_t *priv = data;

	if (!data) {
		debug("[stream_audio_write] GENERALERROR: DATA MUST BE NOT NULL\n");
		return -1;
	}

	if (type == 1) {
		if (priv->wave) {
			unsigned long temp = priv->parent->outb + sizeof(WAVEHDR) - sizeof(CHUNKHDR);
			priv->wave->chkRiff.dwSize = cpu_to_le32(temp);
			priv->wave->chkData.dwSize = cpu_to_le32(priv->parent->outb);

			if ((lseek(fd, 0, SEEK_SET) != (off_t) -1) && ((write(fd, priv->wave, sizeof(WAVEHDR)) == sizeof(WAVEHDR)))) {
				debug("[stream_audio_write] type == 1. UPDATING WAVEHDR succ. size: %d\n", priv->parent->outb);
			} else	debug("[stream_audio_write] type == 1. UPDATING WAVEHDR fail\n");
		}
		close(fd);	/* mh? */
		return 0;
	}

	return write(fd, buf->str, buf->len);
}

AUDIO_CONTROL(stream_audio_control) {
	va_list ap;

	if (type == AUDIO_CONTROL_INIT) {
		stream_private_t *priv;

		audio_codec_t *co;
		audio_io_t *out;
		char *directory = NULL;

		void *succ = (void *) 1;

		va_start(ap, aio);
		co	= va_arg(ap, audio_codec_t *);
		out	= va_arg(ap, audio_io_t *);
		va_end(ap);

		if (!aio || !out) return NULL;
		if (!(priv = aio->private)) return NULL;

		if (way == AUDIO_READ)	directory = "__input";
		if (way == AUDIO_WRITE) directory = "__output";

	/* :) */
		if (co) 
			co->c->control_handler(AUDIO_CONTROL_SET, AUDIO_RDWR, co, directory, "stream", NULL);
			out->a->control_handler(AUDIO_CONTROL_SET, !way, out, directory, "stream", NULL);

		if (!xstrcmp(priv->format, "wave")) {

			if (way == AUDIO_READ) {
				WAVEHDR *fileheader	= xmalloc(sizeof(WAVEHDR));

				if ((read(aio->fd, fileheader, sizeof(WAVEHDR)) == sizeof(WAVEHDR))) {
#define __SET(args...) 	  (co) ? co->c->control_handler(AUDIO_CONTROL_SET, AUDIO_RDWR, co, args) : \
				out->a->control_handler(AUDIO_CONTROL_SET, AUDIO_RDWR, co, args)
					int freq	= le32_to_cpu(fileheader->nSamplesPerSec);
					int channels	= le16_to_cpu(fileheader->nChannels);
					int sample	= le16_to_cpu(fileheader->wBitsPerSample);

					debug("[stream_audio_control] WAVE: SUCC read WAVE HEADER freq: %d channels: %d sample: %d\n", freq, channels, sample);

					__SET("freq", itoa(freq), "channels", itoa(channels), "sample", itoa(sample));

					xfree(priv->format);
					priv->format = xstrdup("pcm");
				} else {
					debug("[stream_audio_control] WAVE: reading WAVE HEADER failed... mhh... FALLBACK on raw format\n");
					lseek(aio->fd, 0, SEEK_SET);
					xfree(fileheader);
					fileheader = NULL;

					xfree(priv->format);
					priv->format = xstrdup("raw");
				}

				priv->wave = fileheader;
			} else if (way == AUDIO_WRITE) {
				WAVEHDR *fileheader;
				char *freq = NULL, *sample = NULL, *channels = NULL;

				if (co) co->c->control_handler(AUDIO_CONTROL_GET, AUDIO_WRITE, co, "freq", &freq, "sample", &sample, "channels", &channels, NULL);
				else	out->a->control_handler(AUDIO_CONTROL_GET, AUDIO_WRITE, out, "freq", &freq, "sample", &sample, "channels", &channels, NULL);
				debug("[stream_audio_control] WAVE: AUDIO_CONTROL_SET freq: %s sample: %s channels: %s\n", freq, sample, channels);

				if (!(fileheader = audio_wav_set_header(freq, sample, channels)))
					return NULL;

				lseek(aio->fd, 0, SEEK_SET);
				if ((write(aio->fd, fileheader, sizeof(WAVEHDR)) == sizeof(WAVEHDR))) {
					debug("[stream_audio_control] WAVE: SUCC write WAVE HEADER\n");
					xfree(priv->format);
					priv->format = xstrdup("pcm");
				} else {
					debug("[stream_audio_control] WAVE: writting WAVE HEADER failed... mhh... FALLBACK on raw format\n");
					lseek(aio->fd, 0, SEEK_SET);
					xfree(fileheader);
					fileheader = NULL;

					xfree(priv->format);
					priv->format = xstrdup("raw");
				}

				xfree(freq); xfree(sample); xfree(channels);

				priv->wave = fileheader;
			}
		}
		return succ;
	} else if ((type == AUDIO_CONTROL_SET && !aio) || (type == AUDIO_CONTROL_GET && aio)) {
		stream_private_t *priv = NULL;

		char *attr;
		const char *file = NULL, *format = NULL;

		int fd = -1;
		int suc = 1;

		if (type == AUDIO_CONTROL_GET && !(priv = aio->private)) return NULL;

		va_start(ap, aio);
		while ((attr = va_arg(ap, char *))) {
			if (type == AUDIO_CONTROL_GET) {
				char **val = va_arg(ap, char **);
				debug("[stream_audio_control] AUDIO_CONTROL_GET attr: %s value: 0x%x\n", attr, val);
				if (!xstrcmp(attr, "format"))	*val = xstrdup(priv->format);
				else				*val = NULL;
			} else {
				char *val = va_arg(ap, char *);
				debug("[stream_audio_control] AUDIO_CONTROL_SET attr: %s value: %s\n", attr, val);
				if (!xstrcmp(attr, "file"))		file	= val;
				else if (!xstrcmp(attr, "format"))	format	= val;
			}
		}
		va_end(ap);
		if (type == AUDIO_CONTROL_GET) return aio;

			/* if no file specified, continue with strange defaults ;) */
		if (!file && way == AUDIO_READ)		file = "/dev/urandom";
		else if (!file && way == AUDIO_WRITE)	file = "/dev/null";

		if (!format)				format = "raw";

		if (!xstrcmp(format, "guess")) {
			const char *fileext = xstrrchr(file, '.');
			format = "raw";

			if (fileext) {
				fileext++;
				if (!xstrcasecmp(fileext, "wav"))
					format = "wave";

				if (!xstrcasecmp(fileext, "ogg"))
					format = "ogg";
			}
		}

		if (xstrcmp(format, "raw") && xstrcmp(format, "pcm") && xstrcmp(format, "wave")) { 
			debug("[stream_audio_control] WRONG FORMAT: %s\n", format);
			return NULL;
		}

		if (suc && file) {
			fd = open(file, (
				way == AUDIO_READ 	? O_RDONLY : 
				way == AUDIO_WRITE 	? O_CREAT | O_TRUNC | O_WRONLY : O_CREAT | O_TRUNC | O_RDWR), S_IRUSR | S_IWUSR);

			if (fd == -1 && way == AUDIO_WRITE) 
				fd = open(file, (
					way == AUDIO_READ       ? O_RDONLY :
					way == AUDIO_WRITE      ? O_CREAT | O_WRONLY : O_CREAT | O_RDWR), S_IRUSR | S_IWUSR);

			if (fd == -1) { 
				debug("[stream_audio_control] OPENING FILE: %s FAILED %d %s!\n", file, errno, strerror(errno));
				return NULL;
			}
		}
		priv		= xmalloc(sizeof(stream_private_t));
		priv->file	= xstrdup(file);
		priv->format	= xstrdup(format);

		aio		= xmalloc(sizeof(audio_io_t));
		aio->a 		= &stream_audio;
		aio->fd		= fd;
		aio->private	= priv;

		priv->parent	= aio;

	} else if (type == AUDIO_CONTROL_DEINIT && aio) {
		if (aio->private) {
			stream_private_t *priv = aio->private;
			xfree(priv->file);
			xfree(priv->format);

			xfree(priv->wave);
		}
		aio = NULL;
	} else if (type == AUDIO_CONTROL_HELP) {
		static char *arr[] = { 
			"-stream",			"", 			/* bidirectional, no required params */
			"-stream:file", 		"*",			/* bidirectional, file, everythink can be passed as param */
			"-stream:format", 		"guess raw pcm wave",	/* bidirectional, format, possible vars: 'guess' 'raw' 'pcm' 'wave' */
			NULL, };
		return (void *) arr;
	}
	return aio;
}

WATCHER_LINE(stream_handle_write) {
	stream_t *s = data;
	audio_io_t *audio = NULL;
	audio_handler_func_t *w = NULL;
	int len;

/*	debug("stream_handle_write() name: %s type: %d fd: %d wtype: %d\n", s->stream_name, type, fd, watch); */

	audio = s->output;

	if (!audio) debug("stream_handle_write() audio_t: 0x%x\n", audio);
	if (!audio) return -1;

	if (type == 0 && !audio->buffer->len) {	/* oldcode, shouldn't happen */
		debug("stream_handle_write() ERROR ?\n");
		if (s->input->fd == -1 && !s->input->buffer->len) return -1;	/* we won't receive any NEW data from watcher with fd == -1 */
		return 0;							/* we should set w->type to 0... and only to WATCH_WRITE when we have smth to write... */
	}

	w = audio->a->write_handler;

	if (!w) debug("stream_handle() watch_t: 0x%x\n", w);
	if (!w) return -1;

	len = w(type, fd, audio->buffer, audio->private);

	if (type) {
		close(fd);
		audio->fd = fd		= -1;
		audio->buffer		= string_init(NULL);
	}
	if (!type) {
		if (len == -1 && (errno == EAGAIN || errno == EINTR)) 
			return 0;
		if (len > 0) 
			audio->outb += len;
	}
	return len;
}

WATCHER(stream_handle) {
	stream_t *s = data;
	audio_io_t *audio = NULL;
	audio_codec_t *codec;
	audio_handler_func_t *w = NULL;
	int len;

/*	debug("stream_handle() name: %s type: %d fd: %d wtype: %d\n", s->stream_name, type, fd, watch); */

	audio = s->input;
	codec = s->codec;

	if (!audio) debug("stream_handle() audio_t: 0x%x\n", audio);
	if (!audio) return -1;

	w = audio->a->read_handler;

	if (!w) debug("stream_handle() watch_t: 0x%x\n", w);
	if (!w) return -1;

	len = w(type, fd, audio->buffer, audio->private);

	if (len > 0 && s->output) {	/* if watch write do nothing */
		if (!codec) {
			s->input->outb += len;
			/* we just copy data from input->buf */
			string_append_raw(s->output->buffer, audio->buffer->str, audio->buffer->len);
			string_clear(s->input->buffer);
		} else {
			int res = -1;
			if (codec->way == CODEC_CODE) {
				res = codec->c->code_handler(type, audio->buffer, s->output->buffer, codec->private);
			} else if (s->codec->way == CODEC_DECODE) {
				res = codec->c->decode_handler(type, audio->buffer, s->output->buffer, codec->private);
			}
/*			debug("[AUDIO, CODEC, RECODE]: %d\n", res); */
			if (res > 0) {
				string_remove(audio->buffer, res);
				s->input->outb += res;
			}
		}
		/* if this is read handler, and we don't have watch handler for writing stream->out->fd == -1 then we don't need to wait for WATCH_WRITE and we here do it */
		if (s->output->fd == -1) {
			int res;
			debug("[audio_handle_write] in queue: %d bytes.... ", s->output->buffer->len);
			res = s->output->a->write_handler(type, -1, s->output->buffer, s->output->private);
			debug(" ... wrote:%d bytes (handler: 0x%x) ", res, s->output->a->write_handler);
			if (res > 0) {
				string_remove(s->output->buffer, res);
				s->output->outb += res;
			}
			debug(" ... left:%d bytes\n", s->output->buffer->len);
		}
	}

	if (type) {
		close(fd);
		audio->fd = fd = -1;
	}

	return len;
}

/**
 * stream_create()
 *
 * Function to create streams /input fd/ --> [codec function] --> /output fd or function/
 *
 * @note	@a in->fd must != -1, @a out->fd can be -1
 *
 * @todo 	Implement errors. make param , char **error<br>
 * 			Pass it to AUDIO_CONTROL_INIT and if smth fail, there should be allocated description of error.
 *
 * @todo	Implement stream_close()
 *
 * @return	1 on sucess [created stream_t] or 0 if fail.
 */

int stream_create(char *name, audio_io_t *in, audio_codec_t *co, audio_io_t *out) {
	stream_t *s;

	debug("stream_create() name: %s in: 0x%x codec: 0x%x output: 0x%x\n", name, in, co, out);

	if (!in || !out) goto fail;	/* from? where? */

	debug("stream_create() infd: %d outfd: %d\n", in->fd, out->fd);

	if (in->fd == -1) goto fail; /* reading from fd == -1? i don't think so... */

	if (!in->a->control_handler(AUDIO_CONTROL_INIT, AUDIO_READ, in, co, out))		goto fail;
	if (!out->a->control_handler(AUDIO_CONTROL_INIT, AUDIO_WRITE, out, co, in))		goto fail;
	if (co && !co->c->control_handler(AUDIO_CONTROL_INIT, AUDIO_RDWR, co, in, out))		goto fail;

	s = xmalloc(sizeof(stream_t));
	s->stream_name	= xstrdup(name);
	s->input	= in;
	s->codec	= co;
	s->output	= out;

	streams_add(s);

	watch_add(NULL, in->fd, WATCH_READ, stream_handle, s);
/* allocate buffers */
	in->buffer	= string_init(NULL);
	out->buffer 	= string_init(NULL);

	if (out->fd != -1) {
		watch_t *tmp	= watch_add_line(NULL, out->fd, WATCH_WRITE, stream_handle_write, s);
		tmp->buf	= out->buffer;
	} 

	return 1;
fail:
	/* deinit */
	if (in)		{ in->a->control_handler(AUDIO_CONTROL_DEINIT, AUDIO_READ, in);		if (in->fd != -1) close(in->fd);	xfree(in); }
	if (co)		{ co->c->control_handler(AUDIO_CONTROL_DEINIT, AUDIO_RDWR, co); 	;					xfree(co); }
	if (out)	{ out->a->control_handler(AUDIO_CONTROL_DEINIT, AUDIO_WRITE, out);	if (out->fd != -1) close(out->fd);	xfree(out); }
	return 0;
}

/**
 * stream_as_audio()
 *
 * @todo	Ever not begin.
 *
 * @note	This is not implemented, that was only idea howto do multiple encoding/decoding for example we want to decode MPEG 1 Layer 3 stream
 * 		and reencode it to OGG
 * 		so we could do:<br>
 *		<code>
		stream_create("Reencoding from MPEG to OGG",<br>
				__AINIT_F("stream", AUDIO_READ, "file", "plik.mp3", "format", "mp3"),		READING FROM FILE: plik.mp3 WITH FORMAT mp3<br>
				__CINIT_F("lame", ....),							INIT LAME CODEC<br>
				__AINI(stream_as_audio(								INIT ANOTHER STREAM, HERE WE HAVE DATA IN PCM FORMAT<br>
					stream_create("Reencoding from MPEG to OGG (part II)",<br>
						NULL,								WE PASS AS INPUT HERE NULL.<br>
						__CINIT_F("ogg", .....),					INIT OGG CODEC<br>
						__AINIT_F("stream", AUDIO_WRITE, "file", "plik.ogg", "format", "ogg")	WRITE OGG FILE TO DISK<br>
						))<br>
					)<br>
				);<br>
		</code>
 *		But it was only idea... and i had/have no time for it. For now... So if you really want this feature. implement it ;)
 *
 *
 * @param s - stream_t to convert.
 *
 * @return audio_io_t struct.	[NULL for now]
 */

audio_io_t *stream_as_audio(stream_t *s) {
	return NULL;
}

int audio_initialize() {
	audio_register(&stream_audio);

	/* move it to formats.c */
	format_add("audio_device", 	_("%> Audio device: %1"), 1);
	format_add("audio_codec",	_("%> Audio codec: %1"), 1);

	format_add("audio_not_found",	_("%! Audio device not found: %1"), 1);
	format_add("codec_not_found",	_("%! Codec not found: %1"), 1);

	format_add("stream_info",	_("%> Stream name: %1"), 1);
	format_add("stream_info_in",	_("%>  [IN] %[10]1 fd: %2 [Buffer: %3 bytes, out: %4 bytes]"), 1);	/* %1: device name, %2 fd, %3 size of buffer, %4 out bytes */
	format_add("stream_info_out",	_("%> [OUT] %[10]1 fd: %2 [Buffer: %3 bytes, out: %4 bytes]"), 1);	/* %1: device name, %2 fd, %3 size of buffer, %4 out bytes */

	return 0;
}

int audio_deinitialize() {
	/* trzeba dorobic */
	audio_unregister(&stream_audio);
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
