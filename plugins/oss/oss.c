#include <sys/ioctl.h>
#include <linux/soundcard.h>
#include <unistd.h>
#include <fcntl.h>

#include <ekg/audio.h>
#include <ekg/plugins.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

char *config_audio_device = NULL;

int oss_voice_fd = -1;
int usage = 0;
int loop_usage;

char oss_buf_loop[4097];

PLUGIN_DEFINE(oss, PLUGIN_AUDIO, NULL);
AUDIO_DEFINE(oss);

/* mhhh. what about /dev/mixer? */

typedef struct {
	int freq;		/* SNDCTL_DSP_SPEED */
	int sample;		/* SNDCTL_DSP_SAMPLESIZE */
	int channels;		/* SNDCTL_DSP_CHANNELS */

	int bufsize;		/* SNDCTL_DSP_GETBLKSIZE */
} oss_private_t;

WATCHER(oss_audio_read) {
	int len;
	if (type == 1) {
		return 0;
	}
	len = read(fd, oss_buf_loop, 4096);
	debug("OSS READ: %d bytes from %d\n", len, fd);
	stream_buffer_resize((stream_buffer_t *) watch, oss_buf_loop, len);
	return len;
}

WATCHER(oss_audio_write) {
	oss_private_t *priv = data;
	stream_buffer_t *buf = (stream_buffer_t *) watch;
	int len, maxlen;
	if (type == 1) return 0;

	if (!data)	maxlen = buf->len; 	/* FUUUJ */
	else		maxlen = priv->bufsize;

	if (maxlen > buf->len) maxlen = buf->len;

	len = write(fd, buf->buf, maxlen);
	debug("OSS WRITE: %d bytes from %d\n", len, fd);
	stream_buffer_resize(buf, NULL, -len);
	return len;

}

AUDIO_CONTROL(oss_audio_control) {
	audio_io_t *aio = NULL;
	va_list ap;

	if (type == AUDIO_CONTROL_MODIFY) { debug("stream_audio_control() AUDIO_CONTROL_MODIFY called but we not support it right now, sorry\n"); return NULL; }

	va_start(ap, way);

	if (type == AUDIO_CONTROL_INIT) {
		char *attr;
		char *device = NULL;

		char *pathname;
		int voice_fd, value;
		oss_private_t *priv = xmalloc(sizeof(oss_private_t));

		while ((attr = va_arg(ap, char *))) {
			char *val = va_arg(ap, char *);
			int v;

			debug("[oss_audio_control] attr: %s value: %s\n", attr, val);
			if (!xstrcmp(attr, "device")) device = xstrdup(val);
			else if (!xstrcmp(attr, "freq"))	{ v = atoi(val);	priv->freq = v;		} 
			else if (!xstrcmp(attr, "sample"))	{ v = atoi(val);	priv->sample = v;	} 
			else if (!xstrcmp(attr, "channels"))	{ v = atoi(val);	priv->channels = v;	} 
		}
		pathname = (device) ? device : config_audio_device;

		if (!device && (oss_voice_fd != -1)) { 
			usage++;
			voice_fd = oss_voice_fd;
		} else if ((voice_fd = open(pathname, O_RDWR)) == -1) goto fail;
		if (!device) { 
			if (usage > 0) { 
				debug("SORRY, currently only once oss stream can be used..\n");
				return NULL;
			}
			usage++;
		}
		
		if (!priv->freq) priv->freq = 8000;
		ioctl(voice_fd, SNDCTL_DSP_SPEED, &(priv->freq));

		if (!priv->sample) priv->sample = 16;
		ioctl(voice_fd, SNDCTL_DSP_SAMPLESIZE, &(priv->sample));

		if (!priv->channels) priv->channels = 1;
		ioctl(voice_fd, SNDCTL_DSP_CHANNELS, &(priv->channels));

		value = AFMT_S16_LE;
		ioctl(voice_fd, SNDCTL_DSP_SETFMT, &value);
		
		if ((ioctl(voice_fd, SNDCTL_DSP_GETBLKSIZE, &(priv->bufsize)) == -1)) {
			/* to mamy problem.... */
			priv->bufsize = 4096;
		}

		aio = xmalloc(sizeof(audio_io_t));
		aio->a  = &oss_audio;
		aio->fd = voice_fd;
		aio->private = priv;

fail:
		xfree(device);
	} else if (type == AUDIO_CONTROL_DEINIT) {
		aio = *(va_arg(ap, audio_io_t **));

#if 0
		if (fd == -1) return -1;
		usage--;
		if (fd != oss_voice_fd || --usage == 0) { 
			close(fd);
			if (fd == oss_voice_fd) oss_voice_fd = -1;
			return 0;
		}
		return usage;
#endif
		if (aio && aio->private) {
			xfree(aio->private);
		}
		xfree(aio);
		aio = NULL;
	} else if (type == AUDIO_CONTROL_HELP) {
		debug("[stream_audio_control] known atts:\n"
			"	--device <DEV>>\n"
			" ....\n"
			"this audio_t can WORK in two-ways READ/WRITE\n");
	}

	va_end(ap);
	return aio;
}

QUERY(oss_setvar_default) {
	xfree(config_audio_device);
	config_audio_device = xstrdup("/dev/dsp");
	return 0;
}

int oss_plugin_init(int prio) {
	plugin_register(&oss_plugin, prio);
	oss_setvar_default(NULL, NULL);
	audio_register(&oss_audio);

	variable_add(&oss_plugin, TEXT("audio_device"), VAR_STR, 1, &config_audio_device, NULL, NULL, NULL);
	query_connect(&oss_plugin, "set-vars-default", oss_setvar_default, NULL);

	return 0;
}

static int oss_plugin_destroy() {
	audio_unregister(&oss_audio);
	return 0;
}
