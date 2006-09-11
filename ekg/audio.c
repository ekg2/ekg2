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
#include "plugins.h"
#include "themes.h"
#include "stuff.h"
#include "xmalloc.h"

list_t audio_codecs;
list_t audio_inputs;
list_t streams;

AUDIO_DEFINE(stream);

/* *.wav I/O stolen from xawtv (recode program) which was stolen from cdda2wav */
/* Copyright (C) by Heiko Eissfeldt */

typedef uint8_t   BYTE;
typedef uint16_t  WORD;
typedef uint32_t  DWORD;
typedef uint32_t  FOURCC;	/* a four character code */

/* flags for 'wFormatTag' field of WAVEFORMAT */
#define WAVE_FORMAT_PCM 1

/* MMIO macros */
#define mmioFOURCC(ch0, ch1, ch2, ch3) \
  ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | \
  ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))

#define FOURCC_RIFF	mmioFOURCC ('R', 'I', 'F', 'F')
#define FOURCC_LIST	mmioFOURCC ('L', 'I', 'S', 'T')
#define FOURCC_WAVE	mmioFOURCC ('W', 'A', 'V', 'E')
#define FOURCC_FMT	mmioFOURCC ('f', 'm', 't', ' ')
#define FOURCC_DATA	mmioFOURCC ('d', 'a', 't', 'a')

typedef struct CHUNKHDR {
    FOURCC ckid;		/* chunk ID */
    DWORD dwSize; 	        /* chunk size */
} CHUNKHDR;

/* simplified Header for standard WAV files */
typedef struct WAVEHDR {
    CHUNKHDR chkRiff;
    FOURCC fccWave;
    CHUNKHDR chkFmt;
    WORD wFormatTag;	   /* format type */
    WORD nChannels;	   /* number of channels (i.e. mono, stereo, etc.) */
    DWORD nSamplesPerSec;  /* sample rate */
    DWORD nAvgBytesPerSec; /* for buffer estimation */
    WORD nBlockAlign;	   /* block size of data */
    WORD wBitsPerSample;
    CHUNKHDR chkData;
} WAVEHDR;

#define cpu_to_le32(x) (x)
#define cpu_to_le16(x) (x)
#define le32_to_cpu(x) (x)
#define le16_to_cpu(x) (x)

/*********************************************************************************/

typedef struct {
	char *file;
	char *format;
	WAVEHDR *wave;
	int size;
} stream_private_t; 

COMMAND(cmd_streams) {
	const char **create = NULL;
	int display = 0;
	list_t l;

/* /_stream --create "INPUT inputparams"    "CODEC codecparams"			"OUTPUT outputparams" */

/* /_stream --create "STREAM file:dupa.raw" 					"STREAM file:dupa_backup.raw"	*/ /* <--- ekg2 can copy files! :) */
/* /_stream --create "STREAM file:dupa.raw" "CODEC rawcodec fortune-telling:ON" "LAME file:dupa.mp3" 		*/
/* /_stream --create "STREAM file:/dev/tcp host:jakishost port:port http:radio" "STREAM file:radio.raw"		*/ /* and download files too? :) */

/* i think that api if i (we?) write it... will be nice, but code can be obscure... sorry */

	if (match_arg(params[0], 'c', "create", 2)) {		/* --create */
		create = &params[1];
	} else if (match_arg(params[0], 'l', "list", 2) || !params[0]) {	/* list if --list. default action is --list (if we don't have params)  */
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
			char **arr;
			debug("[AUDIO_INPUT] name: %s\n", a->name);

			for (arr = (char **) a->control_handler(AUDIO_CONTROL_HELP, AUDIO_RDWR, NULL); arr && *arr;) {
				char *attr = *(arr++);
				char *val  = *(arr++);
				debug("... AUDIO_CONTROL_HELP: %s %s\n", attr, val);
			}
		}

		for (l = audio_codecs; l; l = l->next) {
			codec_t *c = l->data;
			char **arr;
			debug("[AUDIO_CODEC] name: %s\n", c->name);

			for (arr = (char **) c->control_handler(AUDIO_CONTROL_HELP, AUDIO_RDWR, NULL); arr && *arr;) {
				char *attr = *(arr++);
				char *val  = *(arr++);
				debug("... AUDIO_CONTROL_HELP: %s %s\n", attr, val);
			}
		}
	} 
	return 0;
}

codec_t *codec_find(const char *name) {
	list_t l;
	if (!name) 
		return NULL;
	for (l = audio_codecs; l; l = l->next) {
		codec_t *c = l->data;
		if (!xstrcmp(c->name, name)) 
			return c;

	}
	return NULL;
}

int codec_register(codec_t *codec) {
	if (!codec)			return -1;
	if (codec_find(codec->name))	return -2;
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
			unsigned long temp = priv->size + sizeof(WAVEHDR) - sizeof(CHUNKHDR);
			priv->wave->chkRiff.dwSize = cpu_to_le32(temp);
			priv->wave->chkData.dwSize = cpu_to_le32(priv->size);

			if ((lseek(fd, 0, SEEK_SET) != (off_t) -1) && ((write(fd, priv->wave, sizeof(WAVEHDR)) == sizeof(WAVEHDR)))) {
				debug("[stream_audio_write] type == 1. UPDATING WAVEHDR succ. size: %d\n", priv->size);
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
			WAVEHDR *fileheader	= xmalloc(sizeof(WAVEHDR));

			if (way == AUDIO_READ) {
				if ((read(aio->fd, fileheader, sizeof(fileheader)) != sizeof(fileheader))) {
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
			} else if (way == AUDIO_WRITE) {
				char *freq = NULL, *sample = NULL, *channels = NULL;

				if (co) co->c->control_handler(AUDIO_CONTROL_GET, AUDIO_WRITE, co, "freq", &freq, "sample", &sample, "channels", &channels, NULL);
				else	out->a->control_handler(AUDIO_CONTROL_GET, AUDIO_WRITE, out, "freq", &freq, "sample", &sample, "channels", &channels, NULL);
				debug("[stream_audio_control] WAVE: AUDIO_CONTROL_SET freq: %s sample: %s channels: %s\n", freq, sample, channels);

				if (freq && sample && channels) {
					int rate		= atoi(freq);
					int nchannels		= atoi(channels);
					int nBitsPerSample	= atoi(sample);

					/* stolen from xawtv && cdda2wav */
					unsigned long nBlockAlign = nchannels * ((nBitsPerSample + 7) / 8);
					unsigned long nAvgBytesPerSec = nBlockAlign * rate;
					unsigned long temp = /* data length */ 0 + sizeof(WAVEHDR) - sizeof(CHUNKHDR);

					fileheader->chkRiff.ckid    = cpu_to_le32(FOURCC_RIFF);
					fileheader->fccWave         = cpu_to_le32(FOURCC_WAVE);
					fileheader->chkFmt.ckid     = cpu_to_le32(FOURCC_FMT);
					fileheader->chkFmt.dwSize   = cpu_to_le32(16);
					fileheader->wFormatTag      = cpu_to_le16(WAVE_FORMAT_PCM);
					fileheader->nChannels       = cpu_to_le16(nchannels);
					fileheader->nSamplesPerSec  = cpu_to_le32(rate);
					fileheader->nAvgBytesPerSec = cpu_to_le32(nAvgBytesPerSec);
					fileheader->nBlockAlign     = cpu_to_le16(nBlockAlign);
					fileheader->wBitsPerSample  = cpu_to_le16(nBitsPerSample);
					fileheader->chkData.ckid    = cpu_to_le32(FOURCC_DATA);
					fileheader->chkRiff.dwSize  = cpu_to_le32(temp);
					fileheader->chkData.dwSize  = cpu_to_le32(0 /* data length */);

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

				} else	succ = NULL;
				xfree(freq); xfree(sample); xfree(channels);
			}
			priv->wave = fileheader;
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
			"-stream",			"", 		/* bidirectional, no required params */
			"-stream:file", 		"*",		/* bidirectional, file, everythink can be passed as param */
			"-stream:format", 		"raw pcm wave",	/* bidirectional, format, possible vars: 'raw' 'pcm' 'wave' */
			NULL, };
		return arr;
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
	if (len == -1) return 0;
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
				memmove(audio->buffer->str, audio->buffer->str + res, audio->buffer->len - res);
				audio->buffer->len -= res;
			}
		}
		/* if this is read handler, and we don't have watch handler for writing stream->out->fd == -1 then we don't need to wait for WATCH_WRITE and we here do it */
		if (s->output->fd == -1) {
			int res;
			debug("[audio_handle_write] in queue: %d bytes.... ", s->output->buffer->len);
			res = s->output->a->write_handler(type, -1, s->output->buffer, s->output->private);
			debug(" ... wrote:%d bytes (handler: 0x%x) ", res, s->output->a->write_handler);
			if (res > 0) {
				memmove(s->output->buffer->str, s->output->buffer->str + res, s->output->buffer->len - res);
				s->output->buffer->len -= res;
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

	list_add(&streams, s, 0);

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
	if (in)		{ in->a->control_handler(AUDIO_CONTROL_DEINIT, AUDIO_READ, in);		close(in->fd);	xfree(in); }
	if (co)		{ co->c->control_handler(AUDIO_CONTROL_DEINIT, AUDIO_RDWR, co); 	;		xfree(co); }
	if (out)	{ out->a->control_handler(AUDIO_CONTROL_DEINIT, AUDIO_WRITE, out);	close(out->fd);	xfree(out); }
	return 0;
}

audio_io_t *stream_as_audio(stream_t *s) {
	return NULL;

}

int audio_initialize() {
	audio_t *inp, *out;
	codec_t *co;
	audio_register(&stream_audio);

	if (0 && (inp = audio_find("stream") && (out = audio_find("stream"))))
		stream_create("Now playing: /dev/urandom",
				__AINIT(inp, AUDIO_READ, "file", "/dev/urandom"),	/* reading from /dev/urandom */
				NULL, 							/* no codec */
				__AINIT(out, AUDIO_WRITE, "file", "/dev/dsp")		/* writing to /dev/dsp */);
	if (0 && (inp = audio_find("oss")) && (out = audio_find("stream")))
		stream_create("Now recording: /dev/dsp",
				__AINIT(inp, AUDIO_READ, "freq", "8000", "sample", "16", "channels", "1"),
				NULL,
				__AINIT(out, AUDIO_WRITE, "file", "plik.raw", "format", "raw"));
	if (0 &&  (out = audio_find("oss")) && (inp = audio_find("stream"))) 
		stream_create("Now playing to: /dev/dsp",
				__AINIT(inp, AUDIO_READ, "file", "plik.raw"),
				NULL,
				__AINIT(out, AUDIO_WRITE, "freq", "8000", "sample", "16", "channels", "1"));
	if (0 && (out = audio_find("stream")) && (inp = audio_find("oss"))) 
		stream_create("Now recoring /dev/dsp to WAVE file.",
				__AINIT(inp, AUDIO_READ, "freq", "44100", "sample", "16", "channels", "2"),
				NULL,
				__AINIT(out, AUDIO_WRITE, "file", "plik.wav", "format", "wave"));
	if (0 && (out = audio_find("stream")) && (inp = audio_find("stream")) && (co = codec_find("pcm"))) 
		stream_create("Now let's recode some pcm data...",
				__AINIT(inp, AUDIO_READ, "file", "plik.raw", "format", "raw"),
				__CINIT(co,  "ifreq", "8000", "ofreq", "44100", "sample", "16", "channels", "1"),
				__AINIT(out, AUDIO_WRITE, "file", "recoded.wav", "format", "wave"));
	return 0;
#undef __AINIT
#undef __CINIT
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
